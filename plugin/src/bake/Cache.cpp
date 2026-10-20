#include "bake/Cache.h"

#include "bake/Spec.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

#ifndef WIN32_LEAN_AND_MEAN
#	define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#	define NOMINMAX
#endif
#include <Windows.h>

namespace mbake
{
	namespace
	{
		constexpr int kFormat = 1;  // raise when what is written changes: every cache is then rebuilt

		std::uint32_t Crc(std::string_view a_text, std::uint32_t a_seed = 0)
		{
			std::uint32_t crc = ~a_seed;
			for (const unsigned char c : a_text) {
				crc ^= c;
				for (int k = 0; k < 8; ++k) {
					crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
				}
			}
			return ~crc;
		}

		std::string Lower(std::string a_text)
		{
			for (auto& c : a_text) {
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			}
			return a_text;
		}

		std::string EnvironmentPath(const wchar_t* a_name)
		{
			wchar_t    buffer[1024];
			const auto length = GetEnvironmentVariableW(a_name, buffer, 1024);
			return length > 0 && length < 1024 ? std::filesystem::path(buffer).string() : std::string();
		}

		void WriteText(const std::filesystem::path& a_file, const std::string& a_text)
		{
			std::ofstream out(a_file, std::ios::binary);
			if (!out) {
				throw std::runtime_error("cannot write " + a_file.string());
			}
			out.write(a_text.data(), static_cast<std::streamsize>(a_text.size()));
		}
	}

