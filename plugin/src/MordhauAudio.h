#pragma once

// MORDHAU mutes itself while its window is not in front - and in the mashup it always runs behind Skyrim, so its swings,
// hits and parries went unheard. Before the mod starts MORDHAU, its own setting for that (Engine.ini, [Audio]
// UnfocusedVolumeMultiplier) is set to full; once MORDHAU has read it at its start (it is live), the player's own value
// is put back into the file, and again after MORDHAU has closed in case it never was (sheets/builds.json mordhau_background_audio). Nothing else of MORDHAU's settings is touched.

#include <filesystem>

namespace dm::MordhauAudio
{
	// before MORDHAU starts: the setting to 1, the player's old one kept in <cache>/audio_restore.txt
	void KeepAudible(const std::filesystem::path& a_cache);
	// once MORDHAU is live, and after it has closed: the player's own value back (only if KeepAudible changed it)
	void Restore(const std::filesystem::path& a_cache);
}
