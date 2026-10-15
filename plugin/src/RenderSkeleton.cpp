#include "CharacterRenderInternal.h"

namespace dm::CharacterRender
{
	inline namespace internal
	{
		std::shared_ptr<FullClip> GetClip(const std::string& a_name)
		{
			if (const auto it = g_clips.find(a_name); it != g_clips.end()) {
				return it->second;
			}
			std::shared_ptr<FullClip> clip;
			std::ifstream             in(g_cache / (a_name + ".dmfull"), std::ios::binary);
			char                      magic[4]{};
			auto                      loaded = std::make_shared<FullClip>();
			in.read(magic, 4);
			in.read(reinterpret_cast<char*>(&loaded->length), 4);
			in.read(reinterpret_cast<char*>(&loaded->frames), 4);
			in.read(reinterpret_cast<char*>(&loaded->bones), 4);
			if (in && std::memcmp(magic, "DMF1", 4) == 0 && loaded->frames > 0 && loaded->frames < 100000 && loaded->bones == static_cast<std::int32_t>(g_bones.size())) {
				loaded->data.resize(static_cast<std::size_t>(loaded->frames) * static_cast<std::size_t>(loaded->bones) * 7);
				in.read(reinterpret_cast<char*>(loaded->data.data()), static_cast<std::streamsize>(loaded->data.size() * 4));
				if (in) {
					clip = std::move(loaded);
				}
			}
			logger::info("character: full clip '{}' {}", a_name, clip ? "loaded" : "not available (rest pose)");
			g_clips.emplace(a_name, clip);
			return clip;
		}

		void RestPose(LocalPose& a_out)
		{
			a_out.rot.resize(g_bones.size());
			a_out.pos.resize(g_bones.size());
			for (std::size_t i = 0; i < g_bones.size(); ++i) {
				a_out.rot[i] = g_bones[i].restRot;
				a_out.pos[i] = g_bones[i].restPos;
			}
		}

		// a_time in seconds; a_loop wraps it.
		void SampleClip(const FullClip& a_clip, float a_time, bool a_loop, LocalPose& a_out)
		{
			const float length = std::max(a_clip.length, 1e-4f);
			float       u = a_time / length;
			u = a_loop ? u - std::floor(u) : std::clamp(u, 0.0f, 1.0f);
			const float       frame = u * static_cast<float>(a_clip.frames - 1);
			const std::size_t f0 = std::min(static_cast<std::size_t>(frame), static_cast<std::size_t>(a_clip.frames - 1));
			const std::size_t f1 = std::min(f0 + 1, static_cast<std::size_t>(a_clip.frames - 1));
			const float       t = frame - static_cast<float>(f0);
			a_out.rot.resize(g_bones.size());
			a_out.pos.resize(g_bones.size());
			for (std::size_t i = 0; i < g_bones.size(); ++i) {
				const float* a = &a_clip.data[(f0 * g_bones.size() + i) * 7];
				const float* b = &a_clip.data[(f1 * g_bones.size() + i) * 7];
				XMStoreFloat4(&a_out.rot[i], XMQuaternionSlerp(XMVectorSet(a[0], a[1], a[2], a[3]), XMVectorSet(b[0], b[1], b[2], b[3]), t));
				XMStoreFloat3(&a_out.pos[i], XMVectorLerp(XMVectorSet(a[4], a[5], a[6], 0.0f), XMVectorSet(b[4], b[5], b[6], 0.0f), t));
			}
		}

		// a_io = a_io * (1 - t) + a_other * t, on the bones a_mask allows (null = all).
		void Blend(LocalPose& a_io, const LocalPose& a_other, float a_t, const std::vector<bool>* a_mask)
		{
			if (a_t <= 0.0f) {
				return;
			}
			for (std::size_t i = 0; i < g_bones.size(); ++i) {
				if (a_mask && !(*a_mask)[i]) {
					continue;
				}
				XMStoreFloat4(&a_io.rot[i], XMQuaternionSlerp(XMLoadFloat4(&a_io.rot[i]), XMLoadFloat4(&a_other.rot[i]), a_t));
				XMStoreFloat3(&a_io.pos[i], XMVectorLerp(XMLoadFloat3(&a_io.pos[i]), XMLoadFloat3(&a_other.pos[i]), a_t));
			}
		}

		float AngleBetween(float a, float b)
		{
			float d = std::fmod(a - b + 540.0f, 360.0f) - 180.0f;
			return std::abs(d);
		}

		// Two nearest samples of one axis row by direction, blended; false when the row is empty.
		bool SampleRow(const std::vector<Sample>& a_samples, float a_axis, float a_direction, float a_phase, LocalPose& a_out, float* a_travel)
		{
			const Sample* first = nullptr;
			const Sample* second = nullptr;
			float         d1 = 1e9f, d2 = 1e9f;
			for (const auto& s : a_samples) {
				if (s.axis != a_axis || !s.clip) {
					continue;
				}
				const float d = AngleBetween(s.direction, a_direction);
				if (d < d1) {
					second = first, d2 = d1;
					first = &s, d1 = d;
				} else if (d < d2 && (!first || s.clip != first->clip || s.direction != first->direction)) {
					second = &s, d2 = d;
				}
			}
			if (!first) {
				return false;
			}
			SampleClip(*first->clip, a_phase * first->clip->length, true, a_out);
			float travel = first->travel;
			if (second && d1 + d2 > 1e-3f && second->clip != first->clip) {
				static LocalPose other;
				SampleClip(*second->clip, a_phase * second->clip->length, true, other);
				const float t = d1 / (d1 + d2);
				Blend(a_out, other, t);
				travel = first->travel * (1.0f - t) + second->travel * t;
			}
			if (a_travel) {
				*a_travel = travel;
			}
			return true;
		}

		void LoadLocomotion()
		{
			g_locomotionLoaded = true;
			std::ifstream in(g_cache / "locomotion.txt");
			std::string   line;
			float         walkSum = 0.0f, runSum = 0.0f;
			int           walkN = 0, runN = 0;
			while (std::getline(in, line)) {
				std::vector<std::string> f;
				std::size_t              start = 0;
				for (std::size_t i = 0; i <= line.size(); ++i) {
					if (i == line.size() || line[i] == ';') {
						f.push_back(line.substr(start, i - start));
						start = i + 1;
					}
				}
				if (f.size() < 6) {
					continue;
				}
				Sample s;
				s.clip = GetClip(f[1]);
				s.direction = std::strtof(f[2].c_str(), nullptr);
				s.axis = std::strtof(f[3].c_str(), nullptr);
				s.travel = std::strtof(f[4].c_str(), nullptr);
				if (!s.clip) {
					continue;
				}
				if (f[0] == "lower") {
					g_lower.push_back(s);
					if (s.direction == 0.0f) {
						(s.axis == 0.0f ? walkSum : runSum) += s.travel;
						++(s.axis == 0.0f ? walkN : runN);
					}
				} else if (f[0] == "upper") {
					g_upper.push_back(s);
				} else if (f[0] == "still") {
					g_still = s.clip;
				} else if (f[0] == "fall") {
					g_fall = s.clip;
				}
			}
			if (walkN) {
				g_walkSpeed = walkSum / static_cast<float>(walkN);
			}
			if (runN) {
				g_runSpeed = runSum / static_cast<float>(runN);
			}
			// the upper layer owns the lower back and everything above it (this mod's choice of split point)
			g_upperBone.assign(g_bones.size(), false);
			for (std::size_t i = 0; i < g_bones.size(); ++i) {
				const int parent = g_bones[i].parent;
				g_upperBone[i] = _stricmp(g_bones[i].name.c_str(), "LowerBack") == 0 || (parent >= 0 && static_cast<std::size_t>(parent) < i && g_upperBone[static_cast<std::size_t>(parent)]);
			}
			logger::info("character: movement: {} leg samples, {} upper-body samples, still {}, fall {}, walk {:.0f} cm/s, run {:.0f} cm/s", g_lower.size(), g_upper.size(),
				g_still ? "yes" : "no", g_fall ? "yes" : "no", g_walkSpeed, g_runSpeed);
		}

		bool LivePose(LocalPose& a_io, float a_dt)
		{
			static LocalPose    target;
			std::vector<float>& raw = g_liveRaw;
			static std::uint32_t      lastSequence = 0;
			static bool               shown = false;
			if (!Link::Poll(g_bones.size(), raw) || raw.size() != g_bones.size() * 7) {
				shown = false;
				return false;
			}
			bool fresh = false;
			if (const auto sequence = Link::Current().sequence; sequence != lastSequence) {
				lastSequence = sequence;
				fresh = true;
				target.rot.resize(g_bones.size());
				target.pos.resize(g_bones.size());
				for (std::size_t i = 0; i < g_bones.size(); ++i) {
					std::memcpy(&target.rot[i], &raw[i * 7], 16);
					std::memcpy(&target.pos[i], &raw[i * 7 + 4], 12);
				}
			}
			if (target.rot.empty()) {
				return false;
			}
			// MORDHAU sends a pose with every frame it draws, more often than Skyrim draws: the newest one is shown as it
			// is. (Easing put the body some 20 ms behind the weapon and the first-person camera, which are not eased: the
			// weapon slid in the hand and everything swung late.)
			static bool wasFresh = false;
			const bool  everyFrame = fresh && wasFresh;
			wasFresh = fresh;
			if (!shown || everyFrame) {
				a_io = target;
				shown = true;
			} else {
				Blend(a_io, target, 1.0f - std::exp(-a_dt * 45.0f));  // (a slow stream: ease between its poses)
			}
			return true;
		}

		void BuildBones(const Pose& a_pose, XMFLOAT4X4* a_out)
		{
			if (!g_locomotionLoaded) {
				LoadLocomotion();
			}
			const auto  now = std::chrono::steady_clock::now();
			const float dt = std::clamp(std::chrono::duration<float>(now - g_lastFrame).count(), 0.0f, 0.1f);
			g_lastFrame = now;
			g_idleTime += dt;

			static LocalPose pose, layer;
			RestPose(pose);
			const bool  moving = a_pose.moveSpeed > 15.0f && !g_lower.empty();
			const float gait = std::clamp((a_pose.moveSpeed - g_walkSpeed) / std::max(g_runSpeed - g_walkSpeed, 1.0f), 0.0f, 1.0f);

			// legs
			if (a_pose.airborne && g_fall) {
				SampleClip(*g_fall, g_idleTime, true, pose);
			} else if (moving) {
				static LocalPose run;
				float            walkTravel = g_walkSpeed, runTravel = g_runSpeed;
				const bool       haveWalk = SampleRow(g_lower, 0.0f, a_pose.moveDirection, g_phase, pose, &walkTravel);
				const bool       haveRun = SampleRow(g_lower, 1.0f, a_pose.moveDirection, g_phase, run, &runTravel);
				float            travel = walkTravel;
				float            length = 1.0f;
				if (haveWalk && haveRun) {
					Blend(pose, run, gait);
					travel = walkTravel * (1.0f - gait) + runTravel * gait;
					length = 1.0f * (1.0f - gait) + 0.7667f * gait;
				} else if (haveRun) {
					pose = run;
					travel = runTravel;
					length = 0.7667f;
				}
				// play at the rate that makes the feet keep up with the ground actually covered
				const float rate = std::clamp(a_pose.moveSpeed / std::max(travel, 1.0f), 0.4f, 2.2f);
				static auto lastLog = std::chrono::steady_clock::now();
				if (g_logMovement && std::chrono::duration<float>(now - lastLog).count() > 0.5f) {
					lastLog = now;
					logger::info("character: legs at {:.0f} cm/s, direction {:.0f}: {:.0f}% walk clips + {:.0f}% run clips, playback rate {:.2f}", a_pose.moveSpeed,
						a_pose.moveDirection, (1.0f - gait) * 100.0f, gait * 100.0f, rate);
				}
				g_phase += dt * rate / length;
				g_phase -= std::floor(g_phase);
			} else if (g_still) {
				SampleClip(*g_still, g_idleTime, true, pose);
			}

			// upper body: the weapon's own carry animation for this speed and direction
			if (!g_upper.empty() && !(a_pose.airborne && g_fall)) {
				// the blend's speed value: 0 standing, 60 at a walk, 90 at a run
				const float axis = !moving ? 0.0f : a_pose.moveSpeed <= g_walkSpeed ? 60.0f * a_pose.moveSpeed / g_walkSpeed : 60.0f + 30.0f * gait;
				const float lowRow = axis < 60.0f ? 0.0f : 60.0f, highRow = axis < 60.0f ? 60.0f : 90.0f;
				const float t = (axis - lowRow) / (highRow - lowRow);
				const float upperPhase = moving ? g_phase : g_idleTime - std::floor(g_idleTime);
				static LocalPose high;
				const bool       haveLow = SampleRow(g_upper, lowRow, a_pose.moveDirection, upperPhase, layer, nullptr);
				const bool       haveHigh = SampleRow(g_upper, highRow, a_pose.moveDirection, upperPhase, high, nullptr);
				if (haveLow && haveHigh) {
					Blend(layer, high, t);
				} else if (haveHigh) {
					layer = high;
				}
				if (haveLow || haveHigh) {
					Blend(pose, layer, 1.0f, &g_upperBone);
				}
			}

			// an attack or parry in progress takes the upper body
			if (!a_pose.action.empty() && a_pose.actionWeight > 0.0f) {
				if (const auto clip = GetClip(a_pose.action)) {
					SampleClip(*clip, a_pose.actionTime, false, layer);
					Blend(pose, layer, std::clamp(a_pose.actionWeight, 0.0f, 1.0f), &g_upperBone);
				}
			}

			static LocalPose livePose;
			if (livePose.rot.size() != g_bones.size()) {
				livePose = pose;
			}
			if (LivePose(livePose, dt)) {
				pose = livePose;
				// MORDHAU's own draw animation over the live pose's upper body (sheets/builds.json mordhau_draw_clips): the
				// arm reaches for the hip or the back and brings the weapon out; backwards, it puts it away. Third person.
				if (const int kind = g_drawKind.load(); kind != 0) {
					const long long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
					const float     t = static_cast<float>(now - g_drawBegan.load()) / 1000.0f;
					auto*           camera = RE::PlayerCamera::GetSingleton();
					const auto      clip = camera && camera->IsInFirstPerson() ? nullptr : GetClip(std::abs(kind) == 2 ? "Draw_Right_Back" : "Draw_Right_LeftHip");
					if (!clip || t < 0.0f || t > clip->length) {
						g_drawKind.store(0);
					} else {
						// (the upper body: everything that hangs from the lower back)
						static std::vector<bool> upper;
						if (upper.size() != g_bones.size()) {
							upper.assign(g_bones.size(), false);
							for (std::size_t i = 0; i < g_bones.size(); ++i) {
								const int parent = g_bones[i].parent;
								upper[i] = g_bones[i].name == "LowerBack" || g_bones[i].name == "Spine" || (parent >= 0 && static_cast<std::size_t>(parent) < i && upper[static_cast<std::size_t>(parent)]);
							}
						}
						static LocalPose layer;
						RestPose(layer);
						SampleClip(*clip, kind > 0 ? t : clip->length - t, false, layer);
						const float weight = std::clamp(std::min(t / 0.12f, (clip->length - t) / 0.18f), 0.0f, 1.0f);
						Blend(pose, layer, weight, &upper);
					}
				}
			} else {
				livePose = pose;
			}

			static std::vector<XMMATRIX> comp;
			comp.resize(g_bones.size());
			for (std::size_t i = 0; i < g_bones.size(); ++i) {
				const auto&    bone = g_bones[i];
				const XMMATRIX local = XMMatrixRotationQuaternion(XMLoadFloat4(&pose.rot[i])) * XMMatrixTranslationFromVector(XMLoadFloat3(&pose.pos[i]));
				comp[i] = bone.parent >= 0 && static_cast<std::size_t>(bone.parent) < i ? local * comp[static_cast<std::size_t>(bone.parent)] : local;
				XMStoreFloat4x4(&a_out[i], XMLoadFloat4x4(&bone.invRest) * comp[i]);
			}
			g_playerComp = comp;
			g_playerCompValid = true;
			if (g_headBone >= 0) {
				if (g_underHead.size() != g_bones.size()) {
					g_underHead.assign(g_bones.size(), false);
					for (std::size_t i = 0; i < g_bones.size(); ++i) {
						const int parent = g_bones[i].parent;
						g_underHead[i] = static_cast<int>(i) == g_headBone || (parent >= 0 && static_cast<std::size_t>(parent) < i && g_underHead[static_cast<std::size_t>(parent)]);
					}
				}
				XMFLOAT4X4 head{};
				XMStoreFloat4x4(&head, comp[static_cast<std::size_t>(g_headBone)]);
				g_headAt = { head.m[3][0], head.m[3][1], head.m[3][2] };
			}
			// the held weapon: one more matrix after the bones (its vertices are in the weapon's own space)
			if (g_bones.size() < kMaxBones) {
				XMMATRIX   weapon = XMMatrixIdentity();
				const auto link = Link::Current();
				if (link.live && link.weaponState >= 1) {
					weapon = XMMatrixRotationQuaternion(XMVectorSet(link.weaponRot[0], link.weaponRot[1], link.weaponRot[2], link.weaponRot[3])) *
							 XMMatrixTranslation(link.weaponPos[0], link.weaponPos[1], link.weaponPos[2]);
				} else if (link.live) {
					weapon = XMMatrixScaling(0.0f, 0.0f, 0.0f);  // put away in MORDHAU: not in the hand
				} else if (g_weaponBone >= 0) {
					weapon = comp[static_cast<std::size_t>(g_weaponBone)];
				}
				XMStoreFloat4x4(&a_out[g_bones.size()], weapon);
			}
		}

		RE::NiMatrix3 MirrorX(RE::NiMatrix3 a_m)
		{
			a_m.entry[0][1] = -a_m.entry[0][1];
			a_m.entry[0][2] = -a_m.entry[0][2];
			a_m.entry[1][0] = -a_m.entry[1][0];
			a_m.entry[2][0] = -a_m.entry[2][0];
			return a_m;
		}

		int BoneNamed(const std::vector<Bone>& a_bones, const char* a_name)
		{
			for (std::size_t i = 0; i < a_bones.size(); ++i) {
				if (_stricmp(a_bones[i].name.c_str(), a_name) == 0) {
					return static_cast<int>(i);
				}
			}
			return -1;
		}

		void PrepareRetarget(const std::vector<Bone>& a_bones, RE::Actor* a_anyHuman)
		{
			g_retarget.tried = true;
			if (!g_skyrimRest.Loaded() && !g_skyrimRest.Load(a_anyHuman)) {
				logger::warn("everyone: Skyrim's rest skeleton could not be read; people who are not fought stay as they are");
				return;
			}
			const std::size_t     n = a_bones.size();
			std::vector<XMMATRIX> rest(n);
			for (std::size_t i = 0; i < n; ++i) {
				const XMMATRIX local = XMMatrixRotationQuaternion(XMLoadFloat4(&a_bones[i].restRot)) * XMMatrixTranslationFromVector(XMLoadFloat3(&a_bones[i].restPos));
				rest[i] = a_bones[i].parent >= 0 && static_cast<std::size_t>(a_bones[i].parent) < i ? local * rest[static_cast<std::size_t>(a_bones[i].parent)] : local;
			}
			const auto restRot = [&](int a_bone) {   // MORDHAU rest rotation, Skyrim-handed, column form
				XMFLOAT4X4 f{};
				XMStoreFloat4x4(&f, rest[static_cast<std::size_t>(a_bone)]);
				RE::NiMatrix3 m;
				for (int r = 0; r < 3; ++r) {
					for (int c = 0; c < 3; ++c) {
						m.entry[r][c] = f.m[c][r];
					}
				}
				return MirrorX(m);
			};
			const auto restPos = [&](int a_bone) {   // MORDHAU rest position, Skyrim-handed (cm)
				XMFLOAT4X4 f{};
				XMStoreFloat4x4(&f, rest[static_cast<std::size_t>(a_bone)]);
				return RE::NiPoint3{ -f.m[3][0], f.m[3][1], f.m[3][2] };
			};
			g_retarget.rowOfBone.assign(n, -1);
			for (const auto& row : sheets::kBones) {
				if (!*row.mordhauBone || std::string_view(row.id) == "weapon") {
					continue;
				}
				const int   bone = BoneNamed(a_bones, row.mordhauBone);
				const auto* node = g_skyrimRest.Find(row.skyrimNode);
				if (bone < 0 || !node) {
					continue;
				}
				RE::NiMatrix3 align;  // turns Skyrim's rest so this bone points the way MORDHAU's does at rest
				const int     dirBone = *row.dirMordhauBone ? BoneNamed(a_bones, row.dirMordhauBone) : -1;
				const auto*   dirNode = *row.dirSkyrimNode ? g_skyrimRest.Find(row.dirSkyrimNode) : nullptr;
				if (dirBone >= 0 && dirNode) {
					RE::NiPoint3 ds = dirNode->pos - node->pos;
					RE::NiPoint3 dm = restPos(dirBone) - restPos(bone);
					if (ds.Length() > 1e-3f && dm.Length() > 1e-3f) {
						const bool pelvis = std::string_view(row.id) == "pelvis";
						if (pelvis) {
							const float turn = std::acos(std::clamp((ds / ds.Length()).Dot(dm / dm.Length()), -1.0f, 1.0f)) * 57.29578f;
							logger::info("everyone: pelvis: Skyrim's rest direction to its spine ({:.1f}, {:.1f}, {:.1f}; {:.1f} units long) is {:.0f} degrees off MORDHAU's ({:.1f}, {:.1f}, {:.1f}): {}",
								ds.x, ds.y, ds.z, ds.Length(), turn, dm.x, dm.y, dm.z, g_pelvisAlign.load() ? "turned to agree" : "left as the rest poses are (sheets/builds.json pelvis_align 0)");
						}
						if (!pelvis || g_pelvisAlign.load()) {
							align = ToMatrix(Between(ds / ds.Length(), dm / dm.Length()));
						}
					}
				}
				RetargetRow out;
				out.bone = bone;
				out.node = row.skyrimNode;
				out.post = (align * ToMatrix(node->rot)).Transpose() * restRot(bone);
				g_retarget.rowOfBone[static_cast<std::size_t>(bone)] = static_cast<int>(g_retarget.rows.size());
				g_retarget.rows.push_back(out);
				if (std::string_view(row.id) == "pelvis") {
					g_retarget.hips = bone;
					g_retarget.hipsNode = row.skyrimNode;
					g_retarget.hipsScale = node->pos.z > 1.0f ? restPos(bone).z / (node->pos.z * 1.42875f) : 1.0f;
				}
			}
			g_retarget.ready = g_retarget.rows.size() >= 12 && g_retarget.hips >= 0;
			logger::info("everyone: {} bones carry Skyrim's own animation onto MORDHAU's skeleton (hip height factor {:.2f}){}", g_retarget.rows.size(), g_retarget.hipsScale,
				g_retarget.ready ? "" : " - NOT enough, people who are not fought stay as they are");
		}

		// A body's bone by the name Skyrim's people have it under - or, on a creature, by that name without its bracketed
		// tag: a Falmer has "NPC L Thigh" where a person has "NPC L Thigh [LThg]", a werewolf "NPC L Clavicle [RClv]".
		RE::NiAVObject* BoneOf(RE::NiAVObject* a_root, const std::string& a_name)
		{
			if (auto* exact = a_root->GetObjectByName(a_name)) {
				return exact;
			}
			const auto bracket = a_name.find(" [");
			if (bracket == std::string::npos) {
				return nullptr;
			}
			const std::string plain = a_name.substr(0, bracket);
			RE::NiAVObject*   found = nullptr;
			RE::BSVisit::TraverseScenegraphObjects(a_root, [&](RE::NiAVObject* a_object) {
				if (a_object && !a_object->name.empty()) {
					const std::string_view its = a_object->name.c_str();
					if (its == plain || (its.size() > plain.size() && its.substr(0, plain.size()) == plain && its.substr(plain.size(), 2) == " [")) {
						found = a_object;
						return RE::BSVisit::BSVisitControl::kStop;
					}
				}
				return RE::BSVisit::BSVisitControl::kContinue;
			});
			return found;
		}

		// Skin matrices for a_bones from the character's current Skyrim pose. False when it cannot be done.
		bool BuildFromSkyrim(RE::Actor* a_actor, RE::NiAVObject* a_root, const std::vector<Bone>& a_bones, XMFLOAT4X4* a_out)
		{
			if (!g_retarget.ready) {
				return false;
			}
			auto& cached = g_figureNodes[a_actor->GetFormID()];
			if (cached.root != a_root || cached.nodes.size() != g_retarget.rows.size()) {
				cached.root = a_root;
				cached.nodes.assign(g_retarget.rows.size(), nullptr);
				for (std::size_t r = 0; r < g_retarget.rows.size(); ++r) {
					cached.nodes[r] = a_root->GetObjectByName(g_retarget.rows[r].node);
				}
			}
			const int hipsRow = g_retarget.rowOfBone[static_cast<std::size_t>(g_retarget.hips)];
			if (hipsRow < 0 || !cached.nodes[static_cast<std::size_t>(hipsRow)]) {
				return false;
			}
			const RE::NiMatrix3   rootInverse = a_root->world.rotate.Transpose();
			const std::size_t     n = a_bones.size();
			static std::vector<XMMATRIX> comp;
					comp.resize(n);
			for (std::size_t i = 0; i < n; ++i) {
				const auto&    bone = a_bones[i];
				const XMMATRIX local = XMMatrixRotationQuaternion(XMLoadFloat4(&bone.restRot)) * XMMatrixTranslationFromVector(XMLoadFloat3(&bone.restPos));
				XMMATRIX       c = bone.parent >= 0 && static_cast<std::size_t>(bone.parent) < i ? local * comp[static_cast<std::size_t>(bone.parent)] : local;
				const int      row = g_retarget.rowOfBone[i];
				if (row >= 0 && cached.nodes[static_cast<std::size_t>(row)]) {
					auto*               node = cached.nodes[static_cast<std::size_t>(row)];
					const RE::NiMatrix3 skyrim = rootInverse * node->world.rotate;
					const RE::NiMatrix3 mordhau = MirrorX(skyrim * g_retarget.rows[static_cast<std::size_t>(row)].post);
					XMFLOAT4X4          f{};
					XMStoreFloat4x4(&f, c);
					for (int r = 0; r < 3; ++r) {
						for (int col = 0; col < 3; ++col) {
							f.m[col][r] = mordhau.entry[r][col];
						}
					}
					if (static_cast<int>(i) == g_retarget.hips) {
						// where the hips are (sitting, crouching, lying): Skyrim's, scaled to MORDHAU's leg length
						const RE::NiPoint3 p = (rootInverse * (node->world.translate - a_root->world.translate)) / std::max(a_root->world.scale, 1e-4f);
						f.m[3][0] = -p.x * 1.42875f * g_retarget.hipsScale;
						f.m[3][1] = p.y * 1.42875f * g_retarget.hipsScale;
						f.m[3][2] = p.z * 1.42875f * g_retarget.hipsScale;
					}
					c = XMLoadFloat4x4(&f);
				}
				comp[i] = c;
				XMStoreFloat4x4(&a_out[i], XMLoadFloat4x4(&bone.invRest) * c);
			}
			return true;
		}

		// The reverse of BuildFromSkyrim: the character's Skyrim skeleton takes a MORDHAU pose (a_comp: each MORDHAU
		// bone in MORDHAU character space). Skyrim then draws its own model in MORDHAU's movement. Called after Skyrim's
		// animation for the frame, before the frame is drawn; Skyrim's animation overwrites it again next frame.
		void PoseSkyrimBody(RE::Actor* a_actor, const std::vector<XMMATRIX>& a_comp)
		{
			auto* root = a_actor ? a_actor->Get3D(false) : nullptr;
			if (!root || !g_retarget.ready || a_comp.size() < g_retarget.rowOfBone.size()) {
				return;
			}
			auto& cached = g_figureNodes[a_actor->GetFormID()];
			if (cached.root != root || cached.nodes.size() != g_retarget.rows.size()) {
				cached.root = root;
				cached.nodes.assign(g_retarget.rows.size(), nullptr);
				for (std::size_t r = 0; r < g_retarget.rows.size(); ++r) {
					cached.nodes[r] = BoneOf(root, g_retarget.rows[r].node);
				}
			}
			// (rows in MORDHAU bone order: parents before children)
			static std::vector<int> order;
			if (order.size() != g_retarget.rows.size()) {
				order.resize(g_retarget.rows.size());
				for (std::size_t i = 0; i < order.size(); ++i) {
					order[i] = static_cast<int>(i);
				}
				std::sort(order.begin(), order.end(), [](int x, int y) { return g_retarget.rows[static_cast<std::size_t>(x)].bone < g_retarget.rows[static_cast<std::size_t>(y)].bone; });
			}
			RE::NiPoint3 spineBase;
			bool         spineSet = false;
			const auto*  pelvisRest = g_skyrimRest.Find("NPC Pelvis [Pelv]");
			const auto*  spineRest = g_skyrimRest.Find("NPC Spine [Spn0]");
			std::vector<std::string> skyrimsOwn;
			const bool               leftArmSkyrims = a_actor->IsPlayerRef() && g_leftCasting.load();
			// (the casting arm: how Skyrim's animation has the upper arm against the whole body, taken before MORDHAU's
			// pose turns the chest - kept against the chest, the arm pointed out sideways with MORDHAU's chest turned)
			RE::NiMatrix3 castArm{};
			bool          castArmKnown = false;
			if (leftArmSkyrims) {
				// (from the bones' own turns as Skyrim's animation set them this frame, up to the body's root: their world
				// places may still be last frame's, where MORDHAU's pose had them)
				if (auto* upper = root->GetObjectByName("NPC L UpperArm [LUar]")) {
					castArm = upper->local.rotate;
					for (auto* up = upper->parent; up && up != root; up = up->parent) {
						castArm = up->local.rotate * castArm;
					}
					castArmKnown = true;
				}
			}
			{
				std::scoped_lock lock(g_ownHipsLock);
				if (const auto it = g_skyrimBones.find(a_actor->GetFormID()); it != g_skyrimBones.end()) {
					skyrimsOwn = it->second;
				}
			}
			for (const int r : order) {
				const auto& row = g_retarget.rows[static_cast<std::size_t>(r)];
				auto*       node = cached.nodes[static_cast<std::size_t>(r)];
				if (!node || !node->parent) {
					continue;
				}
				// (the player casting with the left hand: that arm in Skyrim's own casting animation, sheets/builds.json cast_arm_skyrim)
				if (leftArmSkyrims && LeftArmBone(row.node)) {
					if (castArmKnown && std::string_view(row.node).rfind("NPC L UpperArm", 0) == 0) {
						node->local.rotate = node->parent->world.rotate.Transpose() * (root->world.rotate * castArm);
						RE::NiUpdateData update;
						node->UpdateDownwardPass(update, 0);
					}
					continue;
				}
				// (sheets/creature_fighters.json skyrim_bones: left in Skyrim's own pose)
				if (!skyrimsOwn.empty()) {
					const std::string_view name = row.node;
					const std::string_view plain = name.substr(0, name.find(" ["));
					if (std::find(skyrimsOwn.begin(), skyrimsOwn.end(), plain) != skyrimsOwn.end()) {
						continue;
					}
				}
				if (spineSet && pelvisRest && spineRest && std::string_view(row.node).substr(0, 9) == "NPC Spine" && std::string_view(row.node).substr(0, 10) != "NPC Spine1" && std::string_view(row.node).substr(0, 10) != "NPC Spine2") {
					// (where the pelvis now is, plus the rest skeleton's step from pelvis to spine, turned with the pelvis)
					if (auto* pelvis = BoneOf(root, "NPC Pelvis [Pelv]")) {
						const RE::NiPoint3 step = ToMatrix(pelvisRest->rot).Transpose() * (spineRest->pos - pelvisRest->pos);
						const RE::NiPoint3 want = spineBase + (pelvis->world.rotate * step) * root->world.scale;
						const auto&        up = node->parent->world;
						node->local.translate = (up.rotate.Transpose() * (want - up.translate)) / std::max(up.scale, 1e-4f);
					}
				}
				XMFLOAT4X4 f{};
				XMStoreFloat4x4(&f, a_comp[static_cast<std::size_t>(row.bone)]);
				RE::NiMatrix3 mordhau;
				for (int i = 0; i < 3; ++i) {
					for (int j = 0; j < 3; ++j) {
						mordhau.entry[i][j] = f.m[j][i];
					}
				}
				const RE::NiMatrix3 world = root->world.rotate * (MirrorX(mordhau) * row.post.Transpose());
				const auto&         parent = node->parent->world;
				node->local.rotate = parent.rotate.Transpose() * world;
				bool ownHips = false;
				if (row.bone == g_retarget.hips) {
					std::scoped_lock lock(g_ownHipsLock);
					ownHips = g_ownHips.contains(a_actor->GetFormID());
				}
				if (row.bone == g_retarget.hips && ownHips) {
					// (a creature on a MORDHAU creature's moves - sheets/creature_fighters.json: the hip height above was
					// worked out for a PERSON's hips in MORDHAU; scaled from an Ogre's, a Dwarven Centurion stood in the
					// ground to below its knees. Its hips stay where Skyrim's own animation has them; the turns are MORDHAU's.)
					{
						std::scoped_lock lock(g_bodyLiftLock);
						g_bodyLift[a_actor->GetFormID()] = RE::NiPoint3{};
					}
					spineBase = node->world.translate;
					spineSet = true;
				} else if (row.bone == g_retarget.hips) {
					// (a skeleton with other legs than a person's - a draugr - stands by its OWN hip height: with a
					// person's it stood in the ground to its ankles. Its rest skeleton is read once per race and sex.)
					static std::unordered_map<std::string, float> legFactor;
					float                                         legs = 1.0f;
					if (auto* race = a_actor->GetRace(); race && a_actor->GetActorBase() && pelvisRest && pelvisRest->pos.z > 1.0f) {
						const std::string key = std::format("{:08X}{}", race->GetFormID(), a_actor->GetActorBase()->IsFemale() ? 'f' : 'm');
						auto              it = legFactor.find(key);
						if (it == legFactor.end()) {
							float        factor = 1.0f;
							dm::RestPose own;
							if (own.Load(a_actor)) {
								if (const auto* pelvis = own.Find("NPC Pelvis [Pelv]"); pelvis && pelvis->pos.z > 1.0f) {
									factor = std::clamp(pelvis->pos.z / pelvisRest->pos.z, 0.7f, 1.4f);
								}
							}
							logger::info("everyone: the skeleton of {} has its hips at {:.2f} of a person's height", race->GetFormEditorID() && *race->GetFormEditorID() ? race->GetFormEditorID() : key.c_str(), factor);
							it = legFactor.emplace(key, factor).first;
						}
						legs = it->second;
					}
					const float        k = legs / (1.42875f * std::max(g_retarget.hipsScale, 1e-3f));
					const RE::NiPoint3 local{ -f.m[3][0] * k, f.m[3][1] * k, f.m[3][2] * k };
					const RE::NiPoint3 want = root->world.translate + (root->world.rotate * local) * root->world.scale;
					{
						std::scoped_lock lock(g_bodyLiftLock);
						g_bodyLift[a_actor->GetFormID()] = (root->world.rotate * (local * (1.0f - 1.0f / std::max(legs, 1e-3f)))) * root->world.scale;
					}
					node->local.translate = (parent.rotate.Transpose() * (want - parent.translate)) / std::max(parent.scale, 1e-4f);
					// Skyrim's spine does not hang from the pelvis but beside it, from the node above both: its first node
					// is put at its rest-skeleton distance from the pelvis (left alone, the torso stayed where Skyrim's
					// animation had it and the waist stretched). Both are set outright each frame: nothing adds up.
					spineBase = want;
					spineSet = true;
				}
				RE::NiUpdateData update;
				node->UpdateDownwardPass(update, 0);
			}
			// The twist bones along the arms carry sleeves, bracers and gauntlets: the arm's roll is shared out along
			// them by the routine made for this skeleton earlier (Pose.cpp ApplyTwistBones, sheets/twist_bones.json).
			// (Left on Skyrim's own animation, or simply laid on their parent, the arm wrung into an S above the elbow.)
			dm::ApplyTwist(root, g_skyrimRest);
			RE::NiUpdateData update;
			root->UpdateDownwardPass(update, 0);
			// The shoulder plates of an armour hang from the upper spine, not from the collar bone, and Skyrim's animation
			// moves them along with the shoulder. With the collar bone in MORDHAU's pose they were left behind, hovering
			// over the shoulder: each is put where it sits on the collar bone in the rest skeleton
			// (sheets/builds.json pauldrons_follow).
			if (g_pauldronsFollow.load()) {
				// (First tried: the plate rigidly on the collar bone - it tipped outward with MORDHAU's collar-bone turn.
				// Now it keeps its rest-skeleton attitude on the spine and is only MOVED, by as much as the shoulder
				// joint - the head of the upper arm - is away from its rest place on the spine.)
				static const char* pairs[2][2]{ { "NPC L Pauldron", "NPC L UpperArm [LUar]" }, { "NPC R Pauldron", "NPC R UpperArm [RUar]" } };
				for (const auto& pair : pairs) {
					auto*       plate = root->GetObjectByName(pair[0]);
					auto*       joint = root->GetObjectByName(pair[1]);
					const auto* plateRest = g_skyrimRest.Find(pair[0]);
					const auto* jointRest = g_skyrimRest.Find(pair[1]);
					const auto* upRest = plate && plate->parent ? g_skyrimRest.Find(plate->parent->name.c_str()) : nullptr;
					if (!plate || !joint || !plateRest || !jointRest || !upRest) {
						continue;
					}
					const auto&         up = plate->parent->world;
					const RE::NiMatrix3 upBack = ToMatrix(upRest->rot).Transpose();
					const RE::NiPoint3  jointNow = (up.rotate.Transpose() * (joint->world.translate - up.translate)) / std::max(up.scale, 1e-4f);
					const RE::NiPoint3  moved = jointNow - upBack * (jointRest->pos - upRest->pos);
					const RE::NiPoint3  place = upBack * (plateRest->pos - upRest->pos) + moved;
					{
						static std::chrono::steady_clock::time_point told{};
						if (std::chrono::steady_clock::now() - told > std::chrono::seconds(4)) {
							told = std::chrono::steady_clock::now();
							logger::info("pauldrons: {} of {:08X}: the shoulder joint is {:.1f} units from its rest place on the spine; the plate was {:.1f} units from where it is put", pair[0],
								a_actor->GetFormID(), moved.Length(), plate->local.translate.GetDistance(place));
						}
					}
					plate->local.rotate = upBack * ToMatrix(plateRest->rot);
					plate->local.translate = place;
					plate->UpdateDownwardPass(update, 0);
				}
			}
		}

		// Skyrim's own bodies in MORDHAU's movement: the fought ones and the player (third person), each frame.
		void PoseSkyrimBodiesNow()
		{
			if (g_skyrimBodies.load() && Link::Current().live) {
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (!g_retarget.tried && player && !g_bones.empty()) {
					PrepareRetarget(g_bones, player);
				}
				auto* camera = RE::PlayerCamera::GetSingleton();
				if (player && g_playerCompValid && camera && !camera->IsInFirstPerson() && !player->IsInRagdollState() && !g_skyrimOwns.load()) {
					PoseSkyrimBody(player, g_playerComp);
				}
				const auto                   link = Link::Current();
				static Link::GhostPose       ghost;
				static std::vector<XMMATRIX> comp;
				for (int slot = 0; slot < Link::kGhosts; ++slot) {
					auto* actor = link.ghosts[slot].id ? RE::TESForm::LookupByID<RE::Actor>(link.ghosts[slot].id) : nullptr;
					const std::size_t n = g_bones.size();
					if (!actor || actor->IsDead() || actor->IsInRagdollState() || !Link::GhostPoseAt(slot, ghost) || ghost.bones.size() != n * 7) {
						continue;
					}
					if (actor->AsActorState()->IsBleedingOut() || SkyrimHasBody(actor->GetFormID())) {
						// (yielding, fleeing or on its knees in Skyrim: Skyrim's own body and animation are shown. Its stand-in
						// holds its attacks meanwhile, and MORDHAU's pose on the body showed it standing and trying to fight.)
						if (auto* root = actor->Get3D(false)) {
							if (auto* object = root->GetObjectByName("WEAPON")) {
								object->SetAppCulled(false);
								object->CullGeometry(false);
							}
						}
						continue;
					}
					{
						// Skyrim's own unsheathe is seen first: MORDHAU's pose is not put on the body until Skyrim has the
						// weapon out, and (for at most unsheathe_wait seconds) until the stand-in holds its weapon too - the
						// stand-in is simply handed one, there is no drawing of it to show. (Seen in play: stand-ins had no
						// draw animation in Skyrim.)
						struct Since
						{
							std::uint32_t                         id = 0;
							std::chrono::steady_clock::time_point at{};
						};
						static Since since[Link::kGhosts];
						if (since[slot].id != actor->GetFormID()) {
							since[slot] = { actor->GetFormID(), std::chrono::steady_clock::now() };
						}
						const float waited = std::chrono::duration<float>(std::chrono::steady_clock::now() - since[slot].at).count();
						const auto* held = actor->GetEquippedObject(false);
						const bool  armedInSkyrim = held && held->As<RE::TESObjectWEAP>();
						const bool  drawing = actor->AsActorState()->GetWeaponState() != RE::WEAPON_STATE::kDrawn;
						const bool  standInEmpty = armedInSkyrim && ghost.weaponState == 0 && waited < g_unsheatheWait.load();
						// (an archer keeps Skyrim's own body for as long as it holds the bow: its stand-in only stands there to
						// be hit, and MORDHAU's pose on it showed a parry made with an arrow in the hand)
						const auto* heldWeapon = held ? held->As<RE::TESObjectWEAP>() : nullptr;
						const bool  shoots = heldWeapon && (heldWeapon->IsBow() || heldWeapon->IsCrossbow());
						if (armedInSkyrim && (drawing || standInEmpty || shoots)) {
							if (auto* root = actor->Get3D(false)) {
								if (auto* object = root->GetObjectByName("WEAPON")) {
									object->SetAppCulled(false);
									object->CullGeometry(false);  // (Skyrim's own weapon, coming out of its sheath)
								}
							}
							continue;
						}
					}
					comp.resize(n);
					for (std::size_t i = 0; i < n; ++i) {
						const float*   b = &ghost.bones[i * 7];
						const bool     streamed = b[0] != 0.0f || b[1] != 0.0f || b[2] != 0.0f || b[3] != 0.0f;
						const auto&    bone = g_bones[i];
						const XMVECTOR rot = streamed ? XMVectorSet(b[0], b[1], b[2], b[3]) : XMLoadFloat4(&bone.restRot);
						const XMVECTOR pos = streamed && i <= 2 ? XMVectorSet(b[4], b[5], b[6], 0.0f) : XMLoadFloat3(&bone.restPos);
						const XMMATRIX local = XMMatrixRotationQuaternion(rot) * XMMatrixTranslationFromVector(pos);
						comp[i] = bone.parent >= 0 && static_cast<std::size_t>(bone.parent) < i ? local * comp[static_cast<std::size_t>(bone.parent)] : local;
					}
					PoseSkyrimBody(actor, comp);
					// The Skyrim weapon follows what MORDHAU did with its counterpart: in the hand (Skyrim's own place for it),
					// lying on the ground where the disarmed stand-in's weapon fell, or out of sight (put away, taken).
					if (auto* root = actor->Get3D(false)) {
						auto* object = root->GetObjectByName("WEAPON");
						auto& fallen = g_fallen[slot];
						if (ghost.weaponState != 2 || fallen.id != actor->GetFormID()) {
							if (fallen.id != 0 && fallen.id != actor->GetFormID()) {
								fallen = {};
							} else if (ghost.weaponState != 2) {
								fallen = {};
							}
						}
						if (object) {
							object->SetAppCulled(false);
							// (the Skyrim weapon is seen only while it is in the hand - and never when it is a MORDHAU item,
							// whose own model the renderer draws there)
							object->CullGeometry(ghost.weaponState != 1 || !MordhauHeldBy(actor->GetFormID()).empty());
							if (auto* node = object->AsNode(); node && node->parent && ghost.weaponState == 1 && ghost.hasWeapon && g_standinWeaponMordhau.load()) {
								// (sheets/builds.json standin_weapon_as_mordhau: turned the way the stand-in holds its weapon - riding the
								// Skyrim hand it had its edge facing back at the holder; it stays at the hand's own place)
								XMFLOAT4X4 turn{};
								for (int r = 0; r < 3; ++r) {
									for (int c = 0; c < 3; ++c) {
										turn.m[r][c] = root->world.rotate.entry[c][r];
									}
								}
								turn.m[3][3] = 1.0f;
								const XMMATRIX toWorld = XMMatrixRotationQuaternion(XMVectorSet(ghost.weaponRot[0], ghost.weaponRot[1], ghost.weaponRot[2], ghost.weaponRot[3])) * XMMatrixScaling(-1.0f, 1.0f, 1.0f) * XMLoadFloat4x4(&turn);
								XMFLOAT3       along{}, thin{};
								XMStoreFloat3(&along, XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), toWorld)));
								XMStoreFloat3(&thin, XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), toWorld)));
								EnemyWeapon held;
								held.id = actor->GetFormID();
								held.grip = node->world.translate;
								held.along = RE::NiPoint3{ along.x, along.y, along.z };
								held.thin = RE::NiPoint3{ thin.x, thin.y, thin.z };
								PlaceWeaponNode(node, held, false);
							}
						}
						if (ghost.weaponState == 2) {
							// where MORDHAU's dropped weapon is: kept for the renderer, which draws MORDHAU's model lying there
							const float    size = kCmToUnits * root->world.scale;
							XMFLOAT4X4     turn{};
							for (int r = 0; r < 3; ++r) {
								for (int c = 0; c < 3; ++c) {
									turn.m[r][c] = root->world.rotate.entry[c][r];
								}
							}
							turn.m[3][3] = 1.0f;
							const XMMATRIX toWorld = XMMatrixRotationQuaternion(XMVectorSet(ghost.weaponRot[0], ghost.weaponRot[1], ghost.weaponRot[2], ghost.weaponRot[3])) *
													 XMMatrixTranslation(ghost.weaponPos[0], ghost.weaponPos[1], ghost.weaponPos[2]) * XMMatrixScaling(-size, size, size) * XMLoadFloat4x4(&turn) *
													 XMMatrixTranslation(root->world.translate.x, root->world.translate.y, root->world.translate.z);
							if (fallen.id != actor->GetFormID()) {
								fallen = {};
								fallen.id = actor->GetFormID();
								fallen.since = std::chrono::steady_clock::now();
								if (const auto wanted = g_standinWeapons.find(fallen.id); wanted != g_standinWeapons.end()) {
									fallen.model = wanted->second;
								}
							}
							fallen.seen = true;
							if (!fallen.settled) {
								XMStoreFloat4x4(&fallen.lying, toWorld);
								fallen.settled = std::chrono::duration<float>(std::chrono::steady_clock::now() - fallen.since).count() > 1.5f;
								if (fallen.settled) {
									fallen.lying.m[3][2] = GroundBelow(actor, fallen.lying.m[3][0], fallen.lying.m[3][1], fallen.lying.m[3][2]) + 3.0f;
								}
							}
						}
					}
				}
			}
		}
	}
}
