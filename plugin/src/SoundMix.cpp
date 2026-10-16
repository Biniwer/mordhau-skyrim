#include "SoundMix.h"

#include "Settings.h"
#include "gen/Sheets.h"

#include <cmath>
#include <string_view>
#include <unordered_map>

namespace dm::SoundMix
{
	namespace
	{
		// sound_style: 0 the sliders, 1 only MORDHAU, 2 only Skyrim (2026-10-15: "Both in full" taken out - set as default,
		// it made the sliders do nothing until "Custom" was chosen: "i have skyrim parries set to 0 but still hear them")
		int Style()
		{
			return static_cast<int>(Settings::Value("sound_style", 0.0f));
		}

		// the volume, 0..1, of one kind of Skyrim's weapon sounds
		float SkyrimVolume(std::string_view a_kind)
		{
			switch (Style()) {
			case 1:
				return 0.0f;
			case 2:
				return 1.0f;
			default:
				return std::clamp(Settings::Value(std::string("skyrim_") + std::string(a_kind), 100.0f) / 100.0f, 0.0f, 1.0f);
			}
		}

		// each record's own attenuation, as the game loaded it
		std::unordered_map<RE::BGSStandardSoundDef*, std::uint16_t> g_own;
	}

	namespace
	{
		// one of Skyrim's weapon sounds (sheets/skyrim_combat_sounds.json, by editor id) at the actor, following it
		bool Play(RE::Actor* a_actor, const char* a_edid)
		{
			auto*                       data = RE::TESDataHandler::GetSingleton();
			RE::BGSSoundDescriptorForm* sound = nullptr;
			for (const auto& row : sheets::kSkyrimCombatSounds) {
				if (std::string_view(row.edid) == a_edid) {
					sound = data ? data->LookupForm<RE::BGSSoundDescriptorForm>(row.id, row.file) : nullptr;
					break;
				}
			}
			auto*             audio = RE::BSAudioManager::GetSingleton();
			RE::BSSoundHandle handle;
			if (!sound || !audio || !audio->GetSoundHandle(handle, sound) || !handle.IsValid()) {
				return false;
			}
			handle.SetPosition(a_actor->GetPosition() + RE::NiPoint3{ 0.0f, 0.0f, a_actor->GetHeight() * 0.7f });
			if (auto* root = a_actor->Get3D()) {
				handle.SetObjectToFollow(root);
			}
			return handle.Play();
		}
	}

	float MordhauVolume()
	{
		switch (Style()) {
		case 1:
			return 1.0f;
		case 2:
			return 0.0f;
		default:
			return std::clamp(Settings::Value("mordhau_volume", 100.0f) / 100.0f, 0.0f, 1.0f);
		}
	}

	void ApplySkyrim()
	{
		auto* data = RE::TESDataHandler::GetSingleton();
		if (!data) {
			return;
		}
		int changed = 0, missing = 0;
		for (const auto& row : sheets::kSkyrimCombatSounds) {
			auto* form = data->LookupForm<RE::BGSSoundDescriptorForm>(row.id, row.file);
			auto* def = form ? skyrim_cast<RE::BGSStandardSoundDef*>(form->soundDescriptor) : nullptr;
			if (!def) {
				++missing;
				continue;
			}
			const auto own = g_own.try_emplace(def, def->soundCharacteristics.staticAttenuation).first->second;
			// hundredths of a decibel: the volume v is 20 log10(v) dB; nothing (0) is 100 dB down
			const float volume = SkyrimVolume(row.kind);
			const float down = volume <= 0.001f ? 10000.0f : -2000.0f * std::log10(volume);
			const auto  value = static_cast<std::uint16_t>(std::min(65535.0f, own + down));
			if (def->soundCharacteristics.staticAttenuation != value) {
				def->soundCharacteristics.staticAttenuation = value;
				++changed;
			}
		}
		logger::info("sound mix: Skyrim's weapon sounds - hits {:.0f}%, swings {:.0f}%, blocks {:.0f}%, draw {:.0f}% ({} records changed{}); MORDHAU {:.0f}%",
			SkyrimVolume("hits") * 100.0f, SkyrimVolume("swings") * 100.0f, SkyrimVolume("blocks") * 100.0f, SkyrimVolume("draw") * 100.0f, changed,
			missing ? std::format(", {} not found", missing) : std::string(), MordhauVolume() * 100.0f);
	}

