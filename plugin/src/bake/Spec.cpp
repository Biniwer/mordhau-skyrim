#include "bake/Spec.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <regex>
#include <stdexcept>

namespace mbake
{
	namespace
	{
		const char* const kSingleton = "Mordhau/Blueprints/BP_MordhauSingleton";
		const char* const kBody = "UMA/UMA/Master/UMA_Master";
		// MORDHAU's slot order in a profile's wearable list (the last three: inferred, see tools/character_spec.py)
		const char* const kOrder[9]{ "Head", "Coif", "UpperChest", "LowerChest", "Shoulders", "Arms", "Hands", "Legs", "Feet" };

		std::int32_t Int(const std::vector<char>& a_bytes, std::size_t a_at)
		{
			if (a_at + 4 > a_bytes.size()) {
				throw std::runtime_error("read past the end of a property");
			}
			std::int32_t v;
			std::memcpy(&v, a_bytes.data() + a_at, 4);
			return v;
		}

		std::string Leaf(const std::string& a_path, char a_separator = '/')
		{
			const auto at = a_path.find_last_of(a_separator);
			return at == std::string::npos ? a_path : a_path.substr(at + 1);
		}

		std::string WithoutClassSuffix(const std::string& a_name)  // "BP_Thing_C" -> "BP_Thing"
		{
			return a_name.size() >= 2 ? a_name.substr(0, a_name.size() - 2) : std::string();
		}

		std::string WithoutGame(std::string a_path)
		{
			if (const auto at = a_path.find("/Game/"); at != std::string::npos) {
				a_path.erase(at, 6);
			}
			return a_path;
		}

		const Property* Find(const std::vector<Property>& a_properties, std::string_view a_name)
		{
			for (const auto& p : a_properties) {
				if (p.name == a_name) {
					return &p;
				}
			}
			return nullptr;
		}

		const Package::Export* DefaultObject(const Package& a_package)
		{
			for (const auto& ex : a_package.exports()) {
				if (ex.name.rfind("Default__", 0) == 0) {
					return &ex;
				}
			}
			throw std::runtime_error("a package without a default object");
		}

		// A list of soft references as MORDHAU stores them: the names' last part, without "_C".
		std::vector<std::string> SoftList(const Package& a_package, const std::vector<char>& a_raw)
		{
			std::vector<std::string> out;
			const auto               count = Int(a_raw, 0);
			for (std::int32_t i = 0; i < count; ++i) {
				const auto& name = a_package.names().at(static_cast<std::size_t>(Int(a_raw, 4 + 12 * static_cast<std::size_t>(i))));
				out.push_back(WithoutClassSuffix(Leaf(name, '.')));
			}
			return out;
		}

		double Rounded5(float a_value)
		{
			char text[64];
			std::snprintf(text, sizeof(text), "%.5f", static_cast<double>(a_value));
			return std::strtod(text, nullptr);
		}

		std::vector<int> Numbers(const std::string& a_text)
		{
			std::vector<int> out;
			std::string      digits;
			for (const char c : a_text + ",") {
				if (c >= '0' && c <= '9') {
					digits.push_back(c);
				} else if (!digits.empty()) {
					out.push_back(std::atoi(digits.c_str()));
					digits.clear();
				}
			}
			return out;
		}

		std::string Lower(std::string a_text)
		{
			for (auto& c : a_text) {
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			}
			return a_text;
		}

		// One "CharacterProfiles=" line of Game.ini.
		struct ProfileLine
		{
			std::string         name, line;
			std::vector<Chosen> wearables;  // item numbers, in profile order (item holds the number as text until resolved)
			std::vector<int>    ids;
		};

		std::vector<ProfileLine> ReadProfiles(const std::filesystem::path& a_gameIni)
		{
			std::vector<ProfileLine> out;
			std::ifstream            in(a_gameIni);
			std::string              line;
			static const std::regex  nameRe(R"rx(Name=INVTEXT\("([^"]*)"\))rx");
			static const std::regex  wearRe(R"rx(Wearables=\((.*?)\),Equipment=)rx");
			static const std::regex  entryRe(R"rx(\((?:ID=(\d+))?(?:,?Colors=\(([\d,]*)\))?(?:,?Pattern=(\d+))?\))rx");
			while (std::getline(in, line)) {
				if (line.rfind("CharacterProfiles=", 0) != 0) {
					continue;
				}
				while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
					line.pop_back();
				}
				ProfileLine profile;
				profile.line = line;
				std::smatch m;
				profile.name = std::regex_search(line, m, nameRe) ? m[1].str() : "?";
				if (std::regex_search(line, m, wearRe)) {
					const std::string list = m[1].str();
					for (auto it = std::sregex_iterator(list.begin(), list.end(), entryRe); it != std::sregex_iterator(); ++it) {
						Chosen chosen;
						chosen.colours = (*it)[2].matched ? Numbers((*it)[2].str()) : std::vector<int>{};
						chosen.pattern = (*it)[3].matched ? std::atoi((*it)[3].str().c_str()) : 0;
						profile.ids.push_back((*it)[1].matched ? std::atoi((*it)[1].str().c_str()) : 0);
						profile.wearables.push_back(std::move(chosen));
					}
				}
				out.push_back(std::move(profile));
			}
			return out;
		}

