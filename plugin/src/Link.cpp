#include "Link.h"

#include <Windows.h>
#include <chrono>
#include <deque>
#include <format>
#include <mutex>

namespace dm::Link
{
	namespace
	{
		using Clock = std::chrono::steady_clock;
		constexpr std::size_t kStateBytes = 836 + (kGhosts - 3) * 28 + 32 + kGhosts * 20;  // (408 + the bow's pose, 428, + the stand-ins after the third)

		std::filesystem::path g_cache;
		std::mutex            g_lock;
		State                 g_state;
		std::uint32_t         g_lastSequence = 0;
		bool                  g_seenOne = false;
		Clock::time_point     g_lastChange{};
		std::vector<char>     g_buffer;

		struct Wish
		{
			float forward = 0.0f, right = 0.0f, pitch = 0.0f;
			bool  sprint = false, firstPerson = false;
		};
		Wish                                      g_wish;
		std::deque<std::pair<std::uint32_t, std::string>> g_actions;  // the latest few, so one lost write loses nothing
		std::uint32_t                             g_actionId = 0;
		std::uint32_t                             g_writeSequence = 0;
		std::vector<Ghost>                        g_ghosts;
		float                                     g_health = 100.0f;
		GhostPose                                 g_ghostPoses[kGhosts];
		double                                    g_mouseRight = 0.0, g_mouseUp = 0.0;  // running sums; MORDHAU's side applies the change
	}

