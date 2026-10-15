#pragma once

// The baker's job as the sheets describe it (generated gen/Sheets.h -> the plain rows Cache.h takes).

#include "bake/Cache.h"
#include "gen/Sheets.h"

namespace mbake
{
	inline Job JobFromSheets(const std::filesystem::path& a_paks, const std::filesystem::path& a_gameIni, const std::filesystem::path& a_cache)
	{
		Job job;
		job.paks = a_paks;
		job.gameIni = a_gameIni;
		job.cache = a_cache;
		job.defaultMercenary = dm::sheets::kDefaultMercenary;
		for (const auto& row : dm::sheets::kLooks) {
			job.looks.push_back({ row.id, row.lookClass, row.head, row.coif, row.upperChest, row.lowerChest, row.shoulders, row.arms, row.hands, row.legs, row.feet, row.colour1, row.colour2,
				row.model, row.colourMap, row.normalMap });
		}
		for (const auto& row : dm::sheets::kProps) {
			job.props.push_back({ row.id, row.kind, row.model, row.colourMap, row.normalMap, row.surfaceMap, row.weapon });
		}
		for (const auto& row : dm::sheets::kBlessingTextures) {
			job.effectTextures.emplace_back(row.id, row.path);
		}
		return job;
	}
}
