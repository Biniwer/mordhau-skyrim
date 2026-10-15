#pragma once

namespace dm
{
	// The three moments SKSE would message a plugin about, derived from the game's own events so the
	// plugin does not depend on which of them SKSE sends on a given game version (sheets/hooks.json: boot).
	struct BootEvents
	{
		void (*dataLoaded)() = nullptr;  // forms exist, main menu is up
		void (*gameLoaded)() = nullptr;  // new game started or a save finished loading
	};

	void InstallBoot(const BootEvents& a_events);
}
