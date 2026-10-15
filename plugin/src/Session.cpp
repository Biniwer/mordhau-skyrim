#include "Session.h"

#include "MordhauAudio.h"
#include "FirstStart.h"
#include "bake/Cache.h"

#include <Windows.h>
#include <Psapi.h>
#include <TlHelp32.h>

#include <chrono>
#include <ctime>
#include <format>
#include <fstream>
#include <random>
#include <thread>

namespace dm::Session
{
	namespace
	{
		namespace fs = std::filesystem;
		using Clock = std::chrono::steady_clock;

		struct Settings
		{
			fs::path mordhau;               // MORDHAU's folder (the one holding "Mordhau\Binaries") when Steam does not have it
			fs::path cache;                 // another cache folder
			int      width = 1920;          // MORDHAU's window (its health / stamina display is read from it)
			int      height = 1080;
			bool     simpleEnemies = false;  // enemies only copy the Skyrim character's timing (default: MORDHAU's own bot brains)
			bool     startMordhau = true;    // 0: the plugin starts nothing (someone else runs the session)
		};
		Settings g_settings;
		fs::path g_payload;        // ...\Data\SKSE\Plugins\MordhauSkyrim\mordhau
		HANDLE   g_job = nullptr;  // MORDHAU is in it: it ends when Skyrim's process ends, however that ends

		constexpr auto kExe = L"Mordhau-Win64-Shipping.exe";
		constexpr auto kLoader = L"dwmapi.dll";  // UE4SS's proxy: what makes MORDHAU load UE4SS.dll. Never left in MORDHAU's folder.

		float Seconds(Clock::time_point a_since)
		{
			return std::chrono::duration<float>(Clock::now() - a_since).count();
		}

		std::string Trim(std::string a_text)
		{
			while (!a_text.empty() && (a_text.back() == '\r' || a_text.back() == ' ' || a_text.back() == '\t' || a_text.back() == '"')) {
				a_text.pop_back();
			}
			std::size_t first = 0;
			while (first < a_text.size() && (a_text[first] == ' ' || a_text[first] == '\t' || a_text[first] == '"')) {
				++first;
			}
			return a_text.substr(first);
		}

		void ReadSettings(const fs::path& a_file)
		{
			std::ifstream in(a_file);
			std::string   line;
			while (std::getline(in, line)) {
				const auto eq = line.find('=');
				if (eq == std::string::npos || line[0] == ';' || line[0] == '#' || line[0] == '[') {
					continue;
				}
				const auto key = Trim(line.substr(0, eq)), value = Trim(line.substr(eq + 1));
				if (key == "MordhauPath") {
					g_settings.mordhau = value;
				} else if (key == "CacheFolder") {
					g_settings.cache = value;
				} else if (key == "MordhauWidth" && std::atoi(value.c_str()) >= 640) {
					g_settings.width = std::atoi(value.c_str());
				} else if (key == "MordhauHeight" && std::atoi(value.c_str()) >= 360) {
					g_settings.height = std::atoi(value.c_str());
				} else if (key == "SimpleEnemies") {
					g_settings.simpleEnemies = value == "1";
				} else if (key == "StartMordhau") {
					g_settings.startMordhau = value != "0";
				}
			}
		}

		// MORDHAU's Binaries\Win64, or nothing when the game is not there.
		fs::path MordhauBin()
		{
			fs::path        bin;
			std::error_code ec;
			if (!g_settings.mordhau.empty()) {
				bin = g_settings.mordhau / "Mordhau" / "Binaries" / "Win64";
			} else if (const auto paks = mbake::FindMordhauPaks(); !paks.empty()) {
				bin = paks.parent_path().parent_path() / "Binaries" / "Win64";  // (...\Mordhau\Content\Paks)
			}
			return !bin.empty() && fs::exists(bin / kExe, ec) ? bin : fs::path();
		}

		DWORD MordhauRunning()
		{
			DWORD        found = 0;
			const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
			if (snapshot == INVALID_HANDLE_VALUE) {
				return 0;
			}
			PROCESSENTRY32W entry{};
			entry.dwSize = sizeof(entry);
			for (BOOL more = Process32FirstW(snapshot, &entry); more; more = Process32NextW(snapshot, &entry)) {
				if (_wcsicmp(entry.szExeFile, kExe) == 0) {
					found = entry.th32ProcessID;
					break;
				}
			}
			CloseHandle(snapshot);
			return found;
		}