	void PlaySkyrimSwing(RE::Actor* a_actor, bool a_kick)
	{
		if (!a_actor || SkyrimVolume("swings") <= 0.001f) {
			return;
		}
		const char* edid = "WPNSwingUnarmed";
		const auto* form = a_kick ? nullptr : a_actor->GetEquippedObject(false);
		if (const auto* weapon = form ? form->As<RE::TESObjectWEAP>() : nullptr) {
			switch (weapon->GetWeaponType()) {
			case RE::WEAPON_TYPE::kOneHandSword:
			case RE::WEAPON_TYPE::kOneHandDagger: edid = "WPNSwingBladeMediumSD"; break;
			case RE::WEAPON_TYPE::kOneHandAxe: edid = "WPNSwingBladeAxeSD"; break;
			case RE::WEAPON_TYPE::kOneHandMace: edid = "WPNSwingBlunt1Hand"; break;
			case RE::WEAPON_TYPE::kTwoHandSword:
			case RE::WEAPON_TYPE::kTwoHandAxe: edid = "WPNSwing2Hand"; break;
			case RE::WEAPON_TYPE::kBow:
			case RE::WEAPON_TYPE::kCrossbow:
			case RE::WEAPON_TYPE::kStaff: return;  // (no swing: the bow's own sounds are its draw and release)
			default: break;
			}
		}
		Play(a_actor, edid);
	}

	void PlaySkyrimDraw(RE::Actor* a_actor, bool a_draw)
	{
		const auto* form = a_actor ? a_actor->GetEquippedObject(false) : nullptr;
		const auto* weapon = form ? form->As<RE::TESObjectWEAP>() : nullptr;
		if (!weapon || SkyrimVolume("draw") <= 0.001f) {
			return;  // (bare fists: Skyrim has no sound for raising them here)
		}
		const char* kind = nullptr;
		switch (weapon->GetWeaponType()) {
		case RE::WEAPON_TYPE::kOneHandSword: kind = "WPNBlade1Hand"; break;
		case RE::WEAPON_TYPE::kOneHandDagger: kind = "WPNBlade1HandSmall"; break;
		case RE::WEAPON_TYPE::kOneHandAxe: kind = "WPNAxe1Hand"; break;
		case RE::WEAPON_TYPE::kOneHandMace: kind = "WPNMace1Hand"; break;
		case RE::WEAPON_TYPE::kTwoHandSword: kind = "WPNBlade2Hand"; break;
		case RE::WEAPON_TYPE::kTwoHandAxe: kind = weapon->HasKeywordString("WeapTypeWarhammer") ? "WPNBlunt2Hand" : "WPNAxe2Hand"; break;
		case RE::WEAPON_TYPE::kBow: kind = "WPNBow"; break;
		case RE::WEAPON_TYPE::kStaff: kind = "WPNStaffHand"; break;
		case RE::WEAPON_TYPE::kCrossbow: kind = "WPNCrossbow"; break;
		default: return;
		}
		// (most are named <kind>DrawSD / <kind>SheatheSD; the two-handed blunt ones have no SD)
		const std::string name = std::string(kind) + (a_draw ? "Draw" : "Sheathe");
		if (!Play(a_actor, (name + "SD").c_str())) {
			Play(a_actor, name.c_str());
		}
	}

	void PlaySkyrimBlock(RE::Actor* a_defender)
	{
		if (!a_defender || SkyrimVolume("blocks") <= 0.001f) {
			return;
		}
		const char* edid = "WPNBlockBlade1HandVsOtherSD";
		const auto* left = a_defender->GetEquippedObject(true);
		if (left && left->Is(RE::FormType::Armor)) {
			edid = "WPNBlockShieldHSD";
		} else {
			const auto* form = a_defender->GetEquippedObject(false);
			if (const auto* weapon = form ? form->As<RE::TESObjectWEAP>() : nullptr) {
				switch (weapon->GetWeaponType()) {
				case RE::WEAPON_TYPE::kOneHandAxe: edid = "WPNBlockAxeVsOtherSD"; break;
				case RE::WEAPON_TYPE::kOneHandMace: edid = "WPNBlockBlunt1HandVsOtherSD"; break;
				case RE::WEAPON_TYPE::kTwoHandSword: edid = "WPNBlockBlade2HandVsOtherSD"; break;
				case RE::WEAPON_TYPE::kTwoHandAxe: edid = weapon->HasKeywordString("WeapTypeWarhammer") ? "WPNBlockBlunt2HandVsOther" : "WPNBlockAxeVsOtherSD"; break;
				case RE::WEAPON_TYPE::kBow:
				case RE::WEAPON_TYPE::kCrossbow: edid = "WPNBlockBowVsOtherSD"; break;
				default: break;
				}
			}
		}
		Play(a_defender, edid);
	}
}
