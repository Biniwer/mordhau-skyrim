#include "Stats.h"

#include "gen/Sheets.h"

#include <charconv>
#include <fstream>

namespace dm
{
	void Stats::Load(const std::filesystem::path& a_file)
	{
		_values.clear();
		std::ifstream in(a_file);
		std::string   line;
		while (std::getline(in, line)) {
			if (!line.empty() && line.back() == '\r') {
				line.pop_back();
			}
			const auto eq = line.find('=');
			if (eq == std::string::npos) {
				continue;
			}
			float       value = 0.0f;
			const char* first = line.data() + eq + 1;
			const char* last = line.data() + line.size();
			if (std::from_chars(first, last, value).ec == std::errc{}) {
				_values.emplace(line.substr(0, eq), value);
			}
		}
	}

	float Stats::Get(std::string_view a_family, bool a_stab, std::string_view a_statId, bool* a_fromMordhau) const
	{
		if (a_fromMordhau) {
			*a_fromMordhau = false;
		}
		for (const auto& stat : sheets::kAttackStats) {
			if (a_statId != stat.id) {
				continue;
			}
			std::string key(a_family);
			key += a_stab ? ".stab." : ".strike.";
			key += stat.property;
			if (const auto it = _values.find(key); it != _values.end()) {
				if (a_fromMordhau) {
					*a_fromMordhau = true;
				}
				return it->second;
			}
			return stat.fallback;
		}
		return -1.0f;
	}

	float Stats::Parry(std::string_view a_statId, bool* a_fromMordhau) const
	{
		if (a_fromMordhau) {
			*a_fromMordhau = false;
		}
		for (const auto& stat : sheets::kParryStats) {
			if (a_statId != stat.id) {
				continue;
			}
			if (*stat.property) {
				if (const auto it = _values.find(std::string("parry.") + stat.property); it != _values.end()) {
					if (a_fromMordhau) {
						*a_fromMordhau = true;
					}
					return it->second;
				}
			}
			return stat.fallback;
		}
		return -1.0f;
	}

	float Stats::Move(std::string_view a_statId, bool* a_fromMordhau) const
	{
		if (a_fromMordhau) {
			*a_fromMordhau = false;
		}
		for (const auto& stat : sheets::kMoveStats) {
			if (a_statId != stat.id) {
				continue;
			}
			if (*stat.property) {
				if (const auto it = _values.find(std::string("move.") + stat.property); it != _values.end()) {
					if (a_fromMordhau) {
						*a_fromMordhau = true;
					}
					return it->second;
				}
			}
			return stat.fallback;
		}
		return -1.0f;
	}

	float Stats::Raw(const std::string& a_key, float a_default) const
	{
		const auto it = _values.find(a_key);
		return it != _values.end() ? it->second : a_default;
	}
}
