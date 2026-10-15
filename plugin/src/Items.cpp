#include "Items.h"

#include "gen/Sheets.h"

namespace dm::Items
{
	namespace
	{
		constexpr RE::FormID                              kFirstId = 0xFF7E1000;
		std::unordered_map<RE::FormID, const char*>       g_paths;
		bool                                              g_done = false;
		std::vector<RE::TESObjectWEAP*>                   g_all;

		RE::TESObjectWEAP* Named(const RE::BSTArray<RE::TESObjectWEAP*>& a_weapons, std::string_view a_name)
		{
			for (auto* weapon : a_weapons) {
				if (weapon && weapon->GetPlayable() && !weapon->formEnchanting && weapon->GetName() && a_name == weapon->GetName()) {
					return weapon;
				}
			}
			return nullptr;
		}
	}

	void EnsureCreated()
	{
		if (g_done) {
			return;
		}
		auto* data = RE::TESDataHandler::GetSingleton();
		if (!data) {
			return;
		}
		auto& weapons = data->GetFormArray<RE::TESObjectWEAP>();
		if (weapons.size() < 200 || !Named(weapons, "Iron Sword")) {
			return;  // (the game's data is not loaded yet)
		}
		g_done = true;
		auto* factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::TESObjectWEAP>();
		if (!factory) {
			logger::warn("items: no weapon factory; MORDHAU weapons are not added as Skyrim items");
			return;
		}
		// which Skyrim weapon each item sits beside in the lists
		std::unordered_map<RE::TESForm*, std::vector<RE::TESObjectWEAP*>> beside;
		RE::FormID                                                        id = kFirstId;
		int                                                               made = 0, fromFiles = 0;
		for (const auto& row : sheets::kMordhauItems) {
			const RE::FormID mine = id++;
			RE::TESObjectWEAP*              like = nullptr;
			std::vector<RE::TESObjectWEAP*> all;
			std::string_view                names = row.skyrimLike;
			while (!names.empty()) {
				const auto bar = names.find('|');
				if (auto* found = Named(weapons, names.substr(0, bar))) {
					like = like ? like : found;
					all.push_back(found);
				}
				names = bar == std::string_view::npos ? std::string_view{} : names.substr(bar + 1);
			}
			if (!like) {
				logger::warn("items: no Skyrim weapon '{}' to make {} from", row.skyrimLike, row.name);
				continue;
			}
			// The item itself comes from MordhauWeapons.esp (tools/make_items_esp.py: a light plugin made from the
			// player's own Skyrim.esm, id 0x800 + row). Only if that file is not loaded is it made here - and then it
			// does not survive a save and reload (tested: 0 of 24 left in the inventory after a quickload).
			auto* item = data->LookupForm<RE::TESObjectWEAP>(0x800 + (mine - kFirstId), "MordhauWeapons.esp");
			const bool fromFile = item != nullptr;
			if (!item) {
				item = factory->Create();
				if (!item) {
					continue;
				}
				item->Copy(like);
				if (!item->GetModel() || !*item->GetModel()) {
					std::memcpy(reinterpret_cast<char*>(item) + sizeof(RE::TESForm), reinterpret_cast<char*>(like) + sizeof(RE::TESForm), sizeof(RE::TESObjectWEAP) - sizeof(RE::TESForm));
				}
				item->SetFormID(mine, false);
				item->fullName = row.name;
				item->attackDamage = static_cast<std::uint16_t>(row.damage);
				item->weight = row.weight;
				item->value = row.value;
			}
			fromFiles += fromFile ? 1 : 0;
			g_paths[item->GetFormID()] = row.mordhauWeapon;
			g_all.push_back(item);
			for (auto* other : all) {
				beside[other].push_back(item);
			}
			++made;
			logger::info("items: {} ({:08X}{}) like {}: damage {}, weight {:.1f}, value {}, model {}", row.name, item->GetFormID(), fromFile ? ", MordhauWeapons.esp" : ", made now: NOT kept by saves", like->GetName(), item->attackDamage, item->weight, item->value,
				item->GetModel() ? item->GetModel() : "?");
		}
		// into every levelled list that holds one of those Skyrim weapons, at the same level and count
		int lists = 0, added = 0;
		for (auto* list : data->GetFormArray<RE::TESLevItem>()) {
			if (!list || list->numEntries == 0) {
				continue;
			}
			std::vector<RE::LEVELED_OBJECT> entries;
			for (std::uint8_t i = 0; i < list->numEntries; ++i) {
				entries.push_back(list->entries[i]);
			}
			const std::size_t before = entries.size();
			for (std::size_t i = 0; i < before; ++i) {
				const auto found = beside.find(entries[i].form);
				if (found == beside.end()) {
					continue;
				}
				for (auto* item : found->second) {
					if (entries.size() >= 250) {
						break;
					}
					RE::LEVELED_OBJECT extra = entries[i];
					extra.form = item;
					extra.itemExtra = nullptr;
					entries.push_back(extra);
				}
			}
			if (entries.size() == before) {
				continue;
			}
			std::stable_sort(entries.begin(), entries.end(), [](const RE::LEVELED_OBJECT& a, const RE::LEVELED_OBJECT& b) { return a.level < b.level; });
			list->entries.resize(entries.size());
			for (std::size_t i = 0; i < entries.size(); ++i) {
				list->entries[i] = entries[i];
			}
			list->numEntries = static_cast<std::uint8_t>(entries.size());
			++lists;
			added += static_cast<int>(entries.size() - before);
		}
		logger::info("items: {} MORDHAU weapons are Skyrim items ({} from MordhauWeapons.esp); {} entries added to {} levelled lists (loot, shops, what people carry)", made, fromFiles, added, lists);
	}

	const std::vector<RE::TESObjectWEAP*>& All()
	{
		return g_all;
	}

	const char* MordhauWeaponOf(const RE::TESForm* a_weapon)
	{
		if (!a_weapon) {
			return nullptr;
		}
		const auto found = g_paths.find(a_weapon->GetFormID());
		return found == g_paths.end() ? nullptr : found->second;
	}
}
