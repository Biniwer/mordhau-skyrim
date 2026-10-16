#include "MordhauAudio.h"

#include "bake/Cache.h"
#include "Settings.h"
#include "gen/Sheets.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace dm::MordhauAudio
{
	namespace
	{
		namespace fs = std::filesystem;
		constexpr const char* kKey = "UnfocusedVolumeMultiplier";

		fs::path EngineIni()
		{
			return mbake::MordhauGameIni().parent_path() / "Engine.ini";
		}

		std::vector<std::string> Lines(const fs::path& a_file)
		{
			std::vector<std::string> lines;
			std::ifstream            in(a_file, std::ios::binary);
			std::string              line;
			while (std::getline(in, line)) {
				if (!line.empty() && line.back() == '\r') {
					line.pop_back();
				}
				lines.push_back(line);
			}
			return lines;
		}

		bool Write(const fs::path& a_file, const std::vector<std::string>& a_lines)
		{
			std::ofstream out(a_file, std::ios::binary | std::ios::trunc);
			for (const auto& line : a_lines) {
				out << line << "\r\n";
			}
			return static_cast<bool>(out);
		}

		// where [Audio] is, where the key is in it (npos when not there)
		struct Place
		{
			std::size_t section = std::string::npos, key = std::string::npos, end = 0;
		};

		Place Find(const std::vector<std::string>& a_lines)
		{
			Place place;
			for (std::size_t i = 0; i < a_lines.size(); ++i) {
				const auto& line = a_lines[i];
				if (!line.empty() && line.front() == '[') {
					if (place.section != std::string::npos && place.end == 0) {
						place.end = i;
					}
					if (line == "[Audio]") {
						place.section = i;
						place.end = 0;
					}
				} else if (place.section != std::string::npos && place.end == 0 && line.rfind(std::string(kKey) + "=", 0) == 0) {
					place.key = i;
				}
			}
			if (place.section != std::string::npos && place.end == 0) {
				place.end = a_lines.size();
			}
			return place;
		}

		bool On()
		{
			for (const auto& row : sheets::kBuilds) {
				if (std::string_view(row.id) == "mordhau_background_audio") {
					return Settings::Value(row.id, row.value) > 0.5f;  // (the settings menu's value, if it has one)
				}
			}
			return false;
		}
	}

	void KeepAudible(const fs::path& a_cache)
	{
		if (!On()) {
			return;
		}
		std::error_code ec;
		const auto      ini = EngineIni();
		const auto      restore = a_cache / "audio_restore.txt";
		if (fs::exists(restore, ec)) {
			return;  // (changed already by a session that did not get to put it back: the player's value is in that file)
		}
		auto        lines = Lines(ini);
		const auto  place = Find(lines);
		std::string old = "absent";
		if (place.key != std::string::npos) {
			old = lines[place.key].substr(std::string(kKey).size() + 1);
			if (std::strtof(old.c_str(), nullptr) >= 0.99f) {
				logger::info("audio: MORDHAU already plays its sound with its window behind Skyrim ({}={})", kKey, old);
				return;
			}
			lines[place.key] = std::string(kKey) + "=1.000000";
		} else if (place.section != std::string::npos) {
			lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(place.section + 1), std::string(kKey) + "=1.000000");
		} else {
			if (!lines.empty() && !lines.back().empty()) {
				lines.emplace_back();
			}
			lines.emplace_back("[Audio]");
			lines.emplace_back(std::string(kKey) + "=1.000000");
		}
		{
			std::ofstream note(restore, std::ios::binary | std::ios::trunc);
			note << old;
		}
		const bool written = Write(ini, lines);
		logger::info("audio: MORDHAU's sound with its window behind Skyrim: {}={} -> 1 in '{}' ({}; the old value is put back when MORDHAU closes)", kKey, old, ini.string(),
			written ? "written" : "could NOT be written");
	}

	void Restore(const fs::path& a_cache)
	{
		std::error_code ec;
		const auto      restore = a_cache / "audio_restore.txt";
		if (!fs::exists(restore, ec)) {
			return;
		}
		std::string old;
		{
			std::ifstream in(restore, std::ios::binary);
			std::getline(in, old);
		}
		const auto ini = EngineIni();
		auto       lines = Lines(ini);
		const auto place = Find(lines);
		if (place.key != std::string::npos) {
			if (old == "absent") {
				lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(place.key));
			} else {
				lines[place.key] = std::string(kKey) + "=" + old;
			}
			Write(ini, lines);
		}
		fs::remove(restore, ec);
		logger::info("audio: the player's own {} is back in MORDHAU's Engine.ini ({})", kKey, old == "absent" ? "the line taken out again" : old);
	}
}
