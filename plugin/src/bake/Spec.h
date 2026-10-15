#pragma once

// What a MORDHAU character looks like, worked out from MORDHAU's own data: the player's mercenary from Game.ini, or
// an outfit put together from named wearables (sheets/looks.json). Ported from tools/profile.py and
// tools/character_spec.py; the result goes to BakePieces (Bake.h).

#include "bake/Bake.h"

#include <array>
#include <memory>

namespace mbake
{
	using Rgb = std::optional<std::array<double, 3>>;  // linear, as MORDHAU's colour definitions store it

	struct Dye
	{
		int  channel = -1;  // which channel of the pattern mask it colours (0 red, 1 green, 2 blue); -1 = none
		Rgb  colour;
		bool used = false;
	};

	struct SpecPiece
	{
		std::string              slot, item;  // "Head", "BP_KettleHelmet" (a weapon part for a held weapon's pieces)
		std::string              model, colourMap, normalMap, surfaceMap, patternMask;
		std::vector<Dye>         dye;
		std::optional<Dye>       metalTint;
		std::vector<std::string> hide;  // "bHideChest", ...
		bool                     hasEmblem = false;
		double                   emblemCentre[2]{}, emblemScale[2]{}, emblemTurns = 0.0;
		std::string              emblemTexture;
		Rgb                      emblemColours[2];
	};

	struct Spec
	{
		std::string            name;
		std::vector<SpecPiece> pieces;      // the nine wearables, in MORDHAU's slot order
		std::vector<SpecPiece> heldWeapon;  // the first weapon's parts
		bool                   noBody = false;
	};

	// One wearable chosen for a slot: its definition, the colour numbers, the pattern number.
	struct Chosen
	{
		std::string      item;
		std::vector<int> colours;
		int              pattern = 0;
	};

	class Mordhau
	{
	public:
		explicit Mordhau(const PakIndex& a_paks) :
			_paks(a_paks) {}

		// MORDHAU's lists of wearables: "Head", "UpperChest", "Legs" from its singleton; the other slots hang on an
		// item ("CoifWearables" of a head, "ArmsWearables" of an upper chest, ...), its own or its nearest parent's.
		[[nodiscard]] const std::vector<std::string>& Top(const std::string& a_slot);
		[[nodiscard]] const std::vector<std::string>& Equipment();
		[[nodiscard]] const std::vector<std::string>& ItemList(const std::string& a_item, const std::string& a_key);

		// An outfit from named wearables ("" or an unknown name = the first of that slot's list), sheets/looks.json style.
		// Returns false (and says why in a_log) when a slot has no wearable at all.
		bool Outfit(const std::string& a_name, const std::map<std::string, std::string>& a_wanted, int a_colour1, int a_colour2, Spec& a_out, std::string& a_log);
		// The player's own mercenary a_name from MORDHAU's Game.ini (wearables, dyes, metal tint, emblem, first weapon).
		[[nodiscard]] Spec Profile(const std::filesystem::path& a_gameIni, const std::string& a_name);
		// Every mercenary in Game.ini: (name, the whole line it is stored as).
		[[nodiscard]] static std::vector<std::pair<std::string, std::string>> Profiles(const std::filesystem::path& a_gameIni);
		// The index of that mercenary among the profiles in Game.ini (-1: none of that name).
		[[nodiscard]] static int ProfileIndex(const std::filesystem::path& a_gameIni, const std::string& a_name);
		// A weapon in a given make: one piece per chosen part.
		[[nodiscard]] std::vector<SpecPiece> WeaponPieces(const std::string& a_weapon, const std::vector<int>& a_parts, const std::vector<int>& a_colours, int a_skin = 0);
		// The same for an enemy's weapon in its default make; parts whose material does not name its maps the usual
		// way get them by their file-name endings (as tools/bake_props.py did).
		[[nodiscard]] std::vector<SpecPiece> DefaultWeapon(const std::string& a_weapon);

		// (colour map, normal map, surface map) a material names, by the usual file-name endings; "" where none.
		[[nodiscard]] std::array<std::string, 3> TexturesBySuffix(const std::string& a_material);

		[[nodiscard]] const Package& Open(const std::string& a_base);

	private:
		struct Link
		{
			std::string           item;
			const Package*        package = nullptr;
			std::vector<Property> properties;
		};
		struct Merged
		{
			std::map<std::string, std::pair<const Package*, Property>> all;
			[[nodiscard]] const Property* Get(const std::string& a_key) const
			{
				const auto it = all.find(a_key);
				return it == all.end() ? nullptr : &it->second.second;
			}
			[[nodiscard]] const Package* Owner(const std::string& a_key) const
			{
				const auto it = all.find(a_key);
				return it == all.end() ? nullptr : it->second.first;
			}
			[[nodiscard]] Value Decode(const std::string& a_key) const
			{
				const auto it = all.find(a_key);
				return it == all.end() ? Value{} : it->second.first->Decode(it->second.second);
			}
		};
		[[nodiscard]] std::vector<Link> Chain(const std::string& a_item);
		[[nodiscard]] Merged            Merge(const std::string& a_item);
		[[nodiscard]] Spec              Build(const std::string& a_name, const std::vector<std::pair<std::string, Chosen>>& a_entries);
		[[nodiscard]] const std::vector<std::vector<Rgb>>& ColourTables();
		[[nodiscard]] std::vector<Rgb>                     SingleTable(const std::string& a_property);
		[[nodiscard]] std::vector<Rgb>                     ColoursOf(const Package& a_package, const std::vector<char>& a_entries);
		void                                               ReadSingleton();

		const PakIndex&                                         _paks;
		std::map<std::string, std::unique_ptr<Package>>         _open;
		std::map<std::string, std::vector<std::string>>         _top;
		std::vector<std::string>                                _equipment;
		bool                                                    _singleton = false;
		std::map<std::pair<std::string, std::string>, std::vector<std::string>> _lists;
		std::optional<std::vector<std::vector<Rgb>>>            _tables;
	};

	// The pieces BakePieces takes for a spec: the body (unless a_spec.noBody) with what the items hide, the dyed
	// wearables, the held weapon's parts.
	[[nodiscard]] std::vector<Piece> ToPieces(const Spec& a_spec);
}
