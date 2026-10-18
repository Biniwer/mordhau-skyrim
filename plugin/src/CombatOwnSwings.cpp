#include "CombatInternal.h"

namespace dm::Combat
{
	inline namespace internal
	{
		const sheets::Swing* SwingForAngle(float a_degrees)
		{
			for (const auto& swing : sheets::kSwings) {
				if (!swing.hasAngle) {
					continue;
				}
				// compare on a circle: both a and a+360 against [from, to)
				for (const float a : { a_degrees, a_degrees + 360.0f, a_degrees - 360.0f }) {
					if (a >= swing.angleFrom && a < swing.angleTo) {
						return &swing;
					}
				}
			}
			return nullptr;
		}

		std::shared_ptr<Clip> GetClip(const std::string& a_name)
		{
			if (const auto it = g_clips.find(a_name); it != g_clips.end()) {
				return it->second;
			}
			std::shared_ptr<Clip> clip;
			if (!g_dev.cache.empty()) {
				clip = Clip::Load(g_dev.cache / (a_name + ".dmclip"));
			}
			logger::info("clip '{}': {}", a_name, clip ? "loaded" : "not available");
			g_clips.emplace(a_name, clip);
			return clip;
		}

		bool StartPlayerSwing(float a_degrees)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player || !player->Is3DLoaded() || !player->AsActorState()->IsWeaponDrawn()) {
				return false;
			}
			const auto* family = FamilyOf(player);
			const auto* swing = SwingForAngle(a_degrees);
			if (swing && LiveMelee(player)) {
				// MORDHAU is running alongside: its own combat decides what this press becomes (attack, combo, morph)
				// MORDHAU reads the mouse movement itself (Link::AddMouse) and picks the side and angle: its own 360 strike
				g_liveSwing = swing;
				Link::Request("Strike");
				return true;
			}
			if (!family || !swing) {
				return false;
			}
			const sheets::ClipRow* row = nullptr;
			for (const auto& candidate : sheets::kClips) {
				if (std::string_view(candidate.clipSet) == family->clipSet && std::string_view(candidate.move) == swing->move) {
					row = &candidate;
					break;
				}
			}
			if (!row || !*row->clip3p) {
				return false;
			}
			// the dev cache is flat: file name = clip name without its folder
			auto clip = GetClip(std::filesystem::path(row->clip3p).filename().string());
			if (!clip) {
				return false;
			}
			if (!g_rest.Loaded() && !g_rest.Load(player)) {
				return false;
			}
			ActiveSwing active;
			active.clip = std::move(clip);
			active.clipName = std::filesystem::path(row->clip3p).filename().string();
			active.swing = swing;
			active.family = family;
			active.start = Clock::now();
			bool fromW = false, fromR = false, fromRec = false;
			const float windup = g_stats.Get(family->id, swing->stab, "windup", &fromW);
			const float release = g_stats.Get(family->id, swing->stab, "release", &fromR);
			const float recovery = g_stats.Get(family->id, swing->stab, "miss_recovery", &fromRec);
			active.windup = windup > 0.0f ? windup : active.windup;
			active.release = release > 0.0f ? release : active.release;
			active.recovery = recovery > 0.0f ? recovery : active.recovery;
			if (row->windupEnd > 0.0f && row->releaseEnd > row->windupEnd) {
				active.windupEnd = row->windupEnd;
				active.releaseEnd = row->releaseEnd;
			}
			// (a riposte replaces the clip and these fractions just below)
			if (Clock::now() < g_riposteUntil) {
				g_riposteUntil = {};
				const float riposteWindup = g_stats.Parry("riposte_windup");
				if (riposteWindup > 0.0f) {
					active.windup = std::min(active.windup, riposteWindup);
				}
				// MORDHAU's riposte clip starts from the raised parry pose
				auto riposteClip = *row->riposte3p ? GetClip(std::filesystem::path(row->riposte3p).filename().string()) : nullptr;
				if (riposteClip) {
					active.clip = std::move(riposteClip);
					active.clipName = std::filesystem::path(row->riposte3p).filename().string();
					if (row->riposteWindupEnd > 0.0f && row->riposteReleaseEnd > row->riposteWindupEnd) {
						active.windupEnd = row->riposteWindupEnd;
						active.releaseEnd = row->riposteReleaseEnd;
					}
				}
				logger::info("swing: riposte, windup {:.3f}s, {}", active.windup, *row->riposte3p && active.clip ? row->riposte3p : "no riposte clip (normal clip)");
			}
			logger::info("swing: {} ({} deg) with {}: windup {:.3f}s{} release {:.3f}s{} recovery {:.3f}s{}", swing->id, static_cast<int>(a_degrees), family->id,
				active.windup, fromW ? "" : " (fallback)", active.release, fromR ? "" : " (fallback)", active.recovery, fromRec ? "" : " (fallback)");
			g_swing = std::move(active);
			return true;
		}

		void ApplyActiveSwing()
		{
			if (!g_swing) {
				return;
			}
			auto&       swing = *g_swing;
			auto*       player = RE::PlayerCharacter::GetSingleton();
			const float t = Seconds(swing.start);
			// a hit ends the release at once; recovery starts from there
			const float releaseEndTime = swing.hitAt >= 0.0f ? swing.hitAt : swing.windup + swing.release;
			if (!player || t >= releaseEndTime + swing.recovery) {
				g_swing.reset();
				return;
			}
			const float length = swing.clip->length;
			float       clipTime = 0.0f, weight = 1.0f;
			if (t < swing.windup) {
				clipTime = t / swing.windup * swing.windupEnd * length;
			} else if (t < releaseEndTime) {
				clipTime = (swing.windupEnd + (t - swing.windup) / swing.release * (swing.releaseEnd - swing.windupEnd)) * length;
			} else {
				// recovery: carry on from where the release stopped, fading back to Skyrim's own pose
				const float stopped = (swing.windupEnd + (releaseEndTime - swing.windup) / swing.release * (swing.releaseEnd - swing.windupEnd)) * length;
				clipTime = std::min(stopped + (t - releaseEndTime), length);
				weight = 1.0f - (t - releaseEndTime) / swing.recovery;
			}
			weight = std::clamp(std::min(weight, t / kBlendIn), 0.0f, 1.0f);
			if (g_dev.hold >= 0.0f) {
				// development: stay on one pose so pictures can be compared
				swing.start = Clock::now() - std::chrono::milliseconds(200);
				clipTime = g_dev.hold * length;
				weight = 1.0f;
			}
			static Clock::time_point lastArmProbe = Clock::now();
			const bool               armProbe = g_dev.hold >= 0.0f && Seconds(lastArmProbe) > 4.0f;
			if (armProbe) {
				lastArmProbe = Clock::now();
				ProbeArm(player, "before");
			}
			swing.clipTime = clipTime;
			swing.weight = weight;
			const int driven = ApplyClip(player, g_rest, *swing.clip, clipTime, weight, g_dev.twist);
			if (armProbe) {
				ProbeArm(player, "after");
			}
			if (!swing.logged) {
				swing.logged = true;
				logger::info("swing: driving {} bones", driven);
			}

			if (t >= swing.windup && t < releaseEndTime && swing.hitAt < 0.0f) {
				float bladeLength = 0.0f;
				auto* target = BladeTarget(player, swing, &bladeLength);
				if (!swing.releaseLogged) {
					swing.releaseLogged = true;
					logger::info("swing: release at {:.3f}s, blade {:.0f} units", t, bladeLength);
				}
				if (target) {
					swing.hitAt = t;
					ApplyHit(player, target, swing);
				}
			} else if (t >= releaseEndTime && swing.hitAt < 0.0f && !swing.missCharged) {
				swing.missCharged = true;
				const float cost = g_stats.Get(swing.family->id, swing.swing->stab, "miss_stamina");
				auto*       values = player->AsActorValueOwner();
				const float before = values->GetActorValue(RE::ActorValue::kStamina);
				if (cost > 0.0f) {
					values->DamageActorValue(RE::ActorValue::kStamina, cost);
					g_ownStaminaSpent += cost;
				}
				logger::info("miss: {} at {:.3f}s, stamina {:.0f} -> {:.0f} (cost {:.0f})", swing.swing->id, t, before, values->GetActorValue(RE::ActorValue::kStamina), cost);
			}

			if (g_dev.autotest == 1) {
				const int quarter = static_cast<int>(clipTime / length * 4.0f);
				if (quarter >= 1 && quarter <= 3 && quarter != swing.probed) {
					swing.probed = quarter;
					Probe(player, quarter * 25);
				}
			}
		}

		const sheets::ClipRow* ClipFor(const sheets::WeaponFamily* a_family, std::string_view a_move)
		{
			for (const auto& candidate : sheets::kClips) {
				if (std::string_view(candidate.clipSet) == a_family->clipSet && a_move == candidate.move) {
					return &candidate;
				}
			}
			return nullptr;
		}

		// Starts a parry with what the player holds. False = nothing of ours to play, Skyrim's block goes ahead.
		bool StartPlayerParry()
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (player && player->Is3DLoaded() && player->AsActorState()->IsWeaponDrawn() && LiveMelee(player)) {
				Link::Request("Parry");  // MORDHAU's own key: a parry, or a feint during a windup
				g_parryAsked = Clock::now();
				return true;
			}
			if (!player || !player->Is3DLoaded() || !player->AsActorState()->IsWeaponDrawn() || g_swing || g_parry) {
				return false;
			}
			const auto* family = FamilyOf(player);
			const auto* row = family ? ClipFor(family, "Parry") : nullptr;
			if (!row || !*row->clip3p) {
				return false;
			}
			auto clip = GetClip(std::filesystem::path(row->clip3p).filename().string());
			if (!clip || (!g_rest.Loaded() && !g_rest.Load(player))) {
				return false;
			}
			if (Clock::now() < g_parryLockUntil) {
				return true;  // ours, but still recovering from the last parry: the press does nothing
			}
			ActiveParry parry;
			parry.clip = std::move(clip);
			parry.clipName = std::filesystem::path(row->clip3p).filename().string();
			parry.start = Clock::now();
			bool        fromWindow = false;
			const float window = g_stats.Parry("parry_window", &fromWindow);
			const float missRecovery = g_stats.Parry("parry_miss_recovery");
			const float recovery = g_stats.Parry("parry_recovery");
			parry.window = window > 0.0f ? window : parry.window;
			parry.missRecovery = missRecovery > 0.0f ? missRecovery : parry.missRecovery;
			g_parryLockUntil = parry.start + std::chrono::milliseconds(static_cast<int>((recovery > 0.0f ? recovery : 0.65f) * 1000.0f));
			logger::info("parry: started with {}, window {:.3f}s{}", family->id, parry.window, fromWindow ? "" : " (fallback)");
			g_parry = std::move(parry);
			return true;
		}

		void ApplyActiveParry()
		{
			if (!g_parry) {
				return;
			}
			auto&       parry = *g_parry;
			auto*       player = RE::PlayerCharacter::GetSingleton();
			const float t = Seconds(parry.start);
			constexpr float kFade = 0.2f;
			// after stopping a hit the pose lets go quickly (a riposte takes over); otherwise it is held through the miss recovery
			const float end = parry.parriedAt >= 0.0f ? parry.parriedAt + kFade : parry.window + parry.missRecovery;
			if (!player || t >= end) {
				g_parry.reset();
				return;
			}
			if (parry.parriedAt < 0.0f && t >= parry.window && !parry.missLogged) {
				parry.missLogged = true;
				logger::info("parry: window closed at {:.3f}s with nothing stopped", t);
			}
			const float weight = std::clamp(std::min(t / 0.06f, (end - t) / kFade), 0.0f, 1.0f);
			ApplyClip(player, g_rest, *parry.clip, std::min(t, parry.clip->length), weight, g_dev.twist);
		}

		// What the drawn MORDHAU character should show this frame: how the player is moving over the ground, and the
		// attack or parry in progress on top (same clip, same moment).
		CharacterRender::Pose CurrentPose()
		{
			CharacterRender::Pose pose;
			if (auto* player = RE::PlayerCharacter::GetSingleton()) {
				static RE::NiPoint3      lastPos = player->GetPosition();
				static Clock::time_point lastTime = Clock::now();
				static RE::NiPoint3      velocity;  // smoothed, units per second, world
				const float              dt = Seconds(lastTime);
				if (dt > 1e-4f) {
					RE::NiPoint3 step = player->GetPosition() - lastPos;
					step.z = 0.0f;
					if (dt < 0.25f && step.Length() < 200.0f) {  // ignore teleports and pauses
						const float k = std::clamp(dt * 12.0f, 0.0f, 1.0f);
						velocity = velocity * (1.0f - k) + (step / dt) * k;
					} else {
						velocity = {};
					}
					lastPos = player->GetPosition();
					lastTime = Clock::now();
				}
				const float facing = player->GetAngleZ();  // radians, 0 = +Y, clockwise
				const float forward = velocity.x * std::sin(facing) + velocity.y * std::cos(facing);
				const float right = velocity.x * std::cos(facing) - velocity.y * std::sin(facing);
				pose.moveSpeed = velocity.Length() * 1.42875f;  // units -> centimetres
				pose.moveDirection = std::atan2(right, forward) * 57.29578f;
				pose.airborne = player->IsInMidair();
				ApplyMovementRules(pose.moveSpeed, pose.moveDirection);
				LiveTick(player);
				static Clock::time_point lastLog = Clock::now();
				if (g_dev.autotest && (pose.moveSpeed > 15.0f || pose.airborne) && Seconds(lastLog) > 0.5f) {
					lastLog = Clock::now();
					logger::info("move: direction {:.0f} deg, speed {:.0f} cm/s ({:.0f} units/s){}", pose.moveDirection, pose.moveSpeed, velocity.Length(), pose.airborne ? ", airborne" : "");
				}
			}
			if (g_swing) {
				pose.action = g_swing->clipName;
				pose.actionTime = g_swing->clipTime;
				pose.actionWeight = g_swing->weight;
			} else if (g_parry) {
				const float t = Seconds(g_parry->start);
				pose.action = g_parry->clipName;
				pose.actionTime = std::min(t, g_parry->clip->length);
				pose.actionWeight = std::clamp(t / 0.06f, 0.0f, 1.0f);
			}
			return pose;
		}
	}
}
