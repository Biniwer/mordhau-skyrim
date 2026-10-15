#include "CharacterRenderInternal.h"

namespace dm::CharacterRender
{
	inline namespace internal
	{
		int LookCount()
		{
			static bool read = false;
			if (!read) {
				read = true;
				std::ifstream in(g_cache / "enemies" / "looks.txt");
				int           number = 0;
				std::string   lookClass, id;
				while (in >> number >> lookClass >> id) {
					std::error_code ec;
					if (number >= 1 && number <= kMaxLooks && lookClass == "naked") {
						g_nakedLook = number;  // MORDHAU's bare body: the player with nothing on in Skyrim (nobody else gets it)
						continue;
					}
					if (number >= 1 && number <= kMaxLooks && std::filesystem::exists(g_cache / "enemies" / std::to_string(number) / "character.dmchar", ec)) {
						LookInfo info{ number, lookClass == "heavy" ? kHeavy : lookClass == "light" ? kLight : lookClass == "robe" ? kRobe : kCivilian, {} };
						const bool byName = lookClass.rfind("who:", 0) == 0;
						if (lookClass.rfind("wears:", 0) == 0 || byName) {
							info.lookClass = -1;
							std::string text = lookClass.substr(byName ? 4 : 6);
							for (auto& c : text) {
								c = c == '_' ? ' ' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
							}
							// (several texts separated by |: any one is enough)
							for (std::size_t from = 0; from <= text.size();) {
								const auto bar = std::min(text.find('|', from), text.size());
								if (bar > from) {
									(byName ? info.who : info.wears).push_back(text.substr(from, bar - from));
								}
								from = bar + 1;
							}
						}
						if (lookClass.rfind("creature:", 0) == 0) {
							info.lookClass = -2;
							info.creature = lookClass.substr(9);
						}
						g_lookList.push_back(info);
					}
				}
				logger::info("everyone: {} baked outfit(s) to dress Skyrim's people in", g_lookList.size());
			}
			return static_cast<int>(g_lookList.size());
		}

		// The outfit for a Skyrim body armour (null = nothing worn): a standard outfit when its name matches one
		// ("wears:" rows), else one of its kind (clothes, a robe, light or heavy armour) picked by a_key.
		int OutfitFor(RE::TESObjectARMO* a_armour, std::uint32_t a_key, const char** a_kind)
		{
			int         lookClass = kCivilian;
			std::string name;
			if (a_armour) {
				name = a_armour->GetName() ? a_armour->GetName() : "";
				for (auto& c : name) {
					c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
				}
				for (const auto& look : g_lookList) {
					for (const auto& text : look.wears) {
						if (name.find(text) != std::string::npos) {
							if (a_kind) {
								*a_kind = "a standard outfit";
							}
							return look.number;
						}
					}
				}
				if (a_armour->IsHeavyArmor()) {
					lookClass = kHeavy;
				} else if (a_armour->IsLightArmor()) {
					lookClass = kLight;
				} else if (name.find("robe") != std::string::npos) {
					lookClass = kRobe;
				}
			}
			if (a_kind) {
				*a_kind = lookClass == kHeavy ? "heavy armour" : lookClass == kLight ? "light armour" : lookClass == kRobe ? "a robe" : "clothes";
			}
			std::vector<int> fits;
			for (const auto& look : g_lookList) {
				if (look.lookClass == lookClass) {
					fits.push_back(look.number);
				}
			}
			return fits.empty() ? 0 : fits[((a_key * 2654435761u) >> 12) % fits.size()];
		}

		// Dev aid: with a file cache/list_armour.txt present, cache/armour_matches.txt is written once: every body armour
		// of the loaded game by name and the outfit it gets (to check the "wears:" texts against Skyrim's real names).
		void ListArmourMatches()
		{
			static bool done = false;
			std::error_code ec;
			if (done || !std::filesystem::exists(g_cache / "list_armour.txt", ec) || LookCount() == 0) {
				done = true;
				return;
			}
			done = true;
			std::map<std::string, std::string> lines;
			for (auto* armour : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESObjectARMO>()) {
				if (armour && armour->HasPartOf(RE::BGSBipedObjectForm::BipedObjectSlot::kBody) && armour->GetName() && *armour->GetName()) {
					const char* kind = "";
					const int   number = OutfitFor(armour, armour->GetFormID(), &kind);
					lines[armour->GetName()] = std::to_string(number) + "\t" + kind;
				}
			}
			{
				// (same dev aid: what Skyrim offers for blocking things and for fire - statics, hazards, explosions by model name)
				std::ofstream forms(g_cache / "forms_list.txt");
				const auto    lower = [](std::string a_text) {
					for (auto& c : a_text) {
						c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
					}
					return a_text;
				};
				for (auto* form : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::BGSHazard>()) {
					if (form) {
						forms << "HAZARD\t" << std::hex << form->GetFormID() << std::dec << "\t" << (form->GetName() ? form->GetName() : "") << "\t" << (form->GetModel() ? form->GetModel() : "") << "\t"
							  << form->data.radius << "\t" << form->data.lifetime << "\t" << form->data.targetInterval << "\t" << (form->data.spell ? form->data.spell->GetName() : "") << "\n";
					}
				}
				for (auto* form : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::BGSExplosion>()) {
					if (form && form->GetModel() && lower(form->GetModel()).find("fire") != std::string::npos) {
						forms << "EXPLOSION\t" << std::hex << form->GetFormID() << std::dec << "\t" << (form->GetName() ? form->GetName() : "") << "\t" << form->GetModel() << "\n";
					}
				}
				for (auto* form : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESObjectSTAT>()) {
					if (form && form->GetModel()) {
						const auto model = lower(form->GetModel());
						for (const char* word : { "barricade", "palisade", "crate", "fencepost", "collision", "plank" }) {
							if (model.find(word) != std::string::npos) {
								forms << "STATIC\t" << std::hex << form->GetFormID() << std::dec << "\t" << form->GetModel() << "\t" << form->boundData.boundMin.x << "," << form->boundData.boundMin.y << ","
									  << form->boundData.boundMin.z << " to " << form->boundData.boundMax.x << "," << form->boundData.boundMax.y << "," << form->boundData.boundMax.z << "\n";
								break;
							}
						}
					}
				}
			}
			std::ofstream out(g_cache / "armour_matches.txt");
			for (const auto& [name, what] : lines) {
				out << name << "\t" << what << "\n";
			}
			logger::info("everyone: {} body armours listed with their outfits in armour_matches.txt", lines.size());
		}

		int LookFor(RE::Actor* a_actor)
		{
			if (g_creatureId && a_actor->GetFormID() == g_creatureId) {
				for (const auto& look : g_lookList) {
					if (look.creature == g_creatureName) {
						return look.number;
					}
				}
				return 0;
			}
			struct Chosen
			{
				int           number = 0;
				std::uint32_t frame = 0;
			};
			static std::unordered_map<std::uint32_t, Chosen> chosen;
			static std::uint32_t                             frame = 0;
			++frame;
			auto& mine = chosen[a_actor->GetFormID()];
			if (mine.number != 0 && frame - mine.frame < 4000) {
				return mine.number;
			}
			mine.frame = frame;
			// a person with an outfit of their own ("who:" rows) wears it whatever they have on
			{
				std::string name = a_actor->GetDisplayFullName() ? a_actor->GetDisplayFullName() : "";
				for (auto& c : name) {
					c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
				}
				for (const auto& look : g_lookList) {
					for (const auto& text : look.who) {
						if (!name.empty() && name.find(text) != std::string::npos) {
							mine.number = look.number;
							return mine.number;
						}
					}
				}
			}
			mine.number = OutfitFor(a_actor->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kBody), a_actor->GetFormID());
			return mine.number;
		}

