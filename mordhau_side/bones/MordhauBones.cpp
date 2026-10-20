// MordhauBones.dll - a small native helper for the mod's MORDHAU-side Lua script (mordhau_side/PoseStream), loaded by
// the script itself with package.loadlib. It only READS: the bone transforms of a skeletal mesh, straight from
// MORDHAU's memory, so that a stand-in's pose takes one call from Lua instead of one per bone (each bone through
// UE4SS's Lua bridge cost ~30 us - sheets/live_link.json standin_bones_read, timing 2026-10-17).
// UE4SS does not export Lua's C functions; it does export its own C++ wrapper (RC::LuaMadeSimple::Lua), which is used
// here by its exported names. Measured on the D: copy, 2026-10-17: the wrapper's get_* take the value off the stack
// (so every argument is read from position 1, in order), and set_string reads up to a zero byte whatever length it is
// given (so results are hex text ended with a zero; the script turns it back with one gsub). Found in the same test: a mesh keeps its bones relative to their parents
// as TArray<FTransform> at +1736 and relative to the component at +1184 (MORDHAU as installed). Nothing is written into
// MORDHAU's memory, nothing is hooked.
#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace
{
	// RC::LuaMadeSimple::Lua, used through UE4SS.dll's exports (member functions: 'this' first)
	using Ctor = void*(__fastcall*)(void* a_this, void* a_state);
	using Dtor = void(__fastcall*)(void* a_this);
	using GetInteger = std::int64_t(__fastcall*)(const void* a_this, int a_index);
	using SetString = void(__fastcall*)(const void* a_this, std::string_view a_value);
	// (no get_string: it returns its string_view through a hidden pointer, and a member function's hidden pointer comes
	// after 'this' - called as a plain function it crashed MORDHAU in the D: test, 2026-10-17)

	struct Api
	{
		Ctor       ctor = nullptr;
		Dtor       dtor = nullptr;
		GetInteger getInteger = nullptr;
		SetString  setString = nullptr;
		bool       ready = false;
	};

	const Api& GetApi()
	{
		static Api api = [] {
			Api a;
			if (HMODULE ue4ss = GetModuleHandleW(L"UE4SS.dll")) {
				a.ctor = reinterpret_cast<Ctor>(GetProcAddress(ue4ss, "??0Lua@LuaMadeSimple@RC@@QEAA@PEAUlua_State@@@Z"));
				a.dtor = reinterpret_cast<Dtor>(GetProcAddress(ue4ss, "??1Lua@LuaMadeSimple@RC@@QEAA@XZ"));
				a.getInteger = reinterpret_cast<GetInteger>(GetProcAddress(ue4ss, "?get_integer@Lua@LuaMadeSimple@RC@@QEBA_JH@Z"));
				a.setString = reinterpret_cast<SetString>(GetProcAddress(ue4ss, "?set_string@Lua@LuaMadeSimple@RC@@QEBAXV?$basic_string_view@DU?$char_traits@D@std@@@std@@@Z"));
			}
			a.ready = a.ctor && a.dtor && a.getInteger && a.setString;
			return a;
		}();
		return api;
	}

	// The wrapper object for one call (its size is not known here: a generous buffer)
	struct Wrapped
	{
		alignas(16) unsigned char storage[1024]{};
		const Api&                api = GetApi();
		explicit Wrapped(void* a_state) { api.ctor(storage, a_state); }
		~Wrapped() { api.dtor(storage); }
		const void* self() const { return storage; }
	};

	// Whether [a_address, a_address + a_bytes) can be read (committed, readable, not a guard page)
	bool Readable(const void* a_address, std::size_t a_bytes)
	{
		MEMORY_BASIC_INFORMATION info{};
		if (!a_address || VirtualQuery(a_address, &info, sizeof(info)) != sizeof(info)) {
			return false;
		}
		const DWORD bad = PAGE_NOACCESS | PAGE_GUARD;
		if (info.State != MEM_COMMIT || (info.Protect & bad) != 0) {
			return false;
		}
		const auto end = static_cast<const unsigned char*>(info.BaseAddress) + info.RegionSize;
		return static_cast<const unsigned char*>(a_address) + a_bytes <= end;
	}
}

extern "C" {
	// ms_read(objectAddress, offset, count, n, i1, ..., in) -> the listed bones (0-based) of the TArray<FTransform> at that
	// offset, packed as Lua's string.pack("<fffffff") each (rotation x y z w, translation x y z), in the order listed;
	// "" when the array is not as expected. Read only.
	__declspec(dllexport) int ms_read(void* a_state)
	{
		if (!GetApi().ready) {
			return 0;
		}
		Wrapped              lua(a_state);
		const auto*          object = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(lua.api.getInteger(lua.self(), 1)));
		const std::int64_t   at = lua.api.getInteger(lua.self(), 1);
		const int            count = static_cast<int>(lua.api.getInteger(lua.self(), 1));
		const int            listed = std::clamp(static_cast<int>(lua.api.getInteger(lua.self(), 1)), 0, 256);
		int                  bones[256];
		for (int k = 0; k < listed; ++k) {
			bones[k] = static_cast<int>(lua.api.getInteger(lua.self(), 1));
		}
		static thread_local unsigned char out[256 * 28];
		std::size_t          used = 0;
		const unsigned char* data = nullptr;
		std::int32_t         num = 0;
		if (object && at >= 0 && at + 16 <= 0x2000 && Readable(object + at, 16)) {
			std::memcpy(&data, object + at, 8);
			std::memcpy(&num, object + at + 8, 4);
		}
		if (data && num == count && Readable(data, static_cast<std::size_t>(num) * 48)) {
			for (int k = 0; k < listed && used + 28 <= sizeof(out); ++k) {
				if (bones[k] < 0 || bones[k] >= num) {
					continue;
				}
				const float* t = reinterpret_cast<const float*>(data + static_cast<std::size_t>(bones[k]) * 48);
				std::memcpy(out + used, t, 16);           // rotation x y z w
				std::memcpy(out + used + 16, t + 4, 12);  // translation x y z
				used += 28;
			}
		}
		// (as hex text: the wrapper's set_string cut binary strings short - at 0x00 and 0x01 in the D: test; the script turns
		// it back with one gsub over a 256-entry table)
		static thread_local char escaped[sizeof(out) * 2 + 1];
		static const char        digits[] = "0123456789abcdef";
		std::size_t              length = 0;
		for (std::size_t k = 0; k < used; ++k) {
			escaped[length++] = digits[out[k] >> 4];
			escaped[length++] = digits[out[k] & 15];
		}
		// (ended with a zero byte: the wrapper's set_string reads up to it, not the given length - without it the leftover of
		// a longer earlier result came along: "30 of 28 bones" in the D: test, 2026-10-17)
		escaped[length] = 0;
		lua.api.setString(lua.self(), std::string_view(escaped, length));
		return 1;
	}
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
	return TRUE;
}
