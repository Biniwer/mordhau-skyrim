#pragma once

// The side-by-side link: MORDHAU runs next to Skyrim (local offline match, mordhau_side/PoseStream) and is the
// character; Skyrim is the world. Through two small files in the cache folder MORDHAU gives its character's pose,
// attack stage, blade and stamina, and Skyrim gives what the player is doing.
namespace dm::Link
{
	inline constexpr int kGhosts = 24;  // how many enemies fight by MORDHAU's rules at once (sheets/live_link.json stand_ins; the script's MAX_GHOSTS)

	struct State
	{
		bool          live = false;       // MORDHAU is streaming right now
		std::uint32_t sequence = 0;
		float         clock = 0.0f;  // MORDHAU's own game time of this report, seconds
		float         bladeStart[3]{};    // MORDHAU character space, centimetres
		float         bladeEnd[3]{};
		int           stage = -1;         // -1 none, 0 windup, 1 release, 2 recovery
		int           kind = 0;           // 0 other, 1 attack, 2 parry
		float         motionTime = 0.0f;  // seconds in the current motion
		float         stamina = 0.0f;     // MORDHAU's own, 0..100
		float         health = 0.0f;
		float         speedRight = 0.0f;  // cm/s, MORDHAU's character
		float         speedForward = 0.0f;
		char          motion[48]{};
		float         weaponRot[4]{ 0, 0, 0, 1 };  // the held weapon in MORDHAU character space: rotation x,y,z,w
		float         weaponPos[3]{};
		bool          hasWeapon = false;          // in the hand
		// hybrid movement: where each stand-in stands in MORDHAU, for its Skyrim character to follow (sheets/live_link.json standin_move)
		struct GhostMove
		{
			std::uint32_t id = 0;
			float         right = 0.0f, forward = 0.0f;  // where it stands in SKYRIM's world: x and y, cm
			float         yaw = 0.0f;                    // its heading in Skyrim's world, degrees
			int           free = 0;                      // MORDHAU's AI moves it
		};
		GhostMove     moves[kGhosts];
		struct GhostStamina
		{
			std::uint32_t id = 0;
			float         stamina = -1.0f;  // its stand-in's stamina in MORDHAU, of 100 (-1: not known)
		};
		GhostStamina  standinStamina[kGhosts];
		bool          altMode = false;            // the weapon in MORDHAU's hand is in its alternate mode (its other grip)
		int           shieldHas = 0;              // MORDHAU's character carries a shield
		float         shield[7]{};                // ...there, in character space (rotation xyzw, position cm)
		char          weaponClass[32]{};          // MORDHAU class of the weapon the character holds or last held
		bool          menuOpen = false;           // MORDHAU's own voice / emote menu is open
		int           weaponState = 0;            // 0 none, 1 in the hand, 2 lying in the world (dropped, disarmed), 3 put away on the character
		// Stand-ins (see SetGhosts): what MORDHAU's own combat decided
		std::uint32_t playerHits = 0;       // times the player was hit by a stand-in
		float         playerDamage = 0.0f;  // MORDHAU's damage of the last one (of 100 health)
		std::uint32_t playerAttacker = 0;   // id of the stand-in that did it
		struct GhostState
		{
			std::uint32_t id = 0;
			std::uint32_t hits = 0;       // times the player hit it
			float         damage = 0.0f;  // MORDHAU's damage of the last hit
			int           stage = -1;     // its own attack: 0 windup, 1 release, 2 recovery
			std::uint32_t otherHits = 0;      // times another stand-in hit it (two Skyrim characters fighting each other)
			float         otherDamage = 0.0f;
			std::uint32_t otherBy = 0;        // id of that stand-in
		};
		GhostState ghosts[kGhosts];
		// MORDHAU loosed an arrow (see sheets/live_link.json 'shot'): counter, direction in its character space (right, forward,
		// up), speed cm/s, gravity scale, damage by armour tier (none, light, medium, heavy) and its head bonus
		std::uint32_t shots = 0;
		float         shotDir[3]{ 0, 1, 0 };
		float         shotSpeed = 0.0f, shotGravity = 1.0f;
		float         shotDamage[4]{}, shotHead[4]{};
		// MORDHAU's camera in its character space (rotation x,y,z,w; position cm) and whether it is in first person
		float cameraRot[4]{ 0, 0, 0, 1 };
		float cameraPos[3]{};
		bool  firstPerson = false;
		float cameraFov = 0.0f;  // the field of view MORDHAU renders with (its camera's own value); 0 = not known
	};

	void  SetCache(const std::filesystem::path& a_cache);
	// Reads the stream once per frame. a_pose (bones x 7 floats: rotation xyzw, translation) is replaced when a new
	// pose arrived. Returns whether MORDHAU is live.
	bool  Poll(std::size_t a_bones, std::vector<float>& a_pose);
	State Current();