		EnemyLook* PlayerOutfit(ID3D11Device* a_device, RE::PlayerCharacter* a_player)
		{
			static std::uint32_t wornId = 0xFFFFFFFF;
			static int           number = 0;
			static int           countdown = 0;
			// (also the very first time, and after a loading screen: a loaded save can have other armour on)
			const bool           watching = wornId == 0xFFFFFFFF || g_wearWatch.open || std::chrono::steady_clock::now() < g_wearWatch.until;
			if (!watching) {
				countdown = 0;
			} else if (--countdown <= 0) {
				countdown = 20;
				auto*               armour = a_player->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kBody);
				const std::uint32_t id = armour ? armour->GetFormID() : 0;
				if (id != wornId) {
					wornId = id;
					number = 0;
					if (armour && LookCount() > 0) {
						const char* kind = "";
						number = OutfitFor(armour, id, &kind);
						logger::info("wear: the player now wears '{}' in Skyrim ({}): MORDHAU outfit {}", armour->GetName() ? armour->GetName() : "?", kind, number);
					} else {
						LookCount();  // (reads the list of looks, which names the bare one)
						number = g_nakedLook;
						logger::info("wear: nothing on the player's body in Skyrim: MORDHAU's bare body (look {})", number);
					}
				}
			}
			if (number < 1 || number > kMaxLooks) {
				return nullptr;
			}
			auto& look = g_looks[number - 1];
			if (!look.tried) {
				look.tried = true;
				look.ready = LoadMesh(a_device, g_cache / "enemies" / std::to_string(number), look.bones, look.sections, &look.vb, &look.ib, &look.weaponBone, &look.headBone);
			}
			return look.ready && look.bones.size() == g_bones.size() ? &look : nullptr;
		}
	}
}