		std::array<float, 3> Gamma(const std::array<double, 3>& a_linear)
		{
			std::array<float, 3> out{};
			for (int k = 0; k < 3; ++k) {
				out[static_cast<std::size_t>(k)] = static_cast<float>(std::pow(std::clamp(a_linear[static_cast<std::size_t>(k)], 0.0, 1.0), 1.0 / 2.2));
			}
			return out;
		}
	}

	const Package& Mordhau::Open(const std::string& a_base)
	{
		auto it = _open.find(a_base);
		if (it == _open.end()) {
			it = _open.emplace(a_base, std::make_unique<Package>(_paks, a_base)).first;
		}
		return *it->second;
	}

	void Mordhau::ReadSingleton()
	{
		if (_singleton) {
			return;
		}
		const auto& s = Open(kSingleton);
		std::size_t o = static_cast<std::size_t>(DefaultObject(s)->offset);
		const auto  properties = s.Properties(o);
		for (const char* slot : { "Head", "UpperChest", "Legs" }) {
			const auto* list = Find(properties, std::string(slot) + "Wearables");
			if (!list) {
				throw std::runtime_error(std::string("MORDHAU's singleton has no list of ") + slot + " wearables");
			}
			_top[slot] = SoftList(s, list->raw);
		}
		if (const auto* list = Find(properties, "Equipment")) {
			_equipment = SoftList(s, list->raw);
		}
		_singleton = true;
	}

	const std::vector<std::string>& Mordhau::Top(const std::string& a_slot)
	{
		ReadSingleton();
		return _top[a_slot];
	}

	const std::vector<std::string>& Mordhau::Equipment()
	{
		ReadSingleton();
		return _equipment;
	}

	const std::vector<std::string>& Mordhau::ItemList(const std::string& a_item, const std::string& a_key)
	{
		const auto key = std::make_pair(a_item, a_key);
		if (const auto it = _lists.find(key); it != _lists.end()) {
			return it->second;
		}
		std::vector<std::string> out;
		std::string              current = a_item;
		while (!current.empty()) {
			const auto path = _paks.FindByName(current);
			if (path.empty()) {
				break;
			}
			const auto& p = Open(path);
			const auto  properties = p.Defaults("Default__" + current + "_C");
			if (const auto* list = Find(properties, a_key)) {
				out = SoftList(p, list->raw);
				break;
			}
			const auto* cls = p.FindExport(current + "_C");
			const auto  parent = cls ? p.Object(cls->superIndex) : std::string();
			current = parent.size() > 2 && parent.compare(parent.size() - 2, 2, "_C") == 0 ? WithoutClassSuffix(parent) : std::string();
		}
		return _lists.emplace(key, std::move(out)).first->second;
	}

	std::vector<Mordhau::Link> Mordhau::Chain(const std::string& a_item)
	{
		std::vector<Link> out;
		std::string       current = a_item;
		while (!current.empty()) {
			const auto path = _paks.FindByName(current);
			if (path.empty()) {
				break;
			}
			const auto& p = Open(path);
			out.push_back({ current, &p, p.Defaults("Default__" + current + "_C") });
			const auto* cls = p.FindExport(current + "_C");
			const auto  parent = cls ? p.Object(cls->superIndex) : std::string();
			current = parent.size() > 2 && parent.compare(parent.size() - 2, 2, "_C") == 0 ? WithoutClassSuffix(parent) : std::string();
		}
		return out;
	}

	Mordhau::Merged Mordhau::Merge(const std::string& a_item)
	{
		Merged     merged;
		const auto chain = Chain(a_item);
		for (auto link = chain.rbegin(); link != chain.rend(); ++link) {  // parent first, the item itself last
			for (const auto& property : link->properties) {
				merged.all[property.name] = { link->package, property };
			}
		}
		return merged;
	}