	// What the player is doing in Skyrim. Continuous values every frame; actions once each (MORDHAU's own input names:
	// RightStrike, LeftUpperStrike, RightStab, Parry, Feint, Kick, Jump ...).
	void SetMovement(float a_forward, float a_right, bool a_sprint, float a_pitch, bool a_firstPerson);
	void Request(const char* a_action);
	// Sneaking in Skyrim: MORDHAU's character crouches.
	void SetCrouch(bool a_crouch);
	// Enemy skill level 1..5 (sheets/enemy_ai.json skill_levels).
	void SetAiLevel(int a_level);
	void SetHybrid(bool a_on, float a_engageCm);
	void SetPaused(bool a_paused);  // Skyrim's game is paused (a menu): MORDHAU's fighters hold still too
	void SetPlayerPlace(float a_xCm, float a_yCm, float a_facingDegrees);  // where the player is in Skyrim's world (for free stand-ins)  // sheets/builds.json hybrid_*
	void SetShield(const char* a_mordhauShield);  // the MORDHAU shield for the one on the player's arm ("" = none)     // the player carries a shield in Skyrim
	void AddStamina(float a_percent);  // Skyrim restored stamina (a potion): MORDHAU's character gains it too
	void SetPlayerSlow(float a_move);    // Skyrim's slows on the player, as a share of its normal speed: MORDHAU's character moves as slowly
	void DrainStamina(float a_percent);  // Skyrim took stamina (a frost spell, a poison, any mod's effect): MORDHAU's character loses it too
	// Distances (cm) to something solid in Skyrim 45 degrees left, ahead and 45 degrees right of the player; 0 = nothing near.
	void SetWalls(int a_left, int a_ahead, int a_right);
	// The MORDHAU weapon for what the player has equipped in Skyrim ("" = none: keep what MORDHAU's character has).
	void SetPlayerWeapon(const char* a_path);
	// The player's Skyrim health, percent of full: MORDHAU's character is kept at the same level.
	void SetHealth(float a_percent);
	// Nearby Skyrim characters MORDHAU should stand in for: place relative to the player (cm, degrees), a counter that
	// rises each time the character starts an attack, and whether it is blocking.
	struct Ghost
	{
		bool          passive = false;  // the stand-in only stands there to be hit (an archer: Skyrim's AI does the shooting)
		bool          dummy = false;    // ...and stands in for a creature: no body of its own in the player's way (sheets/builds.json creature_standin)
		int           skill10 = 0;      // its skill, tenths of the five levels (10 Peasant .. 50 Duelist): by the level gap (sheets/enemy_ai.json level_skill); 0 = the chosen level
		std::uint32_t id = 0;
		float         right = 0.0f, forward = 0.0f, up = 0.0f, yaw = 0.0f;
		std::uint32_t attacks = 0;
		bool          blocking = false;
		float         velRight = 0.0f, velForward = 0.0f;  // how it is moving, cm/s, in the player's frame
		float         health = 100.0f;  // its Skyrim health, percent of full
		float         size = 1.0f;      // its body's size against the player's (sheets/builds.json standin_scale)
		bool          refused = false;  // hybrid movement: Skyrim refused its stand-in's step this frame (a wall, a ledge)
		bool          hasWall = false;  // a wall near its walked body (sheets/builds.json standin_wall_reach): its normal in the player's frame, distance, margin
		float         wallRight = 0.0f, wallForward = 0.0f, wallCm = 0.0f, wallMarginCm = 0.0f;
		std::uint32_t target = 0;   // whom it fights in Skyrim, when that is not the player (0: the player)
		const char*   shield = "";  // MORDHAU shield class for the shield it wears in Skyrim ("" = none)
		const char*   weapon = "";  // MORDHAU weapon for its moves and reach (sheets/weapon_families.json: mordhau_weapon), by what it holds in Skyrim
		float         staminaDrained = 0.0f;  // percent of its full stamina Skyrim has taken so far (a frost spell, a poison, any mod's effect): its stand-in loses the same
		float         moveScale = 1.0f;       // how fast it may move against its normal speed in Skyrim (a frost spell's slow): its stand-in's movement too
		float         timeScale = 1.0f;       // Skyrim's world time (Slow Time): its stand-in's whole pace, attacks included
	};
	void SetGhosts(const std::vector<Ghost>& a_ghosts);
	// Who is limp in Skyrim right now (thrown by a shout, knocked down): the player and/or stand-ins by id. MORDHAU's own
	// ragdoll fall is started on them.
	void SetDown(bool a_player, const std::vector<std::uint32_t>& a_ids);

	// What a stand-in looks like right now in MORDHAU (slot = index into State::ghosts): which baked look it is drawn
	// with, its bones (x 7 floats, local; a rotation of all zeros = not streamed, use the look's own rest pose), and its
	// weapon in its own character space.
	struct GhostPose
	{
		int                look = 0;  // 1.. ; 0 = nothing to draw
		std::vector<float> bones;
		float              weaponRot[4]{ 0, 0, 0, 1 };
		float              weaponPos[3]{};
		bool               hasWeapon = false;
		int                weaponState = 0;  // as State::weaponState
	};
	bool GhostPoseAt(int a_slot, GhostPose& a_out);  // (shown standin_interp_seconds in the past and blended, when that is set)
	bool GhostPoseAtNewest(int a_slot, GhostPose& a_out);
	// Where a stand-in stands, shown the same moment in the past as its pose; false when that is not set or not known.
	bool GhostMoveAt(std::uint32_t a_id, State::GhostMove& a_out);
	void SetInterpolation(float a_seconds);  // sheets/builds.json standin_interp_seconds; 0 = the newest report
	// Raw mouse movement since the last call (right and up positive): MORDHAU's own attack-angle input reads it.
	void AddMouse(float a_right, float a_up);
	void Flush();
}
