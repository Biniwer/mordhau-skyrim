// The MORDHAU x Skyrim mod's own loader (sheets/live_link.json: own_loader). It is put into MORDHAU's Binaries\Win64
// as dwmapi.dll for the second in which the mod's own offline MORDHAU starts. Windows loads it in place of its own
// dwmapi.dll; it passes every call on to the real one, and - only in a MORDHAU the mod itself started - loads the
// script host UE4SS.dll from the mod's own folder.
//
// The rule, enforced here, at the level of Windows, before anything of the mod runs and before the engine starts:
// the process's command line must have -insecure AND its environment must carry the mod's one-time session key.
// Otherwise this file renames itself (the next start is clean) and the process is ended with TerminateProcess.
#include <Windows.h>

extern "C" void* g_real[];        // the real dwmapi.dll's functions, one per export, each looked up on its first call
extern "C" const int g_count;
extern "C" const char* const g_names[];   // nullptr = exported by number only
extern "C" const unsigned short g_ordinals[];

namespace
{
	// Paths as long as Windows lets them be, not MAX_PATH: a path cut short would rename nothing and leave
	// dwmapi.dll where it is. (Written only in DllMain, which Windows runs one at a time.)
	constexpr DWORD kLong = 32768;
	wchar_t         g_self[kLong];
	wchar_t         g_aside[kLong];
	wchar_t         g_numbered[kLong];
	wchar_t         g_note[kLong];
	wchar_t         g_scriptHost[kLong];

	bool HasInsecure()
	{
		const wchar_t* line = GetCommandLineW();
		const wchar_t  want[] = L"-insecure";
		for (const wchar_t* at = line; at && *at; ++at) {
			int i = 0;
			while (want[i] && at[i] && towlower(at[i]) == want[i]) {
				++i;
			}
			if (!want[i] && (at[i] == 0 || at[i] == L' ' || at[i] == L'"' || at[i] == L'\t')) {
				return true;
			}
		}
		return false;
	}

	bool HasSessionKey()
	{
		wchar_t     key[128]{};
		const DWORD length = GetEnvironmentVariableW(L"MORDHAU_SKYRIM_SESSION", key, 128);
		return length >= 16 && length < 128;
	}

	bool InMordhau()
	{
		wchar_t     exe[MAX_PATH]{};
		const DWORD length = GetModuleFileNameW(nullptr, exe, MAX_PATH);
		const wchar_t name[] = L"Mordhau-Win64-Shipping.exe";
		const DWORD   size = static_cast<DWORD>(sizeof(name) / sizeof(wchar_t) - 1);
		return length >= size && _wcsicmp(exe + length - size, name) == 0;
	}

	// This file out of the way: renamed to dwmapi.dll.old (a loaded file can be renamed, not deleted). An older
	// leftover of that name is removed first, read-only or not; if it cannot be (still loaded by another MORDHAU),
	// a name with a number is taken instead (.old1 to .old9) - whatever happens, no file named dwmapi.dll stays.
	void StepAside(const wchar_t* a_path)
	{
		const int length = lstrlenW(a_path);
		if (length == 0 || length + 6 >= static_cast<int>(kLong)) {  // (room for ".old", a digit and the end)
			return;
		}
		lstrcpyW(g_aside, a_path);
		lstrcpyW(g_aside + length, L".old");
		SetFileAttributesW(g_aside, FILE_ATTRIBUTE_NORMAL);
		DeleteFileW(g_aside);
		if (MoveFileExW(a_path, g_aside, MOVEFILE_REPLACE_EXISTING)) {
			return;
		}
		for (wchar_t digit = L'1'; digit <= L'9'; ++digit) {
			lstrcpyW(g_numbered, g_aside);
			g_numbered[length + 4] = digit;
			g_numbered[length + 5] = 0;
			SetFileAttributesW(g_numbered, FILE_ATTRIBUTE_NORMAL);
			DeleteFileW(g_numbered);
			if (MoveFileExW(a_path, g_numbered, MOVEFILE_REPLACE_EXISTING)) {
				return;
			}
		}
	}

	// This file's own path, whole - false when Windows had to cut it short
	bool SelfPath(HMODULE a_self)
	{
		const DWORD length = GetModuleFileNameW(a_self, g_self, kLong);
		return length > 0 && length < kLong;
	}