	void SetCache(const std::filesystem::path& a_cache)
	{
		g_cache = a_cache;
		// action ids keep rising across game sessions so MORDHAU never mistakes a new request for an old one
		g_actionId = static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count() % 100000000) * 10;
	}

	// MORDHAU stops running its frames while its window is minimised (Skyrim taking the screen can do that): put the
	// window back without giving it the keyboard.
	void KeepMordhauAwake()
	{
		static Clock::time_point last{};
		if (std::chrono::duration<float>(Clock::now() - last).count() < 2.0f) {
			return;
		}
		last = Clock::now();
		HWND window = nullptr;
		while ((window = FindWindowExW(nullptr, window, L"UnrealWindow", nullptr)) != nullptr) {
			wchar_t title[64]{};
			GetWindowTextW(window, title, 63);
			if (std::wstring_view(title).find(L"MORDHAU") != std::wstring_view::npos && IsIconic(window)) {
				ShowWindow(window, SW_SHOWNOACTIVATE);
				logger::info("link: MORDHAU's window was minimised; restored it (it must stay open behind Skyrim)");
			}
		}
	}

	// Shared memory with MORDHAU: a fixed-size file mapped once and kept (see sheets/live_link.json 'transport').
	struct Shared
	{
		HANDLE      file = INVALID_HANDLE_VALUE;
		HANDLE      mapping = nullptr;
		char*       view = nullptr;
		std::size_t size = 0;
	};
	Shared g_poseMemory, g_inputMemory;

	char* Map(Shared& a_shared, const wchar_t* a_name, std::size_t a_size)
	{
		if (a_shared.view) {
			return a_shared.view;
		}
		const auto path = g_cache / a_name;
		a_shared.file = CreateFileW(path.wstring().c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,  // (not shared for delete: nobody may remove the file from under the mapping)
			
			FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_TEMPORARY, nullptr);
		if (a_shared.file == INVALID_HANDLE_VALUE) {
			return nullptr;
		}
		// (CreateFileMapping grows the file to the size asked for when it is shorter)
		a_shared.mapping = CreateFileMappingW(a_shared.file, nullptr, PAGE_READWRITE, 0, static_cast<DWORD>(a_size), nullptr);
		a_shared.view = a_shared.mapping ? static_cast<char*>(MapViewOfFile(a_shared.mapping, FILE_MAP_ALL_ACCESS, 0, 0, a_size)) : nullptr;
		if (!a_shared.view) {
			if (a_shared.mapping) {
				CloseHandle(a_shared.mapping);
			}
			CloseHandle(a_shared.file);
			a_shared = {};
			return nullptr;
		}
		a_shared.size = a_size;
		logger::info("link: {} is shared memory now ({} bytes mapped)", path.filename().string(), a_size);
		return a_shared.view;
	}
	constexpr std::size_t kPoseBytes = 131072, kInputBytes = 16384;  // (doubled for 24 stand-ins: each is about 4.5 KB of pose)

	bool Poll(std::size_t a_bones, std::vector<float>& a_pose)
	{
		if (g_cache.empty()) {
			return false;
		}
		KeepMordhauAwake();
		const auto        now = Clock::now();
		const std::size_t ghostBytes = 4 + a_bones * 28 + 28 + 4;
		const std::size_t size = 16 + a_bones * 28 + kStateBytes + kGhosts * ghostBytes + 4;
		g_buffer.resize(size);
		std::scoped_lock lock(g_lock);
		const char* shared = size <= kPoseBytes ? Map(g_poseMemory, L"live_pose.bin", kPoseBytes) : nullptr;
		if (shared) {
			// (a copy is taken and checked: start and end sequence equal = not caught in the middle of being written)
			std::memcpy(g_buffer.data(), shared, size);
			if (std::memcmp(g_buffer.data(), "DMLP", 4) == 0) {
				std::uint32_t sequence = 0, tail = 0;
				std::int32_t  bones = 0;
				std::memcpy(&sequence, g_buffer.data() + 4, 4);
				std::memcpy(&bones, g_buffer.data() + 12, 4);
				std::memcpy(&tail, g_buffer.data() + size - 4, 4);
				if (sequence == tail && static_cast<std::size_t>(bones) == a_bones && sequence != g_lastSequence) {
					std::memcpy(&g_state.clock, g_buffer.data() + 8, 4);
					// a file left over from an earlier session is not live until it changes
					if (g_seenOne) {
						g_lastChange = now;
						a_pose.resize(a_bones * 7);
						std::memcpy(a_pose.data(), g_buffer.data() + 16, a_bones * 28);
						const char* s = g_buffer.data() + 16 + a_bones * 28;
						std::memcpy(g_state.bladeStart, s, 12);
						std::memcpy(g_state.bladeEnd, s + 12, 12);
						std::memcpy(&g_state.stage, s + 24, 4);
						std::memcpy(&g_state.kind, s + 28, 4);
						std::memcpy(&g_state.motionTime, s + 32, 4);
						std::memcpy(&g_state.stamina, s + 36, 4);
						std::memcpy(&g_state.health, s + 40, 4);
						std::memcpy(&g_state.speedRight, s + 44, 4);
						std::memcpy(&g_state.speedForward, s + 48, 4);
						std::memcpy(g_state.motion, s + 52, 48);
						g_state.motion[47] = 0;
						std::memcpy(g_state.weaponRot, s + 100, 16);
						std::memcpy(g_state.weaponPos, s + 116, 12);
						std::int32_t hasWeapon = 0;
						std::memcpy(&hasWeapon, s + 128, 4);
						g_state.hasWeapon = hasWeapon == 1;
						g_state.weaponState = hasWeapon;
						std::memcpy(&g_state.playerHits, s + 132, 4);
						std::memcpy(&g_state.playerDamage, s + 136, 4);
						std::memcpy(&g_state.playerAttacker, s + 140, 4);
						for (int i = 0; i < kGhosts; ++i) {
							const char* g = i < 3 ? s + 144 + i * 28 : s + 836 + (i - 3) * 28;  // (the first three where they always were)
							std::memcpy(&g_state.ghosts[i].id, g, 4);
							std::memcpy(&g_state.ghosts[i].hits, g + 4, 4);
							std::memcpy(&g_state.ghosts[i].damage, g + 8, 4);
							std::memcpy(&g_state.ghosts[i].stage, g + 12, 4);
							std::memcpy(&g_state.ghosts[i].otherHits, g + 16, 4);
							std::memcpy(&g_state.ghosts[i].otherDamage, g + 20, 4);
							std::memcpy(&g_state.ghosts[i].otherBy, g + 24, 4);
						}
						std::memcpy(g_state.cameraRot, s + 228, 16);
						std::memcpy(&g_state.shots, s + 260, 4);
						std::memcpy(&g_state.shieldHas, s + 836 + (kGhosts - 3) * 28, 4);
						g_state.altMode = (g_state.shieldHas & 256) != 0;  // (carried in the same number: +256)
						g_state.shieldHas &= 255;
						std::memcpy(g_state.shield, s + 836 + (kGhosts - 3) * 28 + 4, 28);
						for (int k = 0; k < kGhosts; ++k) {
							const char* at = s + 836 + (kGhosts - 3) * 28 + 32 + k * 20;
							std::memcpy(&g_state.moves[k].id, at, 4);
							std::memcpy(&g_state.moves[k].right, at + 4, 4);
							std::memcpy(&g_state.moves[k].forward, at + 8, 4);
							std::memcpy(&g_state.moves[k].yaw, at + 12, 4);
							std::memcpy(&g_state.moves[k].free, at + 16, 4);
						}
						std::memcpy(&g_state.bowBones, s + 408, 4);
						std::memcpy(g_state.bow, s + 412, sizeof(g_state.bow));
						std::memcpy(&g_state.arrowShown, s + 748, 4);
						std::memcpy(g_state.arrowAt, s + 752, 28);
						std::memcpy(g_state.arrowBones, s + 780, 56);
						std::memcpy(g_state.weaponClass, s + 376, 32);
						g_state.weaponClass[31] = 0;
						std::memcpy(g_state.shotDir, s + 264, 12);
						std::memcpy(&g_state.shotSpeed, s + 276, 4);
						std::memcpy(&g_state.shotGravity, s + 280, 4);
						std::memcpy(g_state.shotDamage, s + 284, 16);
						std::memcpy(g_state.shotHead, s + 300, 16);
						std::memcpy(g_state.cameraPos, s + 244, 12);
						std::int32_t first = 0;
						std::memcpy(&first, s + 256, 4);
						g_state.firstPerson = (first & 1) != 0;
						g_state.cameraFov = static_cast<float>((first >> 1) & 0x7FF);
						g_state.menuOpen = (first & 4096) != 0;
						for (int i = 0; i < kGhosts; ++i) {
							const char* g = s + kStateBytes + static_cast<std::size_t>(i) * ghostBytes;
							auto&       pose = g_ghostPoses[i];
							std::int32_t look = 0, has = 0;
							std::memcpy(&look, g, 4);
							pose.look = look;
							pose.bones.resize(a_bones * 7);
							std::memcpy(pose.bones.data(), g + 4, a_bones * 28);
							std::memcpy(pose.weaponRot, g + 4 + a_bones * 28, 16);
							std::memcpy(pose.weaponPos, g + 4 + a_bones * 28 + 16, 12);
							std::memcpy(&has, g + 4 + a_bones * 28 + 28, 4);
							pose.hasWeapon = has == 1;
							pose.weaponState = has;
						}
						g_state.sequence = sequence;
					}
					g_seenOne = true;
					g_lastSequence = sequence;
				}
			}
		}
		const bool live = g_state.sequence != 0 && std::chrono::duration<float>(now - g_lastChange).count() < 2.0f;  // (rides through MORDHAU's short hitches)
		if (live != g_state.live) {
			g_state.live = live;
			logger::info("link: {}", live ? "MORDHAU is live: its character drives the player's body and combat" : "MORDHAU's stream stopped: back to the built-in clips");
		}
		return live;
	}

	State Current()
	{
		std::scoped_lock lock(g_lock);
		return g_state;
	}

	void SetMovement(float a_forward, float a_right, bool a_sprint, float a_pitch, bool a_firstPerson)
	{
		std::scoped_lock lock(g_lock);
		g_wish = { a_forward, a_right, a_pitch, a_sprint, a_firstPerson };
	}

	bool GhostPoseAt(int a_slot, GhostPose& a_out)
	{
		std::scoped_lock lock(g_lock);
		if (a_slot < 0 || a_slot >= kGhosts || !g_state.live || g_ghostPoses[a_slot].look <= 0) {
			return false;
		}
		a_out = g_ghostPoses[a_slot];
		return true;
	}

	void SetGhosts(const std::vector<Ghost>& a_ghosts)
	{
		std::scoped_lock lock(g_lock);
		g_ghosts = a_ghosts;
	}

	std::string                g_weaponWish;

	void SetWeaponWish(const char* a_item)
	{
		std::scoped_lock lock(g_lock);
		g_weaponWish = a_item ? a_item : "";
	}

	bool                       g_downPlayer = false;
	std::vector<std::uint32_t> g_downIds;

	void SetDown(bool a_player, const std::vector<std::uint32_t>& a_ids)
	{
		std::scoped_lock lock(g_lock);
		g_downPlayer = a_player;
		g_downIds = a_ids;
	}

	void SetHealth(float a_percent)
	{
		std::scoped_lock lock(g_lock);
		g_health = a_percent;
	}

	void AddMouse(float a_right, float a_up)
	{
		std::scoped_lock lock(g_lock);
		g_mouseRight += a_right;
		g_mouseUp += a_up;
	}

	std::string g_playerWeapon;
	void        SetPlayerWeapon(const char* a_path)
	{
		std::scoped_lock lock(g_lock);
		g_playerWeapon = a_path ? a_path : "";
	}

	int  g_walls[3]{};
	void SetWalls(int a_left, int a_ahead, int a_right)
	{
		std::scoped_lock lock(g_lock);
		g_walls[0] = a_left, g_walls[1] = a_ahead, g_walls[2] = a_right;
	}

	std::string g_shield;
	std::string g_hybrid;
	std::string g_playerPlace;
	bool        g_paused = false;
	void        SetPaused(bool a_paused)
	{
		std::scoped_lock lock(g_lock);
		g_paused = a_paused;
	}
	void        SetPlayerPlace(float a_xCm, float a_yCm, float a_facingDegrees)
	{
		std::scoped_lock lock(g_lock);
		g_playerPlace = std::format(" pp:{:.1f},{:.1f},{:.3f}", a_xCm, a_yCm, a_facingDegrees);
	}
	void        SetHybrid(bool a_on, float a_engageCm, float a_snapCm, float a_holdSeconds)
	{
		std::scoped_lock lock(g_lock);
		g_hybrid = std::format(" hy:{},{:.0f},{:.0f},{:.0f}", a_on ? 1 : 0, a_engageCm, a_snapCm, a_holdSeconds * 1000.0f);
	}
	void        SetShield(const char* a_mordhauShield)
	{
		std::scoped_lock lock(g_lock);
		g_shield = a_mordhauShield ? a_mordhauShield : "";
	}
	int   g_staminaGains = 0;
	float g_staminaGain = 0.0f;
	void  AddStamina(float a_percent)
	{
		std::scoped_lock lock(g_lock);
		++g_staminaGains;
		g_staminaGain = a_percent;
	}
	int  g_aiLevel = 3;
	void SetAiLevel(int a_level)
	{
		std::scoped_lock lock(g_lock);
		g_aiLevel = std::clamp(a_level, 1, 5);
	}

	bool g_crouch = false;
	void SetCrouch(bool a_crouch)
	{
		std::scoped_lock lock(g_lock);
		g_crouch = a_crouch;
	}

	void Request(const char* a_action)
	{
		std::scoped_lock lock(g_lock);
		g_actions.emplace_back(++g_actionId, a_action);
		while (g_actions.size() > 6) {
			g_actions.pop_front();
		}
		logger::info("link: asking MORDHAU for {}", a_action);
	}

	void Flush()
	{
		if (g_cache.empty()) {
			return;
		}
		std::string line;
		{
			std::scoped_lock lock(g_lock);
			line = std::format("{} {:.3f} {:.3f} {} {:.2f} {:.1f} {:.1f} {}", ++g_writeSequence, g_wish.forward, g_wish.right, g_wish.sprint ? 1 : 0, g_wish.pitch, g_mouseRight, g_mouseUp,
				g_wish.firstPerson ? 1 : 0);
			line += std::format(" h:{:.0f}", g_health);
			for (const auto& [id, name] : g_actions) {
				line += std::format(" {}:{}", id, name);
			}
			for (const auto& g : g_ghosts) {
				line += std::format(" g:{},{:.1f},{:.1f},{:.1f},{:.1f},{},{},{:.0f},{:.0f},{}", g.id, g.right, g.forward, g.up, g.yaw, g.attacks, g.blocking ? 1 : 0, g.velRight,
					g.velForward, g.weapon && *g.weapon ? g.weapon : "-");
				line += std::format(",{:.0f}", g.health);
				if (g.refused) {
					line += std::format(" hr:{}", g.id);
				}
				if (std::abs(g.size - 1.0f) > 0.004f) {
					line += std::format(" gz:{},{:.3f}", g.id, g.size);
				}
				if (g.target != 0) {
					line += std::format(" gt:{},{}", g.id, g.target);
				}
				if (g.shield && *g.shield) {
					line += std::format(" gs:{},{}", g.id, g.shield);
				}
				if (g.passive) {
					line += std::format(" gp:{}", g.id);
				}
				if (g.dummy) {
					line += std::format(" gd:{}", g.id);
				}
				if (g.skill10 > 0) {
					line += std::format(" gb:{},{}", g.id, g.skill10);  // (its own skill: the chosen level moved by the level gap)
				}
			}
			line += std::format(" ai:{}", g_aiLevel);
			line += g_hybrid;
			line += g_playerPlace;
			if (g_paused) {
				line += " pz:1";
			}
			if (!g_shield.empty()) {
				line += " sh:" + g_shield;
			}
			if (g_staminaGains > 0) {
				line += std::format(" sg:{},{:.1f}", g_staminaGains, g_staminaGain);
			}
			if (g_crouch) {
				line += " cr:1";
			}
			if (g_walls[0] || g_walls[1] || g_walls[2]) {
				line += std::format(" wl:{},{},{}", g_walls[0], g_walls[1], g_walls[2]);
			}
			if (!g_playerWeapon.empty()) {
				line += " pw:" + g_playerWeapon;
			}
			if (!g_weaponWish.empty()) {
				line += " w:" + g_weaponWish;
			}
			if (g_downPlayer || !g_downIds.empty()) {
				line += std::format(" down:{}", g_downPlayer ? 1 : 0);
				for (const auto id : g_downIds) {
					line += std::format(",{}", id);
				}
			}
			line += "\n";
		}
		// into the shared block in place: sequence, length, the line, the sequence again (MORDHAU's script takes it only
		// when both sequences agree, so it never acts on half a line)
		char* shared = Map(g_inputMemory, L"live_input.mem", kInputBytes);
		if (!shared || line.size() + 12 > kInputBytes) {
			return;
		}
		static std::uint32_t stamp = 0;
		++stamp;
		const auto length = static_cast<std::uint32_t>(line.size());
		std::memcpy(shared, &stamp, 4);
		std::memcpy(shared + 4, &length, 4);
		std::memcpy(shared + 8, line.data(), line.size());
		std::memcpy(shared + 8 + line.size(), &stamp, 4);
	}
}
