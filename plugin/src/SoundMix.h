#pragma once

// The settings menu's page "Sound mix" (sheets/settings_menu.json): how much of MORDHAU's sound and how much of Skyrim's
// own weapon sounds (sheets/skyrim_combat_sounds.json: hits, swings, blocks and bashes, drawing and putting away) is
// heard - from only Skyrim's to only MORDHAU's: the sliders, unless "Sound style" is "Only MORDHAU" or "Only Skyrim".
// Skyrim's sounds are turned down by raising each sound record's own static attenuation in memory (never saved; the
// record's own value is kept and put back at 100%). Nothing else of Skyrim's sound is touched.

namespace dm::SoundMix
{
	// MORDHAU's master volume, 0..1 (sent to MORDHAU's script as Volume<percent>)
	float MordhauVolume();
	// Skyrim's weapon sounds set to the menu's volumes (data loaded, and whenever the settings were read again)
	void ApplySkyrim();
	// Skyrim's own swing sound for a swing MORDHAU makes (the player's attacks are MORDHAU's: Skyrim's attack animation,
	// which carries its swing sound, never plays) - by the weapon in the right hand; a kick or bare fists: the unarmed one.
	// Heard as loud as the menu's Skyrim swings (the record's attenuation).
	void PlaySkyrimSwing(RE::Actor* a_actor, bool a_kick);
	// Skyrim's own draw / sheathe sound for the weapon in the right hand (with MORDHAU linked the one of Skyrim's
	// animation was not heard), as loud as the menu's Skyrim drawing and sheathing
	void PlaySkyrimDraw(RE::Actor* a_actor, bool a_draw);
	// Skyrim's own block sound for a parry MORDHAU decided (Skyrim's block, which carries one, does not happen): by what the
	// defender a_defender parries with - a shield on the left arm, else the weapon in the right hand; as loud as the menu's
	// Skyrim blocks and bashes
	void PlaySkyrimBlock(RE::Actor* a_defender);
}