	std::vector<Rgb> Mordhau::ColoursOf(const Package& a_package, const std::vector<char>& a_entries)
	{
		std::vector<Rgb> out;
		const auto       count = Int(a_entries, 0);
		for (std::int32_t i = 0; i < count; ++i) {
			const auto  definition = WithoutClassSuffix(a_package.Object(Int(a_entries, 4 + 4 * static_cast<std::size_t>(i))));
			const auto& c = Open(_paks.FindByName(definition));
			std::size_t o = static_cast<std::size_t>(DefaultObject(c)->offset);
			const auto  properties = c.Properties(o);
			if (const auto* colour = Find(properties, "Color"); colour && colour->raw.size() >= 12) {
				float rgb[3];
				std::memcpy(rgb, colour->raw.data(), 12);
				out.push_back(std::array<double, 3>{ Rounded5(rgb[0]), Rounded5(rgb[1]), Rounded5(rgb[2]) });
			} else {
				out.push_back(std::nullopt);
			}
		}
		return out;
	}

	const std::vector<std::vector<Rgb>>& Mordhau::ColourTables()
	{
		if (_tables) {
			return *_tables;
		}
		const auto& s = Open(kSingleton);
		std::size_t at = static_cast<std::size_t>(DefaultObject(s)->offset);
		const auto  properties = s.Properties(at);
		const auto* tables = Find(properties, "ColorTables");
		if (!tables) {
			throw std::runtime_error("MORDHAU's singleton has no colour tables");
		}
		const auto& raw = tables->raw;
		const auto  count = Int(raw, 0);
		// one inner tag (name, type, size, array index, struct name, guid, flag), then each table's own properties
		std::size_t o = 4;
		(void)s.Name(raw, o);
		(void)s.Name(raw, o);
		o += 8;
		(void)s.Name(raw, o);
		o += 17;
		std::vector<std::vector<Rgb>> out;
		for (std::int32_t i = 0; i < count; ++i) {
			const auto  sub = s.Properties(raw, o);
			const auto* entries = Find(sub, "Entries");
			out.push_back(entries ? ColoursOf(s, entries->raw) : std::vector<Rgb>{});
		}
		_tables = std::move(out);
		return *_tables;
	}

	std::vector<Rgb> Mordhau::SingleTable(const std::string& a_property)
	{
		const auto& s = Open(kSingleton);
		std::size_t at = static_cast<std::size_t>(DefaultObject(s)->offset);
		const auto  properties = s.Properties(at);
		const auto* table = Find(properties, a_property);
		if (!table) {
			throw std::runtime_error("MORDHAU's singleton has no " + a_property);
		}
		std::size_t o = 0;
		const auto  sub = s.Properties(table->raw, o);
		const auto* entries = Find(sub, "Entries");
		return entries ? ColoursOf(s, entries->raw) : std::vector<Rgb>{};
	}

