// The MORDHAU x Skyrim mod's own loader (sheets/live_link.json: own_loader). It is put into MORDHAU's Binaries\Win64
// as dwmapi.dll for the second in which the mod's own offline MORDHAU starts. Windows loads it in place of its own
// dwmapi.dll; it passes every call on to the real one, and - only in a MORDHAU the mod itself started - loads the
// script host UE4SS.dll.
//
// The rule, enforced here, at the level of Windows, before anything of the mod runs and before the engine starts:
// the process's command line must have -insecure AND its environment must carry the mod's one-time session key.
// Otherwise this file renames itself (the next start is clean) and the process is ended with TerminateProcess.
#include <Windows.h>

extern "C" void* g_real[];        // the real dwmapi.dll's functions, one per export (exports.asm jumps through it)
extern "C" const int g_count;
extern "C" const char* const g_names[];   // nullptr = exported by number only
extern "C" const unsigned short g_ordinals[];

namespace
{
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
	// a name with a number is taken instead - whatever happens, no file named dwmapi.dll stays.
	void StepAside(const wchar_t* a_path)
	{
		wchar_t aside[MAX_PATH]{};
		lstrcpyW(aside, a_path);
		lstrcatW(aside, L".old");
		SetFileAttributesW(aside, FILE_ATTRIBUTE_NORMAL);
		DeleteFileW(aside);
		if (MoveFileExW(a_path, aside, MOVEFILE_REPLACE_EXISTING)) {
			return;
		}
		for (int i = 1; i <= 9; ++i) {
			wchar_t numbered[MAX_PATH]{};
			wsprintfW(numbered, L"%s%d", aside, i);
			SetFileAttributesW(numbered, FILE_ATTRIBUTE_NORMAL);
			DeleteFileW(numbered);
			if (MoveFileExW(a_path, numbered, MOVEFILE_REPLACE_EXISTING)) {
				return;
			}
		}
	}

	// Not the mod's own MORDHAU: this file takes itself out of the way and the process ends, now.
	void Refuse(HMODULE a_self, const wchar_t* a_why)
	{
		wchar_t path[MAX_PATH]{};
		if (GetModuleFileNameW(a_self, path, MAX_PATH - 8)) {
			StepAside(path);
			// a note for whoever wonders why MORDHAU did not start
			wchar_t note[MAX_PATH]{};
			lstrcpyW(note, path);
			if (wchar_t* slash = wcsrchr(note, L'\\')) {
				slash[1] = 0;
				lstrcatW(note, L"MordhauSkyrim-closed-MORDHAU.txt");
				const HANDLE file = CreateFileW(note, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
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

	void Forward()
	{
		wchar_t path[MAX_PATH]{};
		GetSystemDirectoryW(path, MAX_PATH - 16);
		lstrcatW(path, L"\\dwmapi.dll");
		const HMODULE real = LoadLibraryW(path);
		for (int i = 0; real && i < g_count; ++i) {
			g_real[i] = g_names[i] ? reinterpret_cast<void*>(GetProcAddress(real, g_names[i])) :
									 reinterpret_cast<void*>(GetProcAddress(real, MAKEINTRESOURCEA(g_ordinals[i])));
		}
	}
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
	}
	Forward();
	if (mordhau) {
		wchar_t path[MAX_PATH]{};
		if (GetModuleFileNameW(a_self, path, MAX_PATH - 16)) {
			// verified: this file has done its work. It takes itself out of the way at once (renamed - a loaded file
			// cannot be deleted; whoever started the session deletes dwmapi.dll.old when MORDHAU has closed), so the
			// folder has no loadable loader from the first moment of the session on.
			StepAside(path);
			if (wchar_t* slash = wcsrchr(path, L'\\')) {
				slash[1] = 0;
				lstrcatW(path, L"UE4SS.dll");
				LoadLibraryW(path);
			}
		}
	}
	return TRUE;
}
