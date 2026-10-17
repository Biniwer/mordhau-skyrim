#include "CombatInternal.h"

namespace dm::Combat
{
	inline namespace internal
	{
		float Seconds(Clock::time_point a_since)
		{
			return std::chrono::duration<float>(Clock::now() - a_since).count();
		}

		const char* WeaponTypeName(RE::WEAPON_TYPE a_type)
		{
			switch (a_type) {
			case RE::WEAPON_TYPE::kHandToHandMelee:
				return "kHandToHandMelee";
			case RE::WEAPON_TYPE::kOneHandSword:
				return "kOneHandSword";
			case RE::WEAPON_TYPE::kOneHandDagger:
				return "kOneHandDagger";
			case RE::WEAPON_TYPE::kOneHandAxe:
				return "kOneHandAxe";
			case RE::WEAPON_TYPE::kOneHandMace:
				return "kOneHandMace";
			case RE::WEAPON_TYPE::kTwoHandSword:
				return "kTwoHandSword";
			case RE::WEAPON_TYPE::kTwoHandAxe:
				return "kTwoHandAxe";
			default:
				return "";
			}
		}

		// The sheet row for what the actor holds in the right hand; rows with a keyword win over rows without.
		const sheets::WeaponFamily* FamilyOf(RE::Actor* a_actor)
		{
			const auto* form = a_actor->GetEquippedObject(false);
			const auto* weapon = form ? form->As<RE::TESObjectWEAP>() : nullptr;
			const std::string_view type = weapon ? WeaponTypeName(weapon->GetWeaponType()) : "kHandToHandMelee";
			const sheets::WeaponFamily* plain = nullptr;
			for (const auto& family : sheets::kWeaponFamilies) {
				if (type != family.weaponType) {
					continue;
				}
				if (!*family.keyword) {
					plain = plain ? plain : &family;
				} else if (weapon && weapon->HasKeywordString(family.keyword)) {
					return &family;
				}
			}
			return plain;
		}

		// The MORDHAU weapon that stands for what the actor holds: by the family and words in the Skyrim weapon's name
		// (sheets/weapon_map.json), else the family's own.
		// The MORDHAU shield a Skyrim shield is (sheets/shield_map.json: by words in its name); "" for none.
		const char* MordhauShieldOf(const RE::TESObjectARMO* a_shield)
		{
			if (!a_shield || !a_shield->GetName()) {
				return "";
			}
			std::string name = a_shield->GetName();
			std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			for (const auto& row : sheets::kShieldMap) {
				if (std::string_view(row.armour) == "light" && !a_shield->IsLightArmor()) {
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
				if (match) {
					return row.mordhauShield;
				}
			}
			return "";
		}

		const char* MordhauWeaponFor(RE::Actor* a_actor)
		{
			const auto* family = FamilyOf(a_actor);
			if (!family) {
				return nullptr;
			}
			const auto* form = a_actor->GetEquippedObject(false);
			const auto* weapon = form ? form->As<RE::TESObjectWEAP>() : nullptr;
			if (const char* own = Items::MordhauWeaponOf(form)) {
				return own;  // (a MORDHAU weapon as a Skyrim item: itself)
			}
			if (weapon && weapon->GetName()) {
				std::string name = weapon->GetName();
				std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				for (const auto& row : sheets::kWeaponMap) {
					if (std::string_view(row.family) != family->id) {
						continue;
					}
					std::string words = row.contains;
					std::transform(words.begin(), words.end(), words.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
					if (words.empty()) {
						return row.mordhauWeapon;
					}
					std::size_t from = 0;
					while (from <= words.size()) {
						const auto to = words.find('|', from);
						const auto word = words.substr(from, to == std::string::npos ? std::string::npos : to - from);
						if (!word.empty() && name.find(word) != std::string::npos) {
							return row.mordhauWeapon;
						}
						if (to == std::string::npos) {
							break;
						}
						from = to + 1;
					}
				}
			}
			return family->mordhauWeapon;
		}

		void HudTask(std::function<void(RE::GFxMovieView*)> a_task)
		{
			std::scoped_lock lock(g_hudTaskLock);
			if (g_hudTasks.size() >= 64) {
				g_hudTasks.erase(g_hudTasks.begin());  // (the menu is not advancing - paused: the oldest wish goes)
			}
			g_hudTasks.push_back(std::move(a_task));
		}

		// One of Skyrim's own sounds (by its editor id), at a place in the world.
		void SoundAt(const char* a_id, const RE::NiPoint3& a_at)
		{
			if (auto* audio = RE::BSAudioManager::GetSingleton()) {
				RE::BSSoundHandle handle;
				audio->GetSoundHandleByName(handle, a_id, 0x1A);
				if (handle.IsValid()) {
					handle.SetPosition(a_at);
					handle.Play();
				}
			}
		}

		// The Skyrim item a weapon lying on the ground is: its last holder's equipped weapon, else the MORDHAU item of
		// that model ("w_Zweihander" -> the Zweihander of sheets/mordhau_items.json). Null: none known.
		RE::TESObjectWEAP* GroundWeaponItem(std::uint32_t a_owner, const std::string& a_model, RE::Actor** a_holder)
		{
			RE::Actor* holder = a_owner != 0 && a_owner != 0xFFFFFFFF ? RE::TESForm::LookupByID<RE::Actor>(a_owner) : nullptr;
			if (a_holder) {
				*a_holder = holder;
			}
			if (holder) {
				if (auto* form = holder->GetEquippedObject(false)) {
					if (auto* weapon = form->As<RE::TESObjectWEAP>()) {
						return weapon;
					}
				}
			}
			if (a_model.size() > 2) {
				const std::string name = a_model.substr(2);
				for (auto* item : Items::All()) {
					const char*            path = Items::MordhauWeaponOf(item);
					const std::string_view full = path ? path : "";
					if (full.size() >= name.size() + 3 && full.substr(full.size() - name.size()) == name && full[full.size() - name.size() - 1] == '_') {
						return item;
					}
				}
			}
			return nullptr;
		}

		// Who can have a MORDHAU stand-in: people, and (sheets/builds.json undead_fight) draugr and skeletons - they
		// stand on Skyrim's human skeleton and swing weapons. Never animals, giants, dragons.
		bool Humanoid(RE::Actor* a_actor)
		{
			if (a_actor->HasKeywordString("ActorTypeNPC")) {
				return true;
			}
			if (BuildValue("undead_fight") < 0.5f || !a_actor->HasKeywordString("ActorTypeUndead")) {
				return false;
			}
			const auto* race = a_actor->GetRace();
			const char* id = race ? race->GetFormEditorID() : nullptr;
			std::string name = id && id[0] ? id : (race && race->GetName() ? race->GetName() : "");
			std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return name.find("draugr") != std::string::npos || name.find("skeleton") != std::string::npos;
		}

		// A shield on the player's left arm in Skyrim?
		bool PlayerShield(RE::PlayerCharacter* a_player)
		{
			if (const auto* left = a_player->GetEquippedObject(true)) {
				if (const auto* armour = left->As<RE::TESObjectARMO>(); armour && armour->IsShield()) {
					return true;
				}
			}
			return a_player->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kShield) != nullptr;
		}

		// Is the player in something only Skyrim can show - sitting or lying, on a horse, in a kill move, swimming, or in
		// a scripted scene that took the movement or fighting controls away or plays its own animation on the player?
		// Then Skyrim keeps the body and MORDHAU's inputs wait (sheets/builds.json skyrim_scenes).
		const char* SkyrimScene(RE::PlayerCharacter* a_player)
		{
			auto* state = a_player->AsActorState();
			// (making the character: Skyrim's own body, to be seen and shaped)
			if (auto* ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME)) {
				return "making the character";
			}
			if (a_player->IsOnMount()) {
				return "on a mount";
			}
			// drawing or putting away: Skyrim's own animation of it (sheets/builds.json skyrim_draw_sheathe)
			if (BuildValue("skyrim_draw_sheathe") > 0.5f) {
				const auto weaponState = state->GetWeaponState();
				if (weaponState == RE::WEAPON_STATE::kWantToDraw || weaponState == RE::WEAPON_STATE::kDrawing || weaponState == RE::WEAPON_STATE::kWantToSheathe ||
					weaponState == RE::WEAPON_STATE::kSheathing) {
					return "drawing or putting away a weapon";
				}
			}
			// magic and bows are Skyrim's own: its body, arms and animations, untouched
			if (const auto* right = a_player->GetEquippedObject(false)) {
				const auto* weapon = right->As<RE::TESObjectWEAP>();
				if (!weapon) {
					return "a spell in the hand (magic is Skyrim's own)";
				}
				if (weapon->IsStaff()) {
					return "a staff in the hand (magic is Skyrim's own)";
				}
				if (weapon->IsBow() || weapon->IsCrossbow()) {
					return "a bow in the hand (archery is Skyrim's own)";
				}
			}
			if (state->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal) {
				return "sitting, lying or using furniture";
			}
			if (a_player->IsInKillMove()) {
				return "a kill move";
			}
			// (thrown back by a shout or a heavy blow: Skyrim's own stagger is shown and holds the player for as long - with
			// MORDHAU's pose on the body a draugr's shout showed no effect)
			if (state->actorState2.staggered) {
				return "staggered";
			}
			if (state->IsSwimming()) {
				return "swimming";
			}
			if (auto* map = RE::ControlMap::GetSingleton(); map && (!map->IsMovementControlsEnabled() || !map->IsFightingControlsEnabled())) {
				return "a scene took the controls";
			}
			if (a_player->IsAnimationDriven()) {
				return "a scripted animation";
			}
			return nullptr;
		}

		void MuteSkyrimWeapon(RE::Actor* a_actor)
		{
			for (const bool left : { false, true }) {
				auto* form = a_actor ? a_actor->GetEquippedObject(left) : nullptr;
				auto* weapon = form ? form->As<RE::TESObjectWEAP>() : nullptr;
				if (!weapon || weapon->IsBow() || weapon->IsCrossbow() || weapon->IsStaff() || g_mutedWeapons.contains(weapon->GetFormID())) {
					continue;
				}
				g_mutedWeapons[weapon->GetFormID()] = { weapon->attackSound, weapon->attackSound2D, weapon->attackFailSound, weapon->equipSound, weapon->unequipSound };
				weapon->attackSound = weapon->attackSound2D = weapon->attackFailSound = weapon->equipSound = weapon->unequipSound = nullptr;
				logger::info("sounds: Skyrim's own sounds of {} are off while MORDHAU is linked", weapon->GetName());
			}
		}

		void UnmuteSkyrimWeapons()
		{
			for (const auto& [id, sounds] : g_mutedWeapons) {
				if (auto* weapon = RE::TESForm::LookupByID<RE::TESObjectWEAP>(id)) {
					weapon->attackSound = sounds.attack, weapon->attackSound2D = sounds.attack2D, weapon->attackFailSound = sounds.fail;
					weapon->equipSound = sounds.equip, weapon->unequipSound = sounds.unequip;
				}
			}
			g_mutedWeapons.clear();
		}

		// Side-by-side input only counts while the player is actually in control with a weapon out.
		// A menu that takes the mouse or the keys is up without pausing the game (talking, crafting, the favourites,
		// a message box, the console): the keys are Skyrim's there, all of them.
		bool MenuTakesKeys()
		{
			auto* ui = RE::UI::GetSingleton();
			return ui && (ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME) || ui->IsMenuOpen(RE::Console::MENU_NAME) || ui->IsMenuOpen(RE::CraftingMenu::MENU_NAME) ||
						  ui->IsMenuOpen(RE::FavoritesMenu::MENU_NAME) || ui->IsMenuOpen(RE::MessageBoxMenu::MENU_NAME) || ui->IsMenuOpen(RE::BarterMenu::MENU_NAME) ||
						  ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME) || ui->IsMenuOpen(RE::GiftMenu::MENU_NAME) || ui->IsMenuOpen(RE::LockpickingMenu::MENU_NAME) ||
						  ui->IsMenuOpen(RE::BookMenu::MENU_NAME) || ui->IsMenuOpen(RE::TrainingMenu::MENU_NAME) || ui->IsMenuOpen(RE::SleepWaitMenu::MENU_NAME));
		}

		bool LiveInputAllowed()
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* ui = RE::UI::GetSingleton();
			// (not while a menu that takes the mouse or keys is up without pausing the game - talking to someone, above all:
			// scrolling through the replies swung the weapon - nor while Skyrim itself has fighting switched off)
			auto* controls = RE::ControlMap::GetSingleton();
			const bool menu = MenuTakesKeys();
			return player && ui && !ui->GameIsPaused() && !menu && (!controls || controls->IsFightingControlsEnabled()) && player->Is3DLoaded() && player->AsActorState()->IsWeaponDrawn() &&
				   !player->IsInRagdollState() && LiveMelee(player) && !g_skyrimOwns.load();
		}

		// Side-by-side melee only replaces Skyrim's when the right hand holds a melee weapon (or nothing): bows,
		// crossbows, staves and spells stay Skyrim's.
		bool LiveMelee(RE::PlayerCharacter* a_player)
		{
			if (!Link::Current().live) {
				return false;
			}
			const auto* form = a_player->GetEquippedObject(false);
			if (!form) {
				return true;
			}
			const auto* weapon = form->As<RE::TESObjectWEAP>();
			return weapon && !weapon->IsBow() && !weapon->IsCrossbow() && !weapon->IsStaff();
		}

		// The ground (or floor, bridge, stairs) under a point: a ray straight down through Skyrim's collision world, from
		// a_above units over the point to as far under it; the point's own height when nothing is found.
		float GroundUnder(RE::Actor* a_near, const RE::NiPoint3& a_point, float a_above)
		{
			auto* cell = a_near->GetParentCell();
			auto* world = cell ? cell->GetbhkWorld() : nullptr;
			if (!world) {
				return a_point.z;
			}
			const float     scale = RE::bhkWorld::GetWorldScale();
			RE::bhkPickData pick;
			pick.rayInput.from = RE::hkVector4(a_point.x * scale, a_point.y * scale, (a_point.z + a_above) * scale, 0.0f);
			pick.rayInput.to = RE::hkVector4(a_point.x * scale, a_point.y * scale, (a_point.z - a_above) * scale, 0.0f);
			RE::CFilter filter;
			a_near->GetCollisionFilterInfo(filter);
			filter.SetCollisionLayer(RE::COL_LAYER::kLOS);
			pick.rayInput.filterInfo = filter;
			{
				RE::BSReadLockGuard lock(world->worldLock);
				world->PickObject(pick);
			}
			if (!pick.rayOutput.HasHit()) {
				return a_point.z;
			}
			return a_point.z + a_above - 2.0f * a_above * pick.rayOutput.hitFraction;
		}

		float BuildValue(const char* a_id)
		{
			for (const auto& value : sheets::kBuilds) {
				if (std::string_view(value.id) == a_id) {
					return Settings::Value(a_id, value.value);  // (the settings menu's value, if it has one)
				}
			}
			return 0.0f;
		}
	}

	// The local cache of MORDHAU's models: built here, from the player's own MORDHAU, whenever MORDHAU's files, the
	// player's mercenary or the sheets differ from what it was last built from (a few seconds; nothing otherwise).
	// (the model preparation failed in this start: told to the player once a save is loaded - until then nothing
	// on the screen said why MORDHAU never started)
	static bool g_cacheFailed = false;

	void PrepareCache()
	{
		if (g_dev.cache.empty()) {
			g_dev.cache = mbake::DefaultCache();
		}
		auto paks = mbake::FindMordhauPaks();
		if (std::error_code ec; !g_dev.mordhau.empty() && std::filesystem::is_directory(g_dev.mordhau / "Mordhau" / "Content" / "Paks", ec)) {
			paks = g_dev.mordhau / "Mordhau" / "Content" / "Paks";  // (given by hand)
		}
		if (paks.empty()) {
			logger::warn("cache: MORDHAU was not found through Steam; its characters cannot be drawn");
			return;
		}
		const auto started = Clock::now();
		const auto result = mbake::BuildCache(mbake::JobFromSheets(paks, mbake::MordhauGameIni(), g_dev.cache), false, [](const std::string& a_line) { logger::info("cache: {}", a_line); });
		if (result.upToDate) {
			logger::info("cache: '{}' is up to date (mercenary {})", g_dev.cache.string(), result.mercenary);
		} else {
			logger::info("cache: '{}' {} in {:.1f} s from {} (mercenary {}, {} built, {} failed)", g_dev.cache.string(), result.ok ? "built" : "NOT built", Seconds(started), paks.string(),
				result.mercenary, result.built, result.failed);
			g_cacheFailed = !result.ok;
		}
	}

	void Install()
	{
		LoadDevConfig();
		PrepareCache();
		ResolveHitProcessing();
		DisableBashing();
		if (g_dev.movement) {
			ApplyMovementTable();
			RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&g_speedGuard);
		}
		if (g_dev.character && !g_dev.cache.empty()) {
			Link::SetCache(g_dev.cache);
			CharacterRender::Install(g_dev.cache, g_dev.charOffset, g_dev.charLight, CurrentPose);
		}
		if (g_hitSite) {
			IncomingHitHook::func = SKSE::GetTrampoline().write_call<5>(g_hitSite, IncomingHitHook::thunk);
			logger::info("combat: incoming melee hits pass through the parry check");
		}
		REL::Relocation<std::uintptr_t> cameraVtbl{ RE::VTABLE_ThirdPersonState[0] };
		CameraUpdateHook::func = cameraVtbl.write_vfunc(0x3, CameraUpdateHook::thunk);
		{
			REL::Relocation<std::uintptr_t> hudVtbl{ RE::VTABLE_HUDMenu[0] };
			HudMovieHook::func = hudVtbl.write_vfunc(0x5, HudMovieHook::thunk);  // IMenu::AdvanceMovie
			logger::info("combat: HUD calls run on the HUD's own thread");
		}
		{
			// (Skyrim's own movement step of a non-player character - measured first, sheets/builds.json hybrid_movement_hook)
			REL::Relocation<std::uintptr_t> characterVtbl{ RE::VTABLE_Character[0] };
			MovementDataHook::func = characterVtbl.write_vfunc(0x11A, MovementDataHook::thunk);
			logger::info("combat: characters' movement step hooked (Actor::ModifyMovementData)");
		}
		REL::Relocation<std::uintptr_t> firstPersonVtbl{ RE::VTABLE_FirstPersonState[0] };
		FirstPersonUpdateHook::func = firstPersonVtbl.write_vfunc(0x3, FirstPersonUpdateHook::thunk);
		// ProcessButton is slot 4 up to 1.6.x; 1.7.99 added two functions before it (motion gesture, sixaxis).
		const bool                      shifted = REL::Module::get().version() >= SKSE::RUNTIME_SSE_1_7_99;
		const std::size_t               buttonSlot = shifted ? 0x6 : 0x4;
		REL::Relocation<std::uintptr_t> attackVtbl{ RE::VTABLE_AttackBlockHandler[0] };
		AttackButtonHook::func = attackVtbl.write_vfunc(buttonSlot, AttackButtonHook::thunk);
		logger::info("combat: attack button hook in slot {}", buttonSlot);
		if (auto* input = RE::BSInputDeviceManager::GetSingleton()) {
			input->PrependEventSink<RE::InputEvent*>(&g_inputSink);  // (first in line: a key taken here is blanked for Skyrim's own handlers)
		}
		g_stageTime = Clock::now();
		logger::info("combat: hooks installed");
	}

	void OnGameLoaded()
	{
		if (g_cacheFailed) {
			RE::SendHUDMessage::ShowHUDMessage("MORDHAU x Skyrim: MORDHAU's models could not be prepared - see MordhauSkyrim.log");
		}
		g_swing.reset();
		g_parry.reset();
		g_parryLockUntil = {};
		g_riposteUntil = {};
	}
}
