#include "CombatInternal.h"

namespace dm::Combat
{
	inline namespace internal
	{
		// a_first false: re-read while running, so test pictures can change pose without a restart.
		void LoadDevConfig(bool a_first)
		{
			wchar_t exe[MAX_PATH]{};
			GetModuleFileNameW(nullptr, exe, MAX_PATH);
			std::ifstream in(std::filesystem::path(exe).parent_path() / "DirectionalMelee.dev.txt");
			std::string   line;
			while (std::getline(in, line)) {
				if (!line.empty() && line.back() == '\r') {
					line.pop_back();
				}
				const auto eq = line.find('=');
				if (eq == std::string::npos) {
					continue;
				}
				const auto key = line.substr(0, eq), value = line.substr(eq + 1);
				if (key == "hold") {
					g_dev.hold = std::strtof(value.c_str(), nullptr);
				} else if (key == "twist") {
					g_dev.twist = value != "0";
				} else if (!a_first) {
					continue;
				} else if (key == "move") {
					g_dev.movement = value != "0";
				} else if (key == "char") {
					g_dev.character = value != "0";
				} else if (key == "charoffset") {
					g_dev.charOffset = std::strtof(value.c_str(), nullptr);
				} else if (key == "duel") {
					g_dev.duel = value != "0";
				} else if (key == "duelhealth") {
					g_dev.duelHealth = std::stof(value);
				} else if (key == "duelweapon") {
					g_dev.duelWeapon = value;
				} else if (key == "give") {
					g_dev.give = value != "0";
				} else if (key == "types") {
					g_dev.types = value != "0";
				} else if (key == "arsenal") {
					g_dev.arsenal = value != "0";
				} else if (key == "bowdown") {
					g_dev.bowDown = std::strtof(value.c_str(), nullptr);
				} else if (key == "bowkind") {
					g_dev.bowKind = std::atoi(value.c_str());
				} else if (key == "bowslow") {
					g_dev.bowSlow = std::strtof(value.c_str(), nullptr);
				} else if (key == "bowhold") {
					g_dev.bowHold = std::strtof(value.c_str(), nullptr);
				} else if (key == "hybridoff") {
					g_dev.hybridOff = value != "0";
				} else if (key == "pauldronsoff") {
					g_dev.pauldronsOff = value != "0";
				} else if (key == "unarmedtest") {
					g_dev.unarmedTest = value != "0";
				} else if (key == "throwtest") {
					g_dev.throwTest = value != "0";
					g_dev.throwSlow = value == "2";
				} else if (key == "shieldshow") {
					g_dev.shieldShow = value != "0";
				} else if (key == "shieldtest") {
					g_dev.shieldTest = value != "0";
				} else if (key == "staminatest") {
					g_dev.staminaTest = value != "0";
				} else if (key == "demo") {
					g_dev.demo = value != "0";
					g_dev.demoKind = value == "2" ? 2 : 1;
					g_dev.god = g_dev.god || g_dev.demo;
				} else if (key == "duelundead") {
					g_dev.duelUndead = value != "0";
				} else if (key == "arrowtest") {
					g_dev.arrowTest = value != "0";
				} else if (key == "scenetest") {
					g_dev.sceneTest = value != "0";
				} else if (key == "goretest") {
					g_dev.goreTest = value != "0";
				} else if (key == "skyrimbodies") {
					g_dev.skyrimBodies = value != "0" ? 1 : 0;
				} else if (key == "crowd") {
					g_dev.crowd = std::atoi(value.c_str());
				} else if (key == "duelpassive") {
					g_dev.duelPassive = value != "0";
				} else if (key == "measure") {
					g_dev.measure = value != "0";
				} else if (key == "god") {
					g_dev.god = value != "0";
				} else if (key == "bowtest") {
					g_dev.bowTest = value != "0";
				} else if (key == "pushtest") {
					g_dev.pushTest = value != "0";
					g_dev.pushMode = std::atoi(value.c_str());
				} else if (key == "demon") {
					g_dev.demon = value != "0";
				} else if (key == "battle") {
					g_dev.battle = std::atoi(value.c_str());
				} else if (key == "turntest") {
					g_dev.turnTest = std::atoi(value.c_str());
				} else if (key == "turnstep") {
					g_dev.turnStep = std::strtof(value.c_str(), nullptr);
				} else if (key == "skirmish") {
					g_dev.skirmish = std::atoi(value.c_str());
				} else if (key == "birdseye") {
					g_dev.birdsEye = std::strtof(value.c_str(), nullptr);
				} else if (key == "lookat") {
					g_dev.lookAt = value != "0";
				} else if (key == "npcfight") {
					g_dev.npcFight = value != "0";
				} else if (key == "charlight") {
					g_dev.charLight = std::strtof(value.c_str(), nullptr);
				} else if (key == "cache") {
					g_dev.cache = value;
				} else if (key == "mordhau") {
					g_dev.mordhau = value;
				} else if (key == "autotest") {
					g_dev.autotest = value.size() == 1 && value[0] >= '0' && value[0] <= '4' ? value[0] - '0' : 0;
				}
			}
			if (!a_first) {
				return;
			}
			// (installed by a mod manager: no dev file; MordhauSkyrim.ini can name another cache or MORDHAU folder)
			if (g_dev.cache.empty()) {
				g_dev.cache = Session::CacheFolder();
			}
			if (g_dev.mordhau.empty()) {
				g_dev.mordhau = Session::MordhauFolder();
			}
			if (!g_dev.cache.empty()) {
				g_stats.Load(g_dev.cache / "stats.txt");
			}
			logger::info("dev: cache '{}', autotest {}, {} MORDHAU weapon values", g_dev.cache.string(), g_dev.autotest, g_stats.Size());
		}

		// Development check: where the driven bones actually point, in the actor's own space (x right, y forward, z up).
		void Probe(RE::Actor* a_actor, int a_percent)
		{
			auto* root = a_actor->Get3D(false);
			if (!root) {
				return;
			}
			const RE::NiMatrix3 toActor = root->world.rotate.Transpose();
			for (const auto& bone : sheets::kBones) {
				if (bone.layer != sheets::Layer::kUpper || !*bone.dirSkyrimNode) {
					continue;
				}
				auto* node = root->GetObjectByName(bone.skyrimNode);
				auto* child = root->GetObjectByName(bone.dirSkyrimNode);
				if (!node || !child) {
					continue;
				}
				RE::NiPoint3 dir = toActor * (child->world.translate - node->world.translate);
				const float  len = dir.Length();
				if (len > 1e-4f) {
					dir = dir / len;
				}
				logger::info("probe {}% {} dir ({:+.2f},{:+.2f},{:+.2f})", a_percent, bone.id, dir.x, dir.y, dir.z);
			}
		}

		// Development check for stretched gauntlets: every node under the right upper arm, with its parent and where it
		// is in the actor's own space. a_when tells the two calls per frame apart (before / after our pose).
		void ProbeArm(RE::Actor* a_actor, const char* a_when)
		{
			auto* root = a_actor->Get3D(false);
			auto* arm = root ? root->GetObjectByName("NPC R UpperArm [RUar]") : nullptr;
			if (!arm) {
				return;
			}
			const RE::NiMatrix3 toActor = root->world.rotate.Transpose();
			const auto          report = [&](RE::NiAVObject* a_obj, int a_depth, auto&& a_self) -> void {
				if (a_depth > 3 || !a_obj) {
					return;
				}
				const RE::NiPoint3 pos = toActor * (a_obj->world.translate - root->world.translate);
				// where its world position would be if recomputed from its parent right now
				RE::NiPoint3 fromParent = pos;
				if (a_obj->parent) {
					fromParent = toActor * (a_obj->parent->world.translate + a_obj->parent->world.rotate * (a_obj->local.translate * a_obj->parent->world.scale) - root->world.translate);
				}
				logger::info("arm {} d{} '{}' parent '{}' pos ({:.0f},{:.0f},{:.0f}) from-parent ({:.0f},{:.0f},{:.0f})", a_when, a_depth, a_obj->name.c_str(),
					a_obj->parent ? a_obj->parent->name.c_str() : "", pos.x, pos.y, pos.z, fromParent.x, fromParent.y, fromParent.z);
				if (auto* node = a_obj->AsNode()) {
					for (auto& child : node->GetChildren()) {
						a_self(child.get(), a_depth + 1, a_self);
					}
				}
			};
			report(arm, 0, report);
		}

		// Side-by-side mode, once a frame: tell MORDHAU what the player is doing, and while MORDHAU's attack is in its
		// release, sweep MORDHAU's own blade through Skyrim's world.
		// Test scene (dev key bowtest): the player gets a bow and shoots every few seconds.
		void DevBowTest(RE::PlayerCharacter* a_player)
		{
		if (g_dev.bowTest) {
			if (g_dev.bowDown > 0.0f) {
				a_player->data.angle.x = g_dev.bowDown;  // (held: the view looks at where the arrows land)
			}
			static Clock::time_point since = Clock::now();
			static int               step = 0;
			if (step == 0 && Seconds(since) > 6.0f) {
				step = 1;
				since = Clock::now();
				auto* bowForm = RE::TESForm::LookupByID<RE::TESObjectWEAP>(g_dev.bowKind == 1 ? 0x0003B562 : 0x00013985);  // Long Bow (bowkind=1) / Hunting Bow
				auto* arrows = RE::TESForm::LookupByID<RE::TESAmmo>(0x0001397D);          // Iron Arrow
				if (g_dev.bowKind == 2) {
					// bowkind=2: Dawnguard's crossbow and steel bolts
					if (auto* handler = RE::TESDataHandler::GetSingleton()) {
						bowForm = handler->LookupForm<RE::TESObjectWEAP>(0x000801, "Dawnguard.esm");
						arrows = handler->LookupForm<RE::TESAmmo>(0x000BB3, "Dawnguard.esm");
					}
				}
				if (bowForm && arrows) {
					a_player->AddObjectToContainer(bowForm, nullptr, 1, nullptr);
					a_player->AddObjectToContainer(arrows, nullptr, 50, nullptr);
					RE::ActorEquipManager::GetSingleton()->EquipObject(a_player, bowForm);
					RE::ActorEquipManager::GetSingleton()->EquipObject(a_player, arrows, nullptr, 50);
					logger::info("bowtest: {} and {} equipped", bowForm->GetName(), arrows->GetName());
					// (a look in daylight: noon and clear sky for the test)
					if (auto* calendar = RE::Calendar::GetSingleton(); calendar && calendar->gameHour) {
						calendar->gameHour->value = 12.0f;
					}
					if (auto* sky = RE::Sky::GetSingleton()) {
						if (auto* clear = RE::TESForm::LookupByID<RE::TESWeather>(0x0000081A)) {
							sky->ForceWeather(clear, true);
						}
					}
				} else {
					logger::info("bowtest: bow or arrows not found");
				}
			} else if (step >= 1 && step % 2 == 1 && Seconds(since) > 7.0f + g_dev.bowHold) {
				++step;
				since = Clock::now();
				if (auto* root3 = a_player->Get3D(false)) {
					std::string where;
					for (const char* name : { "WEAPON", "SHIELD", "WeaponBow", "QUIVER" }) {
						auto* object = root3->GetObjectByName(name);
						auto* node = object ? object->AsNode() : nullptr;
						int   shapes = 0, culled = 0;
						if (node) {
							for (auto& child : node->GetChildren()) {
								if (child) {
									++shapes;
									culled += child->GetAppCulled() ? 1 : 0;
								}
							}
						}
						where += std::format(" {}:{}{}", name, node ? std::to_string(shapes) : "none", culled ? std::format("(culled {})", culled) : "");
					}
					logger::info("bowtest: Skyrim's nodes hold{} | weapon drawn {}", where, a_player->AsActorState()->IsWeaponDrawn());
				}
				logger::info("bowtest: draw (wish {}, ammo {})", RangedWish(a_player) ? RangedWish(a_player) : "none", a_player->GetCurrentAmmo() ? a_player->GetCurrentAmmo()->GetName() : "none");
				if (RangedWish(a_player) && a_player->GetCurrentAmmo()) {
					if ((step == 2 || step == 6) && g_dev.bowHold < 0.0f) {
						FaceNearestActor(a_player);  // (bowhold=-1: now and then from close up, at somebody)
					}
					if (g_dev.bowDown > 0.0f) {
						a_player->data.angle.x = g_dev.bowDown;  // (bowdown=0.3: aim at the ground some steps ahead, to look at the arrows that stick)
					}
					Link::Request("FirePress");
				}
			} else if (step >= 2 && step % 2 == 0 && Seconds(since) > (g_dev.bowHold > 0.0f ? g_dev.bowHold : 2.2f)) {
				++step;
				since = Clock::now();
				Link::Request("FireRelease");
			}
		}
		}

		// Test (dev key pushtest): every few seconds the nearest person, then the player, is thrown as a shout would.
		// dev (measure=1): the bone names of every kind of creature that comes near, once per kind - for setting them
		// against the skeletons of MORDHAU's creatures
		void DevBoneDump(RE::PlayerCharacter* a_player)
		{
			static std::unordered_set<std::string> told;
			static Clock::time_point               last{};
			if (!g_dev.measure || Seconds(last) < 4.0f) {
				return;
			}
			last = Clock::now();
			// (%TEMP%/mordhauskyrim_spawn.txt: kinds of creature to set down one after the other, each by a piece of its
			// race's name - "falmer,dwarvencenturion"; the one before is taken away again)
			{
				static std::vector<std::string> wanted;
				static RE::ObjectRefHandle      lastOne;
				const auto                      path = std::filesystem::temp_directory_path() / "mordhauskyrim_spawn.txt";
				if (std::ifstream in(path); in) {
					std::string line;
					std::getline(in, line);
					in.close();
					std::error_code ec;
					std::filesystem::remove(path, ec);
					std::stringstream parts(line);
					for (std::string part; std::getline(parts, part, ',');) {
						if (!part.empty()) {
							wanted.push_back(part);
						}
					}
				}
				if (!wanted.empty()) {
					if (auto old = lastOne.get()) {
						old->Disable();
					}
					const std::string want = wanted.front();
					wanted.erase(wanted.begin());
					RE::TESNPC* pick = nullptr;
					if (auto* data = RE::TESDataHandler::GetSingleton()) {
						for (auto* npc : data->GetFormArray<RE::TESNPC>()) {
							auto*       race = npc ? npc->GetRace() : nullptr;
							const char* id = race ? race->GetFormEditorID() : nullptr;
							std::string kind = id && id[0] ? id : (race && race->GetName() ? race->GetName() : "");
							std::transform(kind.begin(), kind.end(), kind.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
							if (!kind.empty() && kind.find(want) != std::string::npos && !npc->IsUnique()) {
								pick = npc;
								break;
							}
						}
					}
					if (pick) {
						if (auto made = a_player->PlaceObjectAtMe(pick, false)) {
							const float facing = a_player->GetAngleZ();
							made->SetPosition(a_player->GetPosition() + RE::NiPoint3{ std::sin(facing), std::cos(facing), 0.0f } * 350.0f);
							lastOne = made->GetHandle();
							logger::info("bones: set down a '{}' for '{}'", pick->GetName() ? pick->GetName() : "?", want);
						}
					} else {
						logger::info("bones: no kind of creature with '{}' in its race's name", want);
					}
				}
			}
			RE::ProcessLists::GetSingleton()->ForEachHighActor([&](RE::Actor* a_actor) {
				auto* race = a_actor ? a_actor->GetRace() : nullptr;
				auto* shape = a_actor && a_actor != a_player ? a_actor->Get3D(false) : nullptr;
				if (!race || !shape) {
					return RE::BSContainer::ForEachResult::kContinue;
				}
				const char* id = race->GetFormEditorID();
				std::string kind = id && id[0] ? id : (race->GetName() ? race->GetName() : "?");
				if (!told.insert(kind).second) {
					return RE::BSContainer::ForEachResult::kContinue;
				}
				std::string names;
				int         count = 0;
				RE::BSVisit::TraverseScenegraphObjects(shape, [&](RE::NiAVObject* a_object) {
					if (a_object && a_object->AsNode() && !a_object->name.empty() && count < 220) {
						names += std::string(a_object->name.c_str()) + "|";
						++count;
					}
					return RE::BSVisit::BSVisitControl::kContinue;
				});
				logger::info("bones: {} '{}' humanoid {} height {:.0f} bounds tall {:.0f} scale {:.2f}: {} nodes: {}", kind, a_actor->GetDisplayFullName(), Humanoid(a_actor), a_actor->GetHeight(),
					(a_actor->GetBoundMax().z - a_actor->GetBoundMin().z) * a_actor->GetScale(), a_actor->GetScale(), count, names);
				return RE::BSContainer::ForEachResult::kContinue;
			});
		}

		void DevPushTest(RE::PlayerCharacter* a_player)
		{
			DevBoneDump(a_player);
		if (g_dev.pushTest) {
			// what Unrelenting Force does to its target: Skyrim's own push (the actor goes limp and flies)
			static Clock::time_point lastPush = Clock::now();
			static int               count = 0;
			if (Seconds(lastPush) > 8.0f) {
				lastPush = Clock::now();
				using Push = void (*)(RE::AIProcess*, RE::Actor*, RE::NiPoint3*, float);
				REL::Relocation<Push> push{ RELOCATION_ID(38858, 39895) };
				RE::Actor*            target = nullptr;
				float                 best = 1500.0f;
				RE::ProcessLists::GetSingleton()->ForEachHighActor([&](RE::Actor* a_actor) {
					if (a_actor && a_actor != a_player && !a_actor->IsDead() && a_actor->Is3DLoaded() && a_actor->HasKeywordString("ActorTypeNPC")) {
						const float d = a_actor->GetPosition().GetDistance(a_player->GetPosition());
						if (d < best && (g_dev.pushMode != 2 || IsMirrored(a_actor))) {
							best = d, target = a_actor;
						}
					}
					return RE::BSContainer::ForEachResult::kContinue;
				});
				RE::NiPoint3 from = a_player->GetPosition();
				if ((++count % 2 == 1 || g_dev.pushMode == 2) && target) {
					logger::info("pushtest: {} thrown ({:.0f} units away, stand-in: {})", target->GetDisplayFullName(), best, IsMirrored(target));
					push(a_player->GetActorRuntimeData().currentProcess, target, &from, 12.0f);
				} else if (target) {
					from = target->GetPosition();
					logger::info("pushtest: the player thrown");
					push(target->GetActorRuntimeData().currentProcess, a_player, &from, 12.0f);
				}
			}
			static bool was = false;
			if (a_player->IsInRagdollState() != was) {
				was = !was;
				logger::info("pushtest: the player is {}", was ? "limp (thrown)" : "up again");
			}
		}
		}

		// Dev aid (dev key give): items named in <cache>/give.txt are put into the player's inventory.
		void DevGiveItems(RE::PlayerCharacter* a_player)
		{
		if (g_dev.give && !g_dev.cache.empty()) {
			// Dev aid: <cache>/give.txt, one item name per line - put into the player's inventory (a weapon, or 30 of
			// an ammunition), then the file is taken away. Looked for once a second. Nothing is typed into the game.
			static auto lastLook = Clock::now();
			if (Clock::now() - lastLook > std::chrono::seconds(1)) {
				lastLook = Clock::now();
				const auto      file = g_dev.cache / "give.txt";
				std::error_code ec;
				if (std::filesystem::exists(file, ec)) {
					std::vector<std::string> names;
					{
						std::ifstream in(file);
						std::string   line;
						while (std::getline(in, line)) {
							while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
								line.pop_back();
							}
							if (!line.empty()) {
								names.push_back(line);
							}
						}
					}
					std::filesystem::remove(file, ec);
					for (auto name : names) {
						// (a line starting with + : the item is also put into the hand)
						const bool equip = name.front() == '+';
						if (equip) {
							name.erase(0, 1);
						}
						RE::TESBoundObject* found = nullptr;
						int                 count = 1;
						for (auto* weapon : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESObjectWEAP>()) {
							if (weapon && weapon->GetPlayable() && !weapon->formEnchanting && !weapon->templateWeapon && weapon->GetName() && name == weapon->GetName()) {
								found = weapon;
								break;
							}
						}
						if (!found) {
							for (auto* ammo : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESAmmo>()) {
								if (ammo && ammo->GetPlayable() && ammo->GetName() && name == ammo->GetName()) {
									found = ammo;
									count = 30;
									break;
								}
							}
						}
						if (found) {
							a_player->AddObjectToContainer(found, nullptr, count, nullptr);
							if (equip) {
								RE::ActorEquipManager::GetSingleton()->EquipObject(a_player, found);
							}
						}
						logger::info("give: {} {}", name, found ? "added to the inventory" : "NOT FOUND (no plain playable weapon or ammunition of that name)");
					}
				}
			}
		}
		}

		// Test (dev key types): one plain weapon of every Skyrim weapon type into the inventory, once.
		void DevGiveWeaponTypes(RE::PlayerCharacter* a_player)
		{
		if (g_dev.types) {
			static bool given = false;
			if (!given && Link::Current().live) {
				given = true;
				std::string got;
				for (const char* name : { "Steel Dagger", "Steel Sword", "Steel War Axe", "Steel Mace", "Steel Greatsword", "Steel Battleaxe", "Steel Warhammer", "Hunting Bow", "Iron Arrow" }) {
					const bool arrows = std::string_view(name) == "Iron Arrow";
					RE::TESBoundObject* found = nullptr;
					if (arrows) {
						for (auto* ammo : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESAmmo>()) {
							if (ammo && ammo->GetPlayable() && ammo->GetName() && std::string_view(ammo->GetName()) == name) {
								found = ammo;
								break;
							}
						}
					} else {
						for (auto* weapon : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESObjectWEAP>()) {
							if (weapon && weapon->GetPlayable() && !weapon->formEnchanting && !weapon->templateWeapon && weapon->GetName() && std::string_view(weapon->GetName()) == name) {
								found = weapon;
								break;
							}
						}
					}
					if (found && a_player->GetItemCount(found) == 0) {
						a_player->AddObjectToContainer(found, nullptr, arrows ? 30 : 1, nullptr);
						got += std::string(name) + ", ";
					}
				}
				logger::info("types: added to the inventory: {}", got.empty() ? "nothing (all there already)" : got);
			}
		}
		}

		// Test scene (dev key skirmish): the player and soldiers against as many bandits.
		void DevSkirmishScene(RE::PlayerCharacter* a_player)
		{
		if (g_dev.skirmish > 0 && Link::Current().live) {
			// A small, even fight to look at: the player and N-1 soldiers in a line, N bandits 520 units ahead, each with
			// one melee weapon and no bow, sturdy enough to last; everyone else about is removed, nobody is replaced.
			static bool                         placed = false;
			static std::vector<RE::ActorHandle> allies, foes;
			static Clock::time_point            lastLook{};
			if (!placed) {
				placed = true;
				RE::TESNPC* bandit = nullptr;
				RE::TESNPC* soldier = nullptr;
				auto*       bandits = RE::TESForm::LookupByID<RE::TESFaction>(0x0001BCC0);
				for (auto* npc : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESNPC>()) {
					if (!npc || npc->IsUnique() || npc->IsFemale() || !npc->GetName()) {
						continue;
					}
					if (!bandit && std::string_view(npc->GetName()) == "Bandit" && bandits && npc->IsInFaction(bandits)) {
						bandit = npc;
					}
					if (!soldier && std::string_view(npc->GetName()) == "Imperial Soldier") {
						soldier = npc;
					}
				}
				const auto weaponNamed = [](std::string_view a_name) -> RE::TESObjectWEAP* {
					for (auto* weapon : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESObjectWEAP>()) {
						if (weapon && weapon->GetPlayable() && !weapon->formEnchanting && weapon->GetName() && a_name == weapon->GetName()) {
							return weapon;
						}
					}
					return nullptr;
				};
				const char* allyWeapons[]{ "Steel Sword", "Steel Greatsword", "Steel Mace" };
				const char* foeWeapons[]{ "Iron Battleaxe", "Iron Sword", "Iron Warhammer", "Iron War Axe" };
				const float        facing = a_player->GetAngleZ();
				const RE::NiPoint3 forward{ std::sin(facing), std::cos(facing), 0.0f }, right{ std::cos(facing), -std::sin(facing), 0.0f };
				g_sceneSide = right;
				const auto set = [&](RE::TESNPC* a_base, const RE::NiPoint3& a_at, const char* a_weapon, bool a_ally) -> RE::Actor* {
					auto  ref = a_base ? a_player->PlaceObjectAtMe(a_base, false) : nullptr;
					auto* actor = ref ? ref->As<RE::Actor>() : nullptr;
					if (!actor) {
						return nullptr;
					}
					RE::NiPoint3 at = a_at;
					at.z = GroundUnder(a_player, at, 300.0f) + 10.0f;
					actor->SetPosition(at, true);
					// no bow (an archer keeps to Skyrim's own fighting), one melee weapon
					std::vector<RE::TESBoundObject*> bows;
					for (const auto& [item, entry] : actor->GetInventory()) {
						const auto* has = item ? item->As<RE::TESObjectWEAP>() : nullptr;
						if (has && (has->IsBow() || has->IsCrossbow())) {
							bows.push_back(item);
						}
					}
					for (auto* bow : bows) {
						actor->RemoveItem(bow, 10, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
					}
					if (auto* weapon = weaponNamed(a_weapon)) {
						actor->AddObjectToContainer(weapon, nullptr, 1, nullptr);
						RE::ActorEquipManager::GetSingleton()->EquipObject(actor, weapon);
					}
					actor->AsActorValueOwner()->SetBaseActorValue(RE::ActorValue::kHealth, 260.0f);
					actor->AsActorValueOwner()->RestoreActorValue(RE::ActorValue::kHealth, 260.0f);
					actor->AsActorValueOwner()->SetActorValue(RE::ActorValue::kConfidence, 4.0f);
					actor->AsActorValueOwner()->SetActorValue(RE::ActorValue::kAggression, a_ally ? 1.0f : 2.0f);
					if (a_ally) {
						actor->GetActorRuntimeData().boolBits.set(RE::Actor::BOOL_BITS::kPlayerTeammate);
					}
					g_scenePeople.push_back(actor->GetHandle());
					return actor;
				};
				for (int k = 0; k < g_dev.skirmish; ++k) {
					const float across = (static_cast<float>(k) - static_cast<float>(g_dev.skirmish - 1) * 0.5f) * 190.0f;
					if (auto* foe = set(bandit, a_player->GetPosition() + forward * 520.0f + right * across, foeWeapons[k % 4], false)) {
						foes.push_back(foe->GetHandle());
					}
					if (k > 0) {
						const float beside = (k % 2 ? -1.0f : 1.0f) * 190.0f * static_cast<float>((k + 1) / 2);
						if (auto* ally = set(soldier, a_player->GetPosition() + right * beside, allyWeapons[(k - 1) % 3], true)) {
							allies.push_back(ally->GetHandle());
						}
					}
				}
				// (to be seen: midday, clear sky)
				if (auto* calendar = RE::Calendar::GetSingleton(); calendar && calendar->gameHour) {
					calendar->gameHour->value = 12.0f;
				}
				if (auto* sky = RE::Sky::GetSingleton()) {
					if (auto* clear = RE::TESForm::LookupByID<RE::TESWeather>(0x0000081A)) {
						sky->ForceWeather(clear, true);
					}
				}
				logger::info("skirmish: the player and {} soldiers against {} bandits", allies.size(), foes.size());
			}
			if (Seconds(lastLook) > 1.5f) {
				lastLook = Clock::now();
				// bystanders out (villagers and guards settle it Skyrim's way)
				RE::ProcessLists::GetSingleton()->ForEachHighActor([&](RE::Actor* a_actor) {
					if (a_actor && a_actor != a_player && !a_actor->IsDead() && !a_actor->IsDisabled()) {
						const bool ours = std::any_of(g_scenePeople.begin(), g_scenePeople.end(), [&](const RE::ActorHandle& a_handle) { return a_handle.get().get() == a_actor; });
						if (!ours) {
							a_actor->Disable();
						}
					}
					return RE::BSContainer::ForEachResult::kContinue;
				});
				// whoever stands idle is set on the nearest living opponent (the first bandit on the player)
				const auto nearest = [](RE::Actor* a_from, const std::vector<RE::ActorHandle>& a_others) -> RE::Actor* {
					RE::Actor* found = nullptr;
					float      best = 1e9f;
					for (const auto& handle : a_others) {
						if (auto other = handle.get(); other && !other->IsDead()) {
							if (const float d = other->GetPosition().GetDistance(a_from->GetPosition()); d < best) {
								best = d, found = other.get();
							}
						}
					}
					return found;
				};
				for (std::size_t i = 0; i < foes.size(); ++i) {
					if (auto foe = foes[i].get(); foe && !foe->IsDead() && !foe->IsInCombat()) {
						RE::Actor* target = i == 0 ? a_player : nearest(foe.get(), allies);
						foe->StartCombat(target ? target : a_player);
					}
				}
				for (const auto& handle : allies) {
					if (auto ally = handle.get(); ally && !ally->IsDead() && !ally->IsInCombat()) {
						if (auto* target = nearest(ally.get(), foes)) {
							ally->StartCombat(target);
						}
					}
				}
			}
		}
		}

		// Test scene (dev key crowd): hostile bandits with random weapons around the player, once.
		void DevCrowdScene(RE::PlayerCharacter* a_player)
		{
		if (g_dev.crowd > 0) {
			static bool placed = false;
			if (!placed && Link::Current().live) {
				placed = true;
				RE::TESNPC* base = nullptr;
				auto*       bandits = RE::TESForm::LookupByID<RE::TESFaction>(0x0001BCC0);
				for (auto* npc : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESNPC>()) {
					if (npc && !npc->IsUnique() && !npc->IsFemale() && npc->GetName() && std::string_view(npc->GetName()) == "Bandit" && bandits && npc->IsInFaction(bandits)) {
						base = npc;
						break;
					}
				}
				std::vector<RE::TESObjectWEAP*> pool = Items::All();
				for (auto* weapon : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESObjectWEAP>()) {
					if (weapon && weapon->GetPlayable() && !weapon->formEnchanting && weapon->GetName() && (std::string_view(weapon->GetName()).rfind("Iron ", 0) == 0 || std::string_view(weapon->GetName()).rfind("Steel ", 0) == 0) &&
						weapon->GetWeaponType() != RE::WEAPON_TYPE::kBow && weapon->GetWeaponType() != RE::WEAPON_TYPE::kCrossbow) {
						pool.push_back(weapon);
					}
				}
				int count = 0;
				for (int k = 0; base && k < g_dev.crowd; ++k) {
					const float turn = a_player->GetAngleZ() + 6.2831853f * static_cast<float>(k) / static_cast<float>(g_dev.crowd);
					RE::NiPoint3 at = a_player->GetPosition() + RE::NiPoint3{ std::sin(turn), std::cos(turn), 0.0f } * (350.0f + 60.0f * static_cast<float>(k % 3));
					at.z = GroundUnder(a_player, at, 300.0f) + 10.0f;
					if (auto ref = a_player->PlaceObjectAtMe(base, false)) {
						if (auto* actor = ref->As<RE::Actor>()) {
							actor->SetPosition(at, true);
							if (!pool.empty()) {
								auto* weapon = pool[static_cast<std::size_t>(std::rand()) % pool.size()];
								actor->AddObjectToContainer(weapon, nullptr, 1, nullptr);
								RE::ActorEquipManager::GetSingleton()->EquipObject(actor, weapon);
							}
							actor->AsActorValueOwner()->SetActorValue(RE::ActorValue::kAggression, 2.0f);
							actor->AsActorValueOwner()->SetActorValue(RE::ActorValue::kConfidence, 4.0f);
							actor->StartCombat(a_player);
							++count;
						}
					}
				}
				logger::info("crowd: {} hostile bandits set down around the player", count);
			}
		}
		}

		// Test scene (dev key duel): one opponent at a time in front of the player, replaced when he dies.
		void DevDuelScene(RE::PlayerCharacter* a_player)
		{
		if (g_dev.duel) {
			// one opponent, nobody else: a bandit with a two-handed weapon, replaced a few seconds after he dies
			static std::uint32_t     duelist = 0;
			static Clock::time_point lastLook = Clock::now(), deadSince{};
			static int               fought = 0;
			if (Seconds(lastLook) > 1.0f && Link::Current().live) {
				lastLook = Clock::now();
				RE::ProcessLists::GetSingleton()->ForEachHighActor([&](RE::Actor* a_actor) {
					if (a_actor && a_actor != a_player && a_actor->GetFormID() != duelist && !a_actor->IsDisabled() && !a_actor->IsDead() && a_actor->GetPosition().GetDistance(a_player->GetPosition()) < 6000.0f) {
						a_actor->Disable();
					}
					return RE::BSContainer::ForEachResult::kContinue;
				});
				auto* current = duelist ? RE::TESForm::LookupByID<RE::Actor>(duelist) : nullptr;
				// (the weapon is put in the opponent's hand only once Skyrim has its body loaded: equipped the moment it was
				// placed, Skyrim read empty memory and the game went down - seen in a test run, 2026-10-15)
				static RE::TESBoundObject* toEquip = nullptr;
				static Clock::time_point   loadedAt{};
				if (current && toEquip && current->Is3DLoaded() && loadedAt == Clock::time_point{}) {
					loadedAt = Clock::now();
				}
				// (and two seconds after that: equipped at once when its body first showed, it still crashed - 2026-10-15)
				if (current && toEquip && loadedAt != Clock::time_point{} && Seconds(loadedAt) > 2.0f && !current->IsDead()) {
					loadedAt = {};
					RE::ActorEquipManager::GetSingleton()->EquipObject(current, toEquip);
					toEquip = nullptr;
				}
				if (current && current->IsDead() && deadSince == Clock::time_point{}) {
					deadSince = Clock::now();
					logger::info("duel: opponent {} is dead", fought);
				}
				if (!current || (current->IsDead() && Seconds(deadSince) > 5.0f)) {
					RE::TESNPC* base = nullptr;
					auto*       bandits = RE::TESForm::LookupByID<RE::TESFaction>(0x0001BCC0);
					for (auto* npc : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESNPC>()) {
						if (npc && !npc->IsUnique() && !npc->IsFemale() && npc->GetName() && std::string_view(npc->GetName()) == "Bandit" && bandits && npc->IsInFaction(bandits)) {
							base = npc;
							break;
						}
					}
					if (g_dev.duelUndead) {
						// (duelundead=1: the opponent is a draugr - the first unleveled draugr the game has)
						for (auto* npc : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESNPC>()) {
							const char* raceId = npc && npc->race ? npc->race->GetFormEditorID() : nullptr;
							const char* raceName = npc && npc->race ? npc->race->GetName() : nullptr;
							const std::string race = raceId && raceId[0] ? raceId : (raceName ? raceName : "");
							if (race.find("Draugr") != std::string::npos && npc->GetName() && std::string_view(npc->GetName()) == "Draugr") {
								base = npc;
								break;
							}
						}
					}
					// (a different weapon each time: any plain melee weapon of the game, so every MORDHAU counterpart shows up)
					static std::vector<RE::TESObjectWEAP*> armoury;
					if (armoury.empty()) {
						std::unordered_set<std::string> names;
						for (auto* weapon : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESObjectWEAP>()) {
							if (!weapon || !weapon->GetPlayable() || weapon->formEnchanting || weapon->templateWeapon || weapon->IsBound() || !weapon->GetName() || !*weapon->GetName()) {
								continue;
							}
							const auto type = weapon->GetWeaponType();
							if (type == RE::WEAPON_TYPE::kHandToHandMelee || type == RE::WEAPON_TYPE::kBow || type == RE::WEAPON_TYPE::kCrossbow || type == RE::WEAPON_TYPE::kStaff) {
								continue;
							}
							const std::string_view name = weapon->GetName();
							bool                   plain = false;
							for (const char* material : { "Iron ", "Steel ", "Orcish ", "Dwarven ", "Elven ", "Glass ", "Ebony ", "Daedric ", "Imperial ", "Ancient Nord " }) {
								plain = plain || name.rfind(material, 0) == 0;
							}
							if (plain && names.insert(std::string(name)).second) {
								armoury.push_back(weapon);
							}
						}
					}
					if (!armoury.empty() && armoury.size() < 400) {
						static bool withItems = false;
						if (!withItems) {
							withItems = true;
							// (MORDHAU's own weapons as Skyrim items: three times each, so they are as likely as the rest)
							for (int round = 0; round < 3; ++round) {
								for (auto* item : Items::All()) {
									armoury.push_back(item);
								}
							}
						}
					}
					auto* greatsword = armoury.empty() ? RE::TESForm::LookupByID<RE::TESObjectWEAP>(0x0001359D) : armoury[static_cast<std::size_t>(std::rand()) % armoury.size()];
					if (!g_dev.duelWeapon.empty()) {
						// (duelweapon=<name>|<name>...: these weapons, one after the other)
						std::vector<std::string> wanted;
						for (std::size_t from = 0; from <= g_dev.duelWeapon.size();) {
							const auto bar = g_dev.duelWeapon.find('|', from);
							wanted.push_back(g_dev.duelWeapon.substr(from, bar == std::string::npos ? std::string::npos : bar - from));
							if (bar == std::string::npos) {
								break;
							}
							from = bar + 1;
						}
						static std::size_t turn = 0;
						const auto&        name = wanted[turn++ % wanted.size()];
						for (auto* weapon : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESObjectWEAP>()) {
							if (weapon && weapon->GetPlayable() && !weapon->formEnchanting && !weapon->templateWeapon && weapon->GetName() && name == weapon->GetName()) {
								greatsword = weapon;
								break;
							}
						}
					}
					if (base) {
						// (the one before stays where it fell: bodies are Skyrim's to keep and to clear away, by its own rules -
						// before, each was taken out of the world when the next opponent came. Only one still alive is removed.)
						if (current && !current->IsDead()) {
							current->Disable();
						}
						const float        facing = a_player->GetAngleZ();
						// (the farthest of three spots ahead that the player can see: never behind a wall or inside a house)
						RE::NiPoint3 at = a_player->GetPosition();
						for (const float ahead : { 450.0f, 300.0f, 160.0f }) {
							const RE::NiPoint3 chest = a_player->GetPosition() + RE::NiPoint3{ 0.0f, 0.0f, 90.0f };
							const RE::NiPoint3 there = chest + RE::NiPoint3{ std::sin(facing) * ahead, std::cos(facing) * ahead, 0.0f };
							bool               clear = true;
							if (auto* cell = a_player->GetParentCell(); cell && cell->GetbhkWorld()) {
								auto*           world = cell->GetbhkWorld();
								const float     scale = RE::bhkWorld::GetWorldScale();
								RE::bhkPickData pick;
								pick.rayInput.from = RE::hkVector4(chest.x * scale, chest.y * scale, chest.z * scale, 0.0f);
								pick.rayInput.to = RE::hkVector4(there.x * scale, there.y * scale, there.z * scale, 0.0f);
								RE::CFilter filter;
								a_player->GetCollisionFilterInfo(filter);
								filter.SetCollisionLayer(RE::COL_LAYER::kLOS);
								pick.rayInput.filterInfo = filter;
								RE::BSReadLockGuard lock(world->worldLock);
								world->PickObject(pick);
								clear = !pick.rayOutput.HasHit();
							}
							if (clear) {
								at = there;
								at.z = GroundUnder(a_player, RE::NiPoint3{ there.x, there.y, a_player->GetPosition().z }, 200.0f) + 10.0f;
								break;
							}
						}
						if (auto placed = a_player->PlaceObjectAtMe(base, false)) {
							if (auto* actor = placed->As<RE::Actor>()) {
								actor->SetPosition(at, true);
								if (greatsword) {
									actor->AddObjectToContainer(greatsword, nullptr, 1, nullptr);
									toEquip = greatsword;
								}
								auto* av = actor->AsActorValueOwner();
								av->SetActorValue(RE::ActorValue::kAggression, 2.0f);
								av->SetActorValue(RE::ActorValue::kConfidence, 4.0f);
								if (g_dev.duelHealth > 0.0f) {
									av->SetActorValue(RE::ActorValue::kHealth, g_dev.duelHealth);
								}
								actor->StartCombat(a_player);
								duelist = actor->GetFormID();
								deadSince = {};
								++fought;
								logger::info("duel: opponent {} placed ({} with {}, level {}, health {:.0f})", fought, actor->GetDisplayFullName(), greatsword ? greatsword->GetName() : "his own weapon",
									actor->GetLevel(), av->GetPermanentActorValue(RE::ActorValue::kHealth));
							}
						}
					} else {
						logger::info("duel: no bandit found to place");
					}
				}
			}
		}
		}

		// Test (dev key demon): one Demon Horde Troll is put into Skyrim's world.
		void DevDemonScene(RE::PlayerCharacter* a_player)
		{
		if (g_dev.demon) {
			static Clock::time_point liveSince = Clock::now();
			static Clock::time_point lastReport = Clock::now();
			static bool              tried = false;
			if (!tried && Seconds(liveSince) > 10.0f) {
				tried = true;
				SpawnDemon(a_player);
			}
			if (g_demonId && Seconds(lastReport) > 2.0f) {
				lastReport = Clock::now();
				ReportDemon(a_player);
			}
		}
		}

		// Test scene (dev key battle): Stormcloak and Imperial soldiers fight each other in front of the player.
		void DevBattleScene(RE::PlayerCharacter* a_player)
		{
		if (g_dev.battle > 0 && Link::Current().live) {
			// A staged battle: two lines of soldiers 900 units ahead of the player, set on each other; whoever falls is
			// replaced at his side's end of the field, up to twelve times the starting number. Everyone else about is removed (guards settled it Skyrim's way).
			static Clock::time_point began = Clock::now(), lastLook{};
			static std::vector<RE::ActorHandle> sides[2];
			static std::vector<RE::TESNPC*>     bases[2];
			static int                          placed[2]{};
			static RE::NiPoint3                 ends[2];
			static bool                         ready = false;
			if (!ready && Seconds(began) > 14.0f) {
				ready = true;
				const char* names[2]{ "Stormcloak Soldier", "Imperial Soldier" };
				for (auto* npc : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESNPC>()) {
					for (int k = 0; k < 2; ++k) {
						if (npc && npc->GetName() && std::string_view(npc->GetName()) == names[k] && bases[k].size() < 6 && !npc->IsUnique()) {
							bases[k].push_back(npc);
						}
					}
				}
				const float        facing = a_player->GetAngleZ();
				const RE::NiPoint3 forward{ std::sin(facing), std::cos(facing), 0.0f }, right{ std::cos(facing), -std::sin(facing), 0.0f };
				// (within 700 units of the player: only there do fighters get a MORDHAU stand-in)
				ends[0] = a_player->GetPosition() + forward * 420.0f - right * 300.0f;
				ends[1] = a_player->GetPosition() + forward * 420.0f + right * 300.0f;
				if (auto* calendar = RE::Calendar::GetSingleton(); calendar && calendar->gameHour) {
					calendar->gameHour->value = 11.0f;
				}
				logger::info("battle: {} kinds of Stormcloak soldier, {} kinds of Imperial soldier in the game's data", bases[0].size(), bases[1].size());
			}
			if (ready && Seconds(lastLook) > 1.5f) {
				lastLook = Clock::now();
				RE::ProcessLists::GetSingleton()->ForEachHighActor([&](RE::Actor* a_actor) {
					if (a_actor && a_actor != a_player && !a_actor->IsDead() && !a_actor->IsDisabled()) {
						bool ours = false;
						for (const auto& side : sides) {
							for (const auto& handle : side) {
								ours = ours || handle.get().get() == a_actor;
							}
						}
						if (!ours) {
							a_actor->Disable();
						}
					}
					return RE::BSContainer::ForEachResult::kContinue;
				});
				for (int k = 0; k < 2; ++k) {
					auto& side = sides[k];
					side.erase(std::remove_if(side.begin(), side.end(), [](const RE::ActorHandle& a_handle) {
						auto actor = a_handle.get();
						return !actor || actor->IsDead();
					}), side.end());
					while (static_cast<int>(side.size()) < g_dev.battle && placed[k] < g_dev.battle * 12 && !bases[k].empty()) {
						auto* base = bases[k][static_cast<std::size_t>(placed[k]) % bases[k].size()];
						auto  ref = a_player->PlaceObjectAtMe(base, false);
						auto* actor = ref ? ref->As<RE::Actor>() : nullptr;
						if (!actor) {
							break;
						}
						const float spread = static_cast<float>(placed[k] % g_dev.battle) * 110.0f;
						actor->SetPosition(ends[k] + RE::NiPoint3{ 0.0f, spread - 110.0f, 20.0f }, true);
						side.push_back(actor->GetHandle());
						++placed[k];
						logger::info("battle: {} {} placed ({} of his side so far)", k == 0 ? "Stormcloak" : "Imperial", actor->GetDisplayFullName(), placed[k]);
					}
				}
				// everyone not fighting is set on the nearest living enemy
				for (int k = 0; k < 2; ++k) {
					for (const auto& handle : sides[k]) {
						auto actor = handle.get();
						if (!actor || actor->IsInCombat()) {
							continue;
						}
						RE::Actor* foe = nullptr;
						float      best = 1e9f;
						for (const auto& other : sides[1 - k]) {
							if (auto o = other.get(); o && !o->IsDead()) {
								if (const float d = o->GetPosition().GetDistance(actor->GetPosition()); d < best) {
									best = d, foe = o.get();
								}
							}
						}
						if (foe) {
							actor->StartCombat(foe);
						}
					}
				}
			}
		}
		}

		// Test (dev key npcfight): the two nearest people are set on each other.
		void DevNpcFight(RE::PlayerCharacter* a_player)
		{
		if (g_dev.npcFight) {
			static Clock::time_point lastPush{};
			if (Seconds(lastPush) > 4.0f) {
				lastPush = Clock::now();
				RE::Actor* a = nullptr;
				RE::Actor* b = nullptr;
				float      da = 1e9f, db = 1e9f;
				RE::ProcessLists::GetSingleton()->ForEachHighActor([&](RE::Actor* a_actor) {
					if (a_actor && a_actor != a_player && !a_actor->IsDead() && a_actor->Is3DLoaded() && a_actor->HasKeywordString("ActorTypeNPC")) {
						const float d = a_actor->GetPosition().GetDistance(a_player->GetPosition());
						if (d < da) {
							b = a, db = da;
							a = a_actor, da = d;
						} else if (d < db) {
							b = a_actor, db = d;
						}
					}
					return RE::BSContainer::ForEachResult::kContinue;
				});
				if (a && b && (!a->IsInCombat() || !b->IsInCombat())) {
					a->StartCombat(b);
					b->StartCombat(a);
					logger::info("autotest: set {} and {} on each other", a->GetDisplayFullName(), b->GetDisplayFullName());
				}
			}
		}
		}

		// Test (dev key shieldshow): the player is given the next of five Skyrim shields every few seconds.
		void DevShieldShow(RE::PlayerCharacter* a_player, const Link::State& link)
		{
		if (g_dev.shieldShow && link.live) {
			static Clock::time_point began = Clock::now(), last = Clock::now() - std::chrono::seconds(20);
			static int               next = 0;
			static const char*       shields[]{ "Hide Shield", "Iron Shield", "Whiterun Guard's Shield", "Steel Shield", "Ebony Shield" };
			if (Seconds(began) > 18.0f && Seconds(last) > 14.0f && next < 5) {
				last = Clock::now();
				for (auto* form : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESObjectARMO>()) {
					if (form && form->GetName() && std::string_view(form->GetName()) == shields[next] && form->IsShield()) {
						a_player->AddObjectToContainer(form, nullptr, 1, nullptr);
						RE::ActorEquipManager::GetSingleton()->EquipObject(a_player, form);
						logger::info("shieldshow: {} equipped", shields[next]);
						break;
					}
				}
				++next;
			}
		}
		}

		// Test (dev key unarmedtest): the fists key is pressed every few seconds.
		void DevUnarmedTest(const Link::State& link)
		{
		if (g_dev.unarmedTest && link.live) {
			static Clock::time_point began = Clock::now();
			static int               pressed = 0;
			if (const int due = static_cast<int>((Seconds(began) - 17.0f) / 8.0f); due > pressed) {
				pressed = due;
				g_unarmedAsk.store(true);
				logger::info("unarmedtest: 4 pressed (MORDHAU holds {})", link.weaponClass);
			}
		}
		}

		// Test (dev key throwtest): the next throwable weapon, thrown at the nearest enemy.
		void DevThrowTest(RE::PlayerCharacter* a_player, const Link::State& link)
		{
		if (g_dev.throwTest && link.live) {
			// Every enemy running in gets a weapon thrown at him: the next of four Skyrim weapons whose MORDHAU weapon
			// throws is equipped, the player looks at his chest, and MORDHAU's mode key is pressed while he is still
			// 250 to 700 units off (at arm's length he strikes every second and no throw gets off).
			static Clock::time_point began = Clock::now(), equippedAt{}, thrownAt{};
			static int               next = 0;
			static bool              armed = false;
			const std::uint32_t      weapons[]{ 0x00013983, 0x00013988, 0x0001398A, 0x0001399E };  // Steel War Axe, Steel Mace, Steel Warhammer, Elven Dagger
			if (Seconds(began) > 22.0f) {
				if (!armed && Seconds(thrownAt) > 4.0f) {
					armed = true;
					equippedAt = Clock::now();
					if (auto* weapon = RE::TESForm::LookupByID<RE::TESObjectWEAP>(weapons[next++ % 4])) {
						a_player->AddObjectToContainer(weapon, nullptr, 1, nullptr);
						RE::ActorEquipManager::GetSingleton()->EquipObject(a_player, weapon);
						logger::info("throwtest: {} equipped", weapon->GetName());
					}
					if (!g_dev.duel) {
						a_player->data.angle.z += 0.55f;  // (no enemy to face: the open road, not the tree the arrival point faces)
					}
				}
				RE::Actor* nearest = nullptr;
				float      best = 3000.0f;
				RE::ProcessLists::GetSingleton()->ForEachHighActor([&](RE::Actor* a_actor) {
					if (a_actor && a_actor != a_player && !a_actor->IsDead() && a_actor->Is3DLoaded() && a_actor->IsHostileToActor(a_player)) {
						const float d = a_actor->GetPosition().GetDistance(a_player->GetPosition());
						if (d < best) {
							best = d, nearest = a_actor;
						}
					}
					return RE::BSContainer::ForEachResult::kContinue;
				});
				if (nearest) {
					const RE::NiPoint3 to = nearest->GetPosition() - a_player->GetPosition();
					float              off = std::atan2(to.x, to.y) - a_player->data.angle.z;
					while (off > 3.14159265f) {
						off -= 6.2831853f;
					}
					while (off < -3.14159265f) {
						off += 6.2831853f;
					}
					if (std::abs(off) > 0.03f) {
						a_player->data.angle.z += off * 0.3f;
					}
					// (looking at his chest from the height the throw leaves at)
					a_player->data.angle.x = -std::atan2(to.z + nearest->GetHeight() * 0.6f - a_player->GetHeight() * 0.85f, std::max(std::sqrt(to.x * to.x + to.y * to.y), 1.0f));
				}
				const bool inRange = g_dev.duel ? nearest && best > 250.0f && best < 700.0f : true;
				if (armed && Seconds(equippedAt) > 3.0f && inRange && std::string_view(link.motion).find("Idle") != std::string_view::npos) {
					armed = false;
					thrownAt = Clock::now();
					Link::Request("WeaponMode");
					logger::info("throwtest: mode key pressed ({} {:.0f} units away, MORDHAU holds {})", nearest ? nearest->GetDisplayFullName() : "nobody", nearest ? best : 0.0f, link.weaponClass);
				}
			}
		}
		}

		// Test (dev key shieldtest): the player gets a shield and holds the block in turns.
		void DevShieldTest(RE::PlayerCharacter* a_player, const Link::State& link)
		{
		if (g_dev.shieldTest && link.live) {
			static Clock::time_point began = Clock::now();
			static int               step = 0;
			const float              t = Seconds(began);
			if (step == 0 && t > 20.0f) {
				step = 1;
				if (auto* shield = RE::TESForm::LookupByID<RE::TESObjectARMO>(0x00012EB6)) {  // Iron Shield
					a_player->AddObjectToContainer(shield, nullptr, 1, nullptr);
					RE::ActorEquipManager::GetSingleton()->EquipObject(a_player, shield);
					logger::info("shieldtest: iron shield equipped");
				}
			}
			if (step >= 1) {
				// (the test stands still and keeps facing whoever is nearest and alive: every swing comes from the front)
				RE::Actor* nearest = nullptr;
				float      best = 2500.0f;
				RE::ProcessLists::GetSingleton()->ForEachHighActor([&](RE::Actor* a_actor) {
					if (a_actor && a_actor != a_player && !a_actor->IsDead() && a_actor->Is3DLoaded() && a_actor->IsHostileToActor(a_player)) {
						const float d = a_actor->GetPosition().GetDistance(a_player->GetPosition());
						if (d < best) {
							best = d, nearest = a_actor;
						}
					}
					return RE::BSContainer::ForEachResult::kContinue;
				});
				if (nearest) {
					// (turned the way the mouse would turn it - the angle the camera follows - and only when the enemy has
					// moved off the front: setting the heading alone every frame left the third-person camera behind)
					const RE::NiPoint3 to = nearest->GetPosition() - a_player->GetPosition();
					const float        want = std::atan2(to.x, to.y);
					float              off = want - a_player->data.angle.z;
					while (off > 3.14159265f) {
						off -= 6.2831853f;
					}
					while (off < -3.14159265f) {
						off += 6.2831853f;
					}
					if (std::abs(off) > 0.12f) {
						a_player->data.angle.z += off * 0.2f;
					}
				}
			}
			if (step >= 1 && t > 35.0f) {
				// Three phases of 25 s, over and over: the wall held up, no block at all, and the timed parry (block
				// tapped the moment the enemy's swing is released). The enemy is kept at striking distance (the player
				// is set 90 units in front of it when it hangs back). Every enemy swing and every hit on the player is
				// counted by phase and logged, so the three can be compared.
				const int         phase = static_cast<int>((t - 35.0f) / 25.0f) % 3;
				static int        lastPhase = -1, lastStage = -1, swings[3]{}, hits[3]{};
				static std::uint32_t lastHits = 0;
				static bool       hitsKnown = false;
				static Clock::time_point tapAt{}, lastPull = Clock::now();
				static bool       tapped = false;
				const char*       names[3]{ "wall held", "no block", "timed parry" };
				if (phase != lastPhase) {
					if (lastPhase >= 0) {
						logger::info("shieldtest: phase '{}' over: {} enemy swings in reach, {} hits on the player | totals: wall {}/{}, none {}/{}, parry {}/{}", names[lastPhase], swings[lastPhase],
							hits[lastPhase], hits[0], swings[0], hits[1], swings[1], hits[2], swings[2]);
					}
					lastPhase = phase;
					const bool up = phase == 0;
					if (up != g_shieldBlock.load()) {
						g_shieldBlock.store(up);
						Link::Request(up ? "BlockDown" : "BlockUp");
					}
					logger::info("shieldtest: phase '{}' begins", names[phase]);
				}
				// the nearest fought enemy: kept in reach, its swings counted, the parry timed on its release
				RE::Actor* foe = nullptr;
				int        foeStage = -1;
				for (int k = 0; k < Link::kGhosts; ++k) {
					if (link.ghosts[k].id != 0) {
						if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(link.ghosts[k].id); actor && !actor->IsDead()) {
							foe = actor;
							foeStage = link.ghosts[k].stage;
							break;
						}
					}
				}
				if (foe) {
					const RE::NiPoint3 to = foe->GetPosition() - a_player->GetPosition();
					const float        d = std::sqrt(to.x * to.x + to.y * to.y);
					if (d > 105.0f && d < 1500.0f && Seconds(lastPull) > 1.0f && !a_player->IsInRagdollState()) {
						lastPull = Clock::now();
						RE::NiPoint3 at = foe->GetPosition() - RE::NiPoint3{ to.x / d, to.y / d, 0.0f } * 90.0f;
						at.z = a_player->GetPosition().z;
						a_player->SetPosition(at, true);
					}
					if (foeStage == 1 && lastStage != 1 && d < 130.0f) {
						++swings[phase];
						if (phase == 2) {
							Link::Request("BlockDown");
							g_parryAsked = Clock::now();
							tapAt = Clock::now();
							tapped = true;
						}
					}
					lastStage = foeStage;
				}
				if (tapped && Seconds(tapAt) > 0.2f) {
					tapped = false;
					Link::Request("BlockUp");
				}
				if (!hitsKnown) {
					hitsKnown = true;
					lastHits = link.playerHits;
				} else if (link.playerHits != lastHits) {
					hits[phase] += static_cast<int>(link.playerHits - lastHits);
					lastHits = link.playerHits;
				}
			}
		}
		}

		// Whether Skyrim has the player's body for a scene, and what the player is doing: told to MORDHAU.
		// Dev (arrowtest): arrows are shot at the player to try MORDHAU's parry against them.
		void DevArrowTest(RE::PlayerCharacter* a_player, const Link::State& link)
		{
		if (g_dev.arrowTest && link.live) {
			// (test: from 20 s on an iron arrow every 4 s from 1500 units ahead; the 2nd, 4th... with a parry asked as it
			// is loosed; every 5th comes from behind, with a parry too: that one must not be stopped)
			static Clock::time_point began = Clock::now(), last = Clock::now();
			static int               shots = 0;
			if (Seconds(began) > 20.0f && Seconds(last) > 4.0f && shots < 12) {
				last = Clock::now();
				++shots;
				auto* ammo = RE::TESForm::LookupByID<RE::TESAmmo>(0x0001397D);          // Iron Arrow
				auto* bow = RE::TESForm::LookupByID<RE::TESObjectWEAP>(0x00013985);     // Hunting Bow
				RE::Actor* shooter = nullptr;
				float      best = 4000.0f;
				RE::ProcessLists::GetSingleton()->ForEachHighActor([&](RE::Actor* a_actor) {
					if (a_actor && a_actor != a_player && !a_actor->IsDead() && a_actor->Is3DLoaded()) {
						const float d = a_actor->GetPosition().GetDistance(a_player->GetPosition());
						if (d < best) {
							best = d, shooter = a_actor;
						}
					}
					return RE::BSContainer::ForEachResult::kContinue;
				});
				const bool  behind = shots % 5 == 0;
				const bool  parry = behind || shots % 2 == 0;
				const float facing = a_player->GetAngleZ() + (behind ? 3.14159265f : 0.0f);
				const RE::NiPoint3 ahead{ std::sin(facing), std::cos(facing), 0.0f };
				const RE::NiPoint3 chest = a_player->GetPosition() + RE::NiPoint3{ 0.0f, 0.0f, a_player->GetHeight() * 0.7f };
				const RE::NiPoint3 origin = chest + ahead * 800.0f;  // (far enough for the flight, and its streak, to be seen)
				if (ammo && bow && shooter) {
					RE::Projectile::ProjectileRot angles{ 0.0f, facing + 3.14159265f };  // (level, flying back at the player)
					RE::ProjectileHandle          handle;
					RE::Projectile::LaunchArrow(&handle, shooter, ammo, bow, origin, angles);
					if (parry) {
						StartPlayerParry();
					}
					logger::info("arrowtest: arrow {} loosed by {} from {}, {}", shots, shooter->GetDisplayFullName(), behind ? "behind" : "ahead", parry ? "parry asked" : "no parry");
				} else {
					logger::info("arrowtest: nothing to shoot with (ammo {}, bow {}, shooter {})", ammo != nullptr, bow != nullptr, shooter != nullptr);
				}
			}
		}
		}

		// Dev (demo, kind 2): the demonstration scene's place and people are set up.
		void DevDemoSceneSetup(RE::PlayerCharacter* a_player)
		{
		if (g_dev.demo && g_dev.demoKind == 2) {
			// nobody else about: everyone who lives here (not the bandits the scene makes) is taken out of the world
			static Clock::time_point lastSweep = Clock::now();
			if (Seconds(lastSweep) > 0.5f) {
				lastSweep = Clock::now();
				std::vector<RE::Actor*> away;
				RE::ProcessLists::GetSingleton()->ForEachHighActor([&](RE::Actor* a_actor) {
					if (a_actor && a_actor != a_player && !a_actor->IsDynamicForm() && !a_actor->IsDisabled() && !a_actor->IsDead()) {
						away.push_back(a_actor);
					}
					return RE::BSContainer::ForEachResult::kContinue;
				});
				for (auto* actor : away) {
					actor->Disable();
				}
				static int told = 0;
				if (!away.empty() && told++ < 3) {
					logger::info("demo: {} people and animals of the place taken out of the scene", away.size());
				}
			}
		}
		}

		// Dev (demo): the demonstration scene - the outfit, the opponent, the camera.
		void DevDemoScene(RE::PlayerCharacter* a_player, const Link::State& link)
		{
		if (g_dev.demo && link.live) {
			// 12 s after MORDHAU is linked: the outfit and the weapon (found by their names), then the opponent
			static Clock::time_point began = Clock::now();
			static int               step = 0;
			static RE::ActorHandle   opponent;
			const float              t = Seconds(began);
			const auto               named = [](RE::FormType a_type, std::string_view a_name) -> RE::TESBoundObject* {
				auto* handler = RE::TESDataHandler::GetSingleton();
				if (a_type == RE::FormType::Armor) {
					for (auto* form : handler->GetFormArray<RE::TESObjectARMO>()) {
						if (form && form->GetName() && a_name == form->GetName() && !form->formEnchanting) {
							return form;
						}
					}
				} else {
					for (auto* form : handler->GetFormArray<RE::TESObjectWEAP>()) {
						if (form && form->GetName() && a_name == form->GetName()) {
							return form;
						}
					}
				}
				return nullptr;
			};
			if (step == 0 && t > 12.0f) {
				step = 1;
				// (the cell's arrival point is on a slope at the edge - the first try slid off the mountain: the scene
				// is moved to the flat top, in front of the summit's word wall)
				if (auto* tes = RE::TES::GetSingleton(); tes && g_dev.demoKind == 1) {
					RE::TESObjectREFR* wall = nullptr;
					tes->ForEachReferenceInRange(a_player, 30000.0f, [&](RE::TESObjectREFR* a_ref) {
						auto*       base = a_ref ? a_ref->GetBaseObject() : nullptr;
						const char* name = base ? base->GetName() : nullptr;
						if (name && std::string_view(name) == "Word Wall" && a_ref->Is3DLoaded()) {
							wall = a_ref;
							return RE::BSContainer::ForEachResult::kStop;
						}
						return RE::BSContainer::ForEachResult::kContinue;
					});
					if (wall) {
						const float        facing = wall->GetAngleZ();
						const RE::NiPoint3 out{ std::sin(facing), std::cos(facing), 0.0f };
						RE::NiPoint3       at = wall->GetPosition() + out * BuildValue("demo_from_wall");
						at.z = GroundUnder(a_player, at + RE::NiPoint3{ 0.0f, 0.0f, 400.0f }, 1200.0f) + 10.0f;
						a_player->SetPosition(at, true);
						a_player->SetHeading(facing + 3.14159265f);
						logger::info("demo: the player stands {:.0f} units in front of the summit's word wall", BuildValue("demo_from_wall"));
					} else {
						logger::info("demo: no word wall found near; the scene stays where the cell put the player");
					}
				}
				std::string got;
				for (const char* piece : { "Daedric Armor", "Daedric Boots", "Daedric Gauntlets", "Daedric Helmet" }) {
					if (auto* item = named(RE::FormType::Armor, piece)) {
						a_player->AddObjectToContainer(item, nullptr, 1, nullptr);
						RE::ActorEquipManager::GetSingleton()->EquipObject(a_player, item);
						got += std::string(piece) + ", ";
					}
				}
				if (auto* axe = named(RE::FormType::Weapon, "Wuuthrad")) {
					a_player->AddObjectToContainer(axe, nullptr, 1, nullptr);
					RE::ActorEquipManager::GetSingleton()->EquipObject(a_player, axe);
					got += "Wuuthrad";
				}
				if (auto* calendar = RE::Calendar::GetSingleton(); calendar && calendar->gameHour) {
					calendar->gameHour->value = 10.0f;
				}
				if (auto* sky = RE::Sky::GetSingleton()) {
					if (auto* clear = RE::TESForm::LookupByID<RE::TESWeather>(0x0000081A)) {
						sky->ForceWeather(clear, true);
					}
				}
				// Unrelenting Force, all three words, ready to shout (the user asked for it in the scene)
				if (auto* shout = RE::TESForm::LookupByID<RE::TESShout>(0x00013E07)) {
					a_player->AddShout(shout);
					for (auto& variation : shout->variations) {
						if (variation.word) {
							variation.word->formFlags |= 0x40;   // known
							a_player->UnlockWord(variation.word);
						}
					}
					a_player->GetActorRuntimeData().selectedPower = shout;
					got += ", the shout Unrelenting Force";
				}
				logger::info("demo: the player wears and holds: {}", got);
			} else if (step == 1 && t > 12.0f + BuildValue("demo_opponent_delay")) {
				step = 2;
				RE::TESNPC* base = nullptr;
				if (g_dev.demoKind == 2) {
					// bandits one after another (the duel scene's own: a new one each time one dies, with a different
					// weapon; they can die - only the player cannot)
					g_dev.duel = true;
					step = 3;
					logger::info("demo: bandits come one after another from now on");
				}
				for (auto* npc : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESNPC>()) {
					if (g_dev.demoKind == 2) {
						break;
					}
					if (npc && npc->GetName() && std::string_view(npc->GetName()) == "Ebony Warrior") {
						base = npc;
						break;
					}
				}
				if (g_dev.demoKind == 2) {
					// (nothing more to set down here)
				} else if (base) {
					base->actorData.actorBaseFlags.set(RE::ACTOR_BASE_DATA::Flag::kInvulnerable);  // (he cannot die either)
					const float        facing = a_player->GetAngleZ();
					const RE::NiPoint3 at = a_player->GetPosition() + RE::NiPoint3{ std::sin(facing) * 420.0f, std::cos(facing) * 420.0f, 30.0f };
					if (auto ref = a_player->PlaceObjectAtMe(base, false)) {
						ref->SetPosition(at);
						if (auto* actor = ref->As<RE::Actor>()) {
							opponent = actor->GetHandle();
							actor->StartCombat(a_player);
							logger::info("demo: {} (level {}) set down 420 units ahead and set on the player; he cannot die", actor->GetDisplayFullName(), actor->GetLevel());
						}
					}
				} else {
					logger::info("demo: no character named Ebony Warrior in the game's data (Dragonborn.esm missing?)");
				}
			} else if (step == 2) {
				// (kept whole and angry)
				if (auto actor = opponent.get()) {
					auto*       values = actor->AsActorValueOwner();
					const float full = values->GetPermanentActorValue(RE::ActorValue::kHealth);
					if (values->GetActorValue(RE::ActorValue::kHealth) < full * 0.9f) {
						values->RestoreActorValue(RE::ActorValue::kHealth, full);
					}
					if (!actor->IsInCombat() && !actor->IsDead()) {
						actor->StartCombat(a_player);
					}
					// (stuck or fallen behind - the mountain is steep: after 4 s more than 1000 units away he is set down
					// in front of the player again)
					static Clock::time_point lastNear = Clock::now();
					if (actor->GetPosition().GetDistance(a_player->GetPosition()) < 1000.0f) {
						lastNear = Clock::now();
					} else if (Seconds(lastNear) > 4.0f) {
						lastNear = Clock::now();
						const float  facing = a_player->GetAngleZ();
						RE::NiPoint3 at = a_player->GetPosition() + RE::NiPoint3{ std::sin(facing) * 380.0f, std::cos(facing) * 380.0f, 0.0f };
						at.z = GroundUnder(a_player, at + RE::NiPoint3{ 0.0f, 0.0f, 300.0f }, 900.0f) + 10.0f;
						actor->SetPosition(at, true);
						logger::info("demo: the opponent was far away or stuck: set down in front of the player again");
					}
				}
			}
		}
		}

		// Dev (scenetest): a Skyrim scene is started to see the hand-over to Skyrim's own animation.
		void DevSceneTest(RE::PlayerCharacter* a_player)
		{
		if (g_dev.sceneTest) {
			// (test: at 25 s sit on the nearest seat, at 40 s stand up, from 50 s the controls are away for 8 s)
			static Clock::time_point began = Clock::now();
			static int               step = 0;
			const float              t = Seconds(began);
			if (step == 0 && t > 25.0f) {
				step = 1;
				RE::TESObjectREFR* seat = nullptr;
				float              best = 3000.0f;
				if (auto* cell = a_player->GetParentCell()) {
					cell->ForEachReference([&](RE::TESObjectREFR* a_ref) {
						if (a_ref && a_ref->GetBaseObject() && a_ref->GetBaseObject()->Is(RE::FormType::Furniture) && !a_ref->IsDisabled()) {
							const float d = a_ref->GetPosition().GetDistance(a_player->GetPosition());
							if (d < best) {
								best = d, seat = a_ref;
							}
						}
						return RE::BSContainer::ForEachResult::kContinue;
					});
				}
				logger::info("scenetest: {}", seat ? std::format("sitting down on {} ({:.0f} units away)", seat->GetBaseObject()->GetName(), best) : std::string("no seat in this cell"));
				if (seat) {
					seat->ActivateRef(a_player, 0, nullptr, 0, false);
				}
			} else if (step == 1 && t > 40.0f) {
				step = 2;
				logger::info("scenetest: standing up");
				a_player->NotifyAnimationGraph("IdleForceDefaultState");
				if (auto seat = a_player->GetOccupiedFurniture().get()) {
					seat->ActivateRef(a_player, 0, nullptr, 0, false);
				}
			} else if (step == 2 && t > 50.0f) {
				step = 3;
				logger::info("scenetest: movement and fighting controls taken away");
				RE::ControlMap::GetSingleton()->ToggleControls(RE::UserEvents::USER_EVENT_FLAG::kMovement, false, false);
				RE::ControlMap::GetSingleton()->ToggleControls(RE::UserEvents::USER_EVENT_FLAG::kFighting, false, false);
			} else if (step == 3 && t > 58.0f) {
				step = 4;
				logger::info("scenetest: controls given back");
				RE::ControlMap::GetSingleton()->ToggleControls(RE::UserEvents::USER_EVENT_FLAG::kMovement, true, false);
				RE::ControlMap::GetSingleton()->ToggleControls(RE::UserEvents::USER_EVENT_FLAG::kFighting, true, false);
			}
		}
		}

		// autotest 3: put the player an arm and a blade away from the nearest living character, facing it.
		bool FaceNearestActor(RE::PlayerCharacter* a_player)
		{
			RE::Actor* nearest = nullptr;
			float      best = 4000.0f;
			RE::ProcessLists::GetSingleton()->ForEachHighActor([&](RE::Actor* a_target) {
				if (a_target && a_target != a_player && !a_target->IsDead() && a_target->Is3DLoaded()) {
					const float d = (a_target->GetPosition() - a_player->GetPosition()).Length();
					if (d < best) {
						best = d;
						nearest = a_target;
					}
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
			if (!nearest) {
				return false;
			}
			const float        facing = nearest->GetAngleZ();  // radians, 0 = +Y, clockwise
			const RE::NiPoint3 forward{ std::sin(facing), std::cos(facing), 0.0f };
			a_player->SetPosition(nearest->GetPosition() + forward * 80.0f, true);
			a_player->SetHeading(facing + 3.14159265f);
			logger::info("autotest: facing {} (was {:.0f} units away)", nearest->GetDisplayFullName(), best);
			return true;
		}

		// autotest: once in game -> sword out in third person -> a right strike every few seconds.
		void AutotestTick()
		{
			if (!g_dev.autotest) {
				return;
			}
			auto* ui = RE::UI::GetSingleton();
			auto* player = RE::PlayerCharacter::GetSingleton();
			static Clock::time_point lastReload = Clock::now();
			if (Seconds(lastReload) > 0.5f) {
				lastReload = Clock::now();
				LoadDevConfig(false);
			}
			// every 5 s: what the test is waiting on
			static Clock::time_point lastReport = Clock::now();
			if (g_stage < 3 && Seconds(lastReport) > 5.0f) {
				lastReport = Clock::now();
				logger::info("autotest: stage {} t {:.0f}s mainMenu {} loading {} player3D {}", g_stage, Seconds(g_stageTime),
					ui && ui->IsMenuOpen(RE::MainMenu::MENU_NAME), ui && ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME), player && player->Is3DLoaded());
			}
			// Once in the scene, clear what would swallow the test's input. Test sessions are never saved.
			if (g_stage >= 1 && ui) {
				// the console used to get here
				static bool consoleClosed = false;
				if (!consoleClosed && ui->IsMenuOpen(RE::Console::MENU_NAME)) {
					consoleClosed = true;
					logger::info("autotest: closing the console");
					RE::UIMessageQueue::GetSingleton()->AddMessage(RE::Console::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
				}
				// question boxes the game raises on a fresh start (Survival Mode): answer "No", or the last button
				// (only in the first 45 s of the scene: later boxes are the player's own - it cancelled every level-up)
				static Clock::time_point inScene = Clock::now();
				if (auto* box = RE::MessageBoxMenu::GetCurrentMessageBoxData(); box && Seconds(inScene) < 45.0f) {
					std::uint32_t pick = box->buttonText.empty() ? 0 : box->buttonText.size() - 1;
					for (std::uint32_t i = 0; i < box->buttonText.size(); ++i) {
						if (_stricmp(box->buttonText[i].c_str(), "No") == 0) {
							pick = i;
						}
					}
					logger::info("autotest: question box with {} buttons, answering '{}'", box->buttonText.size(),
						box->buttonText.empty() ? "" : box->buttonText[pick].c_str());
					RE::MessageBoxMenu::SelectOption(static_cast<std::int32_t>(pick));
				}
			}
			switch (g_stage) {
			case 0:
				// The scene is entered from outside (console `coc`, typed by the test driver): starting the load
				// from inside this sink left the game on its loading screen forever.
				if (ui && !ui->IsMenuOpen(RE::MainMenu::MENU_NAME) && !ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) && player && player->Is3DLoaded()) {
					logger::info("autotest: in game");
					g_stage = 1;
					g_stageTime = Clock::now();
				}
				break;
			case 1:
				if (Seconds(g_stageTime) > 4.0f && ui && !ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) && !ui->IsMenuOpen(RE::MainMenu::MENU_NAME) && player && player->Is3DLoaded()) {
					if (auto* sword = RE::TESForm::LookupByID<RE::TESBoundObject>(0x00012EB7)) {  // Iron Sword
						player->AddObjectToContainer(sword, nullptr, 1, nullptr);
						RE::ActorEquipManager::GetSingleton()->EquipObject(player, sword);
					}
					if (auto* camera = RE::PlayerCamera::GetSingleton()) {
						camera->ForceThirdPerson();
					}
					logger::info("autotest: in game, sword equipped, third person");
					g_stage = 2;
					g_stageTime = Clock::now();
				}
				break;
			case 2:
				if (Seconds(g_stageTime) > 2.0f && player) {
					player->DrawWeaponMagicHands(true);
					if (auto* camera = RE::PlayerCamera::GetSingleton(); camera && camera->currentState && camera->currentState->id == RE::CameraState::kThirdPerson) {
						auto* third = static_cast<RE::ThirdPersonState*>(camera->currentState.get());
						third->targetZoomOffset = 0.6f;  // pull back so the whole body is in view
					}
					logger::info("autotest: weapon drawn");
					g_stage = 3;
					g_stageTime = Clock::now();
				}
				break;
			default:
				if ((g_dev.turnTest || g_dev.turnStep > 0.0f) && player) {
					static Clock::time_point last = Clock::now(), said = Clock::now(), stepAt = Clock::now();
					const float              step = std::min(Seconds(last), 0.1f);
					last = Clock::now();
					if (g_dev.turnStep > 0.0f) {
						// a quarter turn in a third of a second, then nothing until the next one
						if (Seconds(stepAt) > g_dev.turnStep) {
							stepAt = Clock::now();
							logger::info("turnstep: quarter turn begins");
						}
						if (Seconds(stepAt) < 0.333f) {
							player->SetHeading(player->GetAngleZ() + 1.5707963f * step / 0.333f);
						}
					} else {
						player->SetHeading(player->GetAngleZ() + g_dev.turnTest / 57.29578f * step);
					}
					if (Seconds(said) > 0.25f) {
						said = Clock::now();
						if (auto* lists = RE::ProcessLists::GetSingleton()) {
							for (auto& handle : lists->highActorHandles) {
								auto actor = handle.get();
								if (actor && actor.get() != player && !actor->IsDead() && actor->IsHostileToActor(player)) {
									const auto to = actor->GetPosition() - player->GetPosition();
									logger::info("turntest: player heading {:.0f}, enemy at world bearing {:.0f}, distance {:.0f}, enemy place ({:.0f}, {:.0f})", player->GetAngleZ() * 57.29578f,
										std::atan2(to.x, to.y) * 57.29578f, std::sqrt(to.x * to.x + to.y * to.y), actor->GetPosition().x, actor->GetPosition().y);
									break;
								}
							}
						}
					}
				}
				if (g_dev.lookAt && player) {
					// (recordings: the camera stays on the fight. Turned a little each frame, so it reads as a player turning)
					RE::Actor* nearest = nullptr;
					float      best = 1500.0f;
					if (auto* lists = RE::ProcessLists::GetSingleton()) {
						for (auto& handle : lists->highActorHandles) {
							auto actor = handle.get();
							if (!actor || actor.get() == player || actor->IsDead() || !actor->IsHostileToActor(player)) {
								continue;
							}
							const float distance = actor->GetPosition().GetDistance(player->GetPosition());
							if (distance < best) {
								best = distance;
								nearest = actor.get();
							}
						}
					}
					if (nearest) {
						const auto to = nearest->GetPosition() - player->GetPosition();
						float      turn = std::atan2(to.x, to.y) - player->GetAngleZ();
						while (turn > 3.14159265f) {
							turn -= 6.2831853f;
						}
						while (turn < -3.14159265f) {
							turn += 6.2831853f;
						}
						player->SetHeading(player->GetAngleZ() + std::clamp(turn, -0.07f, 0.07f));
					}
				}
				if (g_dev.autotest == 4 && player) {
					static RE::ActorHandle enemyHandle;
					static auto            lastState = RE::ATTACK_STATE_ENUM::kNone;
					static int             swings = 0;
					static bool            riposteDue = false;
					auto                   enemy = enemyHandle.get();
					if (!enemy || enemy->IsDead()) {
						// sturdy enough to outlast the test, then pick a fight with whoever is nearest
						player->AsActorValueOwner()->SetBaseActorValue(RE::ActorValue::kHealth, 5000.0f);
						player->AsActorValueOwner()->RestoreActorValue(RE::ActorValue::kHealth, 5000.0f);
						RE::Actor* nearest = nullptr;
						float      best = 4000.0f;
						RE::ProcessLists::GetSingleton()->ForEachHighActor([&](RE::Actor* a_target) {
							if (a_target && a_target != player && !a_target->IsDead() && a_target->Is3DLoaded()) {
								const float d = (a_target->GetPosition() - player->GetPosition()).Length();
								if (d < best) {
									best = d;
									nearest = a_target;
								}
							}
							return RE::BSContainer::ForEachResult::kContinue;
						});
						if (nearest) {
							enemyHandle = nearest->GetHandle();
							nearest->StartCombat(player);
							logger::info("autotest: {} set on the player", nearest->GetDisplayFullName());
						}
					} else {
						// keep facing the attacker so only timing decides the parry
						RE::NiPoint3 to = enemy->GetPosition() - player->GetPosition();
						if (!g_swing) {
							player->SetHeading(std::atan2(to.x, to.y));
						}
						const auto state = enemy->AsActorState()->GetAttackState();
						if (state == RE::ATTACK_STATE_ENUM::kBash && lastState != RE::ATTACK_STATE_ENUM::kBash) {
							logger::warn("autotest: {} started a BASH", enemy->GetDisplayFullName());
						}
						if (state == RE::ATTACK_STATE_ENUM::kSwing && lastState != RE::ATTACK_STATE_ENUM::kSwing) {
							const bool parryThis = (swings++ % 2) == 0;
							logger::info("autotest: {} swings (#{}), {}", enemy->GetDisplayFullName(), swings, parryThis ? "parrying" : "not parrying");
							if (parryThis) {
								riposteDue = StartPlayerParry();
							}
						}
						lastState = state;
						if (riposteDue && g_parry && g_parry->parriedAt >= 0.0f && Seconds(g_parry->start) > g_parry->parriedAt + 0.1f) {
							riposteDue = false;
							g_parry.reset();
							const bool ok = StartPlayerSwing(0.0f);
							logger::info("autotest: riposte start {}", ok ? "ok" : "refused");
						}
					}
				}
				if (g_dev.autotest == 3 && g_stage == 3 && Seconds(g_stageTime) > 2.5f && !g_swing && player) {
					// alternate: at the nearest character, then at empty air (turned away) to see a miss
					static int round = 0;
					if (FaceNearestActor(player) && (round++ % 3) == 2) {
						player->SetHeading(player->GetAngleZ() + 3.14159265f);
						logger::info("autotest: turned away for a miss");
					}
					g_facedAt = Clock::now();
					g_stage = 4;
				}
				if (g_stage == 4 && Seconds(g_facedAt) > 0.6f && !g_swing) {
					const bool ok = StartPlayerSwing(0.0f);
					logger::info("autotest: swing start {}", ok ? "ok" : "refused");
					g_stageTime = Clock::now();
					g_stage = 3;
				}
				if (g_dev.autotest == 1 && Seconds(g_stageTime) > 4.0f && !g_swing) {
					const bool ok = StartPlayerSwing(0.0f);
					logger::info("autotest: swing start {}", ok ? "ok" : "refused");
					g_stageTime = Clock::now();
				}
				break;
			}
		}
	}
}
