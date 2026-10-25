// What the files of Combat share among themselves: its types, its state and the prototypes of its functions.
// Not for use outside them - the public interface is Combat.h.
#pragma once

#include "InventoryModels.h"
#include "Items.h"
#include "Combat.h"
#include "Link.h"
#include "Session.h"
#include "Settings.h"
#include "SoundMix.h"
#include "CharacterRender.h"
#include "Clip.h"
#include "Pose.h"
#include "Stats.h"
#include "bake/FromSheets.h"
#include "gen/Sheets.h"
#include "api/TrueDirectionalMovementAPI.h"
#include <unordered_set>
#include <Windows.h>
#include <chrono>
#include <fstream>

namespace dm::Combat
{
	inline namespace internal
	{
		using Clock = std::chrono::steady_clock;

		constexpr float kBlendIn = 0.10f;  // seconds

		// Development settings, read from DirectionalMelee.dev.txt next to SkyrimSE.exe (key=value lines).
		// cache: folder with converted clips. autotest=1: set up a scene and swing without any input;
		// autotest=2: set up the scene only, swings come from real input; autotest=3: stand in front of the
		// nearest character and swing at it; autotest=4: get the nearest character to attack and parry every other swing.
		struct DevConfig
		{
			std::filesystem::path cache;
			std::filesystem::path mordhau;  // MORDHAU's folder when it is not where Steam has it (mordhau=<...\Mordhau>): the installer writes it for a game given by hand
			int                   autotest = 0;
			float                 hold = -1.0f;  // freeze every swing at this fraction of its clip (pictures)
			bool                  twist = true;  // false: leave twist bones to Skyrim (comparison)
			bool                  character = true;     // draw the MORDHAU characters (char=0: off)
			bool                  movement = true;      // MORDHAU movement rules on the player (move=0: off)
			float                 charOffset = 0.0f;    // Skyrim units to the player's right; 0 = on the player (replaces it)
			float                 charLight = 1.0f;     // brightness of the drawn character
			int                   pushMode = 1;         // pushtest=2: only people with a stand-in are thrown, never the player
			bool                  duel = false;         // test scene: everyone else is removed, one bandit with a two-handed weapon fights the player and is replaced when he dies (duel=1)
			float                 duelHealth = 0.0f;    // test / recordings: the duel's opponent has this much health (duelhealth=400)
			std::string           duelName;             // test: the duel's opponents are the first ordinary character of this name (duelname=Vampire)
			std::uint32_t         duelSpell = 0;        // test: the duel's opponents hold this spell in the left hand (duelspell=12FCD: Flames)
			std::string           duelWeapon;           // test / recordings: the duel's opponents carry these, in turn (duelweapon=Steel Greatsword|Steel Battleaxe)
			bool                  give = false;         // dev aid: items named in <cache>/give.txt are put into the inventory (give=1)
			bool                  types = false;        // test: one plain weapon of every Skyrim weapon type in the inventory (types=1)
			bool                  arsenal = false;      // test: one of every plain melee weapon in the inventory, no carry limit (arsenal=1)
			bool                  hybridOff = false;    // test: everyone held at Skyrim's place as before the hybrid movement (hybridoff=1)
			bool                  unarmedTest = false;  // test: 4 is pressed every 8 s from 25 s on (unarmedtest=1)
			bool                  throwSlow = false;    // test: a thrown weapon flies at a tenth of its speed, nearly without gravity, to be looked at (throwtest=2)
			bool                  throwTest = false;    // test: every 14 s the next throwable weapon, thrown at the nearest enemy 5 s later (throwtest=1)
			bool                  shieldShow = false;   // test: every 14 s the player is given the next of five Skyrim shields, one per MORDHAU kind (shieldshow=1)
			bool                  shieldTest = false;   // test: the player gets an iron shield; from 35 s the block is held 6 s of every 10 (shieldtest=1)
			bool                  staminaTest = false;  // test: every 8 s Skyrim restores 40 stamina (staminatest=1)
			int                   demoKind = 1;         // 1 the Ebony Warrior on the Throat of the World; 2 bandits one after another in Whiterun, nobody else about (demo=2)
			bool                  demo = false;         // the demonstration scene: Daedric armour, Wuuthrad, the Ebony Warrior, nobody can die (demo=1)
			bool                  duelUndead = false;   // test: the duel's opponent is a draugr (duelundead=1)
			bool                  arrowTest = false;    // test: arrows are shot at the player from ahead, every other one parried, then one from behind with a parry (arrowtest=1)
			bool                  sceneTest = false;    // test: sit on the nearest seat, stand up, then have the controls taken away for a while (scenetest=1)
			bool                  goreTest = false;     // test: every killing blow counts as one on the head (goretest=1)
			int                   skyrimBodies = -1;    // test: 1 / 0 overrides sheets/builds.json skyrim_bodies (skyrimbodies=1)
			int                   crowd = 0;            // test: this many hostile bandits with random weapons are set down around the player, once (crowd=12)
			bool                  duelPassive = false;  // test: every stand-in only stands to be hit - for counting the player's own hits (duelpassive=1)
			bool                  measure = false;      // the eye view logs head heights and the eye point now and then (measure=1)
			bool                  god = false;          // the player takes no damage (god=1); hits are still judged and logged
			bool                  pushTest = false;     // test: every few seconds the nearest person, then the player, is thrown as a shout would (pushtest=1)
			bool                  demon = false;        // test: one Demon Horde Troll is put into Skyrim's world (demon=1)
			int                   battle = 0;           // test scene: this many Stormcloak and as many Imperial soldiers fight each other in front of the player, the fallen replaced (battle=4)
			bool                  npcFight = false;     // test: set the two nearest people on each other (npcfight=1)
			int                   turnTest = 0;         // test: the player is turned steadily (degrees a second) whatever the enemy does, and where the nearest enemy is in the WORLD is logged twice a second (turntest=25)
			float                 turnStep = 0.0f;      // test: every so many seconds the player is turned 90 degrees within a third of a second, then left alone; the enemy's place is logged as with turntest (turnstep=5)
			int                   skirmish = 0;         // test scene: the player and N-1 soldiers at his side against N bandits, everyone else removed, the fallen not replaced (skirmish=3)
			float                 birdsEye = 0.0f;      // test / recordings: the third-person camera stands this many units above the middle of the skirmish, looking down (birdseye=650)
			bool                  lookAt = false;       // test / recordings: the player keeps turning to face the nearest living enemy (lookat=1)
		};

		// One attack in progress: wind-up, then release (the only part that can hit), then recovery.
		// MORDHAU stretches the clip's wind-up and release parts to the weapon's Windup and Release times.
		struct ActiveSwing
		{
			std::shared_ptr<Clip>       clip;
			const sheets::Swing*        swing = nullptr;
			const sheets::WeaponFamily* family = nullptr;
			Clock::time_point           start;
			float                       windup = 0.5f;      // seconds
			float                       release = 0.45f;    // seconds
			float                       recovery = 0.5f;    // seconds
			float                       windupEnd = 0.25f;  // fraction of the clip
			float                       releaseEnd = 0.5f;  // fraction of the clip
			float                       hitAt = -1.0f;      // swing time of the hit, negative = none yet
			std::string                 clipName;           // cache file stem, for the drawn MORDHAU character
			float                       clipTime = 0.0f;    // where in the clip the swing is this frame
			float                       weight = 0.0f;      // how much of the pose is the swing this frame
			bool                        missCharged = false;
			bool                        releaseLogged = false;
			bool                        logged = false;
			int                         probed = 0;
			std::optional<RE::NiPoint3> prevTip;
			std::optional<RE::NiPoint3> prevMid;
		};

		inline DevConfig                                              g_dev;

		float                                                  CameraValue(std::string_view a_id);  // (sheets/camera.json; defined below)

		// First person as an eye view of the player's own Skyrim body (sheets/camera.json first_person_body): Skyrim's
		// camera is in its third-person state, at the eyes.
		inline std::atomic_bool                                       g_eyeView{ false };
		inline RE::NiPoint3                                           g_eyeFromFeet;      // the eye view's camera against the player's feet, last placed (for Skyrim's own aim)
		inline bool                                                   g_eyeKnown = false;

		inline std::atomic_bool                                       g_eyeHeadHidden{ false };  // the eye view has the player's head hidden right now

		inline std::atomic<float>                                     g_eyeNearWas{ 0.0f };      // Skyrim's own nearest draw distance (0 = not noted yet)

		inline std::vector<RE::ActorHandle>                           g_scenePeople;  // the skirmish scene's fighters (for its camera)

		inline RE::NiPoint3                                           g_sceneSide;    // ... and the direction the camera stands off to

		inline RestPose                                               g_rest;

		inline Stats                                                  g_stats;

		inline std::optional<ActiveSwing>                             g_swing;

		// A parry in progress (sheets/moves.json: parry). It can stop a hit until `window` has passed.
		struct ActiveParry
		{
			std::shared_ptr<Clip> clip;
			Clock::time_point     start;
			float                 window = 0.3f;         // seconds a hit can be stopped
			float                 missRecovery = 0.25f;  // seconds the pose is held after an empty window
			float                 parriedAt = -1.0f;     // parry time of the stopped hit, negative = none
			std::string           clipName;              // cache file stem, for the drawn MORDHAU character
			bool                  missLogged = false;
		};

		inline std::optional<ActiveParry> g_parry;

		inline Clock::time_point          g_parryLockUntil;  // no new parry before this

		inline Clock::time_point          g_riposteUntil;    // an attack started before this is a riposte

		inline std::atomic<bool>          g_staminaFromSave{ false };  // a save was loaded: MORDHAU's character takes the stamina it has

		// heavy attacks (sheets/builds.json heavy_attacks): the attack key held for Skyrim's power-attack delay
		struct HeldAttack
		{
			bool              down = false;   // pressed, not yet sent to MORDHAU
			Clock::time_point at{};
			float             degrees = 0.0f;
		};
		inline HeldAttack        g_heldAttack;
		inline Clock::time_point g_heavyUntil{};  // the player's hits before this are its heavy strike's

		inline std::uintptr_t             g_hitSite = 0;     // the game's own call to its hit processing

		inline std::unordered_map<std::string, std::shared_ptr<Clip>> g_clips;  // null = looked for and missing

		inline RE::NiPoint2                                           g_flick;  // recent mouse movement, decayed

		// autotest
		inline int               g_stage = 0;

		inline Clock::time_point g_stageTime = Clock::now();

		inline Clock::time_point g_facedAt = Clock::now();

		float Seconds(Clock::time_point a_since);

		void LoadDevConfig(bool a_first = true);

		const char* WeaponTypeName(RE::WEAPON_TYPE a_type);

		const sheets::WeaponFamily* FamilyOf(RE::Actor* a_actor);

		const char* MordhauShieldOf(const RE::TESObjectARMO* a_shield);

		const char* MordhauWeaponFor(RE::Actor* a_actor);

		const sheets::Swing* SwingForAngle(float a_degrees);

		std::shared_ptr<Clip> GetClip(const std::string& a_name);

		// Starts the sheet swing for a mouse direction. False = nothing of ours to play, Skyrim's attack goes ahead.
		bool LiveMelee(RE::PlayerCharacter* a_player);

		float BuildValue(const char* a_id);

		// Skyrim's own severed heads, thrown the way MORDHAU throws its: away from the blow and up (MORDHAU's launch:
		// 170 units/s outward, 260 up, tumbling), then left to Skyrim's physics - which gives the same arc.
		struct ThrownHead
		{
			RE::ActorHandle   who;
			RE::NiPoint3      away;
			Clock::time_point when;
			int               pushes;
		};

		inline std::vector<ThrownHead> g_thrownHeads;

		// Heads Skyrim would not take off at the blow itself (a draugr's: it is not counted dead yet in that instant):
		// asked again for a second; only a person's is then replaced by MORDHAU's head model - a draugr has no human head.
		inline std::vector<ThrownHead> g_pendingHeads;

		float Seconds(Clock::time_point a_since);

		void ThrowHeads();

		// Calls into Skyrim's HUD movie (Scaleform) are only made from the HUD menu's own AdvanceMovie: queued here from
		// wherever they arise, run there (HudMovieHook). Scaleform is not thread-safe; calling it from the player's
		// update while Skyrim's job thread updated an enemy's health bar crashed the game.
		inline std::mutex                                             g_hudTaskLock;

		inline std::vector<std::function<void(RE::GFxMovieView*)>>    g_hudTasks;

		void HudTask(std::function<void(RE::GFxMovieView*)> a_task);

		struct HudMovieHook
		{
			static void thunk(RE::IMenu* a_this, float a_interval, std::uint32_t a_currentTime)
			{
				std::vector<std::function<void(RE::GFxMovieView*)>> tasks;
				{
					std::scoped_lock lock(g_hudTaskLock);
					tasks.swap(g_hudTasks);
				}
				if (a_this && a_this->uiMovie) {
					for (auto& task : tasks) {
						task(a_this->uiMovie.get());
					}
				}
				func(a_this, a_interval, a_currentTime);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		void SoundAt(const char* a_id, const RE::NiPoint3& a_at);

		RE::TESObjectWEAP* GroundWeaponItem(std::uint32_t a_owner, const std::string& a_model, RE::Actor** a_holder = nullptr);

		bool Humanoid(RE::Actor* a_actor);

		inline std::atomic<bool> g_skyrimOwns{ false };  // a Skyrim scene has the player (see SkyrimScene)

		inline std::atomic<bool> g_unarmedAsk{ false };  // 4 was pressed: MORDHAU's unarmed slot (sheets/live_input.json unarmed)

		inline std::atomic<bool> g_sprintKey{ false };   // Left Shift is held (MORDHAU's sprint: sheets/builds.json mordhau_sprint)

		inline std::atomic<bool> g_shieldBlock{ false };  // the block button is held with a shield on the arm (MORDHAU's held block)

		bool PlayerShield(RE::PlayerCharacter* a_player);

		const char* SkyrimScene(RE::PlayerCharacter* a_player);

		inline std::string g_lastRelease;  // the player's last attack motion that reached its release
		inline std::uint32_t g_releaseNumber = 0;  // counts the player's releases (attacks, kicks)
		inline std::unordered_map<std::uint32_t, std::uint32_t> g_creatureHitRelease;  // creature -> the release that already hit it (one hit per swing, whichever test counts it first)

		// A weapon MORDHAU's character threw, waiting to be put into the plugin's own flight (see FlyThrownWeapons).
		struct PendingShot
		{
			RE::NiPoint3 at, velocity;
			float        gravity = 0.0f;
			float        damage[4]{}, head[4]{};
			const char*        thrownModel = nullptr;  // its prop (cache/props/w_...)
			RE::TESObjectWEAP* thrownForm = nullptr;   // ...and the Skyrim weapon it is
			RE::TESBoundObject* thrownItem = nullptr;  // ...or whatever Skyrim item it is (a shield too)
			bool               spins = false;
		};

		inline std::vector<PendingShot> g_pendingShots;

		void LoadAiLevel();  // (enemy skill, see ShowAiMenu)

		// Skyrim's own weapon sounds (swing, miss, draw, put away) of whoever fights by MORDHAU's rules: silenced on the
		// weapon itself while MORDHAU is linked - those swings are MORDHAU's, and MORDHAU plays its own sounds. (The
		// Skyrim character behind a stand-in is invisible but still swung audibly.) Put back when the link ends.
		struct WeaponSounds
		{
			RE::BGSSoundDescriptorForm *attack, *attack2D, *fail, *equip, *unequip;
		};

		inline std::unordered_map<std::uint32_t, WeaponSounds> g_mutedWeapons;

		void MuteSkyrimWeapon(RE::Actor* a_actor);

		void UnmuteSkyrimWeapons();

		bool MenuTakesKeys();

		bool LiveInputAllowed();

		bool LiveMelee(RE::PlayerCharacter* a_player);

		bool FaceNearestActor(RE::PlayerCharacter* a_player);  // (test scenes; defined with the test code below)

		float GroundUnder(RE::Actor* a_near, const RE::NiPoint3& a_point, float a_above = 120.0f);

		float BuildValue(const char* a_id);

		inline std::unordered_set<std::uint32_t> g_skyrimHasBody;
		// the player's stamina the plugin itself took since the last frame (a jump, a miss, a parry): not a drain of Skyrim's
		// to pass on to MORDHAU (sheets/builds.json player_stamina_drain)
		inline float g_ownStaminaSpent = 0.0f;  // fought characters whose body is Skyrim's alone right now (yielding, fleeing, kneeling)

		inline Clock::time_point g_parryAsked{};  // when the parry button last went to MORDHAU

		inline std::uint32_t g_seenShots = 0;

		inline bool          g_shotsKnown = false;

		inline const sheets::Swing* g_liveSwing = nullptr;  // the last swing asked of MORDHAU (for the hit's log line and stamina reward)

		bool StartPlayerSwing(float a_degrees);

		void Probe(RE::Actor* a_actor, int a_percent);

		float SegmentDistance(const RE::NiPoint3& p1, const RE::NiPoint3& q1, const RE::NiPoint3& p2, const RE::NiPoint3& q2);

		std::optional<std::pair<RE::NiPoint3, RE::NiPoint3>> Blade(RE::Actor* a_actor);

		RE::Actor* BladeTarget(RE::Actor* a_attacker, ActiveSwing& a_swing, float* a_bladeLength);

		// ---- Skyrim's own hit processing (sheets/hooks.json: hit_resolve) -----------------------------------
		// The game's melee hit-frame handler (id 38627) builds a HitData and hands it to id 38586, which applies it
		// to the victim: damage, blocking, hit reactions, pain voice, the hit event (crime, quests, the enemy health
		// bar) and kill credit. Technique from SkyCraft (chasmlol, MIT), which checked it on 1.7.104.
		using ProcessHitFn = void(RE::Actor*, RE::HitData&);

		inline ProcessHitFn* g_processHit = nullptr;

		void ResolveHitProcessing();

		RE::NiAVObject* TorsoNode(RE::Actor* a_actor);

		float Progress(const char* a_id);
		// a creature (not a person): sheets/builds.json creature_damage_scale, the settings menu's "Damage to creatures"; a person: 1
		float CreatureScale(RE::Actor* a_target);
		// the settings menu's "Super kick" (sheets/builds.json super_kick): the kicked one thrown away from the player with
		// Skyrim's own push (what Unrelenting Force and PushActorAway do), as hard as "Super kick force"; not dragons.
		// True if thrown.
		bool SuperKick(RE::Actor* a_player, RE::Actor* a_target);

		float SneakMult(RE::Actor* a_attacker, RE::Actor* a_target, RE::HitData& a_hit, bool* a_inDamage);
		float CritMult(RE::Actor* a_attacker, RE::Actor* a_target, const RE::HitData& a_hit, float* a_critDamage);
		float HitFraction(RE::Actor* a_attacker, RE::Actor* a_target, float a_mordhauDamage, float a_skyrimDamage);
		void  ScaleArrowHit(RE::Actor* a_victim, RE::HitData& a_hit);  // damage_model 1 for Skyrim's own arrows and bolts
		float PowerAttackMult(RE::Actor* a_attacker, RE::Actor* a_target, float a_base);  // Skyrim's power-attack damage with its perks
		bool  SendHeldAttack(bool a_heavy);  // the held attack key: a normal strike, or a heavy one
		bool  DropSkyrimShield(RE::Actor* a_actor);  // MORDHAU knocked its shield away: the Skyrim shield goes to the ground too

		void ApplyHit(RE::Actor* a_attacker, RE::Actor* a_target, const ActiveSwing& a_swing, float a_scale = 1.0f, float a_mordhauDamage = -1.0f);

		void ProbeArm(RE::Actor* a_actor, const char* a_when);

		void ApplyActiveSwing();

		const sheets::ClipRow* ClipFor(const sheets::WeaponFamily* a_family, std::string_view a_move);

		bool StartPlayerParry();

		void ApplyActiveParry();

		// Every melee hit the game itself delivers passes through here before it is applied (the game's own call to its
		// hit processing, sheets/hooks.json: hit_resolve). A hit on the player inside the parry window, from someone the
		// player is facing, becomes a fully blocked hit: no damage, Skyrim's own block sound and reactions.
		bool IsMirrored(const RE::Actor* a_actor);  // has a stand-in in MORDHAU (defined with the stand-in code below)
		bool IsFought(const RE::Actor* a_actor);    // ...and fights through it: not a creature's stand-in, which only stands to be hit

		struct IncomingHitHook
		{
			static void thunk(RE::Actor* a_victim, RE::HitData& a_hit)
			{
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (a_victim && a_victim != player && IsFought(a_victim)) {
					if (auto other = a_hit.aggressor.get(); other && other.get() != player && IsFought(other.get())) {
						return;  // both have stand-ins: whether this lands is decided in MORDHAU
					}
				}
				if (a_victim && a_victim == player && g_dev.god) {
					a_hit.totalDamage = 0.0f;
					a_hit.physicalDamage = 0.0f;
				}
				if (a_victim && a_victim == player) {
					auto        attacker = a_hit.aggressor.get();
					bool fromBow = false;
					if (auto source = a_hit.sourceRef.get()) {
						fromBow = source->As<RE::Projectile>() != nullptr;
					}
					if (attacker && IsFought(attacker.get()) && !fromBow) {
						// this character has a stand-in in MORDHAU: whether its attack lands is decided there
						static Clock::time_point lastLog{};
						if (Seconds(lastLog) > 1.0f) {
							lastLog = Clock::now();
							logger::info("live: Skyrim's own hit from {} dropped; MORDHAU decides", attacker->GetDisplayFullName());
						}
						return;
					}
					const float t = g_parry ? Seconds(g_parry->start) : -1.0f;
					bool        stopped = false;
					float       angle = 180.0f;
					bool        missile = false;
					RE::NiPoint3 from{};
					if (auto source = a_hit.sourceRef.get()) {
						if (auto* projectile = source->As<RE::Projectile>()) {
							from = projectile->GetProjectileRuntimeData().linearVelocity * -1.0f;
							from.z = 0.0f;
							missile = from.Length() > 1.0f;
						}
					}
					if (missile || attacker) {
						RE::NiPoint3 to = missile ? from : attacker->GetPosition() - player->GetPosition();
						to.z = 0.0f;
						const float len = to.Length();
						const float facing = player->GetAngleZ();
						if (len > 1e-3f) {
							angle = std::acos(std::clamp((to.x * std::sin(facing) + to.y * std::cos(facing)) / len, -1.0f, 1.0f)) * 57.29578f;
						}
					}
					const auto  link = Link::Current();
					const float liveWindow = g_stats.Parry("parry_window") > 0.0f ? g_stats.Parry("parry_window") : 0.325f;
					// (MORDHAU's parry motion as streamed back, or the button pressed a moment ago: the link is a few frames late)
					// (...or a shield held up: that blocks for as long as it is held)
					// (...or MORDHAU's shield wall, however it was raised - its own R key too: sheets/builds.json shield_wall_blocks)
					const bool wallUp = BuildValue("shield_wall_blocks") > 0.5f && link.kind == 2 && link.stage == 0 && std::string_view(link.motion).find("ShieldParry") != std::string_view::npos;
					const bool parrying = (link.kind == 2 && link.motionTime <= liveWindow) || wallUp || Seconds(g_parryAsked) <= g_stats.Parry("live_parry_grace") || (g_shieldBlock.load() && PlayerShield(player));
					if (link.live && parrying && angle <= g_stats.Parry("parry_max_angle")) {
						stopped = true;
						logger::info("parried (MORDHAU's parry, {} at {:.3f}s): {}{} at angle {:.0f}, would have done {:.1f}", link.motion, link.motionTime,
							missile ? "an arrow or bolt from " : "", attacker ? attacker->GetDisplayFullName() : "?", angle, a_hit.totalDamage);
						a_hit.totalDamage = 0.0f;
						a_hit.physicalDamage = 0.0f;
						a_hit.percentBlocked = 1.0f;
						a_hit.stagger = 0.0f;
						a_hit.flags.set(RE::HitData::Flag::kBlocked, RE::HitData::Flag::kBlockWithWeapon);
						if (missile) {
							// (an arrow stopped: heard, as the ones the mashup flies itself are)
							const RE::NiPoint3 at = player->GetPosition() + RE::NiPoint3{ 0.0f, 0.0f, player->GetHeight() * 0.7f };
							SoundAt(g_shieldBlock.load() && PlayerShield(player) ? "WPNBlockShieldLSD" : "WPNBlockBlade1HandVsOtherSD", at);
							SoundAt("WPNImpactArrowBounceSD", at);
						}
					} else if (link.live) {
						logger::info("hit on player while MORDHAU is live: {}{} for {:.1f}, not parried (motion {} kind {} at {:.3f}s, angle {:.0f})",
							missile ? "an arrow or bolt from " : "", attacker ? attacker->GetDisplayFullName() : "?", a_hit.totalDamage, link.motion, link.kind, link.motionTime, angle);
					} else if (g_parry && g_parry->parriedAt < 0.0f && t <= g_parry->window && angle <= g_stats.Parry("parry_max_angle")) {
						stopped = true;
						g_parry->parriedAt = t;
						// the defender pays the attack's stamina drain, less what the parrying weapon absorbs
						const auto* attackerFamily = attacker ? FamilyOf(attacker.get()) : nullptr;
						const auto* ownFamily = FamilyOf(player);
						const float drain = attackerFamily ? g_stats.Get(attackerFamily->id, false, "stamina_drain") : -1.0f;
						float       negation = -1.0f;
						for (const auto& stat : sheets::kAttackStats) {
							if (std::string_view(stat.id) == "block_negation") {
								negation = ownFamily ? g_stats.Raw(std::string(ownFamily->id) + "." + stat.property, stat.fallback) : stat.fallback;
							}
						}
						const float cost = std::max(0.0f, (drain > 0.0f ? drain : 0.0f) - (negation > 0.0f ? negation : 0.0f));
						auto*       values = player->AsActorValueOwner();
						const float before = values->GetActorValue(RE::ActorValue::kStamina);
						if (cost > 0.0f) {
							values->DamageActorValue(RE::ActorValue::kStamina, cost);
							g_ownStaminaSpent += cost;
						}
						const float window = g_stats.Parry("riposte_window");
						g_riposteUntil = Clock::now() + std::chrono::milliseconds(static_cast<int>((window > 0.0f ? window : 0.4f) * 1000.0f));
						g_parryLockUntil = {};
						logger::info("parried: {} at {:.3f}s, angle {:.0f}, would have done {:.1f}; stamina {:.0f} -> {:.0f} (drain {:.0f} - {:.0f} absorbed)",
							attacker ? attacker->GetDisplayFullName() : "?", t, angle, a_hit.totalDamage, before, values->GetActorValue(RE::ActorValue::kStamina),
							drain > 0.0f ? drain : 0.0f, negation > 0.0f ? negation : 0.0f);
						a_hit.totalDamage = 0.0f;
						a_hit.physicalDamage = 0.0f;
						a_hit.percentBlocked = 1.0f;
						a_hit.stagger = 0.0f;
						a_hit.flags.set(RE::HitData::Flag::kBlocked, RE::HitData::Flag::kBlockWithWeapon);
					}
					if (g_dev.autotest && a_hit.flags.any(RE::HitData::Flag::kBash)) {
						logger::warn("hit on player is a BASH");
					}
					if (!stopped && g_dev.autotest) {
						logger::info("hit on player: {} for {:.1f}, not parried (parry {} at {:.3f}s, angle {:.0f})", attacker ? attacker->GetDisplayFullName() : "?",
							a_hit.totalDamage, g_parry ? "active" : "not active", t, angle);
					}
					if (stopped && missile) {
						// (an arrow parried or blocked: not given to Skyrim at all - with no damage and marked blocked, Skyrim still
						// played the player's being hit, the user 2026-10-18. Its block and bounce sounds are played above.)
						// (...and the arrow itself is taken away: Skyrim had already stuck it into the player's body. On the next frame -
						// now Skyrim is still in the middle of that arrow's impact)
						Link::Request("ParrySound");  // (MORDHAU's own block sound of the weapon: Skyrim's above alone was hardly heard)
						if (auto source = a_hit.sourceRef.get(); source && source->As<RE::Projectile>()) {
							const RE::ObjectRefHandle arrow = source->CreateRefHandle();
							SKSE::GetTaskInterface()->AddTask([arrow]() {
								if (auto ref = arrow.get()) {
									const bool shown = ref->Get3D() != nullptr;
									ref->Disable();
									ref->SetDelete(true);
									logger::info("parried: the arrow is taken away ({})", shown ? "it was stuck in the player" : "it had no model left");
								} else {
									logger::info("parried: the arrow was already gone");
								}
							});
						}
						return;
					}
				}
				ScaleArrowHit(a_victim, a_hit);
				func(a_victim, a_hit);
			}
			static inline REL::Relocation<void(RE::Actor*, RE::HitData&)> func;
		};

		void DisableBashing();

		// ---- MORDHAU movement (sheets/movement.json; hooks.json: movement) --------------------------------------------
		// Skyrim still moves characters; MORDHAU's numbers decide how fast. Skyrim keeps a table of run speeds per stance
		// and direction: the humanoid entries get MORDHAU's base speed, its slower back-pedal and its sprint speed, once,
		// when the game's data is loaded (in memory only). Sprint building up over time is the one thing a table cannot
		// hold, so while sprinting the player's SpeedMult scales the sprint entry from base speed up to full.
		struct MovementState
		{
			float             sprintHeld = 0.0f;  // seconds of continuous sprinting
			float             mult = 100.0f;      // SpeedMult last set
			// What Skyrim really delivers at SpeedMult 100, in cm/s, running forward [0] and sprinting [1]. Measured once
			// per session (it is about 87 % of the table for a run, and sprint does not follow the table), then held.
			float             gain[2]{ 270.0f, 735.0f };
			float             gainSum[2]{};
			int               gainSamples[2]{};
			bool              calibrated[2]{};
			float             steady = 0.0f;      // seconds in the same state with the same SpeedMult
			bool              wasSprinting = false;
			bool              wasAirborne = false;
			Clock::time_point last = Clock::now();
			Clock::time_point lastLog = Clock::now();
		};

		inline MovementState g_move;

		void ApplyMovementTable();

		void ApplyMovementRules(float a_measuredSpeed, float a_direction);

		// SpeedMult is written into saves, so it must not be left changed when the game might save: put it back to
		// normal whenever a menu opens (the journal, a loading screen, a sleep or wait prompt ...).
		class SpeedGuard : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (a_event && a_event->opening && g_dev.movement && g_move.mult != 100.0f) {
					if (auto* player = RE::PlayerCharacter::GetSingleton()) {
						player->AsActorValueOwner()->SetActorValue(RE::ActorValue::kSpeedMult, 100.0f);
						g_move.mult = 100.0f;
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		inline SpeedGuard g_speedGuard;

		RE::NiPoint3 LiveToWorld(RE::NiAVObject* a_root, const float* a_cm);

		// ---- stand-ins: nearby Skyrim characters mirrored into MORDHAU, whose combat then decides hits and parries ----
		struct Mirrored
		{
			RE::ActorHandle handle;
			std::uint32_t   id = 0;
			std::uint32_t   attacks = 0;     // rises when the character starts an attack
			bool            wasAttacking = false;
			int             swingStage = -1;  // its stand-in's attack stage last frame (Skyrim's swing sound at each release)
			std::uint32_t   playerHitsSeen = 0;  // MORDHAU's count of hits on the player, last frame (a parry: the swing ended without one)
			Clock::time_point lastAttack{};  // (a draugr whose attack is thrown back starts the next one a frame later: counted at most every 0.4 s)
			std::uint32_t   shieldLossesSeen = 0;  // MORDHAU's count of its shield knocked away, already mirrored
			bool            shieldLossesKnown = false;
			std::uint32_t   powerAttack = 0;   // its attack counter at the last power attack Skyrim's AI began (sheets/builds.json heavy_attacks)
			float           powerMult = 0.0f;  // ...that power attack's own damage multiplier, as Skyrim gives it (0: none given)
			Clock::time_point powerUntil{};    // its hits on the player before this are that power attack's
			std::uint32_t   seenHits = 0;    // hits on it already applied
			bool            hitsKnown = false;
			std::uint32_t   seenOther = 0;   // hits on it by other stand-ins already applied
			RE::NiPoint3    lastPos;         // for its walking speed
			RE::NiPoint3    velocity;        // units per second, smoothed
			bool            posKnown = false;
			bool            dummy = false;   // a creature's: only stands to be hit (sheets/builds.json creature_standin); the creature stays Skyrim's
			float           staminaSet = -1.0f;   // its Skyrim stamina as the mirror left it last frame (-1: not mirrored then)
			float           staminaDrained = 0.0f;  // percent of full Skyrim took off it beyond that, so far (sent to MORDHAU)
			Clock::time_point drainLogged{};
		};

		inline std::vector<Mirrored> g_mirrored;

		inline std::uint32_t         g_seenPlayerHits = 0;

		inline bool                  g_playerHitsKnown = false;

		bool IsMirrored(const RE::Actor* a_actor);

		void ApplyHitOnPlayer(RE::Actor* a_attacker, RE::PlayerCharacter* a_player, float a_mordhauDamage);

		// One of MORDHAU's Demon Horde creatures in Skyrim's world (dev setting demon=1: the Troll, its largest). It needs
		// a body of Skyrim's to walk, be hit and pick its fights: a bandit chief (against nearly everyone) at twice the
		// size. It is drawn as the Troll and its MORDHAU stand-in IS a Troll, so how it attacks and what it hits is
		// MORDHAU's; where it walks and whom it goes for is Skyrim's.
		inline std::uint32_t g_demonId = 0;

		void SpawnDemon(RE::PlayerCharacter* a_player);

		void ReportDemon(RE::PlayerCharacter* a_player);

		// Falling matches in both games. Limp in Skyrim (a shout, a giant) -> MORDHAU's own ragdoll fall on that character
		// (Link::SetDown). Knocked down by MORDHAU itself (stage 7: a kick, a troll's slam) -> Skyrim's own push, gently, so
		// its body goes limp where it stands. A fall that began in Skyrim is not echoed back.
		constexpr int kFallen = 7;

		void PushDown(RE::Actor* a_from, RE::Actor* a_target, float a_force);

		// One body: was it limp in Skyrim lately, and should MORDHAU's fall be mirrored onto it now
		struct FallWatch
		{
			Clock::time_point skyrimLimp{};   // last time Skyrim had it limp
			bool              mordhauWas = false;
		};

		bool FallFromMordhau(FallWatch& a_watch, RE::Actor* a_actor, bool a_mordhauFallen);

		inline std::unordered_map<std::uint32_t, FallWatch> g_falls;

		void CalmBodies();

		void WalkAfterStandIn(Mirrored& m, RE::PlayerCharacter* a_player, const Link::State& a_link, const float kCm, RE::NiPointer<RE::Actor>& actor, Link::Ghost& g);

		void LeaveYieldingBodyToSkyrim(Mirrored& m, RE::NiPointer<RE::Actor>& actor, Link::Ghost& g);

		void ApplyStandInVerdicts(Mirrored& m, RE::PlayerCharacter* a_player, const Link::State& a_link, RE::NiPointer<RE::Actor>& actor);

		void MirrorOneEnemy(Mirrored& m, RE::PlayerCharacter* a_player, const Link::State& a_link, const float kCm, const float facing, std::vector<Link::Ghost>& ghosts);

		void MirrorTick(RE::PlayerCharacter* a_player, const Link::State& a_link);


		void DevPushTest(RE::PlayerCharacter* a_player);

		void DevGiveItems(RE::PlayerCharacter* a_player);

		void DevGiveWeaponTypes(RE::PlayerCharacter* a_player);

		void DevSkirmishScene(RE::PlayerCharacter* a_player);

		void DevCrowdScene(RE::PlayerCharacter* a_player);

		void DevDuelScene(RE::PlayerCharacter* a_player);

		void DevDemonScene(RE::PlayerCharacter* a_player);

		void DevBattleScene(RE::PlayerCharacter* a_player);

		void DevNpcFight(RE::PlayerCharacter* a_player);

		void DevShieldShow(RE::PlayerCharacter* a_player, const Link::State& link);

		void DevUnarmedTest(const Link::State& link);

		void DevThrowTest(RE::PlayerCharacter* a_player, const Link::State& link);

		void DevShieldTest(RE::PlayerCharacter* a_player, const Link::State& link);

		void TrackMordhauShots(RE::PlayerCharacter* a_player);

		// A thrown weapon in flight by MORDHAU's rules.
		struct Flying
		{
			const char*        thrownModel = nullptr;  // its prop
			RE::TESObjectWEAP* thrownForm = nullptr;
			RE::TESBoundObject* thrownItem = nullptr;
			bool               spins = false;
			float             damage[4]{}, head[4]{};
			RE::NiPoint3      at, velocity;
			float             gravity = 0.0f;  // units/s^2
			Clock::time_point born{};
		};


		void FlyThrownWeapons(RE::PlayerCharacter* a_player);

		void LogPlayerHealth(RE::PlayerCharacter* a_player);

		void DevArrowTest(RE::PlayerCharacter* a_player, const Link::State& link);

		void DevDemoSceneSetup(RE::PlayerCharacter* a_player);

		void DevDemoScene(RE::PlayerCharacter* a_player, const Link::State& link);

		void DevSceneTest(RE::PlayerCharacter* a_player);

		void SendPlayerState(RE::PlayerCharacter* a_player, float& forward, float& right, const Link::State& link);

		void SendShield(RE::PlayerCharacter* a_player, const Link::State& link);
		void KeepSkyrimBlock(RE::PlayerCharacter* a_player, const Link::State& link);

		void ShowPickUpPrompt(RE::PlayerCharacter* a_player, const Link::State& link);

		void PlaceDroppedWeapons(RE::PlayerCharacter* a_player, const Link::State& link);

		void MatchEnbBrightness(RE::PlayerCharacter* a_player);

		void SendWalls(RE::PlayerCharacter* a_player, const Link::State& link);

		void SendWeaponWish(RE::PlayerCharacter* a_player, const Link::State& link);

		void UpdateSkyrimHud(RE::PlayerCharacter* a_player, const Link::State& link);

		void SetFightingDistance(const Link::State& link);


		void SyncWeaponDrawn(RE::PlayerCharacter* a_player, const Link::State& link);

		void LiveTick(RE::PlayerCharacter* a_player);

		CharacterRender::Pose CurrentPose();

		bool FaceNearestActor(RE::PlayerCharacter* a_player);

		void AutotestTick();

		// Enemy skill, picked in game (F8): Skyrim's own message box with the five levels of sheets/enemy_ai.json.
		inline int g_aiLevel = 0;  // 0 = not read yet

		inline const char* const kAiNames[5]{ "Peasant", "Recruit", "Soldier", "Veteran", "Duelist" };

		void SetAiLevel(int a_level, bool a_tell);

		void LoadAiLevel();

		class AiLevelPicked : public RE::IMessageBoxCallback
		{
		public:
			void Run(std::uint8_t a_button) override
			{
				if (a_button < 5) {
					SetAiLevel(static_cast<int>(a_button) + 1, true);
				}
			}
		};

		void ShowAiMenu();

		// Called every frame on the main thread, also at the main menu: mouse movement and the autotest.
		class InputSink : public RE::BSTEventSink<RE::InputEvent*>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override
			{
				Items::EnsureCreated();
				g_flick.x *= 0.8f;
				g_flick.y *= 0.8f;
				for (auto* event = a_event ? *a_event : nullptr; event; event = event->next) {
					// Crouching as in MORDHAU, if the settings menu says so ("Crouch": Hold): hold to crouch. Skyrim's sneak is a toggle,
					// so a press is only let through while standing, and letting go is turned into the press that stands up again.
					// (The default is Skyrim's own toggle, untouched - MORDHAU's character follows Skyrim's sneaking either way. A
					// streamer hammered Ctrl as Skyrim players do: every tap crouched and stood up again at once.)
					if (auto* sneak = event->AsButtonEvent(); sneak && Link::Current().live && sneak->QUserEvent() == RE::UserEvents::GetSingleton()->sneak &&
						Settings::Value("crouch_mode", 0.0f) > 0.5f) {
						auto* crouchUi = RE::UI::GetSingleton();
						auto* crouchPlayer = RE::PlayerCharacter::GetSingleton();
						if (crouchUi && crouchPlayer && !crouchUi->GameIsPaused()) {
							const bool sneaking = crouchPlayer->IsSneaking();
							if (sneak->IsDown()) {
								if (sneaking) {
									sneak->userEvent = "";  // (already down: nothing to toggle)
								}
							} else if (sneak->IsUp()) {
								if (sneaking) {
									sneak->GetRuntimeData().value = 1.0f;  // (seen by Skyrim as a fresh press: sneaking ends)
									sneak->GetRuntimeData().heldDownSecs = 0.0f;
								}
							} else {
								sneak->userEvent = "";  // (held: not a new press)
							}
						}
					}
					// F8: the enemy skill menu
					if (auto* f8 = event->AsButtonEvent(); f8 && static_cast<int>(f8->GetDevice()) == 0 && f8->GetIDCode() == 66 && f8->IsDown() && Link::Current().live) {
						if (auto* ui = RE::UI::GetSingleton(); ui && !ui->GameIsPaused()) {
							ShowAiMenu();
						}
					}
					// Left Shift is MORDHAU's sprint, and only MORDHAU's: the key is kept from Skyrim (neither its slow walk nor
					// its own sprint) and MORDHAU's character sprints while it is held - in any direction MORDHAU allows,
					// with MORDHAU's animation and stamina. Skyrim's body follows at MORDHAU's speed as always. (Skyrim's own
					// sprint only goes straight ahead - strafing ended it - and played its own animation over the fighter.)
					if (auto* shift = event->AsButtonEvent(); shift && static_cast<int>(shift->GetDevice()) == 0 && shift->GetIDCode() == 42) {
						if (auto* swimmer = RE::PlayerCharacter::GetSingleton(); Link::Current().live && BuildValue("mordhau_sprint") > 0.5f && !(swimmer && swimmer->AsActorState()->IsSwimming())) {  // (in the water Shift is Skyrim's)
							g_sprintKey.store(shift->Value() > 0.0f);
							shift->userEvent = "";
						} else {
							g_sprintKey.store(false);
							if (Link::Current().live) {
								if (auto* ui = RE::UI::GetSingleton(); ui && !ui->GameIsPaused()) {
									shift->userEvent = RE::UserEvents::GetSingleton()->sprint;
								}
							}
						}
					}
					if (const auto* move = event->AsMouseMoveEvent()) {
						g_flick.x += static_cast<float>(move->mouseInputX);
						g_flick.y += static_cast<float>(move->mouseInputY);
						Link::AddMouse(static_cast<float>(move->mouseInputX), -static_cast<float>(move->mouseInputY));
					} else if (auto* awayButton = event->AsButtonEvent(); awayButton && Link::Current().live && static_cast<int>(awayButton->GetDevice()) == 0 &&
							   (awayButton->GetIDCode() == 19 || awayButton->GetIDCode() == 2) && RE::PlayerCharacter::GetSingleton() && RE::UI::GetSingleton() &&
							   !RE::UI::GetSingleton()->GameIsPaused() && !MenuTakesKeys() && !RE::PlayerCharacter::GetSingleton()->AsActorState()->IsWeaponDrawn()) {
						// the weapon is put away: R is not Skyrim's "ready weapon" while MORDHAU is linked; 1 (MORDHAU's slot 1) brings it out
						awayButton->userEvent = "";
						if (awayButton->GetIDCode() == 2 && awayButton->IsDown()) {
							RE::PlayerCharacter::GetSingleton()->DrawWeaponMagicHands(true);
						}
					} else if (auto* liveButton = event->AsButtonEvent(); liveButton && Link::Current().live && LiveInputAllowed()) {
						// MORDHAU's own voice / emote menu is open: the number keys 1-9 are its choices
						if (Link::Current().menuOpen && static_cast<int>(liveButton->GetDevice()) == 0 && liveButton->GetIDCode() >= 2 && liveButton->GetIDCode() <= 10) {
							liveButton->userEvent = "";
							if (liveButton->IsDown()) {
								Link::Request(std::format("Choose{}", liveButton->GetIDCode() - 1).c_str());
							}
							continue;
						}
						// MORDHAU's default bindings for what Skyrim's attack and block buttons do not cover (sheets/live_input.json)
						for (const auto& bind : sheets::kLiveInputs) {
							if (static_cast<int>(liveButton->GetDevice()) == bind.device && static_cast<int>(liveButton->GetIDCode()) == bind.code) {
								const std::string_view request = bind.request;
								if (request == "Sprint" || request == "AiMenu" || request == "CrouchHold") {
									continue;  // (handled above, for every state of the weapon: not blanked here)
								}
								if (request == "Feint") {
									// (Q is MORDHAU's feint only while an attack of the player's is winding up - there is nothing to
									// feint otherwise - and Skyrim's own quick menu the rest of the time: before, the feint on Q had
									// taken the quick menu's key away altogether)
									const auto now = Link::Current();
									if (!(now.kind == 1 && now.stage == 0)) {
										continue;
									}
								}
								if (request == "CastLeft") {
									// a spell in Skyrim's left hand: the button becomes Skyrim's own left-hand button, and Skyrim casts
									// it (AttackButtonHook lets it through rather than taking it as the parry). No spell there: nothing.
									auto*       player = RE::PlayerCharacter::GetSingleton();
									const auto* left = player ? player->GetEquippedObject(true) : nullptr;
									const bool  spell = left && left->Is(RE::FormType::Spell);
									liveButton->userEvent = spell ? RE::UserEvents::GetSingleton()->leftAttack : "";
									if (liveButton->IsDown()) {
										logger::info("input: cast key - {}", spell ? std::format("Skyrim casts {}", left->GetName()) : std::string("no spell in the left hand"));
									}
									continue;
								}
								if (request == "Unarmed") {
									liveButton->userEvent = "";
									if (liveButton->IsDown()) {
										g_unarmedAsk.store(true);
									}
									continue;
								}
								if (request == "PickUp") {
									// E is MORDHAU's Use only at the player's own weapon lying on the ground; otherwise Skyrim's
									auto*         player = RE::PlayerCharacter::GetSingleton();
									if (!liveButton->IsDown()) {
										continue;
									}
									std::uint32_t owner = 0;
									std::string   model;
									if (!player || !CharacterRender::WeaponOnGroundNear(player->GetPosition(), BuildValue("pick_up_reach"), owner, model)) {
										continue;
									}
									liveButton->userEvent = "";
									if (owner != 0) {
										// somebody else's weapon: it becomes the player's Skyrim item too - in the inventory, with its
										// weight and value, and equipped (MORDHAU's character picks the real thing up below)
										RE::Actor* holder = nullptr;
										if (auto* item = GroundWeaponItem(owner, model, &holder)) {
											if (holder && !holder->IsDead()) {
												holder->RemoveItem(item, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
											}
											player->AddObjectToContainer(item, nullptr, 1, nullptr);
											RE::ActorEquipManager::GetSingleton()->EquipObject(player, item);
											logger::info("pick up: {} is the player's now (weight {:.0f}, value {}){}", item->GetName(), item->weight, item->value,
												holder ? std::format(", taken from {}", holder->GetDisplayFullName()) : std::string());
										}
									}
									// (an enemy's: by the Skyrim character it belonged to; nobody's: a new one of its kind)
									Link::Request(owner == 0 ? "PickUp" : owner == 0xFFFFFFFF ? ("Take" + model.substr(2)).c_str() : std::format("PickUp{}", owner).c_str());
									continue;
								}
								// (ours: Skyrim's own use of it - wheel = camera zoom, Q = favourites - does not happen)
								const bool pressed = liveButton->IsDown();
								liveButton->userEvent = "";
								if (!pressed) {
									continue;
								}
								if (request == "DrawWeapon") {
									continue;  // (1 with the weapon already out: nothing; with it away, see the sheathed keys below)
								}
								if (request == "Holster") {
									// Skyrim puts the weapon away; MORDHAU's character follows with its own holster animation
									if (auto* player = RE::PlayerCharacter::GetSingleton()) {
										player->DrawWeaponMagicHands(false);
									}
									continue;
								}
								if (request == "ToggleView") {
									// Skyrim's own camera switch (its F key is the kick while a melee weapon is out)
									if (auto* camera = RE::PlayerCamera::GetSingleton()) {
										if (CameraValue("first_person_body") > 0.5f && g_dev.character) {
											g_eyeView.store(!g_eyeView.load());
											if (camera->IsInFirstPerson()) {
												camera->ForceThirdPerson();
											}
											logger::info("camera: eye view {}", g_eyeView.load() ? "on" : "off");
										} else if (camera->IsInFirstPerson()) {
											camera->ForceThirdPerson();
										} else {
											camera->ForceFirstPerson();
										}
									}
									continue;
								}
								Link::Request(bind.request);
							}
						}
					} else if (const auto* button = event->AsButtonEvent(); button && g_dev.autotest && button->IsDown()) {
						logger::info("input: button down device {} id {} event '{}'", static_cast<int>(button->GetDevice()), button->GetIDCode(), button->QUserEvent().c_str());
					}
				}
				AutotestTick();
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		inline InputSink g_inputSink;

		float CameraValue(std::string_view a_id);

		void ApplyLiveFov();

		void ApplyLiveCamera(RE::ThirdPersonState* a_state);
		// Skyrim's own movement step of a character (Actor::ModifyMovementData): what it is about to move it by, before
		// collision and gravity (CombatStandIns.cpp)
		void OnMovementData(RE::Actor* a_actor, float a_delta, RE::NiPoint3& a_translation, RE::NiPoint3& a_rotation);
		struct MovementDataHook
		{
			static void thunk(RE::Actor* a_this, float a_delta, RE::NiPoint3& a_translation, RE::NiPoint3& a_rotation)
			{
				func(a_this, a_delta, a_translation, a_rotation);
				OnMovementData(a_this, a_delta, a_translation, a_rotation);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		void ApplyEyeCamera(RE::ThirdPersonState* a_state);

		struct CameraUpdateHook
		{
			static void thunk(RE::ThirdPersonState* a_this, RE::BSTSmartPointer<RE::TESCameraState>& a_next)
			{
				func(a_this, a_next);
				ApplyLiveCamera(a_this);
				ApplyActiveSwing();
				ApplyActiveParry();
				CharacterRender::PlaceEnemyWeapons();
				ApplyEyeCamera(a_this);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		void ApplyLiveEye();

		struct FirstPersonUpdateHook
		{
			static void thunk(RE::FirstPersonState* a_this, RE::BSTSmartPointer<RE::TESCameraState>& a_next)
			{
				func(a_this, a_next);
				ApplyLiveFov();
				ApplyLiveEye();
				CharacterRender::PlaceEnemyWeapons();
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		// Attack button: ours when the sheets have a clip for it, Skyrim's otherwise.
		struct AttackButtonHook
		{
			static void thunk(RE::AttackBlockHandler* a_this, RE::ButtonEvent* a_event, RE::PlayerControlsData* a_data)
			{
				const auto* events = RE::UserEvents::GetSingleton();
				if (g_dev.autotest && a_event && a_event->IsDown()) {
					logger::info("input: attack handler got '{}'", a_event->QUserEvent().c_str());
				}
				if (a_event && events && a_event->QUserEvent() == events->leftAttack && a_event->GetDevice() == RE::INPUT_DEVICE::kMouse && a_event->GetIDCode() == 2) {
					func(a_this, a_event, a_data);  // (the cast key, sheets/live_input.json cast_left: Skyrim's own left hand casts)
					return;
				}
				if (a_event && events && a_event->QUserEvent() == events->leftAttack) {
					// the block button: ours when the sheets have a parry clip for the weapon, Skyrim's block otherwise
					if (g_parry || g_swing) {
						return;
					}
					if (auto* bearer = RE::PlayerCharacter::GetSingleton(); bearer && Link::Current().live && bearer->AsActorState()->IsWeaponDrawn() && LiveMelee(bearer) && PlayerShield(bearer)) {
						// a shield: MORDHAU's held block for as long as the button is down
						if (a_event->IsDown()) {
							g_shieldBlock.store(true);
							g_parryAsked = Clock::now();
							Link::Request("BlockDown");
						} else if (a_event->IsUp()) {
							g_shieldBlock.store(false);
							Link::Request("BlockUp");
						}
						return;
					}
					if (a_event->IsDown()) {
						const bool ours = StartPlayerParry();
						if (g_dev.autotest) {
							logger::info("input: block pressed, {}", ours ? "our parry" : "Skyrim's block");
						}
						if (ours) {
							return;
						}
					}
				}
				if (a_event && events && a_event->QUserEvent() == events->rightAttack) {
					if (g_swing) {
						return;  // one swing at a time for now
					}
					if (g_parry) {
						if (g_parry->parriedAt < 0.0f) {
							return;  // parry still open or recovering: no attack
						}
						g_parry.reset();  // a hit was stopped: the attack is the riposte
					}
					// heavy attacks: in MORDHAU's live fight the press is held back - let go before Skyrim's power-attack delay, a
					// normal strike; held that long, a heavy one (sheets/builds.json heavy_attacks)
					if (auto* holder = RE::PlayerCharacter::GetSingleton(); BuildValue("heavy_attacks") > 0.5f && holder && Link::Current().live && holder->AsActorState()->IsWeaponDrawn() &&
																			 LiveMelee(holder)) {
						if (a_event->IsDown()) {
							const bool moving = std::abs(g_flick.x) + std::abs(g_flick.y) > 2.0f;
							g_heldAttack = { true, Clock::now(), moving ? std::atan2(-g_flick.y, g_flick.x) * 57.29578f : 0.0f };
						} else if (g_heldAttack.down) {
							auto*       setting = RE::GameSettingCollection::GetSingleton()->GetSetting("fInitialPowerAttackDelay");
							const float delay = setting ? setting->GetFloat() : 0.3f;
							if (a_event->IsUp()) {
								SendHeldAttack(false);
							} else if (a_event->HeldDuration() >= delay) {
								SendHeldAttack(true);
							}
						}
						return;
					}
					if (a_event->IsDown()) {
						// screen y grows downward; the sheet's angles have up = 90
						const bool  moving = std::abs(g_flick.x) + std::abs(g_flick.y) > 2.0f;
						const float degrees = moving ? std::atan2(-g_flick.y, g_flick.x) * 57.29578f : 0.0f;
						const bool ours = StartPlayerSwing(degrees);
						if (g_dev.autotest) {
							logger::info("input: attack pressed, flick ({:.0f},{:.0f}) -> {:.0f} deg, {}", g_flick.x, g_flick.y, degrees, ours ? "our swing" : "Skyrim's attack");
						}
						if (ours) {
							return;
						}
					}
				}
				func(a_this, a_event, a_data);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};
	}
}