	// Not the mod's own MORDHAU: this file takes itself out of the way and the process ends, now.
	void Refuse(HMODULE a_self, const wchar_t* a_why)
	{
		if (SelfPath(a_self)) {
			StepAside(g_self);
			// a note for whoever wonders why MORDHAU did not start
			const wchar_t name[] = L"MordhauSkyrim-closed-MORDHAU.txt";
			lstrcpyW(g_note, g_self);
			wchar_t* slash = wcsrchr(g_note, L'\\');
			if (slash && (slash - g_note) + 1 + static_cast<long long>(sizeof(name) / sizeof(wchar_t)) < kLong) {
				lstrcpyW(slash + 1, name);
				const HANDLE file = CreateFileW(g_note, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
				if (file != INVALID_HANDLE_VALUE) {
					char  text[600];
					const int length = wsprintfA(text,
						"MORDHAU was closed at once by the loader of the MORDHAU x Skyrim mod (a Skyrim mod).\r\n"
						"Reason: %ls\r\n"
						"The loader (dwmapi.dll) had been left in this folder and MORDHAU was not started by the mod. It has renamed\r\n"
						"itself to dwmapi.dll.old, so MORDHAU starts normally now. You can delete dwmapi.dll.old and this file.\r\n",
						a_why);
					DWORD written = 0;
					WriteFile(file, text, static_cast<DWORD>(length), &written, nullptr);
					CloseHandle(file);
				}
			}
		}
		TerminateProcess(GetCurrentProcess(), 0);
	}

	// What an export the real dwmapi.dll does not have answers (most of dwmapi's functions return an HRESULT)
	HRESULT WINAPI NotThere()
	{
		return E_NOTIMPL;
	}

	void* volatile g_realModule = nullptr;

	// Where the script host is: the mod's own folder, not MORDHAU's (since 0.1.22 nothing of the mod but this loader is
	// ever put into MORDHAU's folder) - named by the session that started this MORDHAU, in MORDHAU_SKYRIM_UE4SS. Only a
	// whole path to a file called UE4SS.dll is taken; anything else, and nothing is loaded.
	bool ScriptHostPath()
	{
		const DWORD length = GetEnvironmentVariableW(L"MORDHAU_SKYRIM_UE4SS", g_scriptHost, kLong);
		const wchar_t name[] = L"\\UE4SS.dll";
		const DWORD   size = static_cast<DWORD>(sizeof(name) / sizeof(wchar_t) - 1);
		if (length <= size || length >= kLong) {
			return false;
		}
		const bool whole = (g_scriptHost[1] == L':' && g_scriptHost[2] == L'\\') || (g_scriptHost[0] == L'\\' && g_scriptHost[1] == L'\\');
		return whole && _wcsicmp(g_scriptHost + length - size, name) == 0;
	}

	// The script host, loaded from a thread of its own: Windows advises against loading another DLL inside DllMain
	// (the loader lock). The thread starts as soon as Windows has finished loading the game's DLLs.
	DWORD WINAPI StartScriptHost(LPVOID)
	{
		LoadLibraryW(g_scriptHost);
		return 0;
	}
}

// The first call of an export (exports.asm): the real dwmapi.dll from Windows' own folder is loaded then - not in
// DllMain - and that export's place in g_real filled, so every later call goes straight to it.
extern "C" void* ResolveReal(int a_index)
{
	if (a_index < 0 || a_index >= g_count) {
		return reinterpret_cast<void*>(&NotThere);
	}
	HMODULE real = static_cast<HMODULE>(g_realModule);
	if (!real) {
		wchar_t     path[MAX_PATH + 16]{};
		const UINT  length = GetSystemDirectoryW(path, MAX_PATH);
		if (length > 0 && length < MAX_PATH) {
			lstrcatW(path, L"\\dwmapi.dll");
			real = LoadLibraryW(path);
		}
		if (!real) {
			return reinterpret_cast<void*>(&NotThere);
		}
		InterlockedExchangePointer(const_cast<void**>(&g_realModule), real);
	}
	void* function = g_names[a_index] ? reinterpret_cast<void*>(GetProcAddress(real, g_names[a_index])) :
										reinterpret_cast<void*>(GetProcAddress(real, MAKEINTRESOURCEA(g_ordinals[a_index])));
	if (!function) {
		function = reinterpret_cast<void*>(&NotThere);
	}
	InterlockedExchangePointer(&g_real[a_index], function);
	return function;
}

BOOL APIENTRY DllMain(HMODULE a_self, DWORD a_reason, LPVOID)
{
	if (a_reason != DLL_PROCESS_ATTACH) {
		return TRUE;
	}
	DisableThreadLibraryCalls(a_self);
	const bool mordhau = InMordhau();
	if (mordhau) {
		// first of all, before anything else of the mod exists in this process
		if (!HasInsecure()) {
			Refuse(a_self, L"MORDHAU was started without -insecure.");
		}
		if (!HasSessionKey()) {
			Refuse(a_self, L"MORDHAU was not started by the mod (no session key).");
		}
		if (SelfPath(a_self)) {
			// verified: this file has done its work. It takes itself out of the way at once (renamed - a loaded file
			// cannot be deleted; whoever started the session deletes dwmapi.dll.old when MORDHAU has closed), so the
			// folder has no loadable loader from the first moment of the session on.
			StepAside(g_self);
			if (ScriptHostPath()) {
				if (const HANDLE thread = CreateThread(nullptr, 0, StartScriptHost, nullptr, 0, nullptr)) {
					CloseHandle(thread);
				}
			}
		}
	}
	return TRUE;
}