	Spec Mordhau::Build(const std::string& a_name, const std::vector<std::pair<std::string, Chosen>>& a_entries)
	{
		const auto& tables = ColourTables();
		Spec        spec;
		spec.name = a_name;
		for (const auto& [slot, chosen] : a_entries) {
			const auto merged = Merge(chosen.item);
			SpecPiece  piece;
			piece.slot = slot;
			piece.item = chosen.item;
			if (const auto* mesh = merged.Get("Mesh")) {
				auto path = WithoutGame(merged.Owner("Mesh")->names().at(static_cast<std::size_t>(Int(mesh->raw, 0))));
				piece.model = path.substr(0, path.find('.'));
			}
			const Value  patterns = merged.Decode("Patterns");
			const Value* pattern = nullptr;
			if (patterns.IsList() && !patterns.list.empty()) {
				pattern = static_cast<std::size_t>(chosen.pattern) < patterns.list.size() ? &patterns.list[static_cast<std::size_t>(chosen.pattern)] : &patterns.list[0];
			}
			// which colour table each colour slot of the item uses (an item with none of its own: Cloth for both)
			std::vector<int> itemTables{ 0, 0 };
			if (const auto* own = merged.Get("ColorTables")) {
				itemTables.clear();
				for (std::size_t i = 4; i < own->raw.size(); ++i) {
					itemTables.push_back(static_cast<unsigned char>(own->raw[i]));
				}
			}
			for (std::size_t i = 0; i < chosen.colours.size(); ++i) {
				Dye dye;
				// mask channel per colour: colour 1 = red, colour 2 = blue; green is the metal and takes the metal tint
				static const int channels[3]{ 0, 2, 1 };
				dye.channel = i < 3 ? channels[i] : -1;
				if (!itemTables.empty()) {
					const auto table = static_cast<std::size_t>(i < itemTables.size() ? itemTables[i] : itemTables.back());
					const auto colour = static_cast<std::size_t>(chosen.colours[i]);
					if (table < tables.size() && colour < tables[table].size()) {
						dye.colour = tables[table][colour];
					}
				}
				if (pattern) {
					const auto* has = pattern->Get("bHasColor" + std::to_string(i + 1));
					dye.used = has && has->Truthy();
				}
				piece.dye.push_back(dye);
			}
			if (const auto* emblem = merged.Get("bHasEmblem"); emblem && emblem->flag) {
				// where the emblem sits in the item's own texture space: a square `scale` wide centred at
				// 0.5 - offset x scale, with the whole texture space turned about its middle by the item's rotation
				double     scale[3]{ 1.0, 1.0, 1.0 }, offset[3]{ 0.0, 0.0, 0.0 };
				const auto read = [&](const char* a_key, double* a_into) {
					if (merged.Get(a_key)) {
						const auto v = merged.Decode(a_key);
						for (std::size_t k = 0; k < 3 && k < v.list.size(); ++k) {
							a_into[k] = v.list[k].number;
						}
					}
				};
				read("EmblemScale", scale);
				read("EmblemOffset", offset);
				const double turns = merged.Get("EmblemRotation") ? merged.Decode("EmblemRotation").number : 0.0;
				const double pi = 3.141592653589793;
				const double cx = -offset[0] * scale[0], cy = -offset[1] * (scale[1] != 0.0 ? scale[1] : scale[0]);
				const double ca = std::cos(turns * 2 * pi), sa = std::sin(turns * 2 * pi);
				piece.hasEmblem = true;
				piece.emblemCentre[0] = 0.5 + cx * ca - cy * sa;
				piece.emblemCentre[1] = 0.5 + cx * sa + cy * ca;
				piece.emblemScale[0] = scale[0];
				piece.emblemScale[1] = scale[1] != 0.0 ? scale[1] : scale[0];
				piece.emblemTurns = turns;
			}
			for (const auto& [key, owned] : merged.all) {  // (in name order, as the tools sorted them)
				if (key.rfind("bHide", 0) == 0 && key != "bHideIn1P" && owned.second.type == "BoolProperty" && owned.second.flag) {
					piece.hide.push_back(key);
				}
			}
			const auto map = [&](const char* a_key) {
				if (!merged.Get(a_key)) {
					return std::string();
				}
				const auto v = merged.Decode(a_key);
				return v.IsText() ? merged.Owner(a_key)->TexturePath(v.text) : std::string();
			};
			piece.colourMap = map("AlbedoMap");
			piece.normalMap = map("NormalMap");
			piece.surfaceMap = map("RoughnessMap");  // MORDHAU's "RMA" map: red roughness, green metal, blue ambient occlusion
			if (pattern) {
				if (const auto* texture = pattern->Get("Texture"); texture && texture->IsText()) {
					piece.patternMask = merged.Owner("Patterns")->TexturePath(texture->text);
				}
			}
			spec.pieces.push_back(std::move(piece));
		}
		return spec;
	}

	bool Mordhau::Outfit(const std::string& a_name, const std::map<std::string, std::string>& a_wanted, int a_colour1, int a_colour2, Spec& a_out, std::string& a_log)
	{
		// the wearable named for a slot from that slot's own list; the list's first entry when "" or not offered
		const auto pick = [&](const std::vector<std::string>& a_options, const char* a_slot) -> std::string {
			if (a_options.empty()) {
				return {};
			}
			const auto it = a_wanted.find(a_slot);
			if (it != a_wanted.end() && !it->second.empty()) {
				const auto item = "BP_" + it->second;
				if (std::find(a_options.begin(), a_options.end(), item) != a_options.end()) {
					return item;
				}
				a_log += "  " + a_name + ": " + a_slot + " \"" + it->second + "\" is not offered here; using " + a_options[0] + "\n";
			}
			return a_options[0];
		};
		std::map<std::string, std::string> items;
		items["Head"] = pick(Top("Head"), "Head");
		items["UpperChest"] = pick(Top("UpperChest"), "UpperChest");
		items["Legs"] = pick(Top("Legs"), "Legs");
		items["Arms"] = pick(ItemList(items["UpperChest"], "ArmsWearables"), "Arms");
		items["Coif"] = pick(ItemList(items["Head"], "CoifWearables"), "Coif");
		items["LowerChest"] = pick(ItemList(items["UpperChest"], "LowerChestWearables"), "LowerChest");
		items["Shoulders"] = pick(ItemList(items["UpperChest"], "ShouldersWearables"), "Shoulders");
		items["Hands"] = items["Arms"].empty() ? std::string() : pick(ItemList(items["Arms"], "HandsWearables"), "Hands");
		items["Feet"] = pick(ItemList(items["Legs"], "FeetWearables"), "Feet");
		std::vector<std::pair<std::string, Chosen>> entries;
		for (const char* slot : kOrder) {
			if (items[slot].empty()) {
				a_log += a_name + ": NOT baked, no wearable for " + slot + "\n";
				return false;
			}
			entries.push_back({ slot, Chosen{ items[slot], { a_colour1, a_colour2 }, 0 } });
		}
		a_out = Build(a_name, entries);
		// no profile: no emblem, the first metal tint
		const auto metal = SingleTable("MetalTintsColorTable");
		for (auto& piece : a_out.pieces) {
			piece.metalTint = Dye{ 1, metal.empty() ? std::nullopt : metal[0], true };
			piece.hasEmblem = false;
		}
		return true;
	}

