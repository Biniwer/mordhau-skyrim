// Dev: runs the plugin's own baker (plugin/src/bake) outside Skyrim, to compare what it writes with what the Python
// tools wrote. Not shipped.
//
//   bake_test <Paks> index
//   bake_test <Paks> prop <out dir> <model> <colour map> [normal map] [surface map]
//   bake_test <Paks> look <out dir> <name> <head> <coif> <upper chest> <lower chest> <shoulders> <arms> <hands> <legs> <feet> <colour 1> <colour 2>
//   bake_test <Paks> weapon <out dir> <BP_Weapon>
//   bake_test <Paks> profile <out dir> <Game.ini> <mercenary name>
//   bake_test auto all <cache dir> [force]      the whole cache, as the plugin builds it ("auto" = find MORDHAU through Steam)
//   bake_test auto find                        only say where MORDHAU and its Game.ini are
// ("-" stands for an empty name.)

#include "bake/Bake.h"
#include "bake/Cache.h"
#include "bake/FromSheets.h"
#include "bake/Spec.h"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>

int main(int a_count, char** a_args)
{
	if (a_count < 3) {
		std::puts("usage: bake_test <Paks folder> index | prop ... | look ... | weapon ... | profile ...");
		return 2;
	}
	try {
		const std::string           mode = a_args[2];
		const std::filesystem::path paksFolder = std::string(a_args[1]) == "auto" ? mbake::FindMordhauPaks() : std::filesystem::path(a_args[1]);
		if (mode == "find") {
			std::printf("MORDHAU's files: %s\nGame.ini: %s\ndefault cache: %s\n", paksFolder.string().c_str(), mbake::MordhauGameIni().string().c_str(), mbake::DefaultCache().string().c_str());
			return paksFolder.empty() ? 1 : 0;
		}
		if (mode == "all" && a_count >= 4) {
			const auto job = mbake::JobFromSheets(paksFolder, mbake::MordhauGameIni(), a_args[3]);
			const auto result = mbake::BuildCache(job, a_count > 4, [](const std::string& a_line) { std::printf("%s\n", a_line.c_str()); });
			std::printf("ok %d, up to date %d, built %d, failed %d, mercenary %s\n", result.ok, result.upToDate, result.built, result.failed, result.mercenary.c_str());
			return result.ok ? 0 : 1;
		}
		mbake::PakIndex paks;
		const auto      files = paks.Open(paksFolder);
		const auto        arg = [&](int a_index) { return a_index < a_count && std::string(a_args[a_index]) != "-" ? std::string(a_args[a_index]) : std::string(); };
		std::string       log;
		if (mode == "index") {
			std::printf("%zu files in MORDHAU's archives\n", files);
			return files ? 0 : 1;
		}
		if (mode == "prop" && a_count >= 6) {
			mbake::Piece piece;
			piece.model = arg(4);
			piece.colourMap = arg(5);
			piece.normalMap = arg(6);
			piece.surfaceMap = arg(7);
			mbake::BakePieces(paks, { piece }, a_args[3], 1024, log);
		} else if (mode == "look" && a_count >= 16) {
			mbake::Mordhau mordhau(paks);
			mbake::Spec    spec;
			const std::map<std::string, std::string> wanted{ { "Head", arg(5) }, { "Coif", arg(6) }, { "UpperChest", arg(7) }, { "LowerChest", arg(8) }, { "Shoulders", arg(9) },
				{ "Arms", arg(10) }, { "Hands", arg(11) }, { "Legs", arg(12) }, { "Feet", arg(13) } };
			if (!mordhau.Outfit(arg(4), wanted, std::atoi(a_args[14]), std::atoi(a_args[15]), spec, log)) {
				std::fputs(log.c_str(), stdout);
				return 1;
			}
			mbake::BakePieces(paks, mbake::ToPieces(spec), a_args[3], 1024, log);
		} else if (mode == "weapon" && a_count >= 5) {
			mbake::Mordhau mordhau(paks);
			mbake::Spec    spec;
			spec.noBody = true;
			spec.pieces = mordhau.DefaultWeapon(arg(4));
			if (spec.pieces.empty()) {
				std::puts("FAILED: no parts found");
				return 1;
			}
			mbake::BakePieces(paks, mbake::ToPieces(spec), a_args[3], 1024, log);
		} else if (mode == "profile" && a_count >= 6) {
			mbake::Mordhau mordhau(paks);
			const auto     spec = mordhau.Profile(a_args[4], arg(5));
			std::printf("mercenary %s: number %d in Game.ini, %zu wearables, %zu weapon parts\n", spec.name.c_str(), mbake::Mordhau::ProfileIndex(a_args[4], arg(5)), spec.pieces.size(),
				spec.heldWeapon.size());
			mbake::BakePieces(paks, mbake::ToPieces(spec), a_args[3], 4096, log);
		} else {
			std::puts("unknown mode or too few arguments");
			return 2;
		}
		std::fputs(log.c_str(), stdout);
		return 0;
	} catch (const std::exception& ex) {
		std::printf("FAILED: %s\n", ex.what());
		return 1;
	}
}