	std::filesystem::path FindMordhauPaks()
	{
		std::vector<std::filesystem::path> libraries;
		wchar_t                            steam[1024];
		DWORD                              size = sizeof(steam);
		if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", RRF_RT_REG_SZ, nullptr, steam, &size) == ERROR_SUCCESS) {
			const std::filesystem::path root(steam);
			libraries.push_back(root);
			// the other libraries: lines like  "path"  "E:\\steam object"
			std::ifstream list(root / "steamapps" / "libraryfolders.vdf");
			std::string   line;
			while (std::getline(list, line)) {
				const auto key = line.find("\"path\"");
				if (key == std::string::npos) {
					continue;
				}
				const auto open = line.find('"', key + 6);
				const auto close = open == std::string::npos ? std::string::npos : line.find('"', open + 1);
				if (close == std::string::npos) {
					continue;
				}
				std::string path;
				for (std::size_t i = open + 1; i < close; ++i) {
					if (line[i] == '\\' && i + 1 < close && line[i + 1] == '\\') {
						++i;
					}
					path.push_back(line[i]);
				}
				libraries.emplace_back(path);
			}
		}
		for (const auto& library : libraries) {
			// (the library that really holds MORDHAU: an old one can still have an empty folder of the same name)
			const auto      paks = library / "steamapps" / "common" / "Mordhau" / "Mordhau" / "Content" / "Paks";
			std::error_code ec;
			if (std::filesystem::is_directory(paks, ec)) {
				for (const auto& entry : std::filesystem::directory_iterator(paks, ec)) {
					if (entry.path().extension() == ".pak") {
						return paks;
					}
				}
			}
		}
		return {};
	}

	std::filesystem::path MordhauGameIni()
	{
		return std::filesystem::path(EnvironmentPath(L"LOCALAPPDATA")) / "Mordhau" / "Saved" / "Config" / "WindowsClient" / "Game.ini";
	}

	std::filesystem::path DefaultCache()
	{
		return std::filesystem::path(EnvironmentPath(L"LOCALAPPDATA")) / "MordhauSkyrim" / "cache";
	}

	namespace
	{
		// a_checkOnly: only whether the cache is up to date (result.upToDate); nothing is built.
		Result Build(const Job& a_job, bool a_force, bool a_checkOnly, const std::function<void(const std::string&)>& a_progress)
	{
		Result     result;
		const auto say = [&](const std::string& a_line) {
			result.log += a_line + "\n";
			if (a_progress) {
				a_progress(a_line);
			}
		};
		std::string step = "looking for MORDHAU";  // (what was being done, for the note when it fails)
		try {
			std::error_code ec;
			std::filesystem::create_directories(a_job.cache, ec);
			// which mercenary is the player: named in <cache>/player.txt, else the default one if the player has it,
			// else the first complete one in Game.ini
			const auto  profiles = Mordhau::Profiles(a_job.gameIni);
			std::string wanted;
			{
				std::ifstream in(a_job.cache / "player.txt");
				std::getline(in, wanted);
				while (!wanted.empty() && (wanted.back() == '\r' || wanted.back() == ' ')) {
					wanted.pop_back();
				}
			}
			std::string profileLine;
			const auto  choose = [&](const std::string& a_name) {
				for (const auto& [name, line] : profiles) {
					if (!a_name.empty() && !line.empty() && Lower(name) == Lower(a_name)) {
						result.mercenary = name;
						profileLine = line;
						return true;
					}
				}
				return false;
			};
			if (!choose(wanted) && !choose(a_job.defaultMercenary)) {
				for (const auto& [name, line] : profiles) {
					if (!line.empty()) {
						result.mercenary = name;
						profileLine = line;
						break;
					}
				}
			}
			if (result.mercenary.empty()) {
				say("no complete mercenary found in MORDHAU's Game.ini (" + a_job.gameIni.string() + "): make one in MORDHAU first");
				return result;
			}
			// what the cache was built from: unchanged = nothing to do
			std::ostringstream stamp;
			stamp << "format " << kFormat << "\n";
			std::vector<std::filesystem::path> paks;
			for (const auto& entry : std::filesystem::directory_iterator(a_job.paks, ec)) {
				if (entry.is_regular_file() && entry.path().extension() == ".pak") {
					paks.push_back(entry.path());
				}
			}
			std::sort(paks.begin(), paks.end());
			for (const auto& pak : paks) {
				stamp << pak.filename().string() << " " << std::filesystem::file_size(pak, ec) << " " << std::filesystem::last_write_time(pak, ec).time_since_epoch().count() << "\n";
			}
			std::uint32_t sheets = 0;
			for (const auto& row : a_job.looks) {
				for (const auto* field : { &row.id, &row.lookClass, &row.head, &row.coif, &row.upperChest, &row.lowerChest, &row.shoulders, &row.arms, &row.hands, &row.legs, &row.feet, &row.model,
						 &row.colourMap, &row.normalMap }) {
					sheets = Crc(*field + "|", sheets);
				}
				sheets = Crc(std::to_string(row.colour1) + "," + std::to_string(row.colour2) + ";", sheets);
			}
			for (const auto& row : a_job.props) {
				for (const auto* field : { &row.id, &row.kind, &row.model, &row.colourMap, &row.normalMap, &row.surfaceMap, &row.weapon }) {
					sheets = Crc(*field + "|", sheets);
				}
			}
			stamp << "sheets " << sheets << "\n";
			stamp << "mercenary " << result.mercenary << " " << Crc(profileLine) << "\n";
			const auto stampFile = a_job.cache / "baked.txt";
			if (!a_force) {
				std::ifstream     in(stampFile, std::ios::binary);
				std::stringstream had;
				had << in.rdbuf();
				if (in && had.str() == stamp.str()) {
					result.ok = result.upToDate = true;
					return result;
				}
			}
			if (a_checkOnly) {
				return result;
			}
			std::filesystem::remove(stampFile, ec);

			step = "reading MORDHAU's archives";
			PakIndex index;
			const auto found = index.Open(a_job.paks);
			for (const auto& skipped : index.Skipped()) {
				say("an archive in MORDHAU's folder is not one of the game's own and was left out: " + skipped);
			}
			if (found == 0) {
				say("MORDHAU's files were not found in " + a_job.paks.string());
				return result;
			}
			step = "reading MORDHAU's equipment tables";
			Mordhau    mordhau(index);
			step = "preparing the models";
			const auto fresh = [&](const std::filesystem::path& a_folder) {
				std::filesystem::remove_all(a_folder, ec);
				std::filesystem::create_directories(a_folder, ec);
			};
			const auto attempt = [&](const std::string& a_what, const std::filesystem::path& a_folder, const std::function<void(std::string&)>& a_bake) {
				std::string log;
				try {
					fresh(a_folder);
					a_bake(log);
					++result.built;
				} catch (const std::exception& ex) {
					std::filesystem::remove_all(a_folder, ec);
					++result.failed;
					say(a_what + " FAILED: " + ex.what());
				}
				result.log += log;
			};

			// the player's mercenary
			say("preparing MORDHAU's models: the mercenary " + result.mercenary);
			attempt("mercenary " + result.mercenary, a_job.cache / "character", [&](std::string& a_log) {
				BakePieces(index, ToPieces(mordhau.Profile(a_job.gameIni, result.mercenary)), a_job.cache / "character", 4096, a_log);
			});

			// the outfits and creatures, by number
			std::string looksText;
			for (std::size_t i = 0; i < a_job.looks.size(); ++i) {
				const auto& row = a_job.looks[i];
				const auto  number = std::to_string(i + 1);
				std::string lookClass = row.lookClass;
				std::replace(lookClass.begin(), lookClass.end(), ' ', '_');
				looksText += number + " " + lookClass + " " + row.id + "\n";
				if (i % 10 == 0) {
					say("preparing MORDHAU's models: outfit " + number + " of " + std::to_string(a_job.looks.size()));
				}
				attempt("outfit " + number + " " + row.id, a_job.cache / "enemies" / number, [&](std::string& a_log) {
					if (row.lookClass.rfind("creature:", 0) == 0) {
						Piece piece;
						piece.model = row.model;
						piece.colourMap = row.colourMap;
						piece.normalMap = row.normalMap;
						BakePieces(index, { piece }, a_job.cache / "enemies" / number, 1024, a_log);
						return;
					}
					Spec       spec;
					const bool ok = mordhau.Outfit(row.id, { { "Head", row.head }, { "Coif", row.coif }, { "UpperChest", row.upperChest }, { "LowerChest", row.lowerChest },
															   { "Shoulders", row.shoulders }, { "Arms", row.arms }, { "Hands", row.hands }, { "Legs", row.legs }, { "Feet", row.feet } },
						row.colour1, row.colour2, spec, a_log);
					if (!ok) {
						throw std::runtime_error("a slot has no wearable");
					}
					BakePieces(index, ToPieces(spec), a_job.cache / "enemies" / number, 1024, a_log);
				});
			}
			std::filesystem::create_directories(a_job.cache / "enemies", ec);
			WriteText(a_job.cache / "enemies" / "looks.txt", looksText);

			// the props
			say("preparing MORDHAU's models: weapons, arrows");
			for (const auto& row : a_job.props) {
				attempt("prop " + row.id, a_job.cache / "props" / row.id, [&](std::string& a_log) {
					if (row.kind == "weapon") {
						Spec spec;
						spec.noBody = true;
						spec.pieces = mordhau.DefaultWeapon(row.weapon);
						if (spec.pieces.empty()) {
							throw std::runtime_error("no parts found");
						}
						BakePieces(index, ToPieces(spec), a_job.cache / "props" / row.id, 1024, a_log);
						return;
					}
					Piece piece;
					piece.model = row.model;
					piece.colourMap = row.colourMap;
					piece.normalMap = row.normalMap;
					piece.surfaceMap = row.surfaceMap;
					BakePieces(index, { piece }, a_job.cache / "props" / row.id, 1024, a_log);
				});
			}

			// what MORDHAU's script reads: which mercenary to spawn as, how many outfits there are, the bone names
			std::string config = "-- written by the mod's first start: which of the player's own mercenaries MORDHAU spawns; enemies = one entry per\n"
								 "-- baked outfit (the MORDHAU-side script only counts them)\nreturn {\n";
			config += "    profile_index = " + std::to_string(Mordhau::ProfileIndex(a_job.gameIni, result.mercenary)) + ", profile_name = \"" + result.mercenary + "\",\n    enemies = {\n";
			for (const auto& row : a_job.looks) {
				config += "        { name = \"" + row.id + "\", weapon = \"\" },\n";
			}
			config += "    },\n}\n";
			WriteText(a_job.cache / "live_config.lua", config);
			std::string names = "return {\n";
			for (const auto& bone : ReadSkeletalMesh(index, "UMA/UMA/Master/UMA_Master").bones) {
				names += "  \"" + bone.name + "\",\n";
			}
			names += "}\n";
			WriteText(a_job.cache / "bone_names.lua", names);

			WriteText(stampFile, stamp.str());
			result.ok = true;
			say("MORDHAU's models are ready: " + std::to_string(result.built) + " built" + (result.failed ? ", " + std::to_string(result.failed) + " failed" : ""));
		} catch (const std::exception& ex) {
			say(std::string("preparing MORDHAU's models failed while ") + step + ": " + ex.what());
		}
		return result;
	}
	}

	Result BuildCache(const Job& a_job, bool a_force, const std::function<void(const std::string&)>& a_progress)
	{
		return Build(a_job, a_force, false, a_progress);
	}

	bool CacheUpToDate(const Job& a_job)
	{
		return Build(a_job, false, true, nullptr).upToDate;
	}
}
