#pragma once

#include <string>
#include <unordered_map>

namespace dm::CharacterRender
{
	// What the drawn character should be doing this frame.
	struct Pose
	{
		std::string action;                // full-skeleton clip of an attack or parry in progress (cache file stem), "" = none
		float       actionTime = 0.0f;     // seconds into that clip
		float       actionWeight = 0.0f;   // 0..1, how much the action replaces the upper body
		float       moveDirection = 0.0f;  // degrees: 0 = forward, 90 = to the character's right, 180 = backward
		float       moveSpeed = 0.0f;      // centimetres per second over the ground
		bool        airborne = false;
	};

	// Draws a MORDHAU character (models, weights and textures read from the player's own MORDHAU; packed into the
	// cache folder at the first start) inside Skyrim's scene, on MORDHAU's own skeleton.
	// a_cache: folder holding character/character.dmchar, its .dds colour maps and the .dmfull clips.
	// a_sideOffset: Skyrim units to the player's right to stand the character at (0 = on the player).
	// a_pose: asked every frame.
	// Moves each mirrored enemy's own Skyrim weapon into its drawn stand-in's hand. Call once a frame after Skyrim's
	// animation update and before the frame is drawn.
	void PlaceEnemyWeapons();

	// MORDHAU models loose in Skyrim's world (arrows, bolts and thrown weapons, flying or stuck): drawn from cache/props/<model>.
	struct Prop
	{
		const char*  model = "";  // folder under cache/props
		RE::NiPoint3 at;          // Skyrim world
		float        yaw = 0;     // radians, as actors count it (the way the thing faces)
		float        pitch = 0;   // radians, nose up
		bool         lengthUp = false;   // the model is long along its +Z (MORDHAU's arrows): laid along the way it faces
	};
	void SetProps(const std::vector<Prop>& a_props);
	// The weapon MORDHAU's character holds stands for the Skyrim weapon the player has equipped: show Skyrim's model.
	void SetPlayerSkyrimWeapon(bool a_show);
	void SetUnsheatheWait(float a_seconds);
	// A body in MORDHAU's pose drawn at a smoothed place following its physics place (sheets/builds.json standin_draw_smooth).
	void SetDrawSmoothing(float a_perSecond, float a_most, bool a_measure);  // how long an enemy keeps Skyrim's own pose while its stand-in is still unarmed
	void ShowHitMarker(bool a_kill);         // MORDHAU's hit marker at the crosshair for a moment (red when the hit killed)
	void SetStandinWeaponMordhau(bool a_on);  // sheets/builds.json standin_weapon_as_mordhau
	void SetThirdWeaponMordhau(bool a_on);  // sheets/builds.json third_person_weapon_mordhau
	// sheets/builds.json arms_to_mordhau_hands, arms_stretch_max, skyrim_weapon_grip_in_fist, skyrim_weapon_half_turn
	void SetAltSlide(float a_cm);  // sheets/builds.json alt_slide_<weapon>_<view> of the weapon now held
	void SetArms(bool a_follow, float a_stretch, bool a_gripInFist, bool a_halfTurn, bool a_altOnBlade);  // + alt_grip_hands_on_blade
	void SetSkyrimHasBody(std::uint32_t a_id, bool a_on);
	void SetMordhauBars(bool a_on);  // MORDHAU's health / stamina strip in Skyrim's picture (sheets/builds.json mordhau_bars)
	void SetSkyrimShieldLook(bool a_on);  // sheets/builds.json skyrim_shield_look
	void SetOutputScale(float a_scale);  // brightness of what this renderer draws (0 = unchanged)
	void SetSkyrimOwnsPlayer(bool a_owns);  // a Skyrim scene has the player's body: nothing of MORDHAU's is put on it
	// Who holds a MORDHAU weapon as a Skyrim item (0 = the player) and its model ("w_Zweihander"): on a Skyrim body
	// the item's Skyrim model is hidden and MORDHAU's is drawn in that hand.
	void SetMordhauHeld(const std::unordered_map<std::uint32_t, std::string>& a_models);
	// Weapons that lie on the ground after a drop or a disarm and have come to rest (owner 0 = the player's).
	struct DroppedWeapon
	{
		std::uint32_t owner = 0;
		RE::NiPoint3  at;
	};
	void DroppedWeapons(std::vector<DroppedWeapon>& a_out);
	// A Skyrim object now lies there for this owner's dropped weapon (a_shown), at a_at: MORDHAU's model is not drawn
	// for it, and picking up reaches for a_at.
	void DroppedAsSkyrim(std::uint32_t a_owner, bool a_shown, const RE::NiPoint3& a_at);
	// Skyrim's own models moved by MORDHAU's skeleton instead of MORDHAU bodies (sheets/builds.json skyrim_bodies).
	void SetSkyrimBodies(bool a_on);
	void SetLeftCasting(bool a_on);  // the player's left hand is casting: its arm keeps Skyrim's own casting animation (sheets/builds.json cast_arm_skyrim)
	void SetCastArm(bool a_on, float a_linger);  // the same for everyone else whose left hand casts (sheets/builds.json cast_arm_skyrim, cast_arm_linger)
	// This character's head was crushed (MORDHAU's head explosion): it is drawn without it from now on.
	// Blood on a character (0 = the player): added to what is there, kept between 0 and 1 (negative = fades).
	void AddBlood(std::uint32_t a_formId, float a_amount);
	void ExplodeHead(std::uint32_t a_formId);
	// ... or cut off: it flies the given way (Skyrim world, flat) and stays lying where it lands.
	void SeverHead(std::uint32_t a_formId, const RE::NiPoint3& a_away, bool a_mordhauHead = true);  // (false: no MORDHAU head flies - Skyrim's own severed head is in the world)
	// MORDHAU's own voice / emote menu is open (a_menu != 0): its corner of MORDHAU's window is shown.
	void SetMenu(int a_menu, int a_page);
	// The dropped weapon nearest to a point within reach: the player's own (a_owner = 0) or a fought character's.
	// a_owner 0xFFFFFFFF = nobody's any more (its owner is dead or gone): a_model names it ("w_Longsword"), and
	// asking takes it out of the world.
	// a_take false only looks (for the pick-up prompt): nothing is taken out of the world.
	bool WeaponOnGroundNear(const RE::NiPoint3& a_from, float a_reach, std::uint32_t& a_owner, std::string& a_model, bool a_take = true);
	// Which MORDHAU weapon each enemy with a stand-in holds (form id -> folder under cache/props, e.g. "w_ArmingSword"): drawn in
	// its hand where MORDHAU has it; the Skyrim weapon is hidden meanwhile.
	void SetStandinWeapons(const std::unordered_map<std::uint32_t, std::string>& a_weapons);

	// This Skyrim character is the body of one of MORDHAU's Demon Horde creatures (a "creature:<name>" look): draw it as that.
	void SetCreature(std::uint32_t a_formId, const char* a_name);

	void Install(const std::filesystem::path& a_cache, float a_sideOffset, float a_light, Pose (*a_pose)());
}
