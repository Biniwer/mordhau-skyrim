#pragma once

namespace dm
{
	// MORDHAU's attack numbers per weapon family (sheets/attack_stats.json). Values come from the player's
	// own MORDHAU; where it has none, the sheet's fallback is used.
	class Stats
	{
	public:
		// Loads "<family>.<strike|stab>.<Property>=<value>" lines (tools/bake_stats.py for now).
		void Load(const std::filesystem::path& a_file);

		// a_statId is a row id of sheets/attack_stats.json, e.g. "windup". Negative = no value anywhere.
		[[nodiscard]] float Get(std::string_view a_family, bool a_stab, std::string_view a_statId, bool* a_fromMordhau = nullptr) const;

		// a_statId is a row id of sheets/parry_stats.json, e.g. "parry_window".
		[[nodiscard]] float Parry(std::string_view a_statId, bool* a_fromMordhau = nullptr) const;

		// a_statId is a row id of sheets/movement.json, e.g. "base_speed".
		[[nodiscard]] float Move(std::string_view a_statId, bool* a_fromMordhau = nullptr) const;

		// A value by its full key, e.g. "one_hand_sword.BlockStaminaNegation"; a_default when absent.
		[[nodiscard]] float Raw(const std::string& a_key, float a_default) const;

		[[nodiscard]] std::size_t Size() const { return _values.size(); }

	private:
		std::unordered_map<std::string, float> _values;
	};
}
