#include "Settings.h"

#include <atomic>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>

namespace dm::Settings
{
	namespace
	{
		using Map = std::unordered_map<std::string, float>;
		std::atomic<std::shared_ptr<const Map>> g_values;
		std::atomic_int                         g_generation{ 0 };

		// "fArmourShare" -> "armour_share" (the type letter off, each capital a new word)
		std::string SheetId(std::string_view a_key)
		{
			std::string id;
			for (const char c : a_key.substr(1)) {
				if (std::isupper(static_cast<unsigned char>(c))) {
					if (!id.empty()) {
						id += '_';
					}
					id += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
				} else {
					id += c;
				}
			}
			return id;
		}

		void Read(const std::filesystem::path& a_file, Map& a_values)
		{
			std::ifstream in(a_file, std::ios::binary);
			std::string   line;
			while (std::getline(in, line)) {
				const auto eq = line.find('=');
				if (line.empty() || line[0] == ';' || line[0] == '[' || eq == std::string::npos) {
					continue;
				}
				auto trim = [](std::string s) {
					s.erase(0, s.find_first_not_of(" \t\r"));
					s.erase(s.find_last_not_of(" \t\r") + 1);
					return s;
				};
				const auto key = trim(line.substr(0, eq));
				const auto value = trim(line.substr(eq + 1));
				if (key.size() < 2 || value.empty()) {
					continue;
				}
				try {
					a_values[SheetId(key)] = std::stof(value);
				} catch (...) {
					logger::warn("settings: '{}' in {} is not a number", line, a_file.string());
				}
			}
		}
	}

	void Load()
	{
		auto values = std::make_shared<Map>();
		Read("Data/MCM/Config/MordhauWeapons/settings.ini", *values);
		Read("Data/MCM/Settings/MordhauWeapons.ini", *values);
		const auto old = g_values.load();
		bool       changed = !old;
		for (const auto& [id, value] : *values) {
			if (!old || !old->contains(id) || old->at(id) != value) {
				logger::info("settings: {} = {}", id, value);
				changed = true;
			}
		}
		g_values.store(std::move(values));
		if (changed) {
			++g_generation;
		}
	}

	int Generation()
	{
		return g_generation;
	}

	float Value(std::string_view a_id, float a_sheetValue)
	{
		auto values = g_values.load();
		if (!values) {
			Load();
			values = g_values.load();
		}
		const auto it = values->find(std::string(a_id));
		return it != values->end() ? it->second : a_sheetValue;
	}
}
