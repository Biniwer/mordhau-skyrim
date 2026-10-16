#pragma once

// The player's choices in the settings menu (sheets/settings_menu.json; MCM Helper's menu "MORDHAU x Skyrim"). MCM Helper
// keeps the defaults in Data/MCM/Config/MordhauWeapons/settings.ini and what the player changed in
// Data/MCM/Settings/MordhauWeapons.ini. A setting "fArmourShare" stands for the sheet value "armour_share": where the
// mod looks a sheet value up, the menu's value for it is used instead, and the sheet's own value when the menu has none.

#include <string_view>

namespace dm::Settings
{
	// both files read again (when data is loaded, when a save is loaded, and when the journal - where the menu lives - closes)
	void Load();
	// the menu's value for the sheet value a_id, or a_sheetValue when the menu has none
	float Value(std::string_view a_id, float a_sheetValue);
	// counts up whenever Load found a value changed (for what has to be passed on, such as the voice to MORDHAU)
	int Generation();
}
