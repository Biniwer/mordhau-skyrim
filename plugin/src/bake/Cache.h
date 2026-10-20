#pragma once

// The whole job: find the player's MORDHAU, and build this mod's local cache from it (the player's mercenary, the
// outfits and creatures of sheets/looks.json, the props of sheets/props.json, and the small files MORDHAU's script
// reads). Nothing is rebuilt while MORDHAU's
// files, the player's mercenary and the sheets are the same as last time (a stamp file in the cache says so).

#include "bake/Bake.h"

#include <functional>

namespace mbake
{
	struct LookRow
	{
		std::string id, lookClass, head, coif, upperChest, lowerChest, shoulders, arms, hands, legs, feet;
		int         colour1 = 0, colour2 = 0;
		std::string model, colourMap, normalMap;  // a "creature:" row's own model
	};
	struct PropRow
	{
		std::string id, kind, model, colourMap, normalMap, surfaceMap, weapon;
	};
	struct Job
	{
		std::filesystem::path paks;     // <MORDHAU>/Mordhau/Content/Paks
		std::filesystem::path gameIni;  // MORDHAU's Game.ini (the player's mercenaries)
		std::filesystem::path cache;    // where to write
		std::vector<LookRow>  looks;
		std::vector<PropRow>  props;
		std::string           defaultMercenary;  // used when the player named none in <cache>/player.txt and has one of this name
	};
	struct Result
	{
		bool        ok = false;
		bool        upToDate = false;  // nothing had changed: nothing was rebuilt
		int         built = 0, failed = 0;
		std::string mercenary;
		std::string log;
	};

	// The player's MORDHAU, found through Steam (its registry entry and library list): the Paks folder, or empty.
	[[nodiscard]] std::filesystem::path FindMordhauPaks();
	// %LOCALAPPDATA%\Mordhau\Saved\Config\WindowsClient\Game.ini
	[[nodiscard]] std::filesystem::path MordhauGameIni();
	// %LOCALAPPDATA%\MordhauSkyrim\cache: where the cache lives unless told otherwise.
	[[nodiscard]] std::filesystem::path DefaultCache();

	// Builds whatever is out of date. a_progress is called with a short line now and then ("outfit 12 of 85 ...").
	Result BuildCache(const Job& a_job, bool a_force, const std::function<void(const std::string&)>& a_progress);
	// Whether BuildCache would find nothing to do (reads only the stamp and MORDHAU's file list: quick).
	[[nodiscard]] bool CacheUpToDate(const Job& a_job);
}