		bool SameBytes(const fs::path& a_one, const fs::path& a_other)
		{
			std::error_code ec;
			if (!fs::exists(a_one, ec) || !fs::exists(a_other, ec) || fs::file_size(a_one, ec) != fs::file_size(a_other, ec)) {
				return false;
			}
			std::ifstream one(a_one, std::ios::binary), other(a_other, std::ios::binary);
			return std::equal(std::istreambuf_iterator<char>(one), std::istreambuf_iterator<char>(), std::istreambuf_iterator<char>(other));
		}

		bool Put(const fs::path& a_from, const fs::path& a_to)
		{
			std::error_code ec;
			if (SameBytes(a_from, a_to)) {
				return true;
			}
			fs::create_directories(a_to.parent_path(), ec);
			if (!CopyFileW(a_from.c_str(), a_to.c_str(), FALSE)) {
				logger::warn("session: could not write '{}' (error {})", a_to.string(), GetLastError());
				return false;
			}
			SetFileAttributesW(a_to.c_str(), FILE_ATTRIBUTE_NORMAL);  // (a mod manager's files can be read-only)
			return true;
		}

		// The loader does not stay in MORDHAU's folder: deleted, or - while a running MORDHAU holds it - renamed so that
		// it is not loaded by the next start. True when nothing loadable is left.
		bool ClearLoader(const fs::path& a_bin)
		{
			const auto on = a_bin / kLoader, old = a_bin / L"dwmapi.dll.old";
			// (every renamed copy the loader may have left: dwmapi.dll.old, and .old1-.old9 when that name was taken;
			// read-only or not)
			{
				std::error_code ec;
				for (const auto& entry : fs::directory_iterator(a_bin, ec)) {
					const auto name = entry.path().filename().wstring();
					if (name.size() >= 14 && _wcsnicmp(name.c_str(), L"dwmapi.dll.old", 14) == 0) {
						SetFileAttributesW(entry.path().c_str(), FILE_ATTRIBUTE_NORMAL);
						DeleteFileW(entry.path().c_str());
					}
				}
			}
			SetFileAttributesW(on.c_str(), FILE_ATTRIBUTE_NORMAL);
			if (!DeleteFileW(on.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND) {
				MoveFileExW(on.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING);
			}
			std::error_code ec;
			if (fs::exists(on, ec)) {
				logger::error("session: the script loader could NOT be taken out of MORDHAU's folder ({}). Do not play MORDHAU online before it is gone: close MORDHAU and delete that file.", on.string());
				return false;
			}
			return true;
		}

		// MORDHAU's side, from beside the plugin into MORDHAU's Binaries\Win64: UE4SS (the script host) unless someone's
		// own is there, this mashup's script, and where the shared cache is. Nothing of MORDHAU's own is changed.
		bool PlaceFiles(const fs::path& a_bin, const fs::path& a_cache)
		{
			std::error_code ec;
			const auto      marker = a_bin / "UE4SS.mashup.txt";  // "this UE4SS was put here by the mashup": it may be updated
			const bool      theirs = fs::exists(a_bin / "UE4SS.dll", ec) && !fs::exists(marker, ec) && !SameBytes(g_payload / "UE4SS.dll", a_bin / "UE4SS.dll");
			bool            ok = true;
			if (theirs) {
				logger::info("session: MORDHAU's folder has a UE4SS of its own: left as it is; only this mashup's script is added");
			}
			for (const auto& entry : fs::recursive_directory_iterator(g_payload, ec)) {
				if (!entry.is_regular_file(ec)) {
					continue;
				}
				const auto relative = fs::relative(entry.path(), g_payload, ec);
				const auto name = relative.generic_string();
				if (_wcsicmp(relative.filename().c_str(), kLoader) == 0) {
					continue;  // (only ever in MORDHAU's folder while a session's MORDHAU starts)
				}
				if (relative.filename().wstring().starts_with(L"__folder_managed_by_") || relative.filename() == L"meta.ini") {
					continue;  // (a mod manager's own markers in the mod's folders - Vortex, Mod Organizer 2: not MORDHAU's business)
				}
				const bool script = name.starts_with("Mods/PoseStream/");
				if (name == "Mods/mods.txt") {
					continue;  // (below: someone's own list is kept)
				}
				if (theirs && !script) {
					continue;
				}
				ok = Put(entry.path(), a_bin / relative) && ok;
			}
			if (!theirs) {
				std::ofstream(marker) << "UE4SS in this folder was put here by the MORDHAU x Skyrim mashup (a Skyrim mod). It is not loaded by a normal start of MORDHAU.\n";
			}
			// UE4SS's list of scripts: this mashup's must be on it and switched on
			const auto list = a_bin / "Mods" / "mods.txt";
			if (!fs::exists(list, ec)) {
				ok = Put(g_payload / "Mods" / "mods.txt", list) && ok;
			} else {
				std::ifstream in(list);
				std::string   line, all;
				bool          listed = false;
				while (std::getline(in, line)) {
					if (Trim(line).starts_with("PoseStream")) {
						line = "PoseStream : 1";
						listed = true;
					}
					all += line + "\n";
				}
				in.close();
				std::ofstream(list, std::ios::trunc) << (listed ? "" : "PoseStream : 1\n") << all;
			}
			std::ofstream(a_bin / "Mods" / "PoseStream" / "cache_dir.txt", std::ios::trunc) << a_cache.string() << "\n";
			return ok && fs::exists(a_bin / "UE4SS.dll", ec) && fs::exists(a_bin / "Mods" / "PoseStream" / "Scripts" / "main.lua", ec);
		}

		// Skyrim's own environment with MORDHAU's Steam id instead of Skyrim's.
		std::wstring EnvironmentForMordhau(const std::wstring& a_key)
		{
			std::wstring block;
			if (const LPWCH all = GetEnvironmentStringsW()) {
				for (const wchar_t* at = all; *at; at += std::wcslen(at) + 1) {
					if (_wcsnicmp(at, L"SteamAppId=", 11) == 0 || _wcsnicmp(at, L"SteamGameId=", 12) == 0 || _wcsnicmp(at, L"SteamOverlayGameId=", 19) == 0 ||
						_wcsnicmp(at, L"MORDHAU_SKYRIM=", 15) == 0 || _wcsnicmp(at, L"MORDHAU_SKYRIM_SESSION=", 23) == 0) {
						continue;
					}
					block.append(at);
					block.push_back(L'\0');
				}
				FreeEnvironmentStringsW(all);
			}
			for (const auto* line : { L"SteamAppId=629760", L"SteamGameId=629760" }) {
				block.append(line);
				block.push_back(L'\0');
			}
			// (the key of this one start: the script runs only in a MORDHAU that has it - sheets/live_link.json session_key)
			block.append(L"MORDHAU_SKYRIM_SESSION=" + a_key);
			block.push_back(L'\0');
			block.push_back(L'\0');
			return block;
		}

		// The last session's script log is kept (UE4SS starts it afresh): evidence when MORDHAU closed by itself.
		void KeepLog(const fs::path& a_bin, const fs::path& a_cache)
		{
			std::error_code ec;
			const auto      log = a_bin / "UE4SS.log";
			if (!fs::exists(log, ec) || fs::file_size(log, ec) == 0) {
				return;
			}
			const auto keep = a_cache / "logs";
			fs::create_directories(keep, ec);
			char             stamp[32]{};
			const std::time_t now = std::time(nullptr);
			std::tm          local{};
			localtime_s(&local, &now);
			std::strftime(stamp, sizeof(stamp), "%m%d-%H%M%S", &local);
			fs::copy_file(log, keep / (std::string("UE4SS-") + stamp + ".log"), fs::copy_options::overwrite_existing, ec);
			std::vector<fs::path> logs;
			for (const auto& entry : fs::directory_iterator(keep, ec)) {
				if (entry.path().filename().string().starts_with("UE4SS-")) {
					logs.push_back(entry.path());
				}
			}
			std::sort(logs.begin(), logs.end());
			for (std::size_t i = 0; i + 12 < logs.size(); ++i) {
				fs::remove(logs[i], ec);
			}
		}

		// Starts MORDHAU into its own offline match (the script opens it and spawns the mercenary). The loader is put into
		// its folder for this start only. The handle of the started game, or nothing.
		HANDLE StartMordhau(const fs::path& a_bin, const fs::path& a_cache, bool& a_other)
		{
			a_other = false;
			KeepLog(a_bin, a_cache);
			DeleteFileW((a_cache / "live_stop.txt").c_str());  // (a stop signal is for the session that raised it)
			std::ofstream(a_cache / "live_options.lua", std::ios::trunc) << "return { bot_brains = " << (g_settings.simpleEnemies ? "false" : "true") << " }\n";
			// (the script closes MORDHAU again unless a mashup session started it: this is how it knows)
			std::string key;
			{
				std::random_device random;
				for (int i = 0; i < 4; ++i) {
					key += std::format("{:08x}", random());
				}
			}
			std::ofstream(a_cache / "live_session.txt", std::ios::trunc) << static_cast<long long>(std::time(nullptr)) << "\n" << key << "\n";
			if (!CopyFileW((g_payload / kLoader).c_str(), (a_bin / kLoader).c_str(), FALSE)) {
				logger::error("session: the script loader could not be put into MORDHAU's folder (error {}); MORDHAU is not started", GetLastError());
				return nullptr;
			}
			SetFileAttributesW((a_bin / kLoader).c_str(), FILE_ATTRIBUTE_NORMAL);
			// (launch race: looked for once more, with the loader in the folder and the launch one call away. One that
			// has appeared - the player's own start - is not to meet the loader: out it goes, the launch waits.)
			if (MordhauRunning()) {
				ClearLoader(a_bin);
				a_other = true;
				logger::info("session: another MORDHAU appeared just before the launch: loader taken out again, waiting for it to close");
				return nullptr;
			}
			auto         environment = EnvironmentForMordhau(std::wstring(key.begin(), key.end()));
			std::wstring command = L"\"" + (a_bin / kExe).wstring() + L"\" -windowed -ResX=" + std::to_wstring(g_settings.width) + L" -ResY=" + std::to_wstring(g_settings.height) +
								   L" -insecure";  // (always: this MORDHAU is not for anti-cheat-protected play - sheets/live_link.json managed_session)
			STARTUPINFOW        startup{};
			PROCESS_INFORMATION started{};
			startup.cb = sizeof(startup);
			startup.dwFlags = STARTF_USESHOWWINDOW;
			startup.wShowWindow = SW_SHOWNOACTIVATE;  // (Skyrim keeps the keyboard where MORDHAU honours this)
			if (!CreateProcessW((a_bin / kExe).c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED, environment.data(), a_bin.c_str(), &startup, &started)) {
				logger::error("session: MORDHAU could not be started (error {})", GetLastError());
				ClearLoader(a_bin);
				return nullptr;
			}
			if (!g_job) {
				g_job = CreateJobObjectW(nullptr, nullptr);
				JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
				limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
				if (g_job) {
					SetInformationJobObject(g_job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
				}
			}
			if (!g_job || !AssignProcessToJobObject(g_job, started.hProcess)) {
				logger::warn("session: MORDHAU could not be tied to Skyrim's process (error {}); it may stay open when Skyrim closes", GetLastError());
			}
			ResumeThread(started.hThread);
			CloseHandle(started.hThread);
			logger::info("session: MORDHAU started (process {}), offline, {}x{}", started.dwProcessId, g_settings.width, g_settings.height);
			return started.hProcess;
		}

		// Has MORDHAU loaded the loader from its own folder yet? (Then the file has done its work.)
		bool LoaderLoaded(HANDLE a_process, const fs::path& a_bin)
		{
			HMODULE modules[1024];
			DWORD   bytes = 0;
			if (!EnumProcessModulesEx(a_process, modules, sizeof(modules), &bytes, LIST_MODULES_64BIT)) {
				return false;
			}
			const auto folder = a_bin.wstring();
			for (DWORD i = 0; i < std::min<DWORD>(bytes / sizeof(HMODULE), 1024); ++i) {
				wchar_t name[MAX_PATH]{};
				if (GetModuleFileNameExW(a_process, modules[i], name, MAX_PATH) && _wcsnicmp(name, folder.c_str(), folder.size()) == 0) {
					const std::wstring_view file(name + folder.size());
					if (file.find(L"dwmapi.dll") != std::wstring_view::npos) {  // (also under the name the script gives it: dwmapi.dll.old)
						return true;
					}
				}
			}
			return false;
		}

		struct Find
		{
			DWORD process = 0;
			HWND  window = nullptr;
		};
		BOOL CALLBACK FindWindowOf(HWND a_window, LPARAM a_find)
		{
			auto* find = reinterpret_cast<Find*>(a_find);
			DWORD process = 0;
			GetWindowThreadProcessId(a_window, &process);
			if (process == find->process && IsWindowVisible(a_window) && !GetWindow(a_window, GW_OWNER)) {
				find->window = a_window;
				return FALSE;
			}
			return TRUE;
		}

		// MORDHAU's window takes the keyboard when it appears: Skyrim gets it back (the player is in Skyrim).
		bool FocusBack(DWORD a_mordhau)
		{
			const HWND front = GetForegroundWindow();
			DWORD      process = 0;
			const auto thread = front ? GetWindowThreadProcessId(front, &process) : 0;
			if (!front || process != a_mordhau) {
				return false;
			}
			Find mine{ GetCurrentProcessId(), nullptr };
			EnumWindows(FindWindowOf, reinterpret_cast<LPARAM>(&mine));
			if (!mine.window) {
				return false;
			}
			if (IsIconic(mine.window)) {
				// (minimised: the player has tabbed away. Left alone - restoring it from here put a borderless Skyrim on
				// another screen, seen in play 2026-10-11)
				return false;
			}
			const DWORD self = GetCurrentThreadId();
			AttachThreadInput(self, thread, TRUE);
			BringWindowToTop(mine.window);
			SetForegroundWindow(mine.window);
			AttachThreadInput(self, thread, FALSE);
			logger::info("session: MORDHAU's window had taken the keyboard; given back to Skyrim");
			return true;
		}

		// A line at the top left of Skyrim's screen, once the game is being played (the main menu shows none).
		void Tell(std::string a_text)
		{
			std::thread([text = std::move(a_text)]() {
				for (int tries = 0; tries < 400; ++tries) {
					std::atomic_bool shown{ false }, done{ false };
					SKSE::GetTaskInterface()->AddTask([&]() {
						auto* ui = RE::UI::GetSingleton();
						if (ui && ui->IsMenuOpen(RE::HUDMenu::MENU_NAME) && !ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) && !ui->IsMenuOpen(RE::MainMenu::MENU_NAME)) {
							RE::SendHUDMessage::ShowHUDMessage(text.c_str());
							shown = true;
						}
						done = true;
					});
					while (!done) {
						Sleep(50);
					}
					if (shown) {
						return;
					}
					Sleep(3000);
				}
			}).detach();
		}

		// MORDHAU is streaming its character: the sequence number at the head of the shared pose file moves.
		bool Streaming(const fs::path& a_cache)
		{
			static std::uint32_t last = 0;
			static bool          seen = false;
			char                 head[8]{};
			std::ifstream        in(a_cache / "live_pose.bin", std::ios::binary);
			if (!in.read(head, 8) || std::memcmp(head, "DMLP", 4) != 0) {
				return false;
			}
			std::uint32_t sequence = 0;
			std::memcpy(&sequence, head + 4, 4);
			const bool moved = seen && sequence != last;
			last = sequence;
			seen = true;
			return moved;
		}

		void Run()
		{
			std::error_code ec;
			const auto      cache = g_settings.cache.empty() ? mbake::DefaultCache() : g_settings.cache;
			const auto      bin = MordhauBin();
			if (bin.empty()) {
				logger::error("session: MORDHAU was not found (through Steam{}). Install it, or put its folder into MordhauSkyrim.ini (MordhauPath).", g_settings.mordhau.empty() ? "" : ", or in MordhauPath");
				Tell("MORDHAU x Skyrim: MORDHAU was not found - see MordhauSkyrim.log");
				return;
			}
			logger::info("session: managed by the plugin. MORDHAU in '{}', cache '{}'", bin.string(), cache.string());
			fs::create_directories(cache, ec);
			// a first start: the plugin builds the cache of MORDHAU's models once Skyrim's main menu is up, and MORDHAU's
			// script needs it. Every later start: MORDHAU at once.
			if (!fs::exists(cache / "baked.txt", ec)) {
				logger::info("session: first start - waiting for the plugin to prepare MORDHAU's models");
				while (!fs::exists(cache / "baked.txt", ec)) {
					Sleep(1000);
				}
			}
			// the player has not yet accepted the first-start message about the risk (sheets/first_start.json): nothing is
			// started before that
			if (!FirstStart::Accepted()) {
				logger::info("session: waiting for the player to accept the first-start message (MORDHAU is not started before that)");
				while (!FirstStart::Accepted()) {
					Sleep(1000);
				}
				logger::info("session: the first-start message was accepted");
			}
			int  quick = 0;  // starts in a row that ended within a minute
			bool told = false;
			for (;;) {
				if (MordhauRunning()) {
					// not this session's: it may be the player's own game, online. Never touched; waited for.
					logger::info("session: a MORDHAU is running already: left alone (the mashup's own is started when it has closed)");
					while (MordhauRunning()) {
						Sleep(5000);
					}
				}
				// (left over from a session that ended badly; someone's own always-on loader is set aside, not deleted)
				if (fs::exists(bin / kLoader, ec) && !SameBytes(g_payload / kLoader, bin / kLoader)) {
					MoveFileExW((bin / kLoader).c_str(), (bin / L"dwmapi.dll.before-mashup").c_str(), MOVEFILE_REPLACE_EXISTING);
					logger::info("session: MORDHAU's folder had a dwmapi.dll of its own (an always-on script loader): set aside as dwmapi.dll.before-mashup");
				}
				ClearLoader(bin);
				if (!PlaceFiles(bin, cache)) {
					logger::error("session: MORDHAU's side could not be put into '{}' (is the folder write-protected?)", bin.string());
					Tell("MORDHAU x Skyrim: could not write into MORDHAU's folder - see MordhauSkyrim.log");
					return;
				}
				bool         other = false;
				MordhauAudio::KeepAudible(cache);  // (MORDHAU heard behind Skyrim: sheets/builds.json mordhau_background_audio)
				const HANDLE mordhau = StartMordhau(bin, cache, other);
				if (!mordhau && other) {
					continue;  // (back to waiting for that MORDHAU to close)
				}
				if (!mordhau) {
					Tell("MORDHAU x Skyrim: MORDHAU could not be started - see MordhauSkyrim.log");
					return;
				}
				const DWORD process = GetProcessId(mordhau);
				const auto  started = Clock::now();
				bool        cleared = false, live = false;
				// the stalled-script watchdog (sheets/live_link.json script_watchdog)
				constexpr float kUnverified = 5.0f, kStall = 30.0f;
				auto            lastData = Clock::now();
				bool            unverified = false, stalled = false;
				int         focus = 2;  // (its splash window and its game window, as they appear - later the player's own tabbing is not undone)
				while (WaitForSingleObject(mordhau, 200) == WAIT_TIMEOUT) {
					// The script's stop signal (sheets/live_link.json offline_stop): the session is no longer an offline
					// one. The process is ended here, from outside, and it is waited for until it is really gone.
					if (WIN32_FILE_ATTRIBUTE_DATA stop{}; GetFileAttributesExW((cache / "live_stop.txt").c_str(), GetFileExInfoStandard, &stop)) {
						FILETIME now{};
						GetSystemTimeAsFileTime(&now);
						const auto ticks = [](const FILETIME& a_time) { return (static_cast<std::uint64_t>(a_time.dwHighDateTime) << 32) | a_time.dwLowDateTime; };
						const auto since = [&]() {
							FILETIME at{};
							GetSystemTimeAsFileTime(&at);
							return static_cast<double>(ticks(at) - ticks(stop.ftLastWriteTime)) / 10000.0;
						};
						const double seen = since();
						TerminateProcess(mordhau, 0);
						const bool gone = WaitForSingleObject(mordhau, 10000) == WAIT_OBJECT_0;
						logger::warn("session: MORDHAU's script reported that the match is no longer offline: process ended by the plugin - signal seen after {:.0f} ms, process {} {:.0f} ms after the signal",
							seen, gone ? "gone" : "STILL THERE", since());
						DeleteFileW((cache / "live_stop.txt").c_str());
						Tell("MORDHAU x Skyrim: MORDHAU was closed - it was no longer in its offline match");
						break;
					}
					if (!cleared && (LoaderLoaded(mordhau, bin) || Seconds(started) > 25.0f)) {
						cleared = ClearLoader(bin);
						logger::info("session: the script loader is out of MORDHAU's folder again ({:.0f} s after the start)", Seconds(started));
					}
					if (focus > 0 && Seconds(started) < 40.0f && FocusBack(process)) {
						--focus;
					}
					const bool data = Streaming(cache);
					if (data) {
						if (unverified) {
							logger::info("session: MORDHAU's script reports again after {:.1f} s without data (a stall that passed)", Seconds(lastData));
							unverified = false;
						}
						lastData = Clock::now();
					}
					if (!live && Seconds(started) > 10.0f && data) {
						live = true;
						quick = 0;
						logger::info("session: MORDHAU is live ({:.0f} s after its start)", Seconds(started));
						MordhauAudio::Restore(cache);  // (MORDHAU has read the setting at its start: the player's own value goes back into the file)
						if (!told) {
							Tell("MORDHAU is live");
							told = true;
						}
					}
					if (live && Seconds(lastData) > kUnverified) {
						// no report from the script: not proof of anything. Tolerated for a while - a lag spike, a hitch,
						// a minimised window - but named for what it is, and not for ever.
						if (!unverified) {
							unverified = true;
							logger::warn("session: no data from MORDHAU's script for {:.0f} s - the session is UNVERIFIED (tolerated up to {:.0f} s)", kUnverified, kStall);
						}
						Find its{ process, nullptr };
						EnumWindows(FindWindowOf, reinterpret_cast<LPARAM>(&its));
						if (its.window && IsIconic(its.window)) {
							ShowWindow(its.window, SW_SHOWNOACTIVATE);  // (minimised, MORDHAU stops running its frames)
						}
						if (Seconds(lastData) > kStall) {
							const float silent = Seconds(lastData);
							const auto  ending = Clock::now();
							TerminateProcess(mordhau, 0);
							const bool gone = WaitForSingleObject(mordhau, 10000) == WAIT_OBJECT_0;
							logger::warn("session: MORDHAU's script was silent for {:.0f} s: it cannot be told what state the session is in - process ended by the plugin ({} after {:.0f} ms)", silent,
								gone ? "gone" : "STILL THERE", Seconds(ending) * 1000.0f);
							stalled = true;
							break;
						}
					}
				}
				CloseHandle(mordhau);
				ClearLoader(bin);
				MordhauAudio::Restore(cache);
				logger::info("session: MORDHAU closed after {:.0f} s", Seconds(started));
				if (Seconds(started) < 60.0f && ++quick >= 3) {
					logger::error("session: MORDHAU closed three times right after starting; not started again (see UE4SS.log in '{}')", bin.string());
					Tell("MORDHAU x Skyrim: MORDHAU keeps closing - see MordhauSkyrim.log");
					return;
				}
				Sleep(5000);
				logger::info("session: starting MORDHAU again (Skyrim carries on in its built-in mode until it is back)");
			}
		}
	}

