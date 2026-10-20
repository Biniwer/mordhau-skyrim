#include "CombatInternal.h"

namespace dm::Combat
{
	inline namespace internal
	{
		// Archery in side-by-side mode: the MORDHAU item for the bow or crossbow the player has out (null: none)
		const char* RangedWish(RE::PlayerCharacter* a_player)
		{
			if (BuildValue("mordhau_archery") < 0.5f) {
				return nullptr;  // (switched off: bows are Skyrim's own)
			}
			if (BuildValue("mordhau_archery") < 0.5f) {
				return nullptr;  // (bows are Skyrim's own: sheets/builds.json mordhau_archery)
			}
			if (!Link::Current().live || !a_player->AsActorState()->IsWeaponDrawn()) {
				return nullptr;
			}
			const auto* form = a_player->GetEquippedObject(false);
			const auto* weapon = form ? form->As<RE::TESObjectWEAP>() : nullptr;
			if (!weapon) {
				return nullptr;
			}
			if (weapon->IsCrossbow()) {
				return "Ranged/BP_Crossbow";
			}
			if (weapon->IsBow()) {
				std::string name = weapon->GetName() ? weapon->GetName() : "";
				for (auto& c : name) {
					c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
				}
				return name.find("long") != std::string::npos ? "Ranged/BP_Longbow" : "Ranged/BP_RecurveBow";
			}
			return nullptr;
		}

		// Bows, and what MORDHAU's character has shot or thrown: each new projectile is taken over into Skyrim's world.
		void TrackMordhauShots(RE::PlayerCharacter* a_player)
		{
		{
			const char* wish = RangedWish(a_player);
			Link::SetWeaponWish(wish ? wish : "");
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
				g_seenShots = link.shots;
				auto*       ammo = a_player->GetCurrentAmmo();
				const auto* form = a_player->GetEquippedObject(false);
				auto*       bowForm = form ? const_cast<RE::TESObjectWEAP*>(form->As<RE::TESObjectWEAP>()) : nullptr;
				if (ammo && bowForm && wish) {
					// MORDHAU's direction (its own aim and sway) turned with the player, from about the bow hand's height
					const float        facing = a_player->GetAngleZ();
					const RE::NiPoint3 forward{ std::sin(facing), std::cos(facing), 0.0f };
					const RE::NiPoint3 right{ std::cos(facing), -std::sin(facing), 0.0f };
					RE::NiPoint3       dir = right * link.shotDir[0] + forward * link.shotDir[1] + RE::NiPoint3{ 0.0f, 0.0f, link.shotDir[2] };
					dir = dir.Length() > 1e-3f ? dir / dir.Length() : forward;
					// (first person: from the eye, as MORDHAU's arrow goes where its crosshair points; third person: from about the bow hand's height)
					auto*              shotCamera = RE::PlayerCamera::GetSingleton();
					const bool         eye = shotCamera && (shotCamera->IsInFirstPerson() || g_eyeView.load()) && shotCamera->cameraRoot;
					RE::NiPoint3       origin = (eye ? shotCamera->cameraRoot->world.translate : a_player->GetPosition() + RE::NiPoint3{ 0.0f, 0.0f, a_player->GetHeight() * 0.82f }) + dir * 40.0f;
					if (eye) {
						// (from the bow, a hand's width right of and below the eye - straight out of the eye it was a dot nobody could see)
						origin += right * BuildValue("arrow_start_right") + RE::NiPoint3{ 0.0f, 0.0f, -BuildValue("arrow_start_down") };
					}
					// MORDHAU's own arrow: its speed (cm/s -> units/s) and gravity, flown by the plugin itself
					PendingShot shot;
					shot.at = origin;
					shot.velocity = dir * (link.shotSpeed / 1.42875f);
					shot.gravity = BuildValue("gravity_cm") * link.shotGravity / 1.42875f;
					if (g_dev.bowSlow > 0.0f) {
						shot.velocity = shot.velocity * g_dev.bowSlow;  // (test: a slow arrow, to look at it in the air)
						shot.gravity = 0.0f;
					}
					std::memcpy(shot.damage, link.shotDamage, sizeof(shot.damage));
					std::memcpy(shot.head, link.shotHead, sizeof(shot.head));
					shot.crossbow = bowForm->IsCrossbow();
					g_pendingShots.push_back(shot);
					const float speed = shot.velocity.Length();
					std::memcpy(g_shot.damage, link.shotDamage, sizeof(g_shot.damage));
					std::memcpy(g_shot.head, link.shotHead, sizeof(g_shot.head));
					g_shot.when = Clock::now();
					a_player->RemoveItem(ammo, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
					logger::info("archery: MORDHAU loosed ({:.0f} cm/s, damage {:.0f}/{:.0f}/{:.0f}/{:.0f}, head +{:.0f}/{:.0f}/{:.0f}/{:.0f}): Skyrim's {} flies at {:.0f} units/s, direction ({:.2f}, {:.2f}, {:.2f}), arrow {}",
						link.shotSpeed, link.shotDamage[0], link.shotDamage[1], link.shotDamage[2], link.shotDamage[3], link.shotHead[0], link.shotHead[1], link.shotHead[2], link.shotHead[3],
						ammo->GetName(), speed, dir.x, dir.y, dir.z, "is used up; MORDHAU's own arrow flies");
				} else {
					logger::info("archery: MORDHAU loosed but Skyrim has no arrow or bow in hand: nothing fired");
				}
			}
		}
		}

		// What flies by MORDHAU's rules in Skyrim's world: the player's arrows, bolts and thrown weapons, and (sheets/builds.json
		// enemy_arrows_own) enemies' arrows. Each has its own arc; a character or the world stops it.
		// Enemy arrows: each one Skyrim launches for somebody else is taken over at once and flown by MORDHAU's rules (sheets/builds.json enemy_arrows_own).
		void TakeOverEnemyArrows(RE::PlayerCharacter* a_player, std::vector<Flying>& flying, const Link::State& link)
		{
		if (link.live && BuildValue("enemy_arrows_own") > 0.5f) {
			if (auto* manager = RE::Projectile::Manager::GetSingleton()) {
				std::vector<RE::NiPointer<RE::Projectile>> taken;
				{
					RE::BSSpinLockGuard guard(manager->projectileLock);
					for (const auto* list : { &manager->unlimited, &manager->limited, &manager->pending }) {
						for (const auto& handle : *list) {
							auto projectile = handle.get();
							if (!projectile || !projectile->As<RE::ArrowProjectile>()) {
								continue;
							}
							auto& data = projectile->GetProjectileRuntimeData();
							auto  owner = data.shooter.get();
							if (!owner || owner.get() == a_player || !data.impacts.empty() || data.flags.any(RE::Projectile::Flags::kDestroyed) || data.linearVelocity.Length() < 300.0f) {
								continue;
							}
							// (each one ONCE: Skyrim keeps a taken arrow in its lists for a while - without this every frame
							// made another arrow of it, and the storm killed the player)
							static std::unordered_set<std::uint32_t> seen;
							if (seen.size() > 4000) {
								seen.clear();
							}
							if (!seen.insert(projectile->GetFormID()).second) {
								continue;
							}
							taken.push_back(RE::NiPointer<RE::Projectile>(projectile.get()));
						}
					}
				}
				for (auto& projectile : taken) {
					auto&       data = projectile->GetProjectileRuntimeData();
					const auto* bow = data.weaponSource;
					std::string name = bow && bow->GetName() ? bow->GetName() : "";
					std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
					const int kind = bow && bow->IsCrossbow() ? 3 : name.find("long") != std::string::npos ? 2 : 1;
					static const float speed[3]{ 5400.0f, 6300.0f, 6700.0f };  // cm/s, MORDHAU's recurve / long bow / crossbow
					static const float damage[3][4]{ { 50, 40, 26, 25 }, { 70, 50, 36, 34 }, { 60, 50, 42, 40 } };
					Flying arrow;
					arrow.arrow = kind == 3 ? 2 : 1;
					arrow.at = projectile->GetPosition();
					const RE::NiPoint3 dir = data.linearVelocity / data.linearVelocity.Length();
					// (Skyrim's archer aimed for Skyrim's arrow: its launch speed is kept and the drop is about Skyrim's own, so
					// the shot still goes where it was aimed; MORDHAU's speed with MORDHAU's drop fell short)
					(void)speed;
					(void)dir;
					arrow.velocity = data.linearVelocity;
					arrow.gravity = BuildValue("enemy_arrow_gravity");
					for (int tier = 0; tier < 4; ++tier) {
						arrow.damage[tier] = damage[kind - 1][tier];
						arrow.head[tier] = tier == 0 ? 50.0f : tier == 1 ? 30.0f : 20.0f;
					}
					if (auto owner = data.shooter.get()) {
						if (auto* actor = owner->As<RE::Actor>()) {
							arrow.shooter = actor->GetHandle();
						}
					}
					arrow.born = Clock::now();
					flying.push_back(arrow);
					// Skyrim's own arrow is stopped and put out of the way (far under the ground) until Skyrim clears it
					data.linearVelocity = RE::NiPoint3{};
					data.flags.set(RE::Projectile::Flags::kDestroyed);
					projectile->SetPosition(projectile->GetPosition() + RE::NiPoint3{ 0.0f, 0.0f, -30000.0f });
					projectile->Disable();
					projectile->SetDelete(true);
					static int told = 0;
					if (told++ < 6) {
						logger::info("archery: an enemy's {} is MORDHAU's now ({:.0f} units/s)", kind == 3 ? "bolt" : kind == 2 ? "long-bow arrow" : "arrow", arrow.velocity.Length());
					}
				}
			}
		}
		}

		// What flies: its own arc for this frame; a character or the world stops it, and what was struck is hurt.
		void MoveFlights(RE::PlayerCharacter* a_player, std::vector<Flying>& flying, const float dt, const auto& armourTier, const auto& hurt, std::vector<StuckArrow>& stuckArrows, const auto& arrowSound, const Link::State& link, auto* world, const std::vector<RE::Actor*>& people)
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
			auto       arrowShooter = f.shooter.get();
			bool       atPlayer = false;
			if (arrowShooter) {
				// the player, the same way (an enemy's arrow only)
				const RE::NiPoint3 feet = a_player->GetPosition();
				const float        height = std::max(a_player->GetHeight(), 60.0f);
				const RE::NiPoint3 seg = to - from;
				const float        len2 = std::max(seg.x * seg.x + seg.y * seg.y, 1e-3f);
				const float        t = std::clamp(((feet.x - from.x) * seg.x + (feet.y - from.y) * seg.y) / len2, 0.0f, 1.0f);
				const RE::NiPoint3 p = from + seg * t;
				const float        dx = p.x - feet.x, dy = p.y - feet.y;
				if (dx * dx + dy * dy < reach * reach && p.z > feet.z - 5.0f && p.z < feet.z + height + 5.0f) {
					best = t, atPlayer = true;
				}
			}
			for (auto* actor : people) {
				if (arrowShooter && actor == arrowShooter.get()) {
					continue;  // (not its own shooter)
				}
				// nearest point of this step to the character's middle line (feet to head)
				const RE::NiPoint3 feet = actor->GetPosition();
				const float        height = std::max(actor->GetHeight(), 60.0f);
				const RE::NiPoint3 seg = to - from;
				const float        len2 = std::max(seg.x * seg.x + seg.y * seg.y, 1e-3f);
				const float        t = std::clamp(((feet.x - from.x) * seg.x + (feet.y - from.y) * seg.y) / len2, 0.0f, 1.0f);
				const RE::NiPoint3 p = from + seg * t;
				const float        dx = p.x - feet.x, dy = p.y - feet.y;
				if (dx * dx + dy * dy < reach * reach && p.z > feet.z - 5.0f && p.z < feet.z + height + 5.0f && t < best) {
					best = t, struck = actor, atPlayer = false;
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
					atPlayer = false;
					stopped = true;
				}
			}
			if (struck) {
				stopped = true;
			}
			if (atPlayer) {
				// MORDHAU's rule for it: a parry in its window, or a raised shield, facing the arrow, stops it
				stopped = true;
				RE::NiPoint3 back = f.velocity * -1.0f;
				back.z = 0.0f;
				const float facing = a_player->GetAngleZ();
				const float len = std::max(back.Length(), 1e-3f);
				const float angle = std::acos(std::clamp((back.x * std::sin(facing) + back.y * std::cos(facing)) / len, -1.0f, 1.0f)) * 57.29578f;
				const float window = g_stats.Parry("parry_window") > 0.0f ? g_stats.Parry("parry_window") : 0.325f;
				const bool  wallUp = BuildValue("shield_wall_blocks") > 0.5f && link.kind == 2 && link.stage == 0 && std::string_view(link.motion).find("ShieldParry") != std::string_view::npos;
				const bool  parrying = (link.kind == 2 && link.motionTime <= window) || wallUp || Seconds(g_parryAsked) <= g_stats.Parry("live_parry_grace") || (g_shieldBlock.load() && PlayerShield(a_player));
				const RE::NiPoint3 hitAt = from + (to - from) * best;
				if (parrying && angle <= g_stats.Parry("parry_max_angle")) {
					// heard and seen: Skyrim's block sound at the spot, and MORDHAU's character flinches its parry (its own motion)
					arrowSound(g_shieldBlock.load() && PlayerShield(a_player) ? "WPNBlockShieldLSD" : "WPNBlockBlade1HandVsOtherSD", hitAt);
					arrowSound("WPNImpactArrowBounceSD", hitAt);
					logger::info("archery: the player parried {}'s {} (angle {:.0f}, {})", arrowShooter->GetDisplayFullName(), f.arrow == 2 ? "bolt" : "arrow", angle, link.motion);
				} else {
					const auto* body = a_player->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kBody);
					const int   tier = body ? (body->IsHeavyArmor() ? 3 : body->IsLightArmor() ? 1 : 0) : 0;
					const bool  head = hitAt.z > a_player->GetPosition().z + a_player->GetHeight() * 0.84f;
					const float mordhau = f.damage[tier] + (head ? f.head[tier] : 0.0f);
					const float full = std::max(a_player->AsActorValueOwner()->GetPermanentActorValue(RE::ActorValue::kHealth), 1.0f);
					const float amount = g_dev.god ? 0.0f : full * HitFraction(arrowShooter.get(), a_player, mordhau, 0.0f);
					a_player->DoDamage(amount, arrowShooter.get(), false);
					arrowSound("WPNImpactArrowVsFleshPLAYER", hitAt);
					CharacterRender::AddBlood(0, 0.12f);
					logger::info("archery: {}'s {} hit the player{} (armour tier {}): MORDHAU damage {:.0f} -> {:.1f} (not parried: {} kind {} at {:.2f} s, angle {:.0f})",
						arrowShooter->GetDisplayFullName(), f.arrow == 2 ? "bolt" : "arrow", head ? " in the head" : "", tier, mordhau, amount, link.motion, link.kind, link.motionTime, angle);
				}
			}
			to = from + (to - from) * best;
			f.at = to;
			if (stopped) {
				if (f.arrow == 3) {
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
					{
						float nearest = 1e9f, nearUp = 0.0f;
						for (auto* actor : people) {
							const RE::NiPoint3 d = to - actor->GetPosition();
							if (const float flat = std::sqrt(d.x * d.x + d.y * d.y); flat < nearest) {
								nearest = flat, nearUp = d.z;
							}
						}
						logger::info("throw: it stopped {:.0f} units (flat) from the nearest of {} people, {:.0f} above their feet; {:.2f} s in the air, {:.0f} above the player's feet", nearest, people.size(), nearUp,
							Seconds(f.born), to.z - a_player->GetPosition().z);
					}
					logger::info("throw: the thrown weapon {} {:.0f} units away{}", struck ? std::format("struck {}", struck->GetDisplayFullName()) : std::string("came down"),
						to.GetDistance(a_player->GetPosition()), f.thrownItem ? std::format("; {} lies there", f.thrownItem->GetName()) : std::string());
				} else if (atPlayer) {
					// (dealt with above)
				} else if (struck) {
					const int  tier = armourTier(struck);
					const bool head = to.z > struck->GetPosition().z + struck->GetHeight() * 0.84f;
					hurt(struck, f.damage[tier] + (head ? f.head[tier] : 0.0f), head ? (f.arrow == 2 ? "a crossbow bolt (head)" : "an arrow (head)") : (f.arrow == 2 ? "a crossbow bolt" : "an arrow"));
					CharacterRender::AddBlood(struck->GetFormID(), 0.15f);
					arrowSound("WPNImpactArrowVsFleshSD", to);
				} else {
					// it stays where it struck for a while, as MORDHAU's arrows do (sheets/builds.json arrow_stays_seconds)
					StuckArrow stuckArrow;
					stuckArrow.bolt = f.arrow == 2;
					stuckArrow.at = to - f.velocity * (36.0f / std::max(f.velocity.Length(), 1.0f));  // (most of the shaft stands out, as in MORDHAU)
					stuckArrow.yaw = std::atan2(f.velocity.x, f.velocity.y);
					stuckArrow.pitch = std::atan2(f.velocity.z, std::sqrt(f.velocity.x * f.velocity.x + f.velocity.y * f.velocity.y));
					stuckArrow.born = Clock::now();
					stuckArrows.push_back(stuckArrow);
					if (stuckArrows.size() > 24) {
						stuckArrows.erase(stuckArrows.begin());
					}
					arrowSound("WPNImpactArrowStickSD", to);
					logger::info("archery: the {} struck the world {:.0f} units away", f.arrow == 2 ? "bolt" : "arrow", to.GetDistance(a_player->GetPosition()));
				}
				it = flying.erase(it);
			} else if (Seconds(f.born) > 6.0f) {
				it = flying.erase(it);
			} else {
				++it;
			}
		}
		}

		void FlyArrowsAndThrownWeapons(RE::PlayerCharacter* a_player)
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

			static std::vector<StuckArrow> stuckArrows;
			// Skyrim's own sound of an arrow striking, where it struck
			const auto arrowSound = [](const char* a_id, const RE::NiPoint3& a_at) { SoundAt(a_id, a_at); };
			// Enemy arrows: each one Skyrim launches for somebody else is taken over at once (sheets/builds.json enemy_arrows_own)
			const auto link = Link::Current();
			TakeOverEnemyArrows(a_player, flying, link);
			for (const auto& shot : g_pendingShots) {
				Flying arrow;
				arrow.arrow = shot.thrownModel ? 3 : shot.crossbow ? 2 : 1;
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
			MoveFlights(a_player, flying, dt, armourTier, hurt, stuckArrows, arrowSound, link, world, people);

			// --- what is drawn
			std::vector<CharacterRender::Prop> shown;
			for (const auto& f : flying) {
				CharacterRender::Prop prop;
				prop.model = f.arrow == 1 ? "arrow" : f.arrow == 2 ? "crossbow_bolt" : f.thrownModel;
				prop.lengthUp = true;
				prop.at = f.at;
				prop.yaw = std::atan2(f.velocity.x, f.velocity.y);
				prop.pitch = std::atan2(f.velocity.z, std::sqrt(f.velocity.x * f.velocity.x + f.velocity.y * f.velocity.y));
				if (f.arrow == 3) {
					// a weapon's model stands upright, its edge to the front: tumbled end over end (top going forward), as
					// MORDHAU's thrown axes and hammers do; a spear is laid point first along its flight
					prop.lengthUp = false;
					prop.pitch = f.spins ? -Seconds(f.born) * (g_dev.throwSlow ? 1.4f : 14.0f) : prop.pitch - 1.5707963f;
				}
				shown.push_back(prop);
				if (g_dev.throwSlow && f.arrow == 3) {
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
			for (auto it = stuckArrows.begin(); it != stuckArrows.end();) {
				if (Seconds(it->born) > BuildValue("arrow_stays_seconds")) {
					it = stuckArrows.erase(it);
					continue;
				}
				CharacterRender::Prop prop;
				prop.model = it->bolt ? "crossbow_bolt" : "arrow";
				prop.lengthUp = true;
				prop.at = it->at;
				prop.yaw = it->yaw;
				prop.pitch = it->pitch;
				shown.push_back(prop);
				++it;
			}
			CharacterRender::SetProps(shown);
			{
				// the streak behind every arrow or bolt in flight: the player's own (flown by this plugin) and Skyrim's
				std::vector<CharacterRender::Trail> trails;
				const float length = BuildValue("arrow_trail_length");
				const auto  streak = [&](const RE::NiPoint3& a_at, const RE::NiPoint3& a_velocity) {
					const float speed = a_velocity.Length();
					if (length > 0.0f && speed > 30.0f && trails.size() < 16) {
						CharacterRender::Trail trail;
						trail.head = a_at;
						trail.tail = a_at - a_velocity * (length / speed);
						trail.width = BuildValue("arrow_trail_width");
						trail.alpha = BuildValue("arrow_trail_alpha");
						trails.push_back(trail);
					}
				};
				for (const auto& f : flying) {
					streak(f.at, f.velocity);
				}
				if (auto* manager = RE::Projectile::Manager::GetSingleton(); manager && length > 0.0f) {
					RE::BSSpinLockGuard guard(manager->projectileLock);
					for (const auto* list : { &manager->unlimited, &manager->limited, &manager->pending }) {
						for (const auto& handle : *list) {
							auto projectile = handle.get();
							if (!projectile || !projectile->Is3DLoaded() || !projectile->As<RE::ArrowProjectile>()) {
								continue;
							}
							const auto& data = projectile->GetProjectileRuntimeData();
							if (!data.impacts.empty() || data.flags.any(RE::Projectile::Flags::kDestroyed)) {
								continue;  // (stuck in something: no longer in flight)
							}
							streak(projectile->GetPosition(), data.linearVelocity);
						}
					}
				}
				CharacterRender::SetTrails(trails);
			}
		}
		}
	}
}