	std::vector<std::pair<std::string, std::string>> Mordhau::Profiles(const std::filesystem::path& a_gameIni)
	{
		std::vector<std::pair<std::string, std::string>> out;
		for (const auto& profile : ReadProfiles(a_gameIni)) {
			out.emplace_back(profile.name, profile.ids.size() >= 9 ? profile.line : std::string());
		}
		return out;
	}

	int Mordhau::ProfileIndex(const std::filesystem::path& a_gameIni, const std::string& a_name)
	{
		const auto profiles = ReadProfiles(a_gameIni);
		for (std::size_t i = 0; i < profiles.size(); ++i) {
			if (Lower(profiles[i].name) == Lower(a_name)) {
				return static_cast<int>(i);
			}
		}
		return -1;
	}

	Spec Mordhau::Profile(const std::filesystem::path& a_gameIni, const std::string& a_name)
	{
		const auto         profiles = ReadProfiles(a_gameIni);
		const ProfileLine* me = nullptr;
		for (const auto& profile : profiles) {
			if (Lower(profile.name) == Lower(a_name)) {
				me = &profile;
				break;
			}
		}
		if (!me) {
			throw std::runtime_error("no mercenary called " + a_name + " in MORDHAU's Game.ini");
		}
		if (me->ids.size() < 9) {
			throw std::runtime_error("the mercenary " + a_name + " has fewer than nine wearable entries");
		}
		// item numbers -> definitions: top-level lists first, then the lists that hang on the items chosen
		std::map<std::string, int> number;
		for (int i = 0; i < 9; ++i) {
			number[kOrder[i]] = me->ids[static_cast<std::size_t>(i)];
		}
		static const std::map<std::string, std::pair<const char*, const char*>> parents{ { "Coif", { "Head", "CoifWearables" } }, { "LowerChest", { "UpperChest", "LowerChestWearables" } },
			{ "Arms", { "UpperChest", "ArmsWearables" } }, { "Shoulders", { "UpperChest", "ShouldersWearables" } }, { "Hands", { "Arms", "HandsWearables" } }, { "Feet", { "Legs", "FeetWearables" } } };
		std::map<std::string, std::string> resolved;
		for (const char* slot : { "Head", "UpperChest", "Legs", "Coif", "LowerChest", "Arms", "Shoulders", "Hands", "Feet" }) {
			const auto  parent = parents.find(slot);
			const auto& list = parent == parents.end() ? Top(slot) : ItemList(resolved[parent->second.first], parent->second.second);
			const auto  index = static_cast<std::size_t>(number[slot]);
			if (index >= list.size()) {
				throw std::runtime_error("the mercenary " + a_name + " names a " + slot + " MORDHAU does not list");
			}
			resolved[slot] = list[index];
		}
		std::vector<std::pair<std::string, Chosen>> entries;
		for (int i = 0; i < 9; ++i) {
			Chosen chosen = me->wearables[static_cast<std::size_t>(i)];
			chosen.item = resolved[kOrder[i]];
			entries.push_back({ kOrder[i], std::move(chosen) });
		}
		Spec spec = Build(me->name, entries);

		const std::string& line = me->line;
		std::smatch        m;
		// the metal tint
		int                     tint = 0;
		static const std::regex appearanceRe(R"rx(AppearanceCustomization=\((.*?)\),FaceCustomization)rx");
		if (std::regex_search(line, m, appearanceRe)) {
			const std::string       appearance = m[1].str();
			static const std::regex pairRe(R"rx((\w+)=([\w.]+))rx");
			for (auto it = std::sregex_iterator(appearance.begin(), appearance.end(), pairRe); it != std::sregex_iterator(); ++it) {
				if ((*it)[1].str() == "MetalTint") {
					tint = std::atoi((*it)[2].str().c_str());
				}
			}
		}
		const auto metal = SingleTable("MetalTintsColorTable");
		for (auto& piece : spec.pieces) {
			piece.metalTint = Dye{ 1, static_cast<std::size_t>(tint) < metal.size() ? metal[static_cast<std::size_t>(tint)] : std::nullopt, true };
		}
		// the weapons carried
		std::vector<std::string> weapons;
		std::string              equipment;
		static const std::regex  equipmentRe(R"rx(Equipment=\((.*?)\)\),AppearanceCustomization)rx");
		const bool               hasEquipment = std::regex_search(line, m, equipmentRe);
		if (hasEquipment) {
			equipment = m[1].str();
			static const std::regex idRe(R"rx(\(ID=(\d+))rx");
			for (auto it = std::sregex_iterator(equipment.begin(), equipment.end(), idRe); it != std::sregex_iterator(); ++it) {
				const auto index = static_cast<std::size_t>(std::atoi((*it)[1].str().c_str()));
				if (index >= Equipment().size()) {
					throw std::runtime_error("the mercenary " + a_name + " carries equipment MORDHAU does not list");
				}
				weapons.push_back(Equipment()[index]);
			}
		}
		// the emblem: its texture from MORDHAU's emblem list, its two colours from the emblem colour table
		std::string emblemTexture;
		Rgb         emblemColours[2];
		try {
			static const std::regex emblemRe(R"rx(Emblem=(\d+))rx"), coloursRe(R"rx(EmblemColors=\(([\d,]*)\))rx");
			if (!std::regex_search(line, m, emblemRe)) {
				throw std::runtime_error("no emblem");
			}
			const auto emblemNumber = static_cast<std::size_t>(std::atoi(m[1].str().c_str()));
			if (!std::regex_search(line, m, coloursRe)) {
				throw std::runtime_error("no emblem colours");
			}
			const auto  picked = Numbers(m[1].str());
			const auto& single = Open(kSingleton);
			const auto  defaults = single.Defaults(DefaultObject(single)->name);
			const auto* list = Find(defaults, "Emblems");
			const auto  emblems = list ? single.Decode(*list) : Value{};
			if (!emblems.IsList() || emblemNumber >= emblems.list.size() || !emblems.list[emblemNumber].IsText()) {
				throw std::runtime_error("emblem not listed");
			}
			const auto& package = Open(_paks.FindByName(WithoutClassSuffix(emblems.list[emblemNumber].text)));
			for (std::size_t i = 0; i < package.imports().size() && emblemTexture.empty(); ++i) {
				if (package.imports()[i].className == "Texture2D") {
					emblemTexture = WithoutGame(package.ImportPath(i));
				}
			}
			if (emblemTexture.empty()) {
				throw std::runtime_error("emblem without a texture");
			}
			const auto table = SingleTable("EmblemColorTable");
			for (std::size_t i = 0; i < 2 && i < picked.size(); ++i) {
				if (static_cast<std::size_t>(picked[i]) < table.size()) {
					emblemColours[i] = table[static_cast<std::size_t>(picked[i])];
				}
			}
		} catch (const std::exception&) {
			emblemTexture.clear();
		}
		for (auto& piece : spec.pieces) {
			if (piece.hasEmblem && !emblemTexture.empty()) {
				piece.emblemTexture = emblemTexture;
				piece.emblemColours[0] = emblemColours[0];
				piece.emblemColours[1] = emblemColours[1];
			} else {
				piece.hasEmblem = false;
			}
		}
		// the first weapon, in the make the profile gives it
		if (hasEquipment && !weapons.empty()) {
			static const std::regex firstRe(R"rx(^\(ID=(\d+)(?:,Colors=\(([\d,]*)\))?(?:,Parts=\(([\d,]*)\))?)rx");
			if (std::regex_search(equipment, m, firstRe)) {
				spec.heldWeapon = WeaponPieces(weapons[0], m[3].matched ? Numbers(m[3].str()) : std::vector<int>{}, m[2].matched ? Numbers(m[2].str()) : std::vector<int>{});
			}
		}
		return spec;
	}

