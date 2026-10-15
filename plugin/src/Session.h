#pragma once

// Installed by a mod manager (sheets/live_link.json: managed_session): the plugin, loaded by SKSE from
// Data/SKSE/Plugins with MORDHAU's side beside it (Data/SKSE/Plugins/MordhauSkyrim/mordhau), runs the session that
// Play-Mashup.ps1 runs otherwise: it starts the player's own MORDHAU offline beside Skyrim, takes the script loader
// out of MORDHAU's folder again as soon as MORDHAU has loaded it, and ends MORDHAU with Skyrim.
namespace dm::Session
{
	// Reads MordhauSkyrim.ini beside the plugin and, when MORDHAU's side is there too, starts the session (its own thread).
	void Begin(void* a_plugin);

	// From MordhauSkyrim.ini; empty = the default (the cache under %LOCALAPPDATA%, MORDHAU found through Steam).
	[[nodiscard]] std::filesystem::path CacheFolder();
	[[nodiscard]] std::filesystem::path MordhauFolder();
}
