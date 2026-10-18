#include "CombatInternal.h"

namespace dm::Combat
{
	inline namespace internal
	{
		// sheets/hooks.json: no_bash. MORDHAU has no shield bash; Skyrim's enemies choose one through their combat
		// style, so the choice is taken away. In memory only: nothing is written to the game's files or to saves.
		void DisableBashing()
		{
			auto* data = RE::TESDataHandler::GetSingleton();
			if (!data) {
				return;
			}
			std::uint32_t styles = 0, changed = 0;
			for (auto* style : data->GetFormArray<RE::TESCombatStyle>()) {
				if (!style) {
					continue;
				}
				++styles;
				auto& melee = style->meleeData;
				if (melee.bashMult != 0.0f || melee.bashRecoilMult != 0.0f || melee.bashAttackMult != 0.0f || melee.bashPowerAttackMult != 0.0f) {
					++changed;
				}
				melee.bashMult = 0.0f;
				melee.bashRecoilMult = 0.0f;
				melee.bashAttackMult = 0.0f;
				melee.bashPowerAttackMult = 0.0f;
			}
			logger::info("combat: shield bash switched off in {} of {} combat styles", changed, styles);
		}

		void ApplyMovementTable()
		{
			auto* data = RE::TESDataHandler::GetSingleton();
			if (!data) {
				return;
			}
			constexpr float kUnits = 1.0f / 1.42875f;  // centimetres -> Skyrim units
			const float     base = g_stats.Move("base_speed") * kUnits;
			const float     back = base * g_stats.Move("backpedal_factor");
			const float     sprint = base * g_stats.Move("sprint_factor");
			// MORDHAU's data has no separate walk speed; its walk clips do: the speed they are authored for, as measured
			// by tools/bake_locomotion.py (line "lower;WalkFwd;..." of the cache's locomotion.txt)
			float walk = 0.0f;
			{
				std::ifstream in(g_dev.cache / "locomotion.txt");
				std::string   line;
				while (std::getline(in, line)) {
					if (line.rfind("lower;WalkFwd;", 0) == 0) {
						std::size_t at = 0;
						for (int field = 0; field < 4 && at != std::string::npos; ++field) {
							at = line.find(';', at) + 1;
						}
						walk = std::strtof(line.c_str() + at, nullptr) * kUnits;
					}
				}
			}
			using D = RE::Movement::SPEED_DIRECTIONS;
			constexpr int kRun = RE::Movement::MaxSpeeds::kRun;
			constexpr int kWalk = RE::Movement::MaxSpeeds::kWalk;
			std::string   changed, left;
			for (auto* type : data->GetFormArray<RE::BGSMovementType>()) {
				if (!type) {
					continue;
				}
				const std::string_view name = type->movementTypeData.typeName.c_str();
				auto&                  speeds = type->movementTypeData.defaultData.speeds;
				// humanoid stances on foot (names as the game has them: NPCDefault, NPC1HM, NPCSprinting ...); not sneaking,
				// swimming, riding, bleeding out or drunk
				const bool humanoid = (name.starts_with("NPC") || name == "AIControlledNPCSprinting") && name.find("Sneak") == name.npos &&
				                      name.find("Swim") == name.npos && name.find("Horse") == name.npos && name.find("Bleedout") == name.npos && name.find("Drunk") == name.npos;
				if (!humanoid) {
					left += std::string(name) + " ";
					continue;
				}
				const float was = speeds[D::kForward][kRun];
				if (name.find("Sprint") != name.npos) {
					speeds[D::kForward][kRun] = sprint;
				} else {
					speeds[D::kForward][kRun] = base;
					speeds[D::kLeft][kRun] = base;
					speeds[D::kRight][kRun] = base;
					speeds[D::kBack][kRun] = back;
					if (walk > 1.0f) {
						speeds[D::kForward][kWalk] = walk;
						speeds[D::kLeft][kWalk] = walk;
						speeds[D::kRight][kWalk] = walk;
						speeds[D::kBack][kWalk] = walk;
					}
				}
				changed += std::format("{}({:.0f}->{:.0f}) ", name, was, speeds[D::kForward][kRun]);
			}
			logger::info("movement: speeds set from MORDHAU: walk {:.0f}, run {:.0f}, back {:.0f}, sprint {:.0f} units/s", walk, base, back, sprint);
			logger::info("movement: changed: {}", changed);
			logger::info("movement: left alone: {}", left);

			// jump: MORDHAU's height (v^2 / 2g) through Skyrim's jump-height setting
			const float v = g_stats.Move("jump_velocity"), g = 980.0f * g_stats.Move("gravity_scale");
			if (auto* setting = RE::GameSettingCollection::GetSingleton()->GetSetting("fJumpHeightMin"); setting && v > 0.0f && g > 0.0f) {
				const float before = setting->GetFloat();
				setting->data.f = v * v / (2.0f * g) * kUnits;
				logger::info("movement: jump height {:.0f} -> {:.0f} units", before, setting->GetFloat());
			}
		}

		void ApplyMovementRules(float a_measuredSpeed, float a_direction)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* controls = RE::PlayerControls::GetSingleton();
			if (!g_dev.movement || !player || !controls || !player->Is3DLoaded()) {
				return;
			}
			const float dt = std::clamp(Seconds(g_move.last), 0.0f, 0.1f);
			g_move.last = Clock::now();
			auto* values = player->AsActorValueOwner();

			// MORDHAU's stamina cost for a jump, on take-off
			const bool airborne = player->IsInMidair();
			if (airborne && !g_move.wasAirborne) {
				const float cost = g_stats.Move("jump_stamina");
				if (cost > 0.0f) {
					values->DamageActorValue(RE::ActorValue::kStamina, cost);
				}
			}
			g_move.wasAirborne = airborne;

			const RE::NiPoint2 input = controls->data.moveInputVec;  // x right, y forward
			const bool         wants = std::abs(input.x) + std::abs(input.y) > 0.05f;
			const float        intent = wants ? std::atan2(input.x, input.y) * 57.29578f : 0.0f;
			const bool         sprinting = wants && player->AsActorState()->IsSprinting();
			// (over-encumbered: Skyrim makes the player walk - its pace, not measured and not corrected: a save loaded
			// over-encumbered measured that walk as the run and made the player 2.2 times too fast)
			const bool         heavy = player->IsOverEncumbered();
			g_move.sprintHeld = sprinting ? g_move.sprintHeld + dt : 0.0f;

			// the speed MORDHAU's rules allow right now, forward (the table already makes backward 0.8 of this):
			// base speed, or while sprinting a build-up from base to base x sprint factor
			const float base = g_stats.Move("base_speed");
			float       desired = base;
			if (sprinting) {
				const float buildup = std::clamp(g_move.sprintHeld / std::max(g_stats.Move("sprint_buildup"), 0.01f), 0.0f, 1.0f);
				desired = base * (1.0f + (std::max(g_stats.Move("sprint_factor"), 1.0f) - 1.0f) * buildup);
			}
			// side-by-side: MORDHAU's own character sets the pace (its weapon's weight, sprint build-up, slowing in an attack)
			if (const auto link = Link::Current(); link.live && wants) {
				float back = std::abs(intent) > g_stats.Move("backpedal_angle") ? g_stats.Move("backpedal_factor") : 1.0f;
				if (back <= 0.0f) {
					back = 1.0f;
				}
				desired = std::max(std::hypot(link.speedRight, link.speedForward), 40.0f) / back;
			}
			const int state = sprinting ? 1 : 0;

			// calibration: after half a second of steady movement straight ahead in this state, take what Skyrim delivers
			const bool settled = wants && !airborne && !heavy && std::abs(intent) < 20.0f && (!sprinting || g_move.sprintHeld > g_stats.Move("sprint_buildup") + 0.3f);
			g_move.steady = settled && sprinting == g_move.wasSprinting ? g_move.steady + dt : 0.0f;
			g_move.wasSprinting = sprinting;
			// (only what can be a normal run or sprint counts: Skyrim moves the player at the speeds this mod gave it from MORDHAU,
			// so at SpeedMult 100 it delivers about what MORDHAU allows - half or less was walk mode, a slope, a crowd or a
			// load; taken as the gain, it made the player 2.2 times too fast for the whole session: "i am going super fast")
			const float sample = a_measuredSpeed / std::max(g_move.mult / 100.0f, 0.2f);
			const bool  plausible = sample > 0.6f * desired && sample < 1.6f * desired;
			if (!g_move.calibrated[state] && g_move.steady > 0.5f && a_measuredSpeed > 60.0f && plausible) {
				g_move.gainSum[state] += sample;
				if (++g_move.gainSamples[state] >= 20) {
					g_move.gain[state] = g_move.gainSum[state] / static_cast<float>(g_move.gainSamples[state]);
					g_move.calibrated[state] = true;
					logger::info("movement: Skyrim delivers {:.0f} cm/s at SpeedMult 100 when {}; corrected to MORDHAU's {:.0f}", g_move.gain[state],
						sprinting ? "sprinting" : "running", desired);
				}
			}
			// once measured, still watched: steady movement that stays far from what MORDHAU allows (beyond a third either way)
			// for two seconds, not held at a limit, measures this state again
			{
				static float off[2]{};
				const bool   atLimit = g_move.mult <= 20.5f || g_move.mult >= 249.5f;
				const bool   wrong = g_move.calibrated[state] && g_move.steady > 0.5f && !atLimit && !heavy && !player->AsActorState()->IsSwimming() && a_measuredSpeed > 30.0f &&
				                   (a_measuredSpeed > 1.35f * desired || a_measuredSpeed < 0.65f * desired);
				off[state] = wrong ? off[state] + dt : 0.0f;
				if (off[state] > 2.0f) {
					off[state] = 0.0f;
					logger::info("movement: {} at {:.0f} cm/s where MORDHAU allows {:.0f} (SpeedMult {:.0f}): measured again", sprinting ? "sprinting" : "running",
						a_measuredSpeed, desired, g_move.mult);
					g_move.calibrated[state] = false;
					g_move.gainSum[state] = 0.0f;
					g_move.gainSamples[state] = 0;
				}
			}
			// while a state is being measured its SpeedMult is held still
			const bool  measuring = !g_move.calibrated[state] && g_move.steady > 0.0f;
			// (in the water Skyrim sets the pace: MORDHAU's character stands on its own dry ground and reports walking
			// speeds, which made swimming crawl)
			const bool  swimming = player->AsActorState()->IsSwimming();
			// (nor during any other Skyrim scene - bound, at the start of the game, the player crept along)
			const float wanted = (!wants || swimming || heavy || g_skyrimOwns.load()) ? 100.0f : std::clamp(100.0f * desired / std::max(g_move.gain[state], 1.0f), 20.0f, 250.0f);
			if (!measuring && std::abs(wanted - g_move.mult) > 0.5f) {
				g_move.mult = wanted;
				values->SetActorValue(RE::ActorValue::kSpeedMult, wanted);
				// Skyrim only re-reads SpeedMult when carry weight changes: nudge it up and straight back
				values->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kCarryWeight, 0.01f);
				values->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kCarryWeight, -0.01f);
			}
			if (g_dev.autotest && wants && Seconds(g_move.lastLog) > 0.5f) {
				g_move.lastLog = Clock::now();
				logger::info("movement: intent {:.0f} deg{}, MORDHAU allows {:.0f}, measured {:.0f} cm/s at {:.0f} deg, SpeedMult {:.0f}", intent,
					sprinting ? std::format(" sprint {:.2f}s", g_move.sprintHeld) : "", desired * (std::abs(intent) > g_stats.Move("backpedal_angle") ? g_stats.Move("backpedal_factor") : 1.0f),
					a_measuredSpeed, a_direction, g_move.mult);
			}
		}

		// MORDHAU character space (cm, facing +Y, its right is -X) -> Skyrim world, standing on the actor.
		RE::NiPoint3 LiveToWorld(RE::NiAVObject* a_root, const float* a_cm)
		{
			constexpr float k = 1.0f / 1.42875f;
			return a_root->world.rotate * RE::NiPoint3{ -a_cm[0] * k, a_cm[1] * k, a_cm[2] * k } + a_root->world.translate;
		}

		// The player's health and what holds it down, written to the log now and then.
		void LogPlayerHealth(RE::PlayerCharacter* a_player)
		{
		{
			// The player's health in full, now and then (seen in play: health stayed low, did not come back,
			// and magic did no damage): what it is, what holds it down, and what regeneration Skyrim has for it.
			static Clock::time_point told{};
			if (Seconds(told) > 20.0f) {
				told = Clock::now();
				auto* av = a_player->AsActorValueOwner();
				logger::info("health: {:.1f} now, base {:.0f}, permanent {:.0f}; modifiers permanent {:.1f} temporary {:.1f} damage {:.1f}; heal rate {:.2f} x mult {:.0f}%, in combat x{:.2f}; magic resistance {:.0f}%; in combat {}, god mode {}, ghost {}, essential {}",
					av->GetActorValue(RE::ActorValue::kHealth), av->GetBaseActorValue(RE::ActorValue::kHealth), av->GetPermanentActorValue(RE::ActorValue::kHealth),
					a_player->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kHealth), a_player->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth),
					a_player->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth), av->GetActorValue(RE::ActorValue::kHealRate), av->GetActorValue(RE::ActorValue::kHealRateMult),
					av->GetActorValue(RE::ActorValue::kCombatHealthRegenMultiply), av->GetActorValue(RE::ActorValue::kResistMagic), a_player->IsInCombat(), RE::PlayerCharacter::IsGodMode(),
					a_player->IsGhost(), a_player->IsEssential());
			}
		}
		}

		void SendPlayerState(RE::PlayerCharacter* a_player, float& forward, float& right, const Link::State& link)
		{
		{
			// a Skyrim scene keeps the body for as long as it lasts and half a second more (no flicker at its edges)
			static Clock::time_point lastScene = Clock::now() - std::chrono::seconds(10);
			static const char*       shown = nullptr;
			const char*              scene = BuildValue("skyrim_scenes") > 0.5f ? SkyrimScene(a_player) : nullptr;
			if (scene) {
				lastScene = Clock::now();
			}
			DevArrowTest(a_player, link);
			DevDemoSceneSetup(a_player);
			DevDemoScene(a_player, link);
			DevSceneTest(a_player);
			const bool owns = scene || Seconds(lastScene) < 0.5f;
			if (owns != g_skyrimOwns.load() || (scene && scene != shown)) {
				logger::info("scene: {}", owns ? std::format("Skyrim has the player's body ({})", scene ? scene : "ending") : std::string("over, MORDHAU has the body again"));
				shown = scene;
			}
			g_skyrimOwns.store(owns);
			CharacterRender::SetSkyrimOwnsPlayer(owns);
			if (owns) {
				forward = right = 0.0f;  // (MORDHAU's character stands still meanwhile)
			}
		}
		}

		// The player's Skyrim shield as its MORDHAU counterpart.
		void SendShield(RE::PlayerCharacter* a_player, const Link::State& link)
		{
		{
			// which MORDHAU shield the Skyrim one is (sheets/shield_map.json: by its material)
			const char* mordhauShield = "";
			if (link.live && LiveMelee(a_player) && PlayerShield(a_player) && BuildValue("mordhau_shield") > 0.5f) {
				const auto* worn = a_player->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kShield);
				std::string name = worn && worn->GetName() ? worn->GetName() : "";
				std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				const bool light = worn && worn->IsLightArmor();
				static const char* lastShield = "";
				if (name.empty()) {
					mordhauShield = lastShield;  // (between two shields Skyrim has none worn for a moment: keep the last)
				}
				for (const auto& row : sheets::kShieldMap) {
					const std::string_view kind = row.armour;
					if (kind == "light" && !light) {
						continue;
					}
					std::string words = row.contains;
					std::transform(words.begin(), words.end(), words.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
					bool match = words.empty();
					for (std::size_t from = 0; !match && from < words.size();) {
						const auto bar = words.find('|', from);
						const auto word = words.substr(from, bar == std::string::npos ? std::string::npos : bar - from);
						match = !word.empty() && name.find(word) != std::string::npos;
						from = bar == std::string::npos ? words.size() : bar + 1;
					}
					if (match && !name.empty()) {
						mordhauShield = row.mordhauShield;
						lastShield = row.mordhauShield;
						break;
					}
				}
				static std::string told;
				if (told != name + mordhauShield) {
					told = name + mordhauShield;
					logger::info("shield: '{}' is MORDHAU's {}", worn && worn->GetName() ? worn->GetName() : "?", mordhauShield);
				}
			}
			Link::SetShield(mordhauShield);
		}
		}

		// Skyrim's own pick-up prompt for a MORDHAU weapon on the ground within reach.
		void ShowPickUpPrompt(RE::PlayerCharacter* a_player, const Link::State& link)
		{
		{
			// The pick-up prompt for a weapon on the ground within reach: Skyrim's own "E Take <name>, weight, value"
			// (the HUD's SetCrosshairTarget), since picking up here goes through MORDHAU and not Skyrim's activation.
			static Clock::time_point lastPrompt{};
			static bool              prompting = false;
			if (Seconds(lastPrompt) > 0.15f) {
				lastPrompt = Clock::now();
				std::uint32_t owner = 0;
				std::string   model;
				const bool    inReach = link.live && CharacterRender::WeaponOnGroundNear(a_player->GetPosition(), BuildValue("pick_up_reach"), owner, model, false);
				auto*         ui = RE::UI::GetSingleton();
				auto          hud = ui ? ui->GetMenu(RE::HUDMenu::MENU_NAME) : nullptr;
				if (hud && (inReach || prompting)) {
					std::string name;
					float       weight = 0.0f;
					int         value = 0;
					if (inReach) {
						auto* own = owner == 0 ? a_player->GetEquippedObject(false) : nullptr;
						auto* weapon = own ? own->As<RE::TESObjectWEAP>() : GroundWeaponItem(owner, model);
						if (weapon) {
							name = weapon->GetName();
							weight = weapon->weight;
							value = weapon->value;
						} else if (model.size() > 2) {
							name = model.substr(2);
						}
					}
					const std::string text = inReach ? "Take" + std::string(1, char(10)) + name : std::string{};
					HudTask([inReach, text, weight, value](RE::GFxMovieView* a_movie) {
						RE::GFxValue args[10];
						args[0].SetBoolean(inReach);
						args[1].SetString(text.c_str());
						args[2].SetBoolean(inReach);    // the E button
						args[3].SetBoolean(false);
						args[4].SetBoolean(false);
						args[5].SetBoolean(false);
						if (inReach) {
							args[6].SetNumber(weight);
							args[7].SetNumber(static_cast<double>(value));
						} else {
							args[6].SetUndefined();  // (cleared: no "weight 0, value 0" line left standing)
							args[7].SetUndefined();
						}
						args[8].SetUndefined();
						args[9].SetUndefined();
						a_movie->Invoke("_root.HUDMovieBaseInstance.SetCrosshairTarget", nullptr, args, 10);
					});
					prompting = inReach;
				}
			}
		}
		}

		// A weapon dropped in MORDHAU lies in Skyrim as the Skyrim weapon itself.
		void PlaceDroppedWeapons(RE::PlayerCharacter* a_player, const Link::State& link)
		{
		{
			// A dropped weapon lies on the ground as the Skyrim weapon itself: once MORDHAU's has come to rest, a real
			// copy of the owner's Skyrim weapon is set down there (it cannot be taken the Skyrim way: picking up is
			// MORDHAU's, see the E key); it goes when the weapon is no longer on MORDHAU's ground.
			static std::unordered_map<std::uint32_t, RE::ObjectRefHandle> laid;
			static std::vector<CharacterRender::DroppedWeapon>           dropped;
			CharacterRender::DroppedWeapons(dropped);
			for (auto it = laid.begin(); it != laid.end();) {
				bool still = false;
				for (const auto& d : dropped) {
					still = still || d.owner == it->first;
				}
				auto ref = it->second.get();
				if (!still || !link.live || !ref) {
					if (ref) {
						ref->Disable();
						ref->SetDelete(true);
					}
					CharacterRender::DroppedAsSkyrim(it->first, false, {});
					it = laid.erase(it);
				} else {
					RE::NiPoint3 spot = ref->GetPosition();
					for (const auto& d : dropped) {
						if (d.owner == it->first && spot.z < d.at.z - 150.0f) {
							// (it went through the ground after all: back above where MORDHAU's lies)
							spot = d.at + RE::NiPoint3{ 0.0f, 0.0f, BuildValue("drop_spawn_height") };
							ref->SetPosition(spot);
						}
					}
					CharacterRender::DroppedAsSkyrim(it->first, true, spot);
					++it;
				}
			}
			if (link.live) {
				for (const auto& d : dropped) {
					if (laid.contains(d.owner)) {
						continue;
					}
					RE::Actor* owner = d.owner == 0 ? a_player : RE::TESForm::LookupByID<RE::Actor>(d.owner);
					auto*      form = owner ? owner->GetEquippedObject(false) : nullptr;
					auto*      weapon = form ? form->As<RE::TESObjectWEAP>() : nullptr;
					if (!weapon || Items::MordhauWeaponOf(weapon) || (d.owner == 0 && std::string_view(link.weaponClass).empty())) {
						continue;
					}
					// (the player's: only when what was dropped stands for the equipped Skyrim weapon)
					if (d.owner == 0) {
						const char*            wish = MordhauWeaponFor(a_player);
						const std::string_view path = wish ? wish : "";
						const auto             name = path.substr(path.find_last_of('/') == std::string_view::npos ? 0 : path.find_last_of('/') + 1);
						if (std::string(name) + "_C" != link.weaponClass) {
							continue;
						}
					}
					if (auto ref = owner->PlaceObjectAtMe(weapon, false)) {
						ref->SetPosition(d.at + RE::NiPoint3{ 0.0f, 0.0f, BuildValue("drop_spawn_height") });
						ref->SetActivationBlocked(true);
						laid[d.owner] = ref->GetHandle();
						CharacterRender::DroppedAsSkyrim(d.owner, true, d.at);
						logger::info("drop: {}'s {} lies on the ground as Skyrim's own object", owner->GetDisplayFullName(), weapon->GetName());
					}
				}
			}
		}
		}

		// With an ENB, what this plugin draws is handed over darker to match.
		void MatchEnbBrightness(RE::PlayerCharacter* a_player)
		{
		{
			// an ENB brightens the finished picture itself: what this plugin draws is handed over darker to match
			static const bool enb = std::filesystem::exists("d3d11.dll") && std::filesystem::exists("enbseries.ini");
			CharacterRender::SetOutputScale(enb ? BuildValue("enb_light_scale") : 0.0f);
			// (not once the player is dead: MORDHAU's bots went on fighting its character, and the Skyrim bodies were slid about after them)
			{
				// (a menu that pauses Skyrim - the inventory - pauses the fight in MORDHAU too: before, the fight carried on behind the inventory)
				auto* pauseUi = RE::UI::GetSingleton();
				Link::SetPaused(pauseUi && pauseUi->GameIsPaused());
			}
			Link::SetHybrid(BuildValue("hybrid_movement") > 0.5f && !g_dev.hybridOff && !a_player->IsDead() && !(RE::UI::GetSingleton() && RE::UI::GetSingleton()->GameIsPaused()), BuildValue("hybrid_engage_cm"), BuildValue("hybrid_snap_cm"), BuildValue("hybrid_hold_seconds"));
			CharacterRender::SetThirdWeaponMordhau(BuildValue("third_person_weapon_mordhau") > 0.5f);
			CharacterRender::SetStandinWeaponMordhau(BuildValue("standin_weapon_as_mordhau") > 0.5f);
			CharacterRender::SetSkyrimShieldLook(BuildValue("skyrim_shield_look") > 0.5f);
			CharacterRender::SetMordhauBars(BuildValue("mordhau_bars") > 0.5f);
			InventoryModels::SetEnabled(BuildValue("inventory_mordhau_models") > 0.5f);
			CharacterRender::SetArms(BuildValue("arms_to_mordhau_hands") > 0.5f, BuildValue("arms_stretch_max"), BuildValue("skyrim_weapon_grip_in_fist") > 0.5f, BuildValue("skyrim_weapon_half_turn") > 0.5f, BuildValue("alt_grip_hands_on_blade") > 0.5f);
			CharacterRender::SetPauldronsFollow(BuildValue("pauldrons_follow") > 0.5f && !g_dev.pauldronsOff);
		}
		}

		// Skyrim's walls at chest height ahead and to both sides, told to MORDHAU.
		void SendWalls(RE::PlayerCharacter* a_player, const Link::State& link)
		{
		{
			// Skyrim's walls for MORDHAU: what is solid at chest height ahead and to both sides (sheets/live_link.json walls)
			int   walls[3]{};
			auto* cell = a_player->GetParentCell();
			auto* world = cell ? cell->GetbhkWorld() : nullptr;
			if (world && link.live && BuildValue("wall_reach") > 0.0f && a_player->AsActorState()->IsWeaponDrawn()) {
				const float        scale = RE::bhkWorld::GetWorldScale(), reach = BuildValue("wall_reach");
				const float        facing = a_player->GetAngleZ();
				const RE::NiPoint3 chest = a_player->GetPosition() + RE::NiPoint3{ 0.0f, 0.0f, a_player->GetHeight() * 0.72f };
				for (int k = 0; k < 3; ++k) {
					const float        turn = facing + (static_cast<float>(k) - 1.0f) * 0.785398f;
					const RE::NiPoint3 to = chest + RE::NiPoint3{ std::sin(turn), std::cos(turn), 0.0f } * reach;
					RE::bhkPickData    pick;
					pick.rayInput.from = RE::hkVector4(chest.x * scale, chest.y * scale, chest.z * scale, 0.0f);
					pick.rayInput.to = RE::hkVector4(to.x * scale, to.y * scale, to.z * scale, 0.0f);
					RE::CFilter filter;
					a_player->GetCollisionFilterInfo(filter);
					filter.SetCollisionLayer(RE::COL_LAYER::kLOS);
					pick.rayInput.filterInfo = filter;
					{
						RE::BSReadLockGuard lock(world->worldLock);
						world->PickObject(pick);
					}
					if (!pick.rayOutput.HasHit() || !pick.rayOutput.rootCollidable) {
						continue;
					}
					const auto layer = pick.rayOutput.rootCollidable->GetCollisionLayer();
					bool       solid = true;
					for (const auto soft : { RE::COL_LAYER::kBiped, RE::COL_LAYER::kCharController, RE::COL_LAYER::kDeadBip, RE::COL_LAYER::kBipedNoCC, RE::COL_LAYER::kProjectile,
							 RE::COL_LAYER::kSpell, RE::COL_LAYER::kTrigger, RE::COL_LAYER::kNonCollidable, RE::COL_LAYER::kWeapon, RE::COL_LAYER::kClutter, RE::COL_LAYER::kActorZone }) {
						solid = solid && layer != soft;
					}
					if (solid) {
						walls[k] = static_cast<int>(reach * pick.rayOutput.hitFraction * 1.42875f);
					}
				}
			}
			Link::SetWalls(walls[0], walls[1], walls[2]);
		}
		}

		// The MORDHAU counterpart of the Skyrim weapon in the player's hand, told to MORDHAU.
		void SendWeaponWish(RE::PlayerCharacter* a_player, const Link::State& link)
		{
		{
			// the MORDHAU counterpart of the Skyrim weapon in the player's hand (not for bare hands or a bow)
			const auto* form = a_player->GetEquippedObject(false);
			const auto* held = form ? form->As<RE::TESObjectWEAP>() : nullptr;
			const char* wish = held && LiveMelee(a_player) ? MordhauWeaponFor(a_player) : nullptr;
			{
				// (sheets/builds.json alt_slide_<weapon>_<view>: how far MORDHAU's other grip moves this weapon along itself)
				float slide = 0.0f;
				if (wish) {
					std::string_view leaf(wish);
					if (const auto at = leaf.rfind("BP_"); at != std::string_view::npos) {
						leaf = leaf.substr(at + 3);
					}
					slide = BuildValue(std::format("alt_slide_{}_{}", leaf, g_eyeView.load() ? "1p" : "3p").c_str());
				}
				CharacterRender::SetAltSlide(slide);
			}
			// (nothing in the right hand in Skyrim = fists: MORDHAU's character puts its weapon away and raises its fists.
			// Before, an empty hand sent no wish at all and MORDHAU kept the last weapon - play-test note)
			const bool bareHands = !form && LiveMelee(a_player);
			Link::SetPlayerWeapon(bareHands ? "Fists" : (wish && std::string_view(wish).find("Fists") == std::string_view::npos ? wish : ""));
			// the Skyrim weapon itself is shown while MORDHAU's character holds its counterpart (sheets/builds.json skyrim_weapon_look)
			bool counterpart = false;
			if (wish && link.live && BuildValue("skyrim_weapon_look") > 0.5f) {
				const std::string_view path = wish;
				const auto             name = path.substr(path.find_last_of('/') == std::string_view::npos ? 0 : path.find_last_of('/') + 1);
				counterpart = std::string(name) + "_C" == link.weaponClass;
			}
			// (a MORDHAU weapon held as a Skyrim item is drawn as MORDHAU's model: its Skyrim model is only a stand-in)
			CharacterRender::SetPlayerSkyrimWeapon(counterpart && !Items::MordhauWeaponOf(form));
			{
				std::unordered_map<std::uint32_t, std::string> mordhauHeld;
				const auto modelOf = [](const char* a_path) {
					const std::string_view path = a_path;
					const auto             name = path.substr(path.find_last_of('/') == std::string_view::npos ? 0 : path.find_last_of('/') + 1);
					return name.rfind("BP_", 0) == 0 ? "w_" + std::string(name.substr(3)) : std::string{};
				};
				if (const char* own = Items::MordhauWeaponOf(form)) {
					mordhauHeld[0] = modelOf(own);
				}
				for (const auto& ghost : link.ghosts) {
					auto* who = ghost.id ? RE::TESForm::LookupByID<RE::Actor>(ghost.id) : nullptr;
					if (const char* theirs = who ? Items::MordhauWeaponOf(who->GetEquippedObject(false)) : nullptr) {
						mordhauHeld[ghost.id] = modelOf(theirs);
					} else if (const auto* fighter = who ? FighterFor(who) : nullptr; fighter && fighter->weapon[0] && std::string_view(fighter->weapon) != "own") {
						mordhauHeld[ghost.id] = modelOf(fighter->weapon);  // (sheets/creature_fighters.json weapon: drawn in place of its own)
					}
				}
				CharacterRender::SetMordhauHeld(mordhauHeld);
			}
		}
		}

		// Skyrim's own HUD while MORDHAU is live: its crosshair away, its stealth eye out of the middle.
		void UpdateSkyrimHud(RE::PlayerCharacter* a_player, const Link::State& link)
		{
		{
			static Clock::time_point lastHud{};
			static int               hidden = -1;
			const int                want = link.live && a_player->AsActorState()->IsWeaponDrawn() ? 1 : 0;
			if (want != hidden || Seconds(lastHud) > 1.0f) {
				lastHud = Clock::now();
				hidden = want;
				const bool   live = link.live;
				// (sheets/ui.json vanilla_hud: Skyrim's meters go only when MORDHAU's own bars are drawn in their place)
				const bool   ownBars = BuildValue("mordhau_bars") > 0.5f;
				const double eyeLeft = BuildValue("stealth_eye_left"), eyeDown = BuildValue("stealth_eye_down");
				HudTask([want, live, ownBars, eyeLeft, eyeDown](RE::GFxMovieView* a_movie) {
					const RE::GFxValue visible(want == 0);
					const RE::GFxValue meters(want == 0 || !ownBars);
					a_movie->SetVariable("_root.HUDMovieBaseInstance.Health._visible", meters);
					a_movie->SetVariable("_root.HUDMovieBaseInstance.Stamina._visible", meters);
					a_movie->SetVariable("_root.HUDMovieBaseInstance.Crosshair._visible", visible);
					// Skyrim's stealth eye: out of the middle of the screen (MORDHAU's crosshair and attack indicator are
					// there), to the lower left, while MORDHAU is linked
					static double homeX = 0.0, homeY = 0.0, setX = -1e9, setY = -1e9;
					const char*   pathX = "_root.HUDMovieBaseInstance.StealthMeterInstance._x";
					const char*   pathY = "_root.HUDMovieBaseInstance.StealthMeterInstance._y";
					RE::GFxValue  x, y;
					if (a_movie->GetVariable(&x, pathX) && a_movie->GetVariable(&y, pathY) && x.IsNumber() && y.IsNumber()) {
						const bool ours = std::abs(x.GetNumber() - setX) < 0.5 && std::abs(y.GetNumber() - setY) < 0.5;
						if (!ours) {
							homeX = x.GetNumber(), homeY = y.GetNumber();  // (a fresh HUD, or Skyrim put it back)
						}
						const double wantX = live ? homeX - eyeLeft : homeX;
						const double wantY = live ? homeY + eyeDown : homeY;
						if (std::abs(x.GetNumber() - wantX) > 0.5 || std::abs(y.GetNumber() - wantY) > 0.5) {
							a_movie->SetVariable(pathX, RE::GFxValue(wantX));
							a_movie->SetVariable(pathY, RE::GFxValue(wantY));
							setX = wantX, setY = wantY;
						}
					}
				});
			}
		}
		}

		// Skyrim's fighting distance while linked: MORDHAU's reach.
		void SetFightingDistance(const Link::State& link)
		{
		{
			// Skyrim's fighting distance while linked: MORDHAU's reach (enemies stopped and swung too far away)
			static float skyrimsOwn = -1.0f;
			if (auto* settings = RE::GameSettingCollection::GetSingleton()) {
				if (auto* distance = settings->GetSetting("fCombatDistance")) {
					if (link.live && skyrimsOwn < 0.0f) {
						skyrimsOwn = distance->data.f;
						distance->data.f = BuildValue("skyrim_combat_distance");
						logger::info("combat distance: Skyrim's {:.0f} -> {:.0f} units while MORDHAU is linked", skyrimsOwn, distance->data.f);
					} else if (!link.live && skyrimsOwn >= 0.0f) {
						distance->data.f = skyrimsOwn;
						skyrimsOwn = -1.0f;
					}
				}
			}
		}
		}

		// MORDHAU's own draw animation as Skyrim begins to draw or put away.
		void PlayMordhauDrawClip(RE::PlayerCharacter* a_player, const Link::State& link)
		{
		{
			// MORDHAU's own draw animation as Skyrim begins to draw or put away (sheets/builds.json mordhau_draw_clips)
			static auto lastState = RE::WEAPON_STATE::kSheathed;
			const auto  weaponState = a_player->AsActorState()->GetWeaponState();
			if (weaponState != lastState) {
				const auto* right = a_player->GetEquippedObject(false);
				const auto* weapon = right ? right->As<RE::TESObjectWEAP>() : nullptr;
				const int   kind = !weapon || weapon->IsBow() || weapon->IsCrossbow() || weapon->IsStaff() ? 0 : (weapon->IsTwoHandedSword() || weapon->IsTwoHandedAxe()) ? 2 : 1;
				const bool  out = (weaponState == RE::WEAPON_STATE::kWantToDraw || weaponState == RE::WEAPON_STATE::kDrawing) && lastState == RE::WEAPON_STATE::kSheathed;
				const bool  away = (weaponState == RE::WEAPON_STATE::kWantToSheathe || weaponState == RE::WEAPON_STATE::kSheathing) && lastState == RE::WEAPON_STATE::kDrawn;
				if (kind != 0 && (out || away) && link.live && BuildValue("mordhau_draw_clips") > 0.5f) {
					CharacterRender::PlayDraw(out ? kind : -kind);
					logger::info("draw: MORDHAU's {} animation {} ({})", kind == 2 ? "from-the-back" : "from-the-hip", out ? "played" : "played backwards", weapon->GetName());
				}
				lastState = weaponState;
			}
		}
		}

		// Weapon out or put away: MORDHAU's character does the same.
		void SyncWeaponDrawn(RE::PlayerCharacter* a_player, const Link::State& link)
		{
		if (link.live) {
			static Clock::time_point lastAsk{};
			// Skyrim's own draw / sheathe sound as Skyrim begins one (its animation's own was not heard with MORDHAU linked)
			{
				static auto was = RE::WEAPON_STATE::kSheathed;
				const auto  now = a_player->AsActorState()->GetWeaponState();
				if (now != was && (now == RE::WEAPON_STATE::kDrawing || now == RE::WEAPON_STATE::kSheathing)) {
					SoundMix::PlaySkyrimDraw(a_player, now == RE::WEAPON_STATE::kDrawing);
				}
				was = now;
			}
			// (Skyrim drawing or putting away: MORDHAU's character is asked at once, so its own draw or holster plays while
			// Skyrim's animation is the one shown. Asked afterwards, it played on its own once Skyrim's was over - seen in
			// play: "the weapon disappears, then hands go down, hands up, hands down".)
			// (bare fists - or a spell - in Skyrim's right hand: MORDHAU's character has no weapon to bring out. Asked anyway,
			// every 2.5 s, its slot-1 switch ticked in MORDHAU all the time - "a menu ticking sound is coming from mordhau")
			{
				const auto* right = a_player->GetEquippedObject(false);
				const auto* weapon = right ? right->As<RE::TESObjectWEAP>() : nullptr;
				if (!weapon || weapon->IsHandToHandMelee()) {
					return;
				}
			}
			const auto weaponState = a_player->AsActorState()->GetWeaponState();
			const bool skyrimDraws = weaponState == RE::WEAPON_STATE::kWantToDraw || weaponState == RE::WEAPON_STATE::kDrawing;
			const bool skyrimSheathes = weaponState == RE::WEAPON_STATE::kWantToSheathe || weaponState == RE::WEAPON_STATE::kSheathing;
			if ((skyrimDraws || skyrimSheathes) && skyrimDraws != link.hasWeapon && link.weaponState != 2 && !RangedWish(a_player)) {
				static bool askedFor = false;
				static Clock::time_point askedAt{};
				if (askedFor != skyrimDraws || Seconds(askedAt) > 2.5f) {
					askedFor = skyrimDraws;
					askedAt = lastAsk = Clock::now();
					Link::Request(skyrimDraws ? "Draw" : "Sheathe");
					logger::info("link: asking MORDHAU for {} as Skyrim begins its own", skyrimDraws ? "Draw" : "Sheathe");
				}
				return;
			}
			const bool               drawn = a_player->AsActorState()->IsWeaponDrawn();
			// (not with a bow out: MORDHAU holds its bow in the other hand, and the script brings it out by itself)
			// (nor while a trap is being set: the trap is in MORDHAU's hand for a few seconds)
			// (nor while the weapon lies on the ground, dropped or disarmed: it has to be picked up, as in MORDHAU)
			static Clock::time_point differsSince{};
			static bool              differs = false;
			const bool               differsNow = drawn != link.hasWeapon && link.weaponState != 2;
			if (differsNow && !differs) {
				differsSince = Clock::now();
			}
			differs = differsNow;
			// (only a difference that lasts: MORDHAU lets go of the weapon for a moment when its mode is switched,
			// and an instant "Draw" then played a holster-and-draw on every R)
			if (differsNow && Seconds(differsSince) > 1.2f && !RangedWish(a_player) && Seconds(lastAsk) > 2.5f) {
				lastAsk = Clock::now();
				Link::Request(drawn ? "Draw" : "Sheathe");
			}
		}
		}

		// True Directional Movement (sheets/builds.json tdm_off_while_linked): while MORDHAU has the player, TDM's own
		// directional movement and its turning of the player are switched off through its own interface, and given back
		// afterwards. (TDM turned the character towards where the player pressed while this mod kept it facing the view:
		// with the weapon put away only walking forward was left - reported by players who run TDM.)
		void KeepTdmOut(bool a_linked)
		{
			static TDM_API::IVTDM2* tdm = nullptr;
			static TDM_API::IVTDM5* tdmLock = nullptr;  // (its target lock: also on the middle mouse button, the cast key here - sheets/live_input.json cast_left)
			static bool             asked = false, holding = false;
			if (!asked) {
				asked = true;
				tdm = static_cast<TDM_API::IVTDM2*>(TDM_API::RequestPluginAPI(TDM_API::InterfaceVersion::V2));
				tdmLock = tdm ? static_cast<TDM_API::IVTDM5*>(TDM_API::RequestPluginAPI(TDM_API::InterfaceVersion::V5)) : nullptr;
				logger::info("tdm: True Directional Movement {}{}", tdm ? "found: its directional movement is switched off while MORDHAU is linked" : "not loaded",
					tdm ? (tdmLock ? "; its target lock too" : "; its target lock can NOT be switched off through this version's interface") : "");
			}
			const bool want = tdm && a_linked && BuildValue("tdm_off_while_linked") > 0.5f;
			if (!tdm || want == holding) {
				return;
			}
			const auto handle = SKSE::GetPluginHandle();
			if (want) {
				const auto moved = tdm->RequestDisableDirectionalMovement(handle);
				const auto yaw = tdm->RequestYawControl(handle, 0.0f);
				const auto lock = tdmLock ? tdmLock->RequestDisableTargetLock(handle) : TDM_API::APIResult::NotOwner;
				holding = moved == TDM_API::APIResult::OK || moved == TDM_API::APIResult::AlreadyGiven;
				logger::info("tdm: asked to switch its directional movement off ({}), to leave the player's turning alone ({}) and to keep its target lock off ({})", static_cast<int>(moved), static_cast<int>(yaw),
					tdmLock ? static_cast<int>(lock) : -1);
			} else {
				tdm->ReleaseDisableDirectionalMovement(handle);
				tdm->ReleaseYawControl(handle);
				if (tdmLock) {
					tdmLock->ReleaseDisableTargetLock(handle);
				}
				holding = false;
				logger::info("tdm: its directional movement is given back");
			}
		}

		// The settings menu's choices MORDHAU's script carries out - the voice (page "Character voice", "Your yells and
		// grunts", "Enemies' MORDHAU voices") as one request VoiceSet<n>, and "MORDHAU volume" as Volume<percent>
		// (main.lua numbered_action): when MORDHAU becomes live and whenever the menu changed them. (The pitch as a step 0..255 over 0.75 - 1.15: every voice's own limits lie within.)
		void SendVoiceSettings(bool a_live)
		{
			static int  sent = -1;
			static bool wasLive = false;
			const int   generation = Settings::Generation();
			if (a_live && (!wasLive || generation != sent)) {
				sent = generation;
				const int  voice = std::clamp(static_cast<int>(Settings::Value("voice", 4.0f)) + 1, 1, 24);  // (the menu's list from 0; the script's from 1)
				const int  pitch = std::clamp(static_cast<int>(std::lround((Settings::Value("voice_pitch", 1.0f) - 0.75f) / 0.4f * 255.0f)), 0, 255);
				const bool heard = Settings::Value("own_voice_sounds", 1.0f) > 0.5f;
				const bool enemies = BuildValue("standin_voices") > 0.5f;
				const auto request = std::format("VoiceSet{}", voice * 1000000 + pitch * 1000 + (heard ? 10 : 0) + (enemies ? 1 : 0));
				Link::Request(request.c_str());
				// MORDHAU's volume (page "Sound mix": its style or slider), in percent: MORDHAU's master sound in this session only (main.lua PS_apply_volume)
				const auto volume = std::format("Volume{}", static_cast<int>(std::lround(SoundMix::MordhauVolume() * 100.0f)));
				Link::Request(volume.c_str());
			}
			wasLive = a_live;
		}

		void LiveTick(RE::PlayerCharacter* a_player)
		{
			KeepTdmOut(Link::Current().live);
			SendVoiceSettings(Link::Current().live);
			// the settings menu's "Enemy skill at equal level" is the F8 menu's choice: changed in the menu, it is taken (the
			// last one changed, F8 or the menu, counts)
			{
				static float seen = -1.0f;
				const float  chosen = Settings::Value("enemy_skill", -1.0f);
				if (chosen >= 0.0f && seen >= 0.0f && chosen != seen) {
					SetAiLevel(static_cast<int>(chosen) + 1, true);
				}
				seen = chosen;
			}
			// the player's attack was parried: MORDHAU throws the attacker back with its BlockedMotion - Skyrim's block sound, by
			// what the enemy parried with (the nearest enemy in front of the player)
			{
				static bool      wasBlocked = false;
				const bool       blocked = std::string_view(Link::Current().motion).find("BlockedMotion") != std::string_view::npos;
				if (blocked && !wasBlocked) {
					RE::Actor* defender = nullptr;
					float      best = 250.0f;
					RE::ProcessLists::GetSingleton()->ForEachHighActor([&](RE::Actor* a_other) {
						if (a_other && !a_other->IsDead() && a_other->Is3DLoaded()) {
							const float d = a_other->GetPosition().GetDistance(a_player->GetPosition());
							if (d < best) {
								best = d;
								defender = a_other;
							}
						}
						return RE::BSContainer::ForEachResult::kContinue;
					});
					SoundMix::PlaySkyrimBlock(defender ? defender : a_player);
				}
				wasBlocked = blocked;
			}
			float forward = 0.0f, right = 0.0f;
			if (auto* controls = RE::PlayerControls::GetSingleton()) {
				forward = controls->data.moveInputVec.y;
				right = controls->data.moveInputVec.x;
			}
			if (g_dev.autotest) {
				// test scenes only: Skyrim's tutorial boxes ("Health Low" ...) would cover the recordings
				if (auto* ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::TutorialMenu::MENU_NAME)) {
					RE::UIMessageQueue::GetSingleton()->AddMessage(RE::TutorialMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
				}
				// test scenes only: the test character does not defend itself, so it is kept alive
				// (not in the duel scene: there the user plays, and can die)
				auto* av = a_player->AsActorValueOwner();
				if (!g_dev.duel && av->GetActorValue(RE::ActorValue::kHealth) < av->GetPermanentActorValue(RE::ActorValue::kHealth) * 0.85f) {
					av->RestoreActorValue(RE::ActorValue::kHealth, 1000.0f);
				}
			}
			TrackMordhauShots(a_player);
			FlyArrowsAndThrownWeapons(a_player);
			DevBowTest(a_player);
			DevPushTest(a_player);
			if (g_dev.autotest != 0) {
				static std::unordered_set<std::uint32_t> listed;
				static Clock::time_point                 lastList = Clock::now();
				if (Seconds(lastList) > 5.0f) {
					lastList = Clock::now();
					RE::ProcessLists::GetSingleton()->ForEachHighActor([&](RE::Actor* a_actor) {
						if (a_actor && a_actor->Is3DLoaded() && a_actor->HasKeywordString("ActorTypeNPC") && listed.insert(a_actor->GetFormID()).second) {
							logger::info("census: {} level {} full health {:.0f}", a_actor->GetDisplayFullName(), a_actor->GetLevel(),
								a_actor->AsActorValueOwner()->GetPermanentActorValue(RE::ActorValue::kHealth));
						}
						return RE::BSContainer::ForEachResult::kContinue;
					});
				}
			}
			DevGiveItems(a_player);
			DevGiveWeaponTypes(a_player);
			if (g_dev.arsenal) {
				// (test: how many of the MORDHAU items are in the inventory - they must still be there after a reload)
				static Clock::time_point lastCount = Clock::now();
				if (Seconds(lastCount) > 8.0f) {
					lastCount = Clock::now();
					int have = 0;
					for (auto* item : Items::All()) {
						have += a_player->GetItemCount(item) > 0 ? 1 : 0;
					}
					logger::info("arsenal: {} of {} MORDHAU items are in the inventory", have, Items::All().size());
				}
			}
			if (g_dev.arsenal) {
				static bool given = false;
				if (!given && Link::Current().live) {
					given = true;
					std::unordered_set<std::string> names;
					int                             count = 0;
					for (auto* weapon : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESObjectWEAP>()) {
						if (!weapon || !weapon->GetPlayable() || weapon->formEnchanting || weapon->templateWeapon || weapon->IsBound() || !weapon->GetName() || !*weapon->GetName()) {
							continue;
						}
						const auto type = weapon->GetWeaponType();
						if (type == RE::WEAPON_TYPE::kHandToHandMelee || type == RE::WEAPON_TYPE::kBow || type == RE::WEAPON_TYPE::kCrossbow || type == RE::WEAPON_TYPE::kStaff) {
							continue;
						}
						if (!names.insert(weapon->GetName()).second || a_player->GetItemCount(weapon) > 0) {
							continue;
						}
						a_player->AddObjectToContainer(weapon, nullptr, 1, nullptr);
						++count;
					}
					for (auto* item : Items::All()) {
						if (a_player->GetItemCount(item) == 0) {
							a_player->AddObjectToContainer(item, nullptr, 1, nullptr);
							++count;
						}
					}
					a_player->AsActorValueOwner()->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kCarryWeight, 100000.0f);
					logger::info("arsenal: {} melee weapons added to the inventory, carry weight +100000", count);
				}
			}
			DevSkirmishScene(a_player);
			DevCrowdScene(a_player);
			LogPlayerHealth(a_player);
			if (g_dev.god) {
				auto* av = a_player->AsActorValueOwner();
				if (av->GetActorValue(RE::ActorValue::kHealth) < av->GetPermanentActorValue(RE::ActorValue::kHealth)) {
					av->RestoreActorValue(RE::ActorValue::kHealth, 100000.0f);
				}
			}
			DevDuelScene(a_player);
			DevDemonScene(a_player);
			DevBattleScene(a_player);
			DevNpcFight(a_player);
			static bool wasAirborne = false;
			const bool  airborne = a_player->IsInMidair();
			const auto  link = Link::Current();
			if (link.live && airborne && !wasAirborne) {
				Link::Request("Jump");
			}
			wasAirborne = airborne;
			auto*      playerCamera = RE::PlayerCamera::GetSingleton();
			if (CameraValue("first_person_body") > 0.5f && g_dev.character) {
				if (playerCamera && playerCamera->IsInFirstPerson() && link.live && !g_skyrimOwns.load()) {
					// Skyrim went into its own first person (its view key, the wheel, a save made that way): that is the
					// eye view's switch. Its camera goes back to the third-person state, where the body is drawn.
					g_eyeView.store(!g_eyeView.load());
					playerCamera->ForceThirdPerson();
					logger::info("camera: eye view {} (first person showing the player's own Skyrim body)", g_eyeView.load() ? "on" : "off");
				}
			} else {
				g_eyeView.store(false);
			}
			if (g_eyeHeadHidden.load() && playerCamera && playerCamera->currentState && playerCamera->currentState->id != RE::CameraState::kThirdPerson) {
				// Skyrim has the camera somewhere else than at the eyes (a scene, a seat, a kill move, the turn as a menu
				// opens): the eye view's own camera update does not run there, and its hidden head showed as a body
				// without one. The head is back until the eye view's camera is.
				if (auto* body = a_player->Get3D(false)) {
					if (auto* head = body->GetObjectByName("NPC Head [Head]")) {
						head->local.scale = 1.0f;
						RE::NiUpdateData update;
						head->UpdateDownwardPass(update, 0);
					}
				}
				if (auto* nearSetting = RE::INISettingCollection::GetSingleton() ? RE::INISettingCollection::GetSingleton()->GetSetting("fNearDistance:Display") : nullptr; nearSetting && g_eyeNearWas.load() > 0.0f) {
					nearSetting->data.f = g_eyeNearWas.load();
				}
				g_eyeHeadHidden.store(false);
				logger::info("camera: Skyrim has the camera (state {}): the head is shown again until the eye view is back", static_cast<int>(playerCamera->currentState->id));
			}
			const bool firstPerson = (playerCamera && playerCamera->IsInFirstPerson()) || g_eyeView.load();
			SendPlayerState(a_player, forward, right, link);
			{
				// (the key let go while a menu was open never reports its release: no sprint without movement or in a pause)
				auto* pauseUi = RE::UI::GetSingleton();
				if ((pauseUi && pauseUi->GameIsPaused()) || !link.live) {
					g_sprintKey.store(false);
				}
			}
			Link::SetMovement(forward, right, a_player->AsActorState()->IsSprinting() || (g_sprintKey.load() && (std::abs(forward) + std::abs(right) > 0.05f)), a_player->data.angle.x * 57.29578f, firstPerson);
			Link::SetCrouch(a_player->IsSneaking());
			DevShieldShow(a_player, link);
			DevUnarmedTest(link);
			DevThrowTest(a_player, link);
			DevShieldTest(a_player, link);
			SendShield(a_player, link);
			if (!PlayerShield(a_player)) {
				g_shieldBlock.store(false);
			}
			ShowPickUpPrompt(a_player, link);
			PlaceDroppedWeapons(a_player, link);
			LoadAiLevel();
			MatchEnbBrightness(a_player);
			CalmBodies();
			ThrowHeads();
			CharacterRender::SetUnsheatheWait(BuildValue("unsheathe_wait"));
			CharacterRender::UpdateBlessings(a_player);
			{
				// the left hand casting (the cast key): its arm in Skyrim's own casting animation until a moment after the cast
				static Clock::time_point castSeen{};
				auto*                    caster = a_player->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand);
				if (caster && caster->state.get() != RE::MagicCaster::State::kNone) {
					castSeen = Clock::now();
				}
				const bool skyrims = BuildValue("cast_arm_skyrim") > 0.5f && castSeen != Clock::time_point{} && Seconds(castSeen) < BuildValue("cast_arm_linger");
				static bool told = false;
				if (skyrims && !told) {
					logger::info("cast: the left arm is in Skyrim's own casting animation while it casts");
				}
				told = told || skyrims;
				CharacterRender::SetLeftCasting(skyrims);
			}
			CharacterRender::SetSkyrimBodies(g_dev.skyrimBodies >= 0 ? g_dev.skyrimBodies == 1 : BuildValue("skyrim_bodies") > 0.5f);
			SendWalls(a_player, link);
			{
				// the player's blood fades (about four minutes from full)
				static Clock::time_point lastFade = Clock::now();
				if (Seconds(lastFade) > 1.0f) {
					lastFade = Clock::now();
					CharacterRender::AddBlood(0, -0.004f);
				}
			}
			if (g_unarmedAsk.exchange(false) && link.live) {
				// MORDHAU's unarmed slot (4): what is held is put away and the fists come up (an empty right hand is fists,
				// below); 4 again brings back what was held, if it is still carried
				static RE::TESBoundObject* putAway = nullptr;
				static RE::TESObjectARMO*  shieldAway = nullptr;
				auto*                      equip = RE::ActorEquipManager::GetSingleton();
				auto*                      inHand = a_player->GetEquippedObject(false);
				if (inHand && inHand->As<RE::TESObjectWEAP>()) {
					putAway = inHand->As<RE::TESBoundObject>();
					shieldAway = a_player->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kShield);
					equip->UnequipObject(a_player, putAway);
					if (shieldAway) {
						equip->UnequipObject(a_player, shieldAway);
					}
					logger::info("unarmed: {} put away{}, the fists are up", putAway->GetName(), shieldAway ? " with the shield" : "");
				} else if (!inHand && putAway) {
					const auto carried = a_player->GetInventoryCounts();
					const auto has = [&](RE::TESBoundObject* a_item) {
						const auto it = carried.find(a_item);
						return it != carried.end() && it->second > 0;
					};
					if (has(putAway)) {
						equip->EquipObject(a_player, putAway);
						logger::info("unarmed: {} is back in the hand", putAway->GetName());
					}
					if (shieldAway && has(shieldAway)) {
						equip->EquipObject(a_player, shieldAway);
					}
					putAway = nullptr;
					shieldAway = nullptr;
				}
			}
			SendWeaponWish(a_player, link);
			CharacterRender::SetMenu(link.live && link.menuOpen ? 1 : 0, 0);
			{
				auto* av = a_player->AsActorValueOwner();
				Link::SetHealth(std::clamp(av->GetActorValue(RE::ActorValue::kHealth) / std::max(av->GetPermanentActorValue(RE::ActorValue::kHealth), 1.0f), 0.0f, 1.0f) * 100.0f);
			}
			// one HUD: while MORDHAU is live and the weapon is out, Skyrim's own health and stamina meters are put away
			// (the two MORDHAU-style bars show them); they come back otherwise
			UpdateSkyrimHud(a_player, link);
			if (link.live && a_player->AsActorState()->IsWeaponDrawn()) {
				HudTask([](RE::GFxMovieView* a_movie) {
					a_movie->SetVariable("_root.HUDMovieBaseInstance.Crosshair._visible", RE::GFxValue(false));
					a_movie->SetVariable("_root.HUDMovieBaseInstance.Crosshair._alpha", RE::GFxValue(0.0));
					a_movie->SetVariable("_root.HUDMovieBaseInstance.CrosshairInstance._alpha", RE::GFxValue(0.0));
				});
			}
			SetFightingDistance(link);
			if (link.live) {
				MuteSkyrimWeapon(a_player);
				MirrorTick(a_player, link);
				// Skyrim's stamina bar shows MORDHAU's stamina (MORDHAU's rules spend and regain it)
				auto*       values = a_player->AsActorValueOwner();
				const float want = std::max(values->GetPermanentActorValue(RE::ActorValue::kStamina), 1.0f) * std::clamp(link.stamina, 0.0f, 100.0f) / 100.0f;
				const float have = values->GetActorValue(RE::ActorValue::kStamina);
				if (g_dev.staminaTest) {
					// (test: every 8 s Skyrim restores 40 stamina, as a potion would)
					static Clock::time_point lastGift = Clock::now();
					if (Seconds(lastGift) > 8.0f) {
						lastGift = Clock::now();
						values->RestoreActorValue(RE::ActorValue::kStamina, 40.0f);
						logger::info("staminatest: Skyrim restores 40 (bar was {:.0f}, MORDHAU's stamina {:.0f})", values->GetActorValue(RE::ActorValue::kStamina), link.stamina);
					}
				}
				// (what Skyrim itself gave since the last frame - a potion, food - goes to MORDHAU's stamina: sheets/builds.json stamina_restore_step)
				static float lastSet = -1.0f;
				const float  full = std::max(values->GetPermanentActorValue(RE::ActorValue::kStamina), 1.0f);
				if (lastSet >= 0.0f && have - lastSet > BuildValue("stamina_restore_step") && !RE::UI::GetSingleton()->GameIsPaused()) {
					Link::AddStamina((have - lastSet) / full * 100.0f);
					logger::info("stamina: Skyrim restored {:.0f} ({:.0f}% of full): passed on to MORDHAU", have - lastSet, (have - lastSet) / full * 100.0f);
				}
				if (std::abs(want - have) > 0.5f) {
					values->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina, want - have);
				}
				lastSet = values->GetActorValue(RE::ActorValue::kStamina);
			} else {
				UnmuteSkyrimWeapons();
				g_mirrored.clear();
				g_playerHitsKnown = false;
				Link::SetGhosts({});
				Link::SetDown(false, {});
			}
			Link::Flush();
			PlayMordhauDrawClip(a_player, link);

			// weapon out or put away: MORDHAU's character does the same, with its own draw and holster animations
			SyncWeaponDrawn(a_player, link);

			static int                         lastStage = -1;
			static bool                        hitDone = false;
			static std::optional<RE::NiPoint3> prevTip, prevMid;
			auto*                              root = a_player->Get3D(false);
			if (!link.live || !root || link.kind != 1 || link.stage != 1) {
				if (link.live && lastStage == 1 && link.stage != 1 && !hitDone) {
					logger::info("live: release over with nothing hit ({}), MORDHAU stamina {:.0f}", link.motion, link.stamina);
				}
				lastStage = link.live ? link.stage : -1;
				prevTip.reset();
				prevMid.reset();
				return;
			}
			const RE::NiPoint3 hand = LiveToWorld(root, link.bladeStart);
			const RE::NiPoint3 tip = LiveToWorld(root, link.bladeEnd);
			const RE::NiPoint3 mid = (hand + tip) * 0.5f;
			// Things in the world that can be broken (spider webs, egg sacs, ...): Skyrim breaks them when its own attack
			// hits them, and Skyrim's attack is not what swings here - so the blade is tested against them itself, each
			// once per swing (sheets/builds.json break_objects).
			static std::unordered_set<std::uint32_t> brokenThisSwing;
			if (lastStage != 1) {
				brokenThisSwing.clear();
			}
			if (BuildValue("break_objects") > 0.5f) {
				if (auto* tes = RE::TES::GetSingleton()) {
					std::vector<RE::TESObjectREFR*> touched;
					tes->ForEachReferenceInRange(a_player, 320.0f, [&](RE::TESObjectREFR* a_ref) {
						auto* base = a_ref ? a_ref->GetBaseObject() : nullptr;
						if (!base || a_ref->IsDisabled() || a_ref->Is(RE::FormType::ActorCharacter) || brokenThisSwing.contains(a_ref->GetFormID())) {
							return RE::BSContainer::ForEachResult::kContinue;
						}
						const RE::BGSDestructibleObjectForm* breakable = nullptr;
						if (auto* activator = base->As<RE::TESObjectACTI>()) {
							breakable = activator;
						} else if (auto* movable = base->As<RE::BGSMovableStatic>()) {
							breakable = movable;
						} else if (auto* container = base->As<RE::TESObjectCONT>()) {
							breakable = container;
						}
						auto* shape = a_ref->Get3D();
						if (!breakable || !breakable->data || !shape) {
							return RE::BSContainer::ForEachResult::kContinue;
						}
						const float radius = std::clamp(shape->worldBound.radius * 0.6f, 12.0f, 90.0f);
						if (SegmentDistance(hand, tip, shape->worldBound.center, shape->worldBound.center) <= radius + 6.0f) {
							touched.push_back(a_ref);
						}
						return RE::BSContainer::ForEachResult::kContinue;
					});
					for (auto* ref : touched) {
						brokenThisSwing.insert(ref->GetFormID());
						ref->DamageObject(BuildValue("break_damage"), false);
						logger::info("live: the blade struck {} (breakable): damaged", ref->GetBaseObject()->GetName() && *ref->GetBaseObject()->GetName() ? ref->GetBaseObject()->GetName() : "an object");
					}
				}
			}
			if (lastStage != 1) {
				g_lastRelease = link.motion;
				++g_releaseNumber;
				hitDone = false;
				logger::info("live: release of {} began, blade {:.0f} units, MORDHAU stamina {:.0f}", link.motion, (tip - hand).Length(), link.stamina);
				// Skyrim's own swing sound too, as loud as the menu says - for attacks only (MORDHAU's draw, EquipmentSwitchMotion,
				// has a release as well: it gave a swing sound on every draw)
				const std::string_view motion(link.motion);
				if (motion.find("Strike") != motion.npos || motion.find("Stab") != motion.npos || motion.find("Kick") != motion.npos || motion.find("Bash") != motion.npos) {
					SoundMix::PlaySkyrimSwing(a_player, motion.find("Kick") != motion.npos);
				}
			}
			// Skyrim's world stops the blade as MORDHAU's own walls would: the outer part of the blade, and the path its
			// tip took since the last frame, against walls, rocks, ground and trees
			{
				static Clock::time_point releaseBegan{};
				static bool              wallDone = false;
				if (lastStage != 1) {
					releaseBegan = Clock::now();
					wallDone = false;
				}
				auto* cell = a_player->GetParentCell();
				auto* world = cell ? cell->GetbhkWorld() : nullptr;
				if (!wallDone && !hitDone && world && Seconds(releaseBegan) > BuildValue("wall_hit_after")) {
					const float scale = RE::bhkWorld::GetWorldScale();
					int         struckLayer = -1;
					const auto  struck = [&](const RE::NiPoint3& a_from, const RE::NiPoint3& a_to) {
						RE::bhkPickData pick;
						pick.rayInput.from = RE::hkVector4(a_from.x * scale, a_from.y * scale, a_from.z * scale, 0.0f);
						pick.rayInput.to = RE::hkVector4(a_to.x * scale, a_to.y * scale, a_to.z * scale, 0.0f);
						RE::CFilter filter;
						a_player->GetCollisionFilterInfo(filter);
						filter.SetCollisionLayer(RE::COL_LAYER::kLOS);
						pick.rayInput.filterInfo = filter;
						{
							RE::BSReadLockGuard lock(world->worldLock);
							world->PickObject(pick);
						}
						if (!pick.rayOutput.HasHit() || !pick.rayOutput.rootCollidable) {
							return false;
						}
						const auto layer = pick.rayOutput.rootCollidable->GetCollisionLayer();
						// (bodies - MORDHAU judges those - and things that are not solid do not stop a blade)
						for (const auto soft : { RE::COL_LAYER::kBiped, RE::COL_LAYER::kCharController, RE::COL_LAYER::kDeadBip, RE::COL_LAYER::kBipedNoCC, RE::COL_LAYER::kProjectile,
								 RE::COL_LAYER::kSpell, RE::COL_LAYER::kTrigger, RE::COL_LAYER::kNonCollidable, RE::COL_LAYER::kWeapon, RE::COL_LAYER::kClutter, RE::COL_LAYER::kActorZone }) {
							if (layer == soft) {
								return false;
							}
						}
						struckLayer = static_cast<int>(layer);
						return true;
					};
					const RE::NiPoint3 outer = hand + (tip - hand) * BuildValue("wall_blade_from");
					if (BuildValue("wall_cut") > 0.5f && (struck(outer, tip) || (prevTip && struck(*prevTip, tip)))) {
						wallDone = true;
						Link::Request("WorldHit");
						logger::info("live: the blade struck Skyrim's world (collision layer {}): MORDHAU stops the attack ({})", struckLayer, link.motion);
					}
				}
			}
			lastStage = 1;
			if (!hitDone) {
				constexpr float kBodyRadius = 22.0f, kBladeRadius = 6.0f;
				RE::Actor*      found = nullptr;
				// (a kick is not the blade - what it reaches is a line from the player's hip
				// forward, and what it does to somebody without a stand-in is Skyrim's stagger)
				const bool         kick = std::string_view(link.motion).find("Kick") != std::string_view::npos;
				const float        facingNow = a_player->GetAngleZ();
				const RE::NiPoint3 kickFrom = a_player->GetPosition() + RE::NiPoint3{ 0.0f, 0.0f, a_player->GetHeight() * 0.4f };
				const RE::NiPoint3 kickTo = kickFrom + RE::NiPoint3{ std::sin(facingNow), std::cos(facingNow), 0.0f } * BuildValue("kick_reach");
				float              foundTall = 0.0f;
				RE::ProcessLists::GetSingleton()->ForEachHighActor([&](RE::Actor* a_target) {
					// (mirrored characters: MORDHAU itself decides - but the super kick throws them too: MORDHAU's kick does no damage,
					// so it never reports one as a hit - "super kick doesnt work ... doesnt launch anyone", all kicks at a stand-in's bandit)
					// (...and one lying limp in Skyrim - paralysed, knocked down - is struck where Skyrim has its body, as well as by
					// MORDHAU's verdict on its falling stand-in: sheets/builds.json limp_hits_skyrim. Either counts, once a swing.)
					const bool superKickOnFought = kick && BuildValue("super_kick") > 0.5f;
					const bool limpFought = a_target && !kick && BuildValue("limp_hits_skyrim") > 0.5f && a_target->IsInRagdollState() && Humanoid(a_target);
					if (!a_target || a_target == a_player || a_target->IsDead() || !a_target->Is3DLoaded() || (IsFought(a_target) && !superKickOnFought && !limpFought) ||
						g_creatureHitRelease[a_target->GetFormID()] == g_releaseNumber) {
						return RE::BSContainer::ForEachResult::kContinue;
					}
					float height = a_target->GetHeight();
					if (height < 40.0f) {
						height = 120.0f;
					}
					RE::NiPoint3 base = a_target->GetPosition() + RE::NiPoint3{ 0.0f, 0.0f, height * 0.15f };
					RE::NiPoint3 top = a_target->GetPosition() + RE::NiPoint3{ 0.0f, 0.0f, height * 0.9f };
					float        body = kBodyRadius;
					if (limpFought) {
						// a body lying in Skyrim: the line from its head to its feet, where its ragdoll has them
						const auto* head = a_target->GetNodeByName("NPC Head [Head]");
						const auto* pelvis = a_target->GetNodeByName("NPC Pelvis [Pelv]");
						const auto* leftFoot = a_target->GetNodeByName("NPC L Foot [Lft ]");
						const auto* rightFoot = a_target->GetNodeByName("NPC R Foot [Rft ]");
						if (head && pelvis) {
							base = head->world.translate;
							top = leftFoot && rightFoot ? (leftFoot->world.translate + rightFoot->world.translate) * 0.5f :
							                              pelvis->world.translate + (pelvis->world.translate - head->world.translate) * 0.9f;  // (legs as long as the trunk)
						}
					} else if (!Humanoid(a_target)) {
						// a creature is not an upright person: its body is taken from its own bounds - a line along the way it
						// faces, as long as it is, at half its height, as thick as it is wide or high (a skeever or a wolf lies
						// low and long; the upright line missed most swings at it)
						const RE::NiPoint3 lo = a_target->GetBoundMin(), hi = a_target->GetBoundMax();
						const float        scale = std::max(a_target->GetScale(), 0.1f);
						const float        halfLength = std::max(std::max(std::abs(lo.y), std::abs(hi.y)) * scale, 10.0f);
						const float        halfWidth = std::max(std::max(std::abs(lo.x), std::abs(hi.x)) * scale, 8.0f);
						const float        tall = std::max((hi.z - lo.z) * scale, 16.0f);
						const float        facing = a_target->GetAngleZ();
						const RE::NiPoint3 along{ std::sin(facing), std::cos(facing), 0.0f };
						const RE::NiPoint3 middle = a_target->GetPosition() + RE::NiPoint3{ 0.0f, 0.0f, tall * 0.5f };
						body = std::max(halfWidth, tall * 0.5f) + BuildValue("creature_hit_margin");
						const float reachAlong = std::max(halfLength - body * 0.5f, 0.0f);
						base = middle - along * reachAlong;
						top = middle + along * reachAlong;
					}
					float dist = kick ? SegmentDistance(kickFrom, kickTo, base, top) : SegmentDistance(hand, tip, base, top);
					if (prevTip && !kick) {
						dist = std::min(dist, SegmentDistance(*prevTip, tip, base, top));
						dist = std::min(dist, SegmentDistance(*prevMid, mid, base, top));
					}
					if (dist <= body + kBladeRadius) {
						found = a_target;
						foundTall = Humanoid(a_target) ? height : (a_target->GetBoundMax().z - a_target->GetBoundMin().z) * std::max(a_target->GetScale(), 0.1f);
						return RE::BSContainer::ForEachResult::kStop;
					}
					return RE::BSContainer::ForEachResult::kContinue;
				});
				if (found) {
					g_creatureHitRelease[found->GetFormID()] = g_releaseNumber;  // (its stand-in's verdict for this swing is not counted again)
				}
				if (found && kick && IsFought(found)) {
					// (one with a MORDHAU stand-in, reached by the kick with the super kick on: only thrown - the kick itself is MORDHAU's)
					hitDone = true;
					SuperKick(a_player, found);
				} else if (found && kick) {
					hitDone = true;
					if (SuperKick(a_player, found)) {
						// (thrown like Fus Ro Dah: no stagger on top)
					} else if (BuildValue("kick_stagger") < 0.5f) {
						// (settings menu "Kicks stagger creatures" off: the kick still lands - marker, sound, combat - without the
						// stagger; it had made the kick a blade swing from the hand, which never reached anything)
						logger::info("live: the kick reached {}: not staggered (kick_stagger off)", found->GetDisplayFullName());
					} else if (foundTall > BuildValue("kick_stagger_tallest")) {
						logger::info("live: the kick reached {} ({:.0f} units tall): too big to be staggered", found->GetDisplayFullName(), foundTall);
					} else {
						// (Skyrim's own stagger: away from where the player stands, as its own hits give it)
						const RE::NiPoint3 to = a_player->GetPosition() - found->GetPosition();
						float              turn = (std::atan2(to.x, to.y) - found->GetAngleZ()) / (2.0f * std::numbers::pi_v<float>);
						turn -= std::floor(turn);
						found->SetGraphVariableFloat("staggerDirection", turn);
						found->SetGraphVariableFloat("staggerMagnitude", BuildValue("kick_stagger_magnitude"));
						const bool taken = found->NotifyAnimationGraph("staggerStart");
						logger::info("live: the kick reached {} ({:.0f} units tall): staggered ({})", found->GetDisplayFullName(), foundTall, taken ? "taken" : "not taken: no stagger in its animations, or it is in one already");
					}
					// a little damage (sheets/builds.json kick_creature_damage: a share of one clean hit), judged as a weapon hit of
					// that size; ApplyHit gives the hit marker and sound and starts the fight. Without damage: the marker and sound here.
					if (const float share = BuildValue("kick_creature_damage"); share > 0.0f) {
						ActiveSwing swing;
						swing.swing = g_liveSwing ? g_liveSwing : &sheets::kSwings[0];
						swing.family = FamilyOf(a_player);
						if (!swing.family) {
							swing.family = &sheets::kWeaponFamilies[0];
						}
						ApplyHit(a_player, found, swing, 1.0f, Progress("clean_hit") * share * CreatureScale(found));
					} else if (BuildValue("hit_marker") > 0.5f) {
						CharacterRender::ShowHitMarker(false);
						Link::Request("HitSound");
					}
					// (MORDHAU knows nothing of this enemy: left alone, its kick runs out as a missed one)
					Link::Request("KickLanded");
					if (!found->IsDead() && !found->IsPlayerTeammate() && !found->IsInCombat() && found->IsHostileToActor(a_player)) {  // (a townsman kicked is shoved, not attacked)
						found->StartCombat(a_player);
					}
				} else if (found) {
					hitDone = true;
					ActiveSwing swing;
					swing.swing = g_liveSwing ? g_liveSwing : &sheets::kSwings[0];
					swing.family = FamilyOf(a_player);
					if (!swing.family) {
						swing.family = &sheets::kWeaponFamilies[0];
					}
					logger::info("live: MORDHAU's blade ({}) reached {}{} at {:.3f}s into the motion", link.motion, found->GetDisplayFullName(),
						IsFought(found) ? " lying limp in Skyrim" : "", link.motionTime);
					// (no stand-in: MORDHAU gave no damage; one clean hit - a creature takes sheets/builds.json creature_damage_scale of it)
					ApplyHit(a_player, found, swing, 1.0f, Progress("clean_hit") * CreatureScale(found));
					{
						// a blunt weapon stops in what it strikes, as in MORDHAU (a blade may carry on through): the attack
						// ends there, the same way it ends on a wall. (Against people MORDHAU does this itself - a creature
						// has no stand-in.)
						std::string weapon = link.weaponClass;
						if (weapon.rfind("BP_", 0) == 0 && weapon.ends_with("_C")) {
							weapon = weapon.substr(3, weapon.size() - 5);
						}
						const std::string list = std::string("|") + sheets::kGoreBlunt + "|";
						if (!weapon.empty() && list.find("|" + weapon + "|") != std::string::npos) {
							Link::Request("WorldHit");
						}
					}
				}
			}
			prevTip = tip;
			prevMid = mid;
		}
	}
}