	std::vector<SpecPiece> Mordhau::WeaponPieces(const std::string& a_weapon, const std::vector<int>& a_parts, const std::vector<int>& a_colours, int a_skin)
	{
		std::vector<SpecPiece> out;
		const auto             merged = Merge(a_weapon);
		const Value            skins = merged.Decode("Skins");
		if (!skins.IsList() || skins.list.empty()) {
			return out;
		}
		const auto& skin = skins.list[std::min<std::size_t>(static_cast<std::size_t>(a_skin), skins.list.size() - 1)];
		const auto& tables = ColourTables();
		std::vector<int> itemTables{ 0, 0, 0 };
		if (const auto* own = skin.Get("ColorTables"); own && own->IsList()) {
			itemTables.clear();
			for (const auto& v : own->list) {
				itemTables.push_back(static_cast<int>(v.number));
			}
		}
		std::vector<Dye> tints;
		for (std::size_t i = 0; i < 3; ++i) {
			Dye        dye;
			const auto table = static_cast<std::size_t>(i < itemTables.size() ? itemTables[i] : 0);
			const auto colour = static_cast<std::size_t>(i < a_colours.size() ? a_colours[i] : 0);
			dye.channel = static_cast<int>(i);
			dye.used = true;
			if (table < tables.size() && colour < tables[table].size()) {
				dye.colour = tables[table][colour];
			}
			tints.push_back(dye);
		}
		const auto* partTypes = skin.Get("PartTypes");
		if (!partTypes || !partTypes->IsList()) {
			return out;
		}
		for (std::size_t i = 0; i < partTypes->list.size(); ++i) {
			const auto* options = partTypes->list[i].Get("Parts");
			if (!options || !options->IsList() || options->list.empty()) {
				continue;
			}
			const auto        wanted = static_cast<std::size_t>(i < a_parts.size() ? a_parts[i] : 0);
			const std::string part = WithoutClassSuffix(options->list[std::min(wanted, options->list.size() - 1)].text);
			const auto&       q = Open(_paks.FindByName(part));
			const auto        defaults = q.Defaults("Default__" + part + "_C");
			const auto*       meshProperty = Find(defaults, "SkeletalMesh");
			const auto        meshName = meshProperty ? q.Decode(*meshProperty) : Value{};
			std::string       mesh;
			for (const auto& name : q.names()) {
				if (meshName.IsText() && name.rfind("/Game/", 0) == 0 && Leaf(name) == meshName.text) {
					mesh = WithoutGame(name);
					break;
				}
			}
			if (mesh.empty() && meshName.IsText()) {
				// (a model name ending in _<number>, no leading zero: the name table has its package path under the base
				// name only - the Scimitar's and the Poleaxe's parts)
				const auto& text = meshName.text;
				const auto  cut = text.rfind('_');
				if (cut != std::string::npos && cut + 1 < text.size() && std::all_of(text.begin() + static_cast<std::ptrdiff_t>(cut) + 1, text.end(), [](unsigned char c) { return std::isdigit(c) != 0; }) &&
					(text[cut + 1] != '0' || cut + 2 == text.size())) {
					const auto base = text.substr(0, cut);
					for (const auto& name : q.names()) {
						if (name.rfind("/Game/", 0) == 0 && Leaf(name) == base) {
							mesh = WithoutGame(name) + text.substr(cut);
							break;
						}
					}
				}
			}
			if (mesh.empty()) {
				continue;
			}
			SpecPiece piece;
			piece.item = part;
			piece.model = mesh;
			piece.dye = tints;
			const auto& mp = Open(mesh);
			std::string materialName;
			for (const auto& name : mp.names()) {
				// (a part's material: M_... / MI_... for weapons, INST_... for shields)
				if (name.rfind("/Game/", 0) == 0 && (Leaf(name).rfind("M", 0) == 0 || Leaf(name).rfind("INST_", 0) == 0)) {
					materialName = name;
					break;
				}
			}
			if (!materialName.empty()) {
				const auto& material = Open(WithoutGame(materialName));
				const auto  materialDefaults = material.Defaults(Leaf(materialName));
				if (const auto* parameters = Find(materialDefaults, "TextureParameterValues")) {
					const auto values = material.Decode(*parameters);
					for (const auto& value : values.list) {
						const auto* info = value.Get("ParameterInfo");
						const auto* name = info ? info->Get("Name") : nullptr;
						const auto* texture = value.Get("ParameterValue");
						if (!name || !name->IsText()) {
							continue;
						}
						const auto path = texture && texture->IsText() ? material.TexturePath(texture->text) : std::string();
						if (name->text == "AlbedoMap") {
							piece.colourMap = path;
						} else if (name->text == "ColorMap") {
							piece.patternMask = path;
						} else if (name->text == "NormalMap") {
							piece.normalMap = path;
						} else if (name->text == "RoughnessMap") {
							piece.surfaceMap = path;
						}
					}
				}
			}
			out.push_back(std::move(piece));
		}
		return out;
	}