	void Begin(void* a_plugin)
	{
		wchar_t path[MAX_PATH]{};
		GetModuleFileNameW(static_cast<HMODULE>(a_plugin), path, MAX_PATH);
		const auto folder = fs::path(path).parent_path();
		ReadSettings(folder / "MordhauSkyrim.ini");
		{
			// (a development copy: the dev file beside SkyrimSE.exe names the cache and MORDHAU's folder for the whole plugin)
			wchar_t exe[MAX_PATH]{};
			GetModuleFileNameW(nullptr, exe, MAX_PATH);
			std::ifstream in(fs::path(exe).parent_path() / "DirectionalMelee.dev.txt");
			std::string   line;
			while (std::getline(in, line)) {
				if (line.starts_with("cache=")) {
					g_settings.cache = Trim(line.substr(6));
				} else if (line.starts_with("mordhau=")) {
					g_settings.mordhau = Trim(line.substr(8));
				}
			}
		}
		g_payload = folder / "MordhauSkyrim" / "mordhau";
		std::error_code ec;
		char            launcher[8]{};
		const bool      byLauncher = GetEnvironmentVariableA("MORDHAU_SKYRIM", launcher, sizeof(launcher)) > 0 && launcher[0] == '1';
		if (!fs::exists(g_payload / "UE4SS.dll", ec) || !fs::exists(g_payload / kLoader, ec)) {
			logger::info("session: MORDHAU's side is not beside the plugin ('{}'): the session is someone else's to run (the launcher)", g_payload.string());
			return;
		}
		if (byLauncher || !g_settings.startMordhau) {
			logger::info("session: not run by the plugin ({})", byLauncher ? "the launcher started this Skyrim" : "StartMordhau=0");
			return;
		}
		std::thread(Run).detach();
	}

	std::filesystem::path CacheFolder()
	{
		return g_settings.cache;
	}

	std::filesystem::path MordhauFolder()
	{
		return g_settings.mordhau;
	}
}
