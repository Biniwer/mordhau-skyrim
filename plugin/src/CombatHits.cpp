#include "CombatInternal.h"

namespace dm::Combat
{
	inline namespace internal
	{
		void ThrowHeads()
		{
			for (auto it = g_pendingHeads.begin(); it != g_pendingHeads.end();) {
				auto actor = it->who.get();
				if (!actor) {
					it = g_pendingHeads.erase(it);
					continue;
				}
				const float t = Seconds(it->when);
				if (actor->IsDead() && actor->Decapitate()) {
					logger::info("gore: {}'s own head came off {:.2f} s after the blow", actor->GetDisplayFullName(), t);
					g_thrownHeads.push_back({ it->who, it->away, Clock::now(), 0 });
					CharacterRender::SeverHead(actor->GetFormID(), it->away, false);
					it = g_pendingHeads.erase(it);
				} else if (t > 1.0f) {
					const bool person = actor->HasKeywordString("ActorTypeNPC");
					logger::info("gore: Skyrim never took {}'s head off: {}", actor->GetDisplayFullName(), person ? "MORDHAU's head model flies" : "it stays on (no human head for this kind)");
					if (person) {
						CharacterRender::SeverHead(actor->GetFormID(), it->away, true);
					}
					it = g_pendingHeads.erase(it);
				} else {
					++it;
				}
			}
			for (auto it = g_thrownHeads.begin(); it != g_thrownHeads.end();) {
				const float t = Seconds(it->when);
				auto        actor = it->who.get();
				if (!actor || t > 1.5f || it->pushes >= 2) {
					it = g_thrownHeads.erase(it);
					continue;
				}
				// (the body goes limp a moment after the blow: the head is pushed once it is, and once more shortly after
				// in case the first came before its joint to the neck was cut)
				if (t > (it->pushes == 0 ? 0.10f : 0.22f) && actor->IsInRagdollState()) {
					auto* root = actor->Get3D(false);
					auto* head = root ? root->GetObjectByName("NPC Head [Head]") : nullptr;
					auto* collision = head ? head->GetCollisionObject() : nullptr;
					auto* body = collision ? collision->GetRigidBody() : nullptr;
					if (body) {
						const float        scale = RE::bhkWorld::GetWorldScale();
						const RE::NiPoint3 v = (it->away * BuildValue("head_throw_out") + RE::NiPoint3{ 0.0f, 0.0f, BuildValue("head_throw_up") }) * scale;
						body->SetLinearVelocity(RE::hkVector4(v.x, v.y, v.z, 0.0f));
						body->SetAngularVelocity(RE::hkVector4(it->away.y * 9.0f, -it->away.x * 9.0f, 2.0f, 0.0f));
						if (it->pushes == 0) {
							logger::info("gore: {}'s own head is thrown on MORDHAU's arc", actor->GetDisplayFullName());
						}
					} else if (it->pushes == 0) {
						logger::info("gore: {}'s head has no physics body to throw (it falls as Skyrim drops it)", actor->GetDisplayFullName());
					}
					++it->pushes;
				}
				++it;
			}
		}

		// Closest distance between segments p1-q1 and p2-q2.
		float SegmentDistance(const RE::NiPoint3& p1, const RE::NiPoint3& q1, const RE::NiPoint3& p2, const RE::NiPoint3& q2)
		{
			const RE::NiPoint3 d1 = q1 - p1, d2 = q2 - p2, r = p1 - p2;
			const float        a = d1.Dot(d1), e = d2.Dot(d2), f = d2.Dot(r);
			float              sc = 0.0f, tc = 0.0f;
			if (a <= 1e-6f && e <= 1e-6f) {
				return r.Length();
			}
			if (a <= 1e-6f) {
				tc = std::clamp(f / e, 0.0f, 1.0f);
			} else {
				const float c = d1.Dot(r);
				if (e <= 1e-6f) {
					sc = std::clamp(-c / a, 0.0f, 1.0f);
				} else {
					const float bb = d1.Dot(d2), denom = a * e - bb * bb;
					sc = denom > 1e-6f ? std::clamp((bb * f - c * e) / denom, 0.0f, 1.0f) : 0.0f;
					tc = (bb * sc + f) / e;
					if (tc < 0.0f) {
						tc = 0.0f;
						sc = std::clamp(-c / a, 0.0f, 1.0f);
					} else if (tc > 1.0f) {
						tc = 1.0f;
						sc = std::clamp((bb - c) / a, 0.0f, 1.0f);
					}
				}
			}
			return ((p1 + d1 * sc) - (p2 + d2 * tc)).Length();
		}

		// The weapon as a segment from the hand to its far end (sheets/hooks.json: weapon_trace). The far end comes
		// from the bounds of the weapon model hanging under the WEAPON node, so it does not depend on the model's axes.
		std::optional<std::pair<RE::NiPoint3, RE::NiPoint3>> Blade(RE::Actor* a_actor)
		{
			auto* root = a_actor->Get3D(false);
			auto* weapon = root ? root->GetObjectByName("WEAPON") : nullptr;
			auto* node = weapon ? weapon->AsNode() : nullptr;
			if (!node) {
				return std::nullopt;
			}
			const RE::NiAVObject* model = nullptr;
			for (auto& child : node->GetChildren()) {
				if (child && child->worldBound.radius > 1.0f && (!model || child->worldBound.radius > model->worldBound.radius)) {
					model = child.get();
				}
			}
			if (!model) {
				return std::nullopt;
			}
			const RE::NiPoint3 hand = weapon->world.translate;
			RE::NiPoint3       dir = model->worldBound.center - hand;
			const float        len = dir.Length();
			if (len < 1.0f) {
				return std::nullopt;
			}
			return std::pair{ hand, hand + dir * ((len + model->worldBound.radius) / len) };
		}

		// The first living character the blade touched between last frame and this one.
		RE::Actor* BladeTarget(RE::Actor* a_attacker, ActiveSwing& a_swing, float* a_bladeLength)
		{
			const auto blade = Blade(a_attacker);
			if (!blade) {
				return nullptr;
			}
			const auto& [hand, tip] = *blade;
			const RE::NiPoint3 mid = (hand + tip) * 0.5f;
			if (a_bladeLength) {
				*a_bladeLength = (tip - hand).Length();
			}
			constexpr float kBodyRadius = 22.0f, kBladeRadius = 6.0f;
			RE::Actor*      found = nullptr;
			RE::ProcessLists::GetSingleton()->ForEachHighActor([&](RE::Actor* a_target) {
				if (!a_target || a_target == a_attacker || a_target->IsDead() || !a_target->Is3DLoaded()) {
					return RE::BSContainer::ForEachResult::kContinue;
				}
				float height = a_target->GetHeight();
				if (height < 40.0f) {
					height = 120.0f;
				}
				const RE::NiPoint3 base = a_target->GetPosition() + RE::NiPoint3{ 0.0f, 0.0f, height * 0.15f };
				const RE::NiPoint3 top = a_target->GetPosition() + RE::NiPoint3{ 0.0f, 0.0f, height * 0.9f };
				float              dist = SegmentDistance(hand, tip, base, top);
				if (a_swing.prevTip) {
					// the paths the tip and the middle of the blade travelled since last frame
					dist = std::min(dist, SegmentDistance(*a_swing.prevTip, tip, base, top));
					dist = std::min(dist, SegmentDistance(*a_swing.prevMid, mid, base, top));
				}
				if (dist <= kBodyRadius + kBladeRadius) {
					found = a_target;
					return RE::BSContainer::ForEachResult::kStop;
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
			a_swing.prevTip = tip;
			a_swing.prevMid = mid;
			return found;
		}

		void ResolveHitProcessing()
		{
			if (!REL::Module::IsAE()) {
				logger::info("hits: not an AE build; using plain damage");
				return;
			}
			// Only trust the address if the game's own melee code calls it from where it should.
			const auto   site = REL::ID(38627).address() + 0x4A8;
			const auto*  code = reinterpret_cast<const std::uint8_t*>(site);
			const auto   target = REL::ID(38586).address();
			std::int32_t rel = 0;
			std::memcpy(&rel, code + 1, 4);
			if (code[0] != 0xE8) {
				logger::warn("hits: the melee call site does not match this game build; using plain damage");
				return;
			}
			if (site + 5 + rel != target) {
				// (another mod has hooked the game's melee call - many combat mods do: the call is there, it only goes to their
				// code first. Skyrim's own hit processing is still what the Address Library names; before, this read as 'the
				// wrong game build' and hits fell back to plain damage.)
				logger::info("hits: the game's melee call is hooked by another mod; Skyrim's own hit processing is used all the same");
			}
			g_processHit = reinterpret_cast<ProcessHitFn*>(target);
			g_hitSite = site;
			logger::info("hits: going through Skyrim's own hit processing");
		}

		RE::NiAVObject* TorsoNode(RE::Actor* a_actor)
		{
			auto* root = a_actor->Get3D();
			if (!root) {
				return nullptr;
			}
			for (const char* name : { "NPC Spine2 [Spn2]", "NPC Spine1 [Spn1]", "NPC Spine [Spn0]", "NPC Pelvis [Pelv]" }) {
				if (auto* node = root->GetObjectByName(name)) {
					return node;
				}
			}
			return root;
		}

		// A blade that reached a character: damage by Skyrim's own rules (weapon, skill, the target's armour and
		// block), delivered the way one of Skyrim's own melee hits is.
		// Skyrim's progression for MORDHAU-judged hits (sheets/progression.json): the share of the target's full health one
		// hit takes. a_skyrimDamage: what Skyrim itself would give this hit (the player's weapon, skill, perks); 0 = not known.
		float Progress(const char* a_id)
		{
			// (settings menu "Level difference matters" off: level counts as equal both ways)
			const std::string_view id(a_id);
			if ((id == "level_min" || id == "level_max") && Settings::Value("level_gap", 1.0f) < 0.5f) {
				return 1.0f;
			}
			for (const auto& value : sheets::kProgression) {
				if (id == value.id) {
					return Settings::Value(id, value.value);
				}
			}
			return 0.0f;
		}

		// (given as the strength of the hit - MORDHAU damage counted against clean_hit - since a hit judged by the progression
		// does not use ApplyHit's scale: passed as that scale, it had done nothing since progression.json came in)
		float CreatureScale(RE::Actor* a_target)
		{
			return Humanoid(a_target) ? 1.0f : std::max(BuildValue("creature_damage_scale"), 0.1f);
		}

		bool SuperKick(RE::Actor* a_player, RE::Actor* a_target)
		{
			if (!a_player || !a_target || a_target->IsDead() || BuildValue("super_kick") < 0.5f || a_target->HasKeywordString("ActorTypeDragon")) {
				return false;
			}
			auto* process = a_target->GetActorRuntimeData().currentProcess;
			if (!process) {
				return false;
			}
			const float force = std::max(BuildValue("super_kick_force"), 0.0f);
			process->KnockExplosion(a_target, a_player->GetPosition(), force);
			logger::info("live: super kick - {} thrown away (force {:.0f})", a_target->GetDisplayFullName(), force);
			return true;
		}

		float SneakMult(RE::Actor* a_attacker, RE::Actor* a_target, RE::HitData& a_hit, bool* a_inDamage)
		{
			// Skyrim's sneak attack (Backstab, Assassin's Blade, Deadly Aim...): what the game worked out for this hit, else the
			// same by its rules - sneaking and unseen, the base multiplier and the attacker's perks on it
			*a_inDamage = false;
			if (a_hit.flags.any(RE::HitData::Flag::kSneakAttack) && a_hit.sneakAttackBonus > 1.0f) {
				*a_inDamage = true;  // (the game's own: already in its damage)
				return a_hit.sneakAttackBonus;
			}
			if (!a_attacker->IsSneaking() || a_target->IsDead() || a_target->RequestDetectionLevel(a_attacker) > 0) {
				return 1.0f;
			}
			auto* setting = RE::GameSettingCollection::GetSingleton()->GetSetting("fCombatSneakAttackBonusMult");
			float mult = setting ? setting->GetFloat() : 2.0f;
			RE::BGSEntryPoint::HandleEntryPoint(RE::BGSEntryPoint::ENTRY_POINT::kModSneakAttackMult, a_attacker, a_hit.weapon, a_target, std::addressof(mult));
			if (mult <= 1.0f) {
				return 1.0f;
			}
			a_hit.flags.set(RE::HitData::Flag::kSneakAttack);
			a_hit.sneakAttackBonus = mult;
			return mult;
		}

		float HitFraction(RE::Actor* a_attacker, RE::Actor* a_target, float a_mordhauDamage, float a_skyrimDamage)
		{
			auto*       player = RE::PlayerCharacter::GetSingleton();
			const float quality = std::clamp(a_mordhauDamage / Progress("clean_hit"), Progress("quality_min"), Progress("quality_max"));
			const float attackerLevel = static_cast<float>(a_attacker->GetLevel());
			const float targetLevel = static_cast<float>(a_target->GetLevel());
			const float soften = Progress("level_soften");
			const float level = std::clamp((attackerLevel + soften) / (targetLevel + soften), Progress("level_min"), Progress("level_max"));
			const float low = Progress("build_min"), high = Progress("build_max");
			float       offence = 1.0f;
			if (a_attacker == player && a_skyrimDamage > 0.0f) {
				// Skyrim's damage in sword terms: which KIND of weapon hits harder is MORDHAU's to say (it is in the quality),
				// Skyrim only adds how good this one is for its kind and how skilled the player
				float       kind = 1.0f;
				const auto* form = a_attacker->GetEquippedObject(false);
				if (const auto* weapon = form ? form->As<RE::TESObjectWEAP>() : nullptr) {
					switch (weapon->GetWeaponType()) {
					case RE::WEAPON_TYPE::kOneHandDagger: kind = 4.0f / 7.0f; break;
					case RE::WEAPON_TYPE::kOneHandAxe: kind = 8.0f / 7.0f; break;
					case RE::WEAPON_TYPE::kOneHandMace: kind = 9.0f / 7.0f; break;
					case RE::WEAPON_TYPE::kTwoHandSword: kind = 15.0f / 7.0f; break;
					case RE::WEAPON_TYPE::kTwoHandAxe: kind = 17.0f / 7.0f; break;
					case RE::WEAPON_TYPE::kBow: kind = 15.0f / 7.0f; break;       // (hunting bow 7 + iron arrow 8)
					case RE::WEAPON_TYPE::kCrossbow: kind = 29.0f / 7.0f; break;  // (crossbow 19 + steel bolt 10)
					default: break;
					}
				} else {
					kind = 4.0f / 7.0f;  // fists
				}
				offence = std::clamp(a_skyrimDamage / kind / (Progress("damage_base") + Progress("damage_per_level") * (attackerLevel - 1.0f)), low, high);
			}
			const float fullHealth = std::max(a_target->AsActorValueOwner()->GetPermanentActorValue(RE::ActorValue::kHealth), 1.0f);
			const bool  onPlayer = a_target == player;
			const float ordinary = Progress(onPlayer ? "health_base" : "npc_health_base") + Progress(onPlayer ? "health_per_level" : "npc_health_per_level") * (targetLevel - 1.0f);
			float       defence = fullHealth / std::max(ordinary, 1.0f);
			float       armour = 0.0f;
			if (onPlayer) {
				armour = std::clamp(a_target->AsActorValueOwner()->GetActorValue(RE::ActorValue::kDamageResist) * 0.0012f, 0.0f, 0.8f);
				defence /= 1.0f - Progress("armour_share") * armour;
			}
			defence = std::clamp(defence, low, high);
			float fraction = std::clamp(quality * level * offence / defence / std::max(Progress("hits_to_kill"), 1.0f), Progress("fraction_min"), Progress("fraction_max"));
			// a blow that kills outright in MORDHAU (a maul to the head) kills here too, short of a higher-level target
			if (a_mordhauDamage >= Progress("lethal_blow")) {
				fraction = std::max(fraction, std::min(1.0f, level));
			}
			// the settings menu's "Damage you deal" / "Damage you take", on top of all of the above
			if (a_attacker == player) {
				fraction *= std::max(Settings::Value("damage_dealt", 1.0f), 0.0f);
			}
			if (onPlayer) {
				fraction *= std::max(Settings::Value("damage_taken", 1.0f), 0.0f);
			}
			logger::info("progress: {} (level {:.0f}) on {} (level {:.0f}, full health {:.0f}): quality {:.2f} (MORDHAU {:.0f}) x level {:.2f} x offence {:.2f} (Skyrim damage {:.0f}) / defence {:.2f} (armour {:.0f}%) -> {:.0f}% of full health = {:.0f}, about {:.1f} such hits to kill",
				a_attacker->GetDisplayFullName(), attackerLevel, a_target->GetDisplayFullName(), targetLevel, fullHealth, quality, a_mordhauDamage, level, offence, a_skyrimDamage, defence,
				armour * 100.0f, fraction * 100.0f, fraction * fullHealth, 1.0f / fraction);
			return fraction;
		}

		// a_mordhauDamage >= 0: a MORDHAU-judged hit (its damage of 100); what it takes follows Skyrim's progression
		void ApplyHit(RE::Actor* a_attacker, RE::Actor* a_target, const ActiveSwing& a_swing, float a_scale, float a_mordhauDamage)
		{
			auto*        node = TorsoNode(a_target);
			RE::NiPoint3 hitPos = node ? node->world.translate : a_target->GetPosition() + RE::NiPoint3{ 0.0f, 0.0f, a_target->GetHeight() * 0.6f };
			RE::NiPoint3 dir = hitPos - a_attacker->GetPosition();
			dir.z = 0.0f;
			dir = dir.Length() > 1e-3f ? dir / dir.Length() : RE::NiPoint3{ 0.0f, 1.0f, 0.0f };
			const auto* form = a_attacker->GetEquippedObject(false);
			const auto* weapon = form ? form->As<RE::TESObjectWEAP>() : nullptr;

			auto*       values = a_target->AsActorValueOwner();
			const float before = values->GetActorValue(RE::ActorValue::kHealth);
			const bool  wasFighting = a_target->IsInCombat();
			float       damage = 0.0f;
			bool        blocked = false, processed = false;
			auto        skill = RE::ActorValue::kNone;
			if (auto* hit = RE::HitData::Create(a_attacker, a_target, a_attacker->GetEquippedEntryData(false), false)) {
				if (a_mordhauDamage >= 0.0f) {
					const float full = std::max(a_target->AsActorValueOwner()->GetPermanentActorValue(RE::ActorValue::kHealth), 1.0f);
					bool        inDamage = false;
					const float sneak = a_attacker == RE::PlayerCharacter::GetSingleton() ? SneakMult(a_attacker, a_target, *hit, &inDamage) : 1.0f;
					hit->totalDamage = full * std::min(HitFraction(a_attacker, a_target, a_mordhauDamage, inDamage ? hit->totalDamage / sneak : hit->totalDamage) * sneak, 1.0f);
					if (sneak > 1.0f) {
						logger::info("progress: sneak attack on {}: x{:.1f} ({})", a_target->GetDisplayFullName(), sneak, inDamage ? "Skyrim's" : "worked out here");
					}
					hit->physicalDamage = hit->totalDamage;
				} else {
					hit->totalDamage *= a_scale;
					hit->physicalDamage *= a_scale;
				}
				damage = hit->totalDamage;
				skill = hit->skill;
				if (g_processHit) {
					hit->hitPosition = hitPos;
					hit->hitDirection = dir;
					hit->flags.set(RE::HitData::Flag::kMeleeAttack);
					g_processHit(a_target, *hit);
					blocked = hit->flags.any(RE::HitData::Flag::kBlocked);
					processed = true;
				}
				hit->attackData.reset();
				RE::free(hit);
			}
			if (!processed) {
				a_target->DoDamage(damage, a_attacker, false);
			}
			// blood and the weapon-on-flesh sound
			if (auto* impacts = RE::BGSImpactManager::GetSingleton(); impacts && weapon && weapon->impactDataSet && node) {
				RE::NiPoint3 pick = dir;
				impacts->PlayImpactEffect(a_target, weapon->impactDataSet, node->name.c_str(), pick, 128.0f, false, false);
			}
			if (auto* player = RE::PlayerCharacter::GetSingleton(); a_attacker == player && weapon && (skill == RE::ActorValue::kOneHanded || skill == RE::ActorValue::kTwoHanded)) {
				player->AddSkillExperience(skill, weapon->GetAttackDamage());
			}
			if (!a_target->IsDead() && !a_target->IsPlayerTeammate() && !a_target->IsInCombat()) {
				a_target->StartCombat(a_attacker);
			}
			const float reward = g_stats.Get(a_swing.family->id, a_swing.swing->stab, "hit_stamina_reward");
			if (reward > 0.0f) {
				a_attacker->AsActorValueOwner()->RestoreActorValue(RE::ActorValue::kStamina, reward);
			}
			if (a_attacker == RE::PlayerCharacter::GetSingleton() && Link::Current().live && BuildValue("hit_marker") > 0.5f) {
				// a hit Skyrim judges (a creature, anybody without a MORDHAU stand-in): MORDHAU's hit marker and hit sound
				CharacterRender::ShowHitMarker(a_target->IsDead());
				Link::Request("HitSound");
			}
			logger::info("hit: {} with {} for {:.1f} damage{}, health {:.0f} -> {:.0f}, {}, fighting {} -> {}, hostile to attacker {}", a_target->GetDisplayFullName(),
				a_swing.swing->id, damage, blocked ? " (blocked)" : "", before, values->GetActorValue(RE::ActorValue::kHealth),
				processed ? "Skyrim hit processing" : "plain damage", wasFighting, a_target->IsInCombat(), a_target->IsHostileToActor(a_attacker));
		}

		// A stand-in's blade reached the player in MORDHAU: the Skyrim character's hit lands, by Skyrim's own rules.
		void ApplyHitOnPlayer(RE::Actor* a_attacker, RE::PlayerCharacter* a_player, float a_mordhauDamage)
		{
			auto*       values = a_player->AsActorValueOwner();
			const float before = values->GetActorValue(RE::ActorValue::kHealth);
			float       damage = 0.0f;
			if (auto* hit = RE::HitData::Create(a_attacker, a_player, a_attacker->GetEquippedEntryData(false), false)) {
				// one health pool, Skyrim's progression: MORDHAU judges the hit, sheets/progression.json says what share of full health it takes
				hit->totalDamage = std::max(values->GetPermanentActorValue(RE::ActorValue::kHealth), 1.0f) * HitFraction(a_attacker, a_player, a_mordhauDamage, 0.0f);
				if (g_dev.god) {
					logger::info("god: {} would have hit the player for {:.1f} (MORDHAU damage {:.0f})", a_attacker->GetDisplayFullName(), hit->totalDamage, a_mordhauDamage);
					hit->totalDamage = 0.0f;
				}
				hit->physicalDamage = hit->totalDamage;
				damage = hit->totalDamage;
				RE::NiPoint3 dir = a_player->GetPosition() - a_attacker->GetPosition();
				dir.z = 0.0f;
				dir = dir.Length() > 1e-3f ? dir / dir.Length() : RE::NiPoint3{ 0.0f, 1.0f, 0.0f };
				hit->hitPosition = a_player->GetPosition() + RE::NiPoint3{ 0.0f, 0.0f, a_player->GetHeight() * 0.6f };
				hit->hitDirection = dir;
				hit->flags.set(RE::HitData::Flag::kMeleeAttack);
				if (g_processHit) {
					g_processHit(a_player, *hit);
				} else {
					a_player->DoDamage(damage, a_attacker, false);
				}
				hit->attackData.reset();
				RE::free(hit);
			}
			CharacterRender::AddBlood(0, 0.2f);
			CharacterRender::AddBlood(a_attacker->GetFormID(), 0.06f);
			logger::info("live: {} hit the player in MORDHAU (its damage {:.0f} of 100): Skyrim hit for {:.1f}, health {:.0f} -> {:.0f}", a_attacker->GetDisplayFullName(),
				a_mordhauDamage, damage, before, values->GetActorValue(RE::ActorValue::kHealth));
		}

		void PushDown(RE::Actor* a_from, RE::Actor* a_target, float a_force)
		{
			using Push = void (*)(RE::AIProcess*, RE::Actor*, RE::NiPoint3*, float);
			REL::Relocation<Push> push{ RELOCATION_ID(38858, 39895) };
			RE::NiPoint3          from = a_from ? a_from->GetPosition() : a_target->GetPosition();
			push((a_from ? a_from : a_target)->GetActorRuntimeData().currentProcess, a_target, &from, a_force);
		}

		bool FallFromMordhau(FallWatch& a_watch, RE::Actor* a_actor, bool a_mordhauFallen)
		{
			if (a_actor->IsInRagdollState()) {
				a_watch.skyrimLimp = Clock::now();
			}
			const bool began = a_mordhauFallen && !a_watch.mordhauWas;
			a_watch.mordhauWas = a_mordhauFallen;
			return began && !a_actor->IsInRagdollState() && Seconds(a_watch.skyrimLimp) > 4.0f;
		}

		// A fought character that has just died does not fly off (sheets/builds.json death_speed_limit): for half a
		// second every part of its limp body is held to that speed. The head of one being decapitated is left to ThrowHeads.
		void CalmBodies()
		{
			const float limit = BuildValue("death_speed_limit");
			if (limit <= 0.0f) {
				return;
			}
			static std::unordered_map<std::uint32_t, Clock::time_point> fought;  // id -> when it was last seen alive with a stand-in
			static std::unordered_map<std::uint32_t, Clock::time_point> died;
			for (const auto& m : g_mirrored) {
				if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(m.id)) {
					if (!actor->IsDead()) {
						fought[m.id] = Clock::now();
					}
				}
			}
			for (auto it = fought.begin(); it != fought.end();) {
				auto* actor = RE::TESForm::LookupByID<RE::Actor>(it->first);
				if (actor && actor->IsDead() && !died.contains(it->first) && Seconds(it->second) < 2.0f) {
					died[it->first] = Clock::now();
				}
				it = Seconds(it->second) > 10.0f ? fought.erase(it) : std::next(it);
			}
			const float scale = RE::bhkWorld::GetWorldScale();
			for (auto it = died.begin(); it != died.end();) {
				auto* actor = RE::TESForm::LookupByID<RE::Actor>(it->first);
				auto* root = actor ? actor->Get3D(false) : nullptr;
				if (!root || Seconds(it->second) > 0.5f) {
					it = died.erase(it);
					continue;
				}
				const bool headThrown = std::any_of(g_thrownHeads.begin(), g_thrownHeads.end(), [&](const ThrownHead& a_head) { return a_head.who.get().get() == actor; }) ||
										std::any_of(g_pendingHeads.begin(), g_pendingHeads.end(), [&](const ThrownHead& a_head) { return a_head.who.get().get() == actor; });
				int                          slowed = 0;
				std::vector<RE::NiAVObject*> pending{ root };
				while (!pending.empty()) {
					auto* at = pending.back();
					pending.pop_back();
					if (auto* node = at->AsNode()) {
						for (auto& child : node->GetChildren()) {
							if (child) {
								pending.push_back(child.get());
							}
						}
					}
					if (headThrown && at->name == "NPC Head [Head]") {
						continue;
					}
					auto* collision = at->GetCollisionObject();
					auto* body = collision ? collision->GetRigidBody() : nullptr;
					auto* havok = body ? static_cast<RE::hkpRigidBody*>(body->referencedObject.get()) : nullptr;
					if (!havok) {
						continue;
					}
					float v[4], w[4];  // (copied out: m128_f32 is MSVC's own, the local clang build has no such member)
					_mm_storeu_ps(v, havok->motion.linearVelocity.quad);
					_mm_storeu_ps(w, havok->motion.angularVelocity.quad);
					const float speed = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
					if (speed > limit * scale) {
						const float k = limit * scale / speed;
						body->SetLinearVelocity(RE::hkVector4(v[0] * k, v[1] * k, v[2] * k, 0.0f));
						++slowed;
					}
					const float spin = std::sqrt(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
					if (spin > 6.0f) {
						const float k = 6.0f / spin;
						body->SetAngularVelocity(RE::hkVector4(w[0] * k, w[1] * k, w[2] * k, 0.0f));
					}
				}
				static std::unordered_set<std::uint32_t> told;
				if (slowed > 0 && told.insert(it->first).second) {
					logger::info("death: {}'s body was about to fly ({} parts over {:.0f} units/s): held back", actor->GetDisplayFullName(), slowed, limit);
				}
				++it;
			}
		}
	}
}
