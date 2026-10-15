#include <psapi.h>
#include "Boot.h"
#include "Combat.h"
#include "Session.h"

#include <Windows.h>
#include <ShlObj.h>

namespace
{
	constexpr auto kVersion = "0.1.0";

	void OnDataLoaded()
	{
		dm::Combat::Install();
	}

	void OnGameLoaded()
	{
		dm::Combat::OnGameLoaded();
	}

	void Start()
	{
		dm::InstallBoot({ .dataLoaded = OnDataLoaded, .gameLoaded = OnGameLoaded });
	}
}

// Dev aid: when a C++ exception is thrown (code 0xE06D7363) or memory is touched wrongly, note where - as offsets
// into this plugin - in the log before the game goes down. (Found with the crowd test: an unhandled throw.)
namespace
{
	HMODULE g_self = nullptr;
	// (a std::exception keeps its text behind its vtable pointer; read carefully)
	const char* WhatOf(ULONG_PTR a_object)
	{
		__try {
			return reinterpret_cast<std::exception*>(a_object)->what();
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			return "?";
		}
	}
	LONG CALLBACK NoteException(EXCEPTION_POINTERS* a_info)
	{
		const DWORD code = a_info->ExceptionRecord->ExceptionCode;
		if (code != 0xE06D7363 && code != EXCEPTION_ACCESS_VIOLATION) {
			return EXCEPTION_CONTINUE_SEARCH;
		}
		static std::atomic<int> noted{ 0 };
		if (noted.fetch_add(1) >= 6) {
			return EXCEPTION_CONTINUE_SEARCH;
		}
		// where it happened (module + offset), and the way there: every frame as module + offset, this plugin's as
		// "+offset" (tools/plugin_addr.py turns those into function and line). Before: only this plugin's frames, among
		// them always this handler's own, so a crash anywhere read as "in this plugin" and said nothing about where.
		const auto nameOf = [](std::uintptr_t a_at) {
			HMODULE module = nullptr;
			if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(a_at), &module) || !module) {
				return std::format("?{:X}", a_at);
			}
			if (module == g_self) {
				return std::format("+{:X}", a_at - reinterpret_cast<std::uintptr_t>(module));
			}
			char       path[MAX_PATH]{};
			const auto length = GetModuleFileNameA(module, path, MAX_PATH);
			std::string_view name(path, length);
			if (const auto slash = name.find_last_of('\\'); slash != std::string_view::npos) {
				name.remove_prefix(slash + 1);
			}
			return std::format("{}+{:X}", name, a_at - reinterpret_cast<std::uintptr_t>(module));
		};
		void*        frames[48]{};
		const USHORT count = CaptureStackBackTrace(0, 48, frames, nullptr);
		const auto   fault = reinterpret_cast<std::uintptr_t>(a_info->ExceptionRecord->ExceptionAddress);
		// (the frames above the faulting one are the system's exception dispatch and this handler: skipped)
		USHORT first = 0;
		for (USHORT i = 0; i < count; ++i) {
			if (reinterpret_cast<std::uintptr_t>(frames[i]) == fault) {
				first = i;
				break;
			}
		}
		if (first == 0) {
			first = std::min<USHORT>(count, 6);  // (not found among the return addresses: skip the usual dispatch frames)
		}
		std::string line;
		bool        ours = nameOf(fault).starts_with('+');
		int         listed = 0;
		for (USHORT i = first; i < count && listed < 14; ++i) {
			const auto name = nameOf(reinterpret_cast<std::uintptr_t>(frames[i]));
			ours = ours || name.starts_with('+');
			line += " " + name;
			++listed;
		}
		// an access violation that some handler catches and carries on from is not a crash: only ones in or through
		// this plugin are noted, and any C++ throw that passes through it
		if (ours || code == EXCEPTION_ACCESS_VIOLATION) {
			const char* what = code == 0xE06D7363 && a_info->ExceptionRecord->NumberParameters >= 2 ? WhatOf(a_info->ExceptionRecord->ExceptionInformation[1]) : "";
			const auto  touched = code == EXCEPTION_ACCESS_VIOLATION && a_info->ExceptionRecord->NumberParameters >= 2 ?
									  std::format(" {} address {:X}", a_info->ExceptionRecord->ExceptionInformation[0] ? "writing" : "reading", a_info->ExceptionRecord->ExceptionInformation[1]) :
									  std::string();
			spdlog::error("exception {:08X} {}{} at {}{}; stack:{}", code, what, touched, nameOf(fault), ours ? " (this plugin is on the stack)" : " (not in this plugin)", line);
			spdlog::default_logger()->flush();
		}
		return EXCEPTION_CONTINUE_SEARCH;
	}
}

BOOL APIENTRY DllMain(HMODULE a_module, DWORD a_reason, LPVOID)
{
	if (a_reason == DLL_PROCESS_ATTACH) {
		g_self = a_module;
	}
	return TRUE;
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	// The log is Documents/My Games/<Skyrim's folder>/SKSE/MordhauSkyrim.log - where every SKSE plugin's log is, under the
	// mod's own name. (Until 0.1.16 it was DirectionalMelee.log one folder up, and SKSE's folder held a one-line file of
	// the same name: players sent that one.)
	SKSE::Init(a_skse, { .logName = "MordhauSkyrim", .logPattern = "[%H:%M:%S.%e] [%l] %v", .trampoline = true, .trampolineSize = 1024 });
	spdlog::flush_on(spdlog::level::info);
	logger::info("DirectionalMelee {} loading under SKSE (runtime {})", kVersion, a_skse->RuntimeVersion().string());
	AddVectoredExceptionHandler(1, NoteException);
	dm::Session::Begin(g_self);  // (installed by a mod manager: the plugin starts MORDHAU beside Skyrim itself)
	SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* a_msg) {
		if (a_msg->type == SKSE::MessagingInterface::kInputLoaded) {
			Start();
		}
	});
	return true;
}
