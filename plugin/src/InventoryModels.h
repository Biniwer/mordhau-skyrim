#pragma once

// MORDHAU's own models in Skyrim's inventory preview, for the MORDHAU weapons that are Skyrim items
// (sheets/builds.json inventory_mordhau_models). A MORDHAU item borrows a Skyrim weapon's model file (a Falx showed as a
// steel greatsword); the inventory can only show a model file, so one is written for each MORDHAU weapon from the
// model the plugin has already prepared in its cache out of the player's own MORDHAU - on the player's PC, into
// Data\meshes\MordhauSkyrim and Data\textures\MordhauSkyrim. Nothing of it is shipped. In the hand and in the world the
// items are drawn as before.
namespace dm::InventoryModels
{
	void SetEnabled(bool a_on);
	// Once a frame, on the game's own thread (also while a menu has the game paused): puts MORDHAU's model in place of
	// the borrowed one in whatever the inventory preview has loaded.
	void Update(const std::filesystem::path& a_cache);
}
