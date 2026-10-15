#pragma once

// MORDHAU weapons as Skyrim items (sheets/mordhau_items.json).
namespace dm::Items
{
	// Makes the items and adds them to the game's levelled lists, once, as soon as the game's own data is there
	// (at the main menu: before any save is loaded, so a save that holds them finds them).
	void EnsureCreated();

	// The MORDHAU weapon ("Specific/...") an item stands for; nullptr for any other weapon.
	const char* MordhauWeaponOf(const RE::TESForm* a_weapon);

	// Every item made (empty before EnsureCreated has run).
	const std::vector<RE::TESObjectWEAP*>& All();
}