	std::array<std::string, 3> Mordhau::TexturesBySuffix(const std::string& a_material)
	{
		std::vector<std::string> names;
		for (const auto& name : Open(a_material).names()) {
			if (name.rfind("/Game/", 0) == 0 && name.find('.') == std::string::npos) {
				names.push_back(name.substr(6));
			}
		}
		const auto pick = [&](std::initializer_list<const char*> a_endings) {
			for (const auto& name : names) {
				const auto lower = Lower(name);
				for (const char* ending : a_endings) {
					const auto length = std::strlen(ending);
					if (lower.size() >= length && lower.compare(lower.size() - length, length, ending) == 0) {
						return name;
					}
				}
			}
			return std::string();
		};
		return { pick({ "_d", "_albedo", "_basecolor", "_diffuse", "_bc", "_color", "_diff" }), pick({ "_n", "_normal", "_nrm" }), pick({ "_rma", "_orm", "_mrao" }) };
	}

	std::vector<SpecPiece> Mordhau::DefaultWeapon(const std::string& a_weapon)
	{
		std::vector<SpecPiece> out;
		for (auto piece : WeaponPieces(a_weapon, std::vector<int>(8, 0), { 0, 0, 0 })) {
			if (piece.model.empty()) {
				continue;
			}
			if (piece.colourMap.empty()) {
				// the part's material does not name its maps the usual way: take them by their file-name endings from
				// whichever material (or its parent) the model lists
				std::set<std::string>    seen;
				std::vector<std::string> todo;
				for (const auto& name : Open(piece.model).names()) {
					if (name.rfind("/Game/", 0) == 0 && name.find('.') == std::string::npos && name.substr(6) != piece.model) {
						todo.push_back(name.substr(6));
					}
				}
				while (!todo.empty() && piece.colourMap.empty()) {
					const auto candidate = todo.front();
					todo.erase(todo.begin());
					if (!seen.insert(candidate).second) {
						continue;
					}
					std::array<std::string, 3> found;
					try {
						found = TexturesBySuffix(candidate);
					} catch (const std::exception&) {
						continue;
					}
					if (!found[0].empty()) {
						piece.colourMap = found[0];
						if (piece.normalMap.empty()) {
							piece.normalMap = found[1];
						}
						piece.surfaceMap.clear();
					} else {
						try {
							for (const auto& name : Open(candidate).names()) {
								const auto leaf = Leaf(name);
								if (name.rfind("/Game/", 0) == 0 && name.find('.') == std::string::npos && (name.find("/Materials/") != std::string::npos || leaf.rfind("M", 0) == 0 || leaf.rfind("INST_", 0) == 0)) {
									todo.push_back(name.substr(6));
								}
							}
						} catch (const std::exception&) {
						}
					}
				}
			}
			if (!piece.colourMap.empty()) {
				out.push_back(std::move(piece));
			}
		}
		return out;
	}

