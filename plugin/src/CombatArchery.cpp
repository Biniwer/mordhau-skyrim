#include "CombatInternal.h"

namespace dm::Combat
{
	inline namespace internal
	{
		// What MORDHAU's character has thrown: each new throw is taken over into Skyrim's world.
		void TrackMordhauShots(RE::PlayerCharacter* a_player)
		{
		{
			const auto link = Link::Current();
			if (!link.live) {
				g_shotsKnown = false;
			} else if (!g_shotsKnown || link.shots < g_seenShots) {
				g_shotsKnown = true;
				g_seenShots = link.shots;
			} else if (link.shots != g_seenShots && link.shotSpeed < 0.0f) {
				// MORDHAU's character threw its weapon (sheets/builds.json thrown_weapons)
				g_seenShots = link.shots;
				const auto* held = a_player->GetEquippedObject(false);
				auto*       weaponForm = held ? const_cast<RE::TESObjectWEAP*>(held->As<RE::TESObjectWEAP>()) : nullptr;
				if (BuildValue("thrown_weapons") > 0.5f) {
					const float        facing = a_player->GetAngleZ();
					const RE::NiPoint3 forward{ std::sin(facing), std::cos(facing), 0.0f };
					const RE::NiPoint3 right{ std::cos(facing), -std::sin(facing), 0.0f };
					RE::NiPoint3       dir = right * link.shotDir[0] + forward * link.shotDir[1] + RE::NiPoint3{ 0.0f, 0.0f, link.shotDir[2] };
					dir = dir.Length() > 1e-3f ? dir / dir.Length() : forward;
					PendingShot shot;
					shot.at = a_player->GetPosition() + RE::NiPoint3{ 0.0f, 0.0f, a_player->GetHeight() * 0.85f } + dir * 45.0f + right * 14.0f;
					shot.velocity = dir * (-link.shotSpeed / 1.42875f);
					shot.gravity = BuildValue("gravity_cm") * link.shotGravity / 1.42875f;
					if (g_dev.throwSlow) {
						shot.velocity = shot.velocity * 0.1f;
						shot.gravity *= 0.01f;
					}
					std::memcpy(shot.damage, link.shotDamage, sizeof(shot.damage));
					std::memcpy(shot.head, link.shotHead, sizeof(shot.head));
					// its model: MORDHAU's class of the weapon held ("BP_Axe_C" -> "w_Axe"), kept in a set so the name outlives the flight
					static std::set<std::string> models;
					std::string                  name = link.weaponClass;
					if (name.rfind("BP_", 0) == 0 && name.ends_with("_C")) {
						name = "w_" + name.substr(3, name.size() - 5);
					}
					// a thrown SHIELD (its gravity scale comes negative): the shield flies, the weapon stays in the hand
					auto*      shieldForm = a_player->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kShield);
					const bool shieldThrown = link.shotGravity < 0.0f;
					if (shieldThrown) {
						const char* cls = MordhauShieldOf(shieldForm);
						name = *cls ? std::string("w_") + (cls + 3) : std::string("w_TargeShield");
						shot.gravity = -shot.gravity;
						weaponForm = nullptr;
					}
					shot.thrownModel = models.insert(name).first->c_str();
					shot.thrownForm = weaponForm;
					shot.thrownItem = shieldThrown ? static_cast<RE::TESBoundObject*>(shieldForm) : static_cast<RE::TESBoundObject*>(weaponForm);
					if (shieldThrown && shieldForm) {
						RE::ActorEquipManager::GetSingleton()->UnequipObject(a_player, shieldForm);
						a_player->RemoveItem(shieldForm, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
					}
					shot.spins = name.find("pear") == std::string::npos && name.find("Corseca") == std::string::npos;
					g_pendingShots.push_back(shot);
					if (weaponForm) {
						// out of the player's hands and bag: it is in the air now, and will lie where it lands
						RE::ActorEquipManager::GetSingleton()->UnequipObject(a_player, weaponForm);
						a_player->RemoveItem(weaponForm, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
					}
					logger::info("throw: MORDHAU threw {} ({:.0f} cm/s, damage {:.0f}/{:.0f}/{:.0f}/{:.0f}): {} flies", name, -link.shotSpeed, link.shotDamage[0], link.shotDamage[1], link.shotDamage[2],
						link.shotDamage[3], shieldThrown ? (shieldForm ? shieldForm->GetName() : "no Skyrim shield") : weaponForm ? weaponForm->GetName() : "nothing of Skyrim's (no weapon equipped)");
				}
			} else if (link.shots != g_seenShots) {
				g_seenShots = link.shots;  // (not a throw: with bows Skyrim's own, nothing of MORDHAU's flies)
			}
		}
		}

		// What flies: its own arc for this frame; a character or the world stops it, and what was struck is hurt.
		void MoveFlights(RE::PlayerCharacter* a_player, std::vector<Flying>& flying, const float dt, const auto& armourTier, const auto& hurt, const auto& arrowSound, auto* world, const std::vector<RE::Actor*>& people)
		{
		for (auto it = flying.begin(); it != flying.end();) {
			auto&              f = *it;
			const RE::NiPoint3 from = f.at;
			f.velocity.z -= f.gravity * dt;
			RE::NiPoint3 to = from + f.velocity * dt;
			bool         stopped = false;
			RE::Actor*   struck = nullptr;
			float        best = 1.0f;
			const float  reach = 22.0f;
			for (auto* actor : people) {
				// nearest point of this step to the character's middle line (feet to head)
				const RE::NiPoint3 feet = actor->GetPosition();
				const float        height = std::max(actor->GetHeight(), 60.0f);
				const RE::NiPoint3 seg = to - from;
				const float        len2 = std::max(seg.x * seg.x + seg.y * seg.y, 1e-3f);
				const float        t = std::clamp(((feet.x - from.x) * seg.x + (feet.y - from.y) * seg.y) / len2, 0.0f, 1.0f);
				const RE::NiPoint3 p = from + seg * t;
				const float        dx = p.x - feet.x, dy = p.y - feet.y;
				if (dx * dx + dy * dy < reach * reach && p.z > feet.z - 5.0f && p.z < feet.z + height + 5.0f && t < best) {
					best = t, struck = actor;
				}
			}
			if (world) {
				const float     scale = RE::bhkWorld::GetWorldScale();
				RE::bhkPickData pick;
				pick.rayInput.from = RE::hkVector4(from.x * scale, from.y * scale, from.z * scale, 0.0f);
				pick.rayInput.to = RE::hkVector4(to.x * scale, to.y * scale, to.z * scale, 0.0f);
				RE::CFilter filter;
				a_player->GetCollisionFilterInfo(filter);
				filter.SetCollisionLayer(RE::COL_LAYER::kLOS);
				pick.rayInput.filterInfo = filter;
				{
					RE::BSReadLockGuard lock(world->worldLock);
					world->PickObject(pick);
				}
				if (pick.rayOutput.HasHit() && pick.rayOutput.hitFraction < best) {
					// (the ray also meets a character's own body a little before its middle line: that is still the character)
					const RE::NiPoint3 hit = from + (to - from) * pick.rayOutput.hitFraction;
					RE::Actor*         body = nullptr;
					for (auto* actor : people) {
						const RE::NiPoint3 feet = actor->GetPosition();
						const float        dx = hit.x - feet.x, dy = hit.y - feet.y;
						if (dx * dx + dy * dy < 45.0f * 45.0f && hit.z > feet.z - 5.0f && hit.z < feet.z + std::max(actor->GetHeight(), 60.0f) + 5.0f) {
							body = actor;
						}
					}
					best = pick.rayOutput.hitFraction;
					struck = body;
					stopped = true;
				}
			}
			if (struck) {
				stopped = true;
			}
			to = from + (to - from) * best;
			f.at = to;
			if (stopped) {
				if (struck) {
					const int  tier = armourTier(struck);
					const bool head = to.z > struck->GetPosition().z + struck->GetHeight() * 0.84f;
					hurt(struck, f.damage[tier] + (head ? f.head[tier] : 0.0f), head ? "a thrown weapon (head)" : "a thrown weapon");
					CharacterRender::AddBlood(struck->GetFormID(), 0.2f);
					arrowSound("WPNImpactArrowVsFleshSD", to);
				} else {
					arrowSound("WPNImpactArrowBounceSD", to);
				}
				if (f.thrownItem) {
					// the Skyrim item itself, lying there: picked up the normal Skyrim way
					if (auto ref = a_player->PlaceObjectAtMe(f.thrownItem, false)) {
						RE::NiPoint3 at = to - f.velocity * (20.0f / std::max(f.velocity.Length(), 1.0f));
						at.z += 12.0f;
						ref->SetPosition(at);
					}
				}
				logger::info("throw: the thrown weapon {} {:.0f} units away after {:.2f} s{}", struck ? std::format("struck {}", struck->GetDisplayFullName()) : std::string("came down"),
					to.GetDistance(a_player->GetPosition()), Seconds(f.born), f.thrownItem ? std::format("; {} lies there", f.thrownItem->GetName()) : std::string());
				it = flying.erase(it);
			} else if (Seconds(f.born) > 6.0f) {
				it = flying.erase(it);
			} else {
				++it;
			}
		}
		}

		void FlyThrownWeapons(RE::PlayerCharacter* a_player)
		{
		{
			static std::vector<Flying>                         flying;
			static Clock::time_point                           lastFrame = Clock::now();
			const float                                        dt = std::clamp(Seconds(lastFrame), 0.001f, 0.1f);
			lastFrame = Clock::now();
			const auto armourTier = [](RE::Actor* a_actor) {
				if (auto* body = a_actor->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kBody)) {
					return body->IsHeavyArmor() ? 3 : body->IsLightArmor() ? 1 : 0;
				}
				return 0;
			};
			const auto hurt = [&](RE::Actor* a_victim, float a_mordhau, const char* a_by) {
				auto*       values = a_victim->AsActorValueOwner();
				const float full = std::max(values->GetPermanentActorValue(RE::ActorValue::kHealth), 1.0f);
				const float before = values->GetActorValue(RE::ActorValue::kHealth);
				const float damage = full * HitFraction(a_player, a_victim, a_mordhau, 0.0f);
				a_victim->DoDamage(damage, a_victim == a_player ? nullptr : a_player, false);
				logger::info("flight: {} hurt {} (MORDHAU damage {:.0f} -> {:.0f}), health {:.0f} -> {:.0f}{}", a_by, a_victim->GetDisplayFullName(), a_mordhau, damage, before,
					values->GetActorValue(RE::ActorValue::kHealth), a_victim->IsDead() ? ", dead" : "");
			};

			// --- everyone near
			std::vector<RE::Actor*> people;
			RE::ProcessLists::GetSingleton()->ForEachHighActor([&](RE::Actor* a_actor) {
				if (a_actor && a_actor != a_player && !a_actor->IsDead() && a_actor->Is3DLoaded()) {
					people.push_back(a_actor);
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});

			// Skyrim's own sound of an arrow striking, where it struck
			const auto arrowSound = [](const char* a_id, const RE::NiPoint3& a_at) {
				if (auto* audio = RE::BSAudioManager::GetSingleton()) {
					RE::BSSoundHandle handle;
					audio->GetSoundHandleByName(handle, a_id, 0x1A);
					if (handle.IsValid()) {
						handle.SetPosition(a_at);
						handle.Play();
					}
				}
			};
			for (const auto& shot : g_pendingShots) {
				Flying arrow;
				arrow.thrownModel = shot.thrownModel;
				arrow.thrownForm = shot.thrownForm;
				arrow.thrownItem = shot.thrownItem;
				arrow.spins = shot.spins;
				arrow.at = shot.at;
				arrow.velocity = shot.velocity;
				arrow.gravity = shot.gravity;
				std::memcpy(arrow.damage, shot.damage, sizeof(arrow.damage));
				std::memcpy(arrow.head, shot.head, sizeof(arrow.head));
				arrow.born = Clock::now();
				flying.push_back(arrow);
			}
			g_pendingShots.clear();
			// --- what flies: its own arc; a character or the world stops it
			auto* cell = a_player->GetParentCell();
			auto* world = cell ? cell->GetbhkWorld() : nullptr;
			MoveFlights(a_player, flying, dt, armourTier, hurt, arrowSound, world, people);

			// --- what is drawn
			std::vector<CharacterRender::Prop> shown;
			for (const auto& f : flying) {
				CharacterRender::Prop prop;
				prop.model = f.thrownModel;
				prop.lengthUp = false;
				prop.at = f.at;
				prop.yaw = std::atan2(f.velocity.x, f.velocity.y);
				// (a weapon's model stands upright, its edge to the front: tumbled end over end (top going forward), as
				// MORDHAU's thrown axes and hammers do; a spear is laid point first along its flight)
				const float along = std::atan2(f.velocity.z, std::sqrt(f.velocity.x * f.velocity.x + f.velocity.y * f.velocity.y));
				prop.pitch = f.spins ? -Seconds(f.born) * (g_dev.throwSlow ? 1.4f : 14.0f) : along - 1.5707963f;
				shown.push_back(prop);
				if (g_dev.throwSlow) {
					// (looking for it: an arrow flying with it, and the same weapon standing still ahead of the player, three ways round)
					static Clock::time_point told{};
					if (Seconds(told) > 1.0f) {
						told = Clock::now();
						const float        facing = a_player->GetAngleZ();
						const RE::NiPoint3 d = f.at - a_player->GetPosition();
						logger::info("throw: in flight {:.0f} ahead, {:.0f} right, {:.0f} up of the player; velocity {:.0f} {:.0f} {:.0f}; yaw {:.2f} pitch {:.2f}", d.x * std::sin(facing) + d.y * std::cos(facing),
							d.x * std::cos(facing) - d.y * std::sin(facing), d.z, f.velocity.x, f.velocity.y, f.velocity.z, prop.yaw, prop.pitch);
					}
					CharacterRender::Prop mark = prop;
					mark.model = "arrow";
					mark.at.z += 20.0f;
					mark.pitch = 0.0f;
					shown.push_back(mark);
				}
			}
			CharacterRender::SetProps(shown);
		}
		}
	}
}
