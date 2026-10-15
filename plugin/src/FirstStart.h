#pragma once

// The first-start message box (sheets/first_start.json): what the mod does and the risk to the player's MORDHAU account,
// shown at Skyrim's main menu until the player chooses "I understand". MORDHAU is not started before that.

namespace dm::FirstStart
{
	// the player has accepted the current text (%LOCALAPPDATA%\MordhauSkyrim\understood.txt holds its version)
	bool Accepted();
	// main menu open, main thread: the message box, once per Skyrim start, unless already accepted
	void ShowIfNeeded();
}