	std::vector<Piece> ToPieces(const Spec& a_spec)
	{
		std::vector<Piece> out;
		if (!a_spec.noBody) {
			Piece body;
			body.model = kBody;
			body.body = true;
			body.perMaterial = true;
			body.byMaterial = { { "INST_Torso", "UMA/Textures/MaleBody01_d" }, { "INST_HeadMaleCaucasian01", "UMA/Separated/MaleCaucasian01/MaleHead01_d" } };
			std::set<std::string> hidden;
			for (const auto& piece : a_spec.pieces) {
				for (const auto& hide : piece.hide) {
					hidden.insert(hide.substr(5));  // "bHideChest" -> "Chest"
				}
			}
			body.hide.assign(hidden.begin(), hidden.end());
			out.push_back(std::move(body));
		}
		const auto tint = [](Piece& a_piece, const Dye& a_dye) {
			if (a_dye.colour && a_dye.channel >= 0 && a_dye.channel < 3) {
				const auto colour = Gamma(*a_dye.colour);
				std::memcpy(a_piece.tints + a_dye.channel * 3, colour.data(), 12);
			}
		};
		for (const auto& source : a_spec.pieces) {
			if (source.model.empty() || source.colourMap.empty()) {
				continue;
			}
			Piece piece;
			piece.model = source.model;
			piece.colourMap = source.colourMap;
			piece.patternMask = source.patternMask;
			piece.normalMap = source.normalMap;
			piece.surfaceMap = source.surfaceMap;
			for (const auto& dye : source.dye) {
				if (dye.used) {
					tint(piece, dye);
				}
			}
			if (source.metalTint) {
				tint(piece, *source.metalTint);
			}
			if (source.hasEmblem && !source.emblemTexture.empty()) {
				piece.emblem = source.emblemTexture;
				piece.emblemValues[0] = static_cast<float>(source.emblemCentre[0]);
				piece.emblemValues[1] = static_cast<float>(source.emblemCentre[1]);
				piece.emblemValues[2] = static_cast<float>(source.emblemScale[0]);
				piece.emblemValues[3] = static_cast<float>(source.emblemScale[1]);
				piece.emblemValues[4] = static_cast<float>(source.emblemTurns);
				for (int c = 0; c < 2; ++c) {
					const std::array<float, 3> colour = source.emblemColours[c] ? Gamma(*source.emblemColours[c]) : std::array<float, 3>{ 1.0f, 1.0f, 1.0f };
					std::memcpy(piece.emblemValues + 5 + c * 3, colour.data(), 12);
				}
			}
			out.push_back(std::move(piece));
		}
		for (const auto& source : a_spec.heldWeapon) {
			if (source.model.empty() || source.colourMap.empty()) {
				continue;
			}
			Piece piece;
			piece.model = source.model;
			piece.colourMap = source.colourMap;
			piece.patternMask = source.patternMask;
			piece.normalMap = source.normalMap;
			piece.surfaceMap = source.surfaceMap;
			piece.weapon = true;
			for (const auto& dye : source.dye) {
				tint(piece, dye);
			}
			out.push_back(std::move(piece));
		}
		return out;
	}
}
