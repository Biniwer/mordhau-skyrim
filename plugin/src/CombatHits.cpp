#include "CombatInternal.h"

namespace dm::Combat
{
	inline namespace internal
	{
		// Skyrim's severed head: a small skeleton of its own ("DismemberedLimb1 Head1" and a head bone with physics) that
		// Skyrim hangs under the body's 3D, with copies of the head's meshes skinned to it. It is there and falls from
		// the blow on, for a Whiterun guard too; the body keeps its own head bone, with the helmet on it. (Play test
		// 2026-10-25 - before, the body's own head bone was the one pushed, and the helmet looked for there.)
		RE::NiAVObject* SeveredLimb(RE::Actor* a_actor, RE::NiAVObject** a_head = nullptr)
		{
			auto*           body = a_actor->Get3D(false);
			RE::NiAVObject* limb = nullptr;
			RE::NiAVObject* head = nullptr;
			std::function<void(RE::NiAVObject*)> visit = [&](RE::NiAVObject* a_object) {
				if (limb) {
					return;
				}
				if (a_object->name == "NPC Head [Head]") {
					bool severed = false;
					for (auto* up = a_object->parent; up && up != body; up = up->parent) {
						severed = severed || std::string_view(up->name.c_str()).starts_with("DismemberedLimb");
					}
					if (severed) {
						head = a_object;
						limb = a_object;
						while (limb->parent && limb->parent != body) {
							limb = limb->parent;
						}
						return;
					}
				}
				if (auto* node = a_object->AsNode()) {
					for (auto& child : node->GetChildren()) {
						if (child) {
							visit(child.get());
						}
					}
				}
			};
			if (body) {
				visit(body);
			}
			if (a_head) {
				*a_head = head;
			}
			return limb;
		}

		std::string Partitions(RE::BSGeometry* a_geometry)
		{
			auto*       skin = a_geometry->GetGeometryRuntimeData().skinInstance.get();
			auto*       dismember = skin ? netimmerse_cast<RE::BSDismemberSkinInstance*>(skin) : nullptr;
			std::string parts;
			if (dismember) {
				auto& data = dismember->GetRuntimeData();
				for (std::int32_t k = 0; k < data.numPartitions; ++k) {
					parts += std::format("{}{}{}", k ? "," : "", data.partitions[k].slot, data.partitions[k].visible ? "" : " hidden");
				}
			}
			return parts;
		}

		// What a beheaded character wears on its head (helmet, hood, circlet) onto its severed head (sheets/builds.json
		// gore_helmet_on_head): a Whiterun guard's helmet stayed on the body (its mesh has no part marked for the severed
		// head). If the severed head carries a copy of it with its parts hidden, they are shown there and the body's is
		// hidden. Logs what it finds either way.
		void HelmetOntoHead(RE::Actor* a_actor, bool a_apply, const char* a_when)
		{
			const auto& biped = a_actor->GetBiped(false);
			auto*       limb = SeveredLimb(a_actor);
			if (!biped) {
				return;
			}
			std::vector<const RE::NiAVObject*> seen;
			int found = 0;
			for (const auto slot : { RE::BIPED_OBJECT::kHead, RE::BIPED_OBJECT::kHair, RE::BIPED_OBJECT::kLongHair, RE::BIPED_OBJECT::kCirclet, RE::BIPED_OBJECT::kEars }) {
				const auto& object = biped->objects[slot];
				auto*       armor = object.item ? object.item->As<RE::TESObjectARMO>() : nullptr;
				auto*       part = object.partClone.get();
				if (!armor || !part || std::find(seen.begin(), seen.end(), part) != seen.end()) {
					continue;
				}
				seen.push_back(part);
				RE::BSVisit::TraverseScenegraphGeometries(part, [&](RE::BSGeometry* a_geometry) {
					++found;
					RE::BSGeometry* copy = nullptr;
					if (limb) {
						RE::BSVisit::TraverseScenegraphGeometries(limb, [&](RE::BSGeometry* a_other) {
							if (a_other->name == a_geometry->name) {
								copy = a_other;
								return RE::BSVisit::BSVisitControl::kStop;
							}
							return RE::BSVisit::BSVisitControl::kContinue;
						});
					}
					std::string done;
					if (a_apply && copy) {
						auto* skin = copy->GetGeometryRuntimeData().skinInstance.get();
						if (auto* dismember = skin ? netimmerse_cast<RE::BSDismemberSkinInstance*>(skin) : nullptr) {
							auto& data = dismember->GetRuntimeData();
							for (std::int32_t k = 0; k < data.numPartitions; ++k) {
								if (!data.partitions[k].visible) {
									dismember->UpdateDismemberPartion(data.partitions[k].slot, true);
								}
							}
						}
						copy->SetAppCulled(false);
						a_geometry->SetAppCulled(true);
						g_headwearOnHeads.push_back({ a_actor->GetHandle(), armor->GetFormID(), a_geometry->name.c_str(), Clock::now() });
						done = std::format("; now shown on the severed head (parts {}), hidden on the body", Partitions(copy));
					}
					logger::info("gore: {}'s {} ({}), {}: on the body parts {}{}; the severed head {}{}", a_actor->GetDisplayFullName(), armor->GetName(), a_geometry->name.c_str(), a_when,
						Partitions(a_geometry), a_geometry->GetAppCulled() ? " (hidden)" : "",
						!limb ? "is not there" : copy ? std::format("has a copy, parts {}{}", Partitions(copy), copy->GetAppCulled() ? " (hidden)" : "") : "has no copy of it", done);
					return RE::BSVisit::BSVisitControl::kContinue;
				});
			}
			if (!found) {
				logger::info("gore: {} wears nothing on its head ({})", a_actor->GetDisplayFullName(), a_when);
			}
		}

		// What the body has for its head: Skyrim's own decapitation parts (biped slots DecapitateHead, Decapitate) and
		// the severed head with what is drawn on it. (A guard's head seemed to appear only once its body was looted.)
		std::string HeadState(RE::Actor* a_actor)
		{
			std::string out;
			const auto& biped = a_actor->GetBiped(false);
			for (const auto slot : { RE::BIPED_OBJECT::kDecapitateHead, RE::BIPED_OBJECT::kDecapitate }) {
				const auto* object = biped ? &biped->objects[slot] : nullptr;
				out += std::format("{}slot {}: {}{}", out.empty() ? "" : "; ", static_cast<int>(slot), object && object->addon ? std::format("{:08X}", object->addon->GetFormID()) : "-",
					object && object->partClone ? (object->partClone->GetAppCulled() ? " (3D, hidden)" : " (3D)") : " (no 3D)");
			}
			auto* limb = SeveredLimb(a_actor);
			if (!limb) {
				return out + "; no severed head";
			}
			int         pieces = 0;
			std::string drawn;
			RE::BSVisit::TraverseScenegraphGeometries(limb, [&](RE::BSGeometry* a_geometry) {
				++pieces;
				drawn += std::format("{}{} [{}]{}", drawn.empty() ? "" : "; ", a_geometry->name.c_str(), Partitions(a_geometry), a_geometry->GetAppCulled() ? " hidden" : "");
				return RE::BSVisit::BSVisitControl::kContinue;
			});
			return out + std::format("; severed head '{}'{}: {} pieces: {}", limb->name.c_str(), limb->GetAppCulled() ? " (hidden)" : "", pieces, drawn);
		}

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
			for (auto it = g_headwearOnHeads.begin(); it != g_headwearOnHeads.end();) {
				auto actor = it->who.get();
				if (!actor || !actor->Is3DLoaded()) {
					it = g_headwearOnHeads.erase(it);
					continue;
				}
				if (Seconds(it->looked) < 0.25f) {
					++it;
					continue;
				}
				it->looked = Clock::now();
				if (actor->GetWornArmor(it->armor)) {
					++it;
					continue;
				}
				// (taken off the body - looted: off the severed head too)
				int hidden = 0;
				if (auto* limb = SeveredLimb(actor.get())) {
					RE::BSVisit::TraverseScenegraphGeometries(limb, [&](RE::BSGeometry* a_geometry) {
						if (a_geometry->name == it->piece.c_str()) {
							a_geometry->SetAppCulled(true);
							++hidden;
						}
						return RE::BSVisit::BSVisitControl::kContinue;
					});
				}
				auto* armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(it->armor);
				logger::info("gore: {}'s {} was taken off its body: {}", actor->GetDisplayFullName(), armor ? armor->GetName() : "headwear", hidden ? "gone from its severed head too" : "its severed head was not found");
				it = g_headwearOnHeads.erase(it);
			}
			for (auto it = g_thrownHeads.begin(); it != g_thrownHeads.end();) {
				const float t = Seconds(it->when);
				auto        actor = it->who.get();
				if (actor && t < 30.0f) {
					// (for 30 s: what the body has for its head, logged whenever it changes - looting it among them)
					if (auto now = HeadState(actor.get()); now != it->state) {
						it->state = std::move(now);
						logger::info("gore: {}'s head parts {:.1f} s after the blow: {}", actor->GetDisplayFullName(), t, it->state);
					}
				}
				if (!actor || t > 30.0f) {
					it = g_thrownHeads.erase(it);
					continue;
				}
				if (!it->told && t > 1.5f) {
					it->told = true;
					HelmetOntoHead(actor.get(), false, "1.5 s on");
				}
				if (it->pushes >= 2) {
					++it;  // (thrown: kept for 30 s to be looked at)
					continue;
				}
				// (the body goes limp a moment after the blow: the head is pushed once it is, and once more shortly after
				// in case the first came before its joint to the neck was cut)
				if (t > (it->pushes == 0 ? 0.10f : 0.22f) && actor->IsInRagdollState()) {
					RE::NiAVObject* head = nullptr;
					SeveredLimb(actor.get(), &head);
					auto* collision = head ? head->GetCollisionObject() : nullptr;
					auto* body = collision ? collision->GetRigidBody() : nullptr;
					if (body) {
						const float        scale = RE::bhkWorld::GetWorldScale();
						const RE::NiPoint3 v = (it->away * BuildValue("head_throw_out") + RE::NiPoint3{ 0.0f, 0.0f, BuildValue("head_throw_up") }) * scale;
						body->SetLinearVelocity(RE::hkVector4(v.x, v.y, v.z, 0.0f));
						body->SetAngularVelocity(RE::hkVector4(it->away.y * 9.0f, -it->away.x * 9.0f, 2.0f, 0.0f));
						if (it->pushes == 0) {
							logger::info("gore: {}'s own head is thrown on MORDHAU's arc", actor->GetDisplayFullName());
							HelmetOntoHead(actor.get(), BuildValue("gore_helmet_on_head") > 0.5f, "at the throw");
						}
					} else if (it->pushes == 0) {
						logger::info("gore: {}'s severed head {} (it falls as Skyrim drops it)", actor->GetDisplayFullName(), head ? "has no physics body to throw" : "was not found");
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

		// (measurement for a damage model built on Skyrim's own damage, 2026-10-20: what the game worked out for a hit
		// before this plugin changes it - is the target's armour, the difficulty, already in it?)
		void LogSkyrimHit(RE::Actor* a_attacker, RE::Actor* a_target, const RE::HitData& a_hit)
		{
			auto*       player = RE::PlayerCharacter::GetSingleton();
			const auto* weapon = a_hit.weapon;
			logger::info("measure: Skyrim's own hit, {} on {}: total {:.1f}, physical {:.1f}, resisted physical {:.1f}, typed {:.1f}, blocked {:.0f}%, crit {}, sneak {} (x{:.1f}), "
						 "power {}; weapon {} (base {}); target armour rating {:.0f}, health {:.0f} of {:.0f}; difficulty {}",
				a_attacker->GetDisplayFullName(), a_target->GetDisplayFullName(), a_hit.totalDamage, a_hit.physicalDamage, a_hit.resistedPhysicalDamage,
				a_hit.resistedTypedDamage, a_hit.percentBlocked * 100.0f, a_hit.flags.any(RE::HitData::Flag::kCritical), a_hit.flags.any(RE::HitData::Flag::kSneakAttack),
				a_hit.sneakAttackBonus, a_hit.flags.any(RE::HitData::Flag::kPowerAttack), weapon && weapon->GetName() ? weapon->GetName() : "none", weapon ? weapon->GetAttackDamage() : 0,
				a_target->AsActorValueOwner()->GetActorValue(RE::ActorValue::kDamageResist), a_target->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth),
				a_target->AsActorValueOwner()->GetPermanentActorValue(RE::ActorValue::kHealth), player ? player->GetGameStatsData().difficulty : -1);
		}

		float CritMult(RE::Actor* a_attacker, RE::Actor* a_target, const RE::HitData& a_hit, float* a_critDamage)
		{
			// Skyrim's critical hit (rolled by the game when the hit was made: the weapon's critical damage, raised by perks
			// such as Bladesman or Bone Breaker), as a multiplier on the hit's share of health - sheets/builds.json skyrim_crits.
			// It was in Skyrim's damage, which only moves the capped offence: the crit did almost nothing (2026-10-20).
			*a_critDamage = 0.0f;
			if (BuildValue("skyrim_crits") < 0.5f || !a_hit.flags.any(RE::HitData::Flag::kCritical) || !a_hit.weapon) {
				return 1.0f;
			}
			float crit = static_cast<float>(a_hit.weapon->GetCritDamage());
			RE::BGSEntryPoint::HandleEntryPoint(RE::BGSEntryPoint::ENTRY_POINT::kCalculateMyCriticalHitDamage, a_attacker, a_hit.weapon, a_target, std::addressof(crit));
			if (crit <= 0.0f) {
				return 1.0f;
			}
			// (the game adds it to the hit's damage: the rest is the ordinary hit. Should the rest look too small for that, the
			// crit is taken as on top of the whole)
			const float total = a_hit.totalDamage;
			const bool  inside = total - crit > 0.3f * total;
			const float plain = inside ? total - crit : total;
			*a_critDamage = inside ? crit : 0.0f;
			return plain > 0.0f ? (plain + crit) / plain : 1.0f;
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

		// sheets/progression.json damage_model 1: Skyrim's own damage for the hit - the weapon, skill and perks, the target's
		// armour, a crit, a sneak attack, all already in it (measured 2026-10-21/23) - times how good MORDHAU says the hit was
		// (where it landed; MORDHAU's side sends it as a damage of clean_hit x quality) times skyrim_damage_dealt or
		// skyrim_damage_taken, for a hit lands less often here than in Skyrim. Skyrim's difficulty applies after, by itself.
		float SkyrimHitFraction(RE::Actor* a_attacker, RE::Actor* a_target, float a_skyrimDamage, float a_mordhauDamage)
		{
			auto*       player = RE::PlayerCharacter::GetSingleton();
			const bool  onPlayer = a_target == player;
			const float quality = std::max(a_mordhauDamage, 0.0f) / std::max(Progress("clean_hit"), 1.0f);
			const float factor = std::max(Progress(onPlayer ? "skyrim_damage_taken" : "skyrim_damage_dealt"), 0.0f);
			const float fullHealth = std::max(a_target->AsActorValueOwner()->GetPermanentActorValue(RE::ActorValue::kHealth), 1.0f);
			float       fraction = std::clamp(std::max(a_skyrimDamage, 0.0f) * quality * factor / fullHealth, Progress("fraction_min"), Progress("fraction_max"));
			// the settings menu's "Damage you deal" / "Damage you take", on top
			if (a_attacker == player) {
				fraction *= std::max(Settings::Value("damage_dealt", 1.0f), 0.0f);
			}
			if (onPlayer) {
				fraction *= std::max(Settings::Value("damage_taken", 1.0f), 0.0f);
			}
			logger::info("damage: {} on {} (full health {:.0f}): Skyrim's {:.1f} x quality {:.2f} x {:.1f} -> {:.0f}% of full health = {:.0f}, about {:.1f} such hits to kill",
				a_attacker->GetDisplayFullName(), a_target->GetDisplayFullName(), fullHealth, a_skyrimDamage, quality, factor, fraction * 100.0f, fraction * fullHealth, 1.0f / fraction);
			return fraction;
		}

		// Skyrim's own arrows and bolts (Skyrim flies and lands them; this mod judged them not at all, they took Skyrim's
		// damage unchanged): damage_model 1 as for a MORDHAU-judged hit - Skyrim's damage x the zone it struck (by where on
		// the body, against the head and pelvis bones) x skyrim_arrow_factor, kept between fraction_min and fraction_max.
		void ScaleArrowHit(RE::Actor* a_victim, RE::HitData& a_hit)
		{
			const auto* weapon = a_hit.weapon;
			if (!a_victim || Progress("damage_model") < 0.5f || a_hit.totalDamage <= 0.0f || !weapon || !(weapon->IsBow() || weapon->IsCrossbow())) {
				return;
			}
			auto source = a_hit.sourceRef.get();
			if (!source || !source->As<RE::Projectile>()) {
				return;
			}
			const auto* head = a_victim->GetNodeByName("NPC Head [Head]");
			const auto* pelvis = a_victim->GetNodeByName("NPC Pelvis [Pelv]");
			const float z = a_hit.hitPosition.z;
			const char* zone = "body";
			if (head && z >= head->world.translate.z - 8.0f) {
				zone = "head";
			} else if (pelvis && z < pelvis->world.translate.z - 4.0f) {
				zone = "legs";
			}
			const float quality = Progress(zone[0] == 'h' ? "zone_head" : zone[0] == 'l' ? "zone_legs" : "zone_body");
			auto*       player = RE::PlayerCharacter::GetSingleton();
			const float full = std::max(a_victim->AsActorValueOwner()->GetPermanentActorValue(RE::ActorValue::kHealth), 1.0f);
			float       fraction = std::clamp(a_hit.totalDamage * quality * std::max(Progress("skyrim_arrow_factor"), 0.0f) / full, Progress("fraction_min"), Progress("fraction_max"));
			auto        attacker = a_hit.aggressor.get();
			if (attacker && attacker.get() == player) {
				fraction *= std::max(Settings::Value("damage_dealt", 1.0f), 0.0f);
			}
			if (a_victim == player) {
				fraction *= std::max(Settings::Value("damage_taken", 1.0f), 0.0f);
			}
			const float before = a_hit.totalDamage;
			const float scale = std::min(fraction, 1.0f) * full / before;
			a_hit.totalDamage *= scale;
			a_hit.physicalDamage *= scale;
			logger::info("damage: {}'s {} on {} ({}, {:.0f} above its feet; full health {:.0f}): Skyrim's {:.1f} x quality {:.2f} x {:.1f} -> {:.0f}% of full health = {:.0f}",
				attacker ? attacker->GetDisplayFullName() : "?", weapon->IsBow() ? "arrow" : "bolt", a_victim->GetDisplayFullName(), zone, z - a_victim->GetPositionZ(), full, before, quality,
				Progress("skyrim_arrow_factor"), fraction * 100.0f, a_hit.totalDamage);
		}

		// Skyrim's power attack on top of its hit damage: a_base (the attack's own multiplier, else power_attack_damage) times the
		// attacker's power-attack damage perks (Mod Power Attack Damage)
		float PowerAttackMult(RE::Actor* a_attacker, RE::Actor* a_target, float a_base)
		{
			float       mult = a_base > 1.0f ? a_base : std::max(BuildValue("power_attack_damage"), 1.0f);
			const auto* form = a_attacker->GetEquippedObject(false);
			auto*       weapon = form ? form->As<RE::TESObjectWEAP>() : nullptr;
			RE::BGSEntryPoint::HandleEntryPoint(RE::BGSEntryPoint::ENTRY_POINT::kModPowerAttackDamage, a_attacker, weapon, a_target, std::addressof(mult));
			return std::max(mult, 1.0f);
		}

		// MORDHAU knocked a character's shield away (it parried with it on too little stamina): the Skyrim character's shield
		// goes to the ground too, as Skyrim's own object - it kept it before, and only MORDHAU's disarm sound was heard
		// (2026-10-23). Picked up again the Skyrim way.
		bool DropSkyrimShield(RE::Actor* a_actor)
		{
			auto* shield = a_actor->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kShield);
			if (!shield) {
				logger::info("shield: MORDHAU knocked {}'s shield away, but it wears none in Skyrim", a_actor->GetDisplayFullName());
				return false;
			}
			// (beside it, a little ahead of its shield arm, where MORDHAU's fell)
			const float  heading = a_actor->GetAngleZ();
			RE::NiPoint3 at = a_actor->GetPosition() + RE::NiPoint3{ std::sin(heading) * 40.0f - std::cos(heading) * 30.0f, std::cos(heading) * 40.0f + std::sin(heading) * 30.0f, 50.0f };
			const auto   ref = a_actor->DropObject(shield, nullptr, 1, std::addressof(at), nullptr);
			logger::info("shield: MORDHAU knocked {}'s {} away: it lies on the ground in Skyrim too{}", a_actor->GetDisplayFullName(), shield->GetName(), ref ? "" : " (Skyrim did not drop it)");
			return static_cast<bool>(ref);
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
				LogSkyrimHit(a_attacker, a_target, *hit);
				if (a_mordhauDamage >= 0.0f) {
					const float full = std::max(a_target->AsActorValueOwner()->GetPermanentActorValue(RE::ActorValue::kHealth), 1.0f);
					bool        inDamage = false;
					const float sneak = a_attacker == RE::PlayerCharacter::GetSingleton() ? SneakMult(a_attacker, a_target, *hit, &inDamage) : 1.0f;
					float       critPart = 0.0f;
					const float crit = CritMult(a_attacker, a_target, *hit, &critPart);
					const float plain = hit->totalDamage - critPart;
					if (Progress("damage_model") > 0.5f) {
						// (the crit is in Skyrim's damage already, and the sneak attack when the game made it one)
						// (the player's heavy strike: Skyrim's power-attack damage, with its perks)
						const bool  heavy = a_attacker == RE::PlayerCharacter::GetSingleton() && Clock::now() < g_heavyUntil;
						const float power = heavy ? PowerAttackMult(a_attacker, a_target, 0.0f) : 1.0f;
						if (heavy) {
							hit->flags.set(RE::HitData::Flag::kPowerAttack);
							logger::info("heavy: the player's heavy strike on {}: Skyrim's power attack x{:.2f}", a_target->GetDisplayFullName(), power);
						}
						hit->totalDamage = full * std::min(SkyrimHitFraction(a_attacker, a_target, hit->totalDamage * power, a_mordhauDamage) * (inDamage ? 1.0f : sneak), 1.0f);
					} else {
						hit->totalDamage = full * std::min(HitFraction(a_attacker, a_target, a_mordhauDamage, inDamage ? plain / sneak : plain) * sneak * crit, 1.0f);
					}
					if (sneak > 1.0f) {
						logger::info("progress: sneak attack on {}: x{:.1f} ({})", a_target->GetDisplayFullName(), sneak, inDamage ? "Skyrim's" : "worked out here");
					}
					if (crit > 1.0f) {
						logger::info("progress: critical hit on {}: x{:.2f} (crit damage {:.0f} of Skyrim's {:.0f})", a_target->GetDisplayFullName(), crit, critPart, plain + critPart);
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
			if (blocked) {
				// (what Skyrim's block cost it is Skyrim's own spending, not a drain for its stand-in: sheets/builds.json standin_stamina)
				for (auto& m : g_mirrored) {
					if (m.handle.get().get() == a_target && m.staminaSet >= 0.0f) {
						m.staminaSet = values->GetActorValue(RE::ActorValue::kStamina);
					}
				}
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
				LogSkyrimHit(a_attacker, a_player, *hit);
				float       critPart = 0.0f;
				const float crit = CritMult(a_attacker, a_player, *hit, &critPart);
				const float full = std::max(values->GetPermanentActorValue(RE::ActorValue::kHealth), 1.0f);
				// (its power attack, as Skyrim's AI chose it: Skyrim's power-attack damage, with its perks)
				float power = 1.0f;
				for (const auto& m : g_mirrored) {
					if (m.handle.get().get() == a_attacker && Clock::now() < m.powerUntil && m.attacks == m.powerAttack) {
						power = PowerAttackMult(a_attacker, a_player, m.powerMult);
						hit->flags.set(RE::HitData::Flag::kPowerAttack);
						logger::info("heavy: {}'s power attack on the player: Skyrim's power attack x{:.2f}", a_attacker->GetDisplayFullName(), power);
					}
				}
				hit->totalDamage = Progress("damage_model") > 0.5f ? full * std::min(SkyrimHitFraction(a_attacker, a_player, hit->totalDamage * power, a_mordhauDamage), 1.0f)
				                                                   : full * std::min(HitFraction(a_attacker, a_player, a_mordhauDamage, 0.0f) * crit, 1.0f);
				if (crit > 1.0f) {
					logger::info("progress: {}'s critical hit on the player: x{:.2f}", a_attacker->GetDisplayFullName(), crit);
				}
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
		// Nor one lying limp alive - paralysed, knocked down - for as long as it lies there (limp_speed_limit): Skyrim's
		// hits pushed paralysed bodies through the air.
		void CalmBodies()
		{
			const float deathLimit = BuildValue("death_speed_limit"), limpLimit = BuildValue("limp_speed_limit");
			if (deathLimit <= 0.0f && limpLimit <= 0.0f) {
				return;
			}
			static std::unordered_map<std::uint32_t, Clock::time_point> fought;  // id -> when it was last seen alive with a stand-in
			static std::unordered_map<std::uint32_t, Clock::time_point> died;
			static std::unordered_set<std::uint32_t>                    limp;  // ...of those, the ones lying limp alive (held while they lie, and half a second after)
			for (const auto& m : g_mirrored) {
				if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(m.id)) {
					if (!actor->IsDead()) {
						fought[m.id] = Clock::now();
						if (!m.dummy && limpLimit > 0.0f && actor->IsInRagdollState()) {
							died[m.id] = Clock::now();
							limp.insert(m.id);
						}
					}
				}
			}
			for (auto it = fought.begin(); it != fought.end();) {
				auto* actor = RE::TESForm::LookupByID<RE::Actor>(it->first);
				if (actor && actor->IsDead() && deathLimit > 0.0f && (!died.contains(it->first) || limp.contains(it->first)) && Seconds(it->second) < 2.0f) {
					died[it->first] = Clock::now();  // (dying while limp: the death's half second from now)
					limp.erase(it->first);
				}
				it = Seconds(it->second) > 10.0f ? fought.erase(it) : std::next(it);
			}
			const float scale = RE::bhkWorld::GetWorldScale();
			for (auto it = died.begin(); it != died.end();) {
				auto* actor = RE::TESForm::LookupByID<RE::Actor>(it->first);
				auto* root = actor ? actor->Get3D(false) : nullptr;
				if (!root || Seconds(it->second) > 0.5f) {
					limp.erase(it->first);
					it = died.erase(it);
					continue;
				}
				const bool  lying = limp.contains(it->first);
				const float limit = lying ? limpLimit : deathLimit;
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
				static std::unordered_set<std::uint32_t> told, toldLimp;
				if (slowed > 0 && (lying ? toldLimp : told).insert(it->first).second) {
					logger::info("{}: {}'s body was about to fly ({} parts over {:.0f} units/s): held back", lying ? "limp" : "death", actor->GetDisplayFullName(), slowed, limit);
				}
				++it;
			}
		}
	}
}
