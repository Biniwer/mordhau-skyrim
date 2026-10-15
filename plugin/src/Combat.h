#pragma once

namespace dm::Combat
{
	// Installs the input and per-frame hooks (sheets/hooks.json: input, attack_start, pose_override).
	void Install();

	// A new game started or a save finished loading.
	void OnGameLoaded();
}
