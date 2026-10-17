#include "CombatInternal.h"

namespace dm::Combat
{
	inline namespace internal
	{
		// After the third-person camera update: this frame's animation is on the skeleton, the frame is not drawn yet.
		float CameraValue(std::string_view a_id)
		{
			// (settings menu "Hide your helmet when looking down" off: never far enough down)
			if (a_id == "eye_head_hidden_looking_down" && Settings::Value("hide_head_looking_down", 1.0f) < 0.5f) {
				return 2.0f;
			}
			for (const auto& row : sheets::kCamera) {
				if (a_id == row.id) {
					return row.value;
				}
			}
			return 0.0f;
		}

		// MORDHAU's field of view while MORDHAU is live (both views); Skyrim's own values come back when it is not.
		void ApplyLiveFov()
		{
			auto* camera = RE::PlayerCamera::GetSingleton();
			if (!camera) {
				return;
			}
			static float savedWorld = -1.0f, savedFirst = -1.0f;
			auto&        fov = camera->GetRuntimeData2();
			const bool   on = Link::Current().live && g_dev.character && g_dev.charOffset == 0.0f;
			if (on) {
				if (savedWorld < 0.0f) {
					savedWorld = fov.worldFOV;
					savedFirst = fov.firstPersonFOV;
					logger::info("camera: MORDHAU's camera: field of view {:.0f} (Skyrim's was {:.0f} / {:.0f}), third person at MORDHAU's boom", Link::Current().cameraFov >= 40.0f ? Link::Current().cameraFov : CameraValue("fov"), savedWorld, savedFirst);
				}
				const float live = Link::Current().cameraFov;
				fov.worldFOV = live >= 40.0f ? live : CameraValue("fov");
				fov.firstPersonFOV = fov.worldFOV;
			} else if (savedWorld >= 0.0f) {
				fov.worldFOV = savedWorld;
				fov.firstPersonFOV = savedFirst;
				savedWorld = savedFirst = -1.0f;
				logger::info("camera: Skyrim's own camera settings are back");
			}
		}

		// Third person: the camera sits where MORDHAU's does (sheets/camera.json): on a boom from a point at the chest,
		// back along the view and up in the camera's own up direction. Skyrim's own view direction is kept (the mouse
		// stays as direct as ever). A ray from the pivot through Skyrim's collision world pulls it in front of walls.
		void ApplyLiveCamera(RE::ThirdPersonState* a_state)
		{
			ApplyLiveFov();
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* root = player ? player->Get3D(false) : nullptr;
			if (g_eyeView.load()) {
				// (the eye view places the camera itself, after the body is posed: ApplyEyeCamera. Until then Skyrim's own
				// third-person camera sat behind the right shoulder for the rest of the frame - and Skyrim aims from it:
				// pick-up prompts came up half a metre to the left of the items looked at. It is put at the eyes too.)
				if (player && g_eyeKnown && !g_skyrimOwns.load()) {
					// (and Skyrim aims from where ITS OWN camera work put the camera, before this: its offset from the
					// character's camera node is set so that this lands at the eyes too. Left alone the prompts were off
					// half a metre to the left with the weapon away (its over-the-shoulder offset); with the offset at
					// nothing they came only when looking above an item (its camera node is lower than the eyes). The
					// offset is in the camera's own axes - right, forward, up - and is corrected by what is still missing.)
					const RE::NiPoint3 eye = player->GetPosition() + g_eyeFromFeet;
					RE::NiMatrix3      view;
					a_state->rotation.ToRotation(view);
					// (only across the view - sideways and up: along the view Skyrim's camera stays pulled back behind the
					// head, which aims the same. Brought all the way to the eyes, Skyrim faded the player's own body out)
					RE::NiPoint3 miss = view.Transpose() * (eye - a_state->translation);
					miss.y = 0.0f;
					RE::NiPoint3 offset = a_state->posOffsetExpected + miss;
					offset.y = 0.0f;
					if (offset.Length() > 250.0f) {
						offset = offset * (250.0f / offset.Length());
					}
					a_state->posOffsetExpected = offset;
					a_state->posOffsetActual = offset;
					{
						static Clock::time_point told{};
						if (Seconds(told) > 10.0f) {
							told = Clock::now();
							logger::info("eye view: Skyrim's own camera was {:.1f} units from the eyes; its offset now ({:.0f}, {:.0f}, {:.0f}); weapon {}", miss.Length(), offset.x, offset.y, offset.z,
								player->IsWeaponDrawn() ? "drawn" : "away");
						}
					}
					a_state->translation = eye;
				}
				return;
			}
			if (!root || !Link::Current().live || !g_dev.character || g_dev.charOffset != 0.0f || g_skyrimOwns.load()) {
				return;  // (a Skyrim scene has the player: its own camera too)
			}
			// Skyrim lets the camera swing around the standing character while the weapon is put away. MORDHAU's camera
			// is always behind the character: that swing turns the character instead.
			if (a_state->freeRotation.x != 0.0f) {
				player->SetHeading(player->GetAngleZ() + a_state->freeRotation.x);
				a_state->freeRotation.x = 0.0f;
			}
			constexpr float k = 1.0f / 1.42875f;  // centimetres -> Skyrim units
			RE::NiMatrix3   view;
			a_state->rotation.ToRotation(view);
			const RE::NiPoint3 forward = view * RE::NiPoint3{ 0.0f, 1.0f, 0.0f };
			const RE::NiPoint3 right = view * RE::NiPoint3{ 1.0f, 0.0f, 0.0f };
			const RE::NiPoint3 up = view * RE::NiPoint3{ 0.0f, 0.0f, 1.0f };
			const float        facing = player->GetAngleZ();
			const RE::NiPoint3 pivot = root->world.translate + RE::NiPoint3{ std::sin(facing), std::cos(facing), 0.0f } * (CameraValue("pivot_forward") * k) +
									   RE::NiPoint3{ 0.0f, 0.0f, CameraValue("pivot_up") * k };
			RE::NiPoint3 wanted = pivot - forward * (CameraValue("arm_back") * k) + up * (CameraValue("arm_up") * k) + right * (CameraValue("side") * k);
			// keep it out of walls and roofs
			if (auto* cell = player->GetParentCell()) {
				if (auto* world = cell->GetbhkWorld()) {
					const float     scale = RE::bhkWorld::GetWorldScale();
					RE::bhkPickData pick;
					pick.rayInput.from = RE::hkVector4(pivot.x * scale, pivot.y * scale, pivot.z * scale, 0.0f);
					pick.rayInput.to = RE::hkVector4(wanted.x * scale, wanted.y * scale, wanted.z * scale, 0.0f);
					RE::CFilter filter;
					player->GetCollisionFilterInfo(filter);
					filter.SetCollisionLayer(RE::COL_LAYER::kLOS);
					pick.rayInput.filterInfo = filter;
					{
						RE::BSReadLockGuard lock(world->worldLock);
						world->PickObject(pick);
					}
					if (pick.rayOutput.HasHit()) {
						const float reach = std::clamp(pick.rayOutput.hitFraction - 0.08f, 0.05f, 1.0f);
						wanted = pivot + (wanted - pivot) * reach;
					}
				}
			}
			if (g_dev.birdsEye > 0.0f) {
				// (recordings: the camera stands still, high above the middle of the skirmish, off to one side, looking down)
				RE::NiPoint3 middle = player->GetPosition();
				int          count = 1;
				for (const auto& handle : g_scenePeople) {
					if (auto actor = handle.get(); actor && !actor->IsDead()) {
						middle += actor->GetPosition();
						++count;
					}
				}
				middle = middle / static_cast<float>(count);
				static RE::NiPoint3 held;
				static bool         has = false;
				held = has ? held + (middle - held) * 0.02f : middle;
				has = true;
				RE::NiPoint3 side = g_sceneSide.Length() > 0.5f ? g_sceneSide : RE::NiPoint3{ 1.0f, 0.0f, 0.0f };
				const RE::NiPoint3 at = held + RE::NiPoint3{ 0.0f, 0.0f, g_dev.birdsEye } - side * (g_dev.birdsEye * 0.45f);
				RE::NiPoint3       look = held + RE::NiPoint3{ 0.0f, 0.0f, 60.0f } - at;
				look = look / look.Length();
				RE::NiPoint3 across = look.Cross(RE::NiPoint3{ 0.0f, 0.0f, 1.0f });
				across = across / across.Length();
				const RE::NiPoint3 above = across.Cross(look);
				RE::NiMatrix3      turn;
				turn.entry[0][0] = across.x, turn.entry[1][0] = across.y, turn.entry[2][0] = across.z;
				turn.entry[0][1] = look.x, turn.entry[1][1] = look.y, turn.entry[2][1] = look.z;
				turn.entry[0][2] = above.x, turn.entry[1][2] = above.y, turn.entry[2][2] = above.z;
				a_state->translation = at;
				if (auto* camera = RE::PlayerCamera::GetSingleton(); camera && camera->cameraRoot) {
					auto* node = camera->cameraRoot.get();
					if (node->parent) {
						const auto& parent = node->parent->world;
						node->local.translate = (parent.rotate.Transpose() * (at - parent.translate)) / std::max(parent.scale, 1e-4f);
						node->local.rotate = parent.rotate.Transpose() * turn;
					} else {
						node->local.translate = at;
						node->local.rotate = turn;
					}
					RE::NiUpdateData update;
					node->UpdateDownwardPass(update, 0);
				}
				return;
			}
			// eased a little, so steps and the pull-in at walls do not snap
			static RE::NiPoint3      shown;
			static Clock::time_point last{};
			const float              dt = Seconds(last);
			last = Clock::now();
			if (dt > 0.25f || (shown - wanted).Length() > 300.0f) {
				shown = wanted;
			} else {
				shown = shown + (wanted - shown) * std::clamp(dt * 30.0f, 0.0f, 1.0f);
			}
			if (g_dev.autotest) {
				static Clock::time_point lastNote{};
				if (Seconds(lastNote) > 2.0f) {
					lastNote = Clock::now();
					auto* cam = RE::Main::WorldRootCamera();
					const auto at = cam ? cam->world.translate : RE::NiPoint3{};
					logger::info("camera: state had ({:.0f}, {:.0f}, {:.0f}), set ({:.0f}, {:.0f}, {:.0f}), drawn camera at ({:.0f}, {:.0f}, {:.0f}), player ({:.0f}, {:.0f}, {:.0f}), forward ({:.2f}, {:.2f}, {:.2f})",
						a_state->translation.x, a_state->translation.y, a_state->translation.z, shown.x, shown.y, shown.z, at.x, at.y, at.z, root->world.translate.x, root->world.translate.y,
						root->world.translate.z, forward.x, forward.y, forward.z);
				}
			}
			a_state->translation = shown;
			// (the state has already put this frame's camera in place by now: move the camera's node as well)
			if (auto* camera = RE::PlayerCamera::GetSingleton(); camera && camera->cameraRoot) {
				auto* node = camera->cameraRoot.get();
				if (node->parent) {
					const auto& parent = node->parent->world;
					node->local.translate = (parent.rotate.Transpose() * (shown - parent.translate)) / std::max(parent.scale, 1e-4f);
				} else {
					node->local.translate = shown;
				}
				RE::NiUpdateData update;
				node->UpdateDownwardPass(update, 0);
			}
		}

		// The eye view (sheets/camera.json first_person_body): the camera at the eyes of the player's own Skyrim body,
		// placed AFTER the body has been posed for this frame (a frame late it trailed the head), and the head not drawn.
		void ApplyEyeCamera(RE::ThirdPersonState* a_state)
		{
			static bool shrunk = false;
			auto*       player = RE::PlayerCharacter::GetSingleton();
			auto*       root = player ? player->Get3D(false) : nullptr;
			auto*       head = root ? root->GetObjectByName("NPC Head [Head]") : nullptr;
			if (!head) {
				return;
			}
			// (sheets/camera.json eye_near: with the eyes inside the body, the hands come nearer than Skyrim's camera draws)
			static RE::Setting* nearSetting = RE::INISettingCollection::GetSingleton() ? RE::INISettingCollection::GetSingleton()->GetSetting("fNearDistance:Display") : nullptr;
			static float        nearWas = nearSetting ? nearSetting->data.f : 15.0f;
			g_eyeNearWas.store(nearWas);
			if (!g_eyeView.load()) {
				if (shrunk || g_eyeHeadHidden.load()) {
					shrunk = false;
					g_eyeHeadHidden.store(false);
					head->local.scale = 1.0f;
					if (nearSetting) {
						nearSetting->data.f = nearWas;
					}
				}
				return;
			}
			if (nearSetting) {
				nearSetting->data.f = CameraValue("eye_near");
			}
			if (auto* drawn = RE::Main::WorldRootCamera()) {
				drawn->viewFrustum.fNear = CameraValue("eye_near");
			}
			// (Skyrim lets the camera swing around the standing character while the weapon is put away: here that
			// swing turns the character - the eyes look where the body faces)
			if (a_state->freeRotation.x != 0.0f) {
				player->SetHeading(player->GetAngleZ() + a_state->freeRotation.x);
				a_state->freeRotation.x = 0.0f;
			}
			constexpr float k = 1.0f / 1.42875f;  // centimetres -> Skyrim units
			RE::NiMatrix3   view;
			a_state->rotation.ToRotation(view);
			RE::NiPoint3 ahead = view * RE::NiPoint3{ 0.0f, 1.0f, 0.0f };
			const float  lookingDown = std::clamp(-ahead.z, 0.0f, 1.0f);  // 0 level or up ... 1 straight down
			ahead.z = 0.0f;
			// (looking (nearly) straight down the view has no level direction left: the body's facing is used. With the
			// view's, at exactly straight down "ahead" came out as nothing and the camera sat inside the head - seen in
			// play, with the neck and the inside of the head in front of the eyes)
			if (const float length = ahead.Length(); length > 0.2f) {
				ahead = ahead / length;
			} else {
				const float facing = player->GetAngleZ();
				ahead = RE::NiPoint3{ std::sin(facing), std::cos(facing), 0.0f };
			}
			// (sheets/camera.json eye_head_hidden: 0 = the head is left alone and the camera sits in front of the face)
			const bool         hideHead = CameraValue("eye_head_hidden") > 0.5f;
			const RE::NiPoint3 at = head->world.translate + RE::NiPoint3{ 0.0f, 0.0f, (CameraValue("eye_up") - CameraValue("eye_lower")) * k } + ahead * ((hideHead ? CameraValue("eye_forward") : CameraValue("eye_forward_head_shown") + CameraValue("eye_forward_looking_down") * lookingDown) * k);
			if (g_dev.measure) {
				// (dev, measure=1: the same thing as MORDHAU's script logs for its side - how far a fought character's head
				// is above or below the player's - with MORDHAU's game time on both lines, so the two can be set side by side:
				// tools/heights_compare.py)
				static Clock::time_point paired{};
				if (Seconds(paired) > 0.1f) {
					paired = Clock::now();
					for (const auto& m : g_mirrored) {
						auto  actor = m.handle.get();
						auto* theirs = actor ? actor->GetNodeByName("NPC Head [Head]") : nullptr;
						auto* hips = actor ? actor->GetNodeByName("NPC Pelvis [Pelv]") : nullptr;
						auto* mine = root->GetObjectByName("NPC Pelvis [Pelv]");
						if (theirs && hips && mine && !actor->IsDead()) {
							logger::info("hm gt={:.3f} id={} head={:.1f} hips={:.1f} feet={:.1f} phead={:.1f} scale={:.3f} pscale={:.3f} ownhead={:.1f}", Link::Current().clock, m.id,
								(theirs->world.translate.z - head->world.translate.z) / k, (hips->world.translate.z - mine->world.translate.z) / k, (actor->GetPositionZ() - player->GetPositionZ()) / k,
								(head->world.translate.z - player->GetPositionZ()) / k, actor->GetHeight() / 128.0f, player->GetHeight() / 128.0f, (theirs->world.translate.z - actor->GetPositionZ()) / k);
						}
					}
				}
			}
			{
				// (sheets/camera.json eye_lower: what to set it by - heads above their own feet, the player's and the fought ones')
				static Clock::time_point told{};
				if (g_dev.measure && Seconds(told) > 0.5f) {
					std::string others;
					for (const auto& m : g_mirrored) {
						auto  actor = m.handle.get();
						auto* theirs = actor ? actor->GetNodeByName("NPC Head [Head]") : nullptr;
						if (!theirs && actor) {
							theirs = actor->GetNodeByName("NPC Head");  // (a Falmer's bones have no bracketed tags)
						}
						if (theirs && !actor->IsDead()) {
							RE::NiPoint3 ahead = theirs->world.translate - player->GetPosition();
							ahead.z = 0.0f;
							others += std::format(" {} {:.0f} (scale {:.2f}, feet {:+.0f}, head {:.0f} cm from the player's feet, body {:.0f})", actor->GetDisplayFullName(), (theirs->world.translate.z - actor->GetPositionZ()) / k, actor->GetHeight() / 128.0f,
								(actor->GetPositionZ() - player->GetPositionZ()) / k, ahead.Length() / k, (actor->GetPosition() - player->GetPosition()).Length() / k);
						}
					}
					if (!others.empty()) {
						told = Clock::now();
						logger::info("eye level: heads above their own feet (cm): the player {:.0f} (scale {:.2f}, camera {:.0f});{}", (head->world.translate.z - player->GetPositionZ()) / k, player->GetHeight() / 128.0f,
							(at.z - player->GetPositionZ()) / k, others);
					}
				}
			}
			{
				// (looking into the camera dipping inside the head for a moment in an overhead swing: where the
				// skeleton's eye point is against the head bone and against the camera - ahead / up in the camera's level frame)
				static Clock::time_point eyeTold{};
				if (g_dev.measure && Seconds(eyeTold) > 1.0f) {
					eyeTold = Clock::now();
					auto* eyes = root->GetObjectByName("NPCEyeBone");
					const RE::NiPoint3 e = eyes ? eyes->world.translate - head->world.translate : RE::NiPoint3{};
					const RE::NiPoint3 c = at - head->world.translate;
					logger::info("eye point: {} - against the head bone {:.1f} ahead, {:.1f} up (cm); the camera {:.1f} ahead, {:.1f} up; looking down {:.2f}", eyes ? "found" : "NOT in this skeleton",
						(e.x * ahead.x + e.y * ahead.y) / k, e.z / k, (c.x * ahead.x + c.y * ahead.y) / k, c.z / k, lookingDown);
				}
			}
			a_state->translation = at;
			g_eyeFromFeet = at - player->GetPosition();
			g_eyeKnown = true;
			if (auto* camera = RE::PlayerCamera::GetSingleton(); camera && camera->cameraRoot) {
				auto* node = camera->cameraRoot.get();
				if (node->parent) {
					const auto& parent = node->parent->world;
					node->local.translate = (parent.rotate.Transpose() * (at - parent.translate)) / std::max(parent.scale, 1e-4f);
				} else {
					node->local.translate = at;
				}
				RE::NiUpdateData update;
				node->UpdateDownwardPass(update, 0);
			}
			// the head (and with it hair and helmet, which hang on that bone) is not drawn: the camera is where it is.
			// Also while looking well down (sheets/camera.json eye_head_hidden_looking_down): a helmet's horns came into
			// the view from above - seen in play.
			if (!hideHead && lookingDown <= CameraValue("eye_head_hidden_looking_down")) {
				if (shrunk) {
					shrunk = false;
					g_eyeHeadHidden.store(false);
					head->local.scale = 1.0f;
				}
				return;
			}
			shrunk = true;
			g_eyeHeadHidden.store(true);
			head->local.scale = 0.002f;
			RE::NiUpdateData update;
			head->UpdateDownwardPass(update, 0);
		}

		// The same moment in first person (only the enemies' weapons need it there).
		// First person: Skyrim's camera at MORDHAU's eye height (sheets/camera.json first_person_eye). MORDHAU's body and
		// blade are drawn from MORDHAU's own camera laid onto Skyrim's; with Skyrim's camera 11 cm lower than MORDHAU's
		// eye, the blade MORDHAU judged with was 11 cm higher on the enemy than the blade seen.
		void ApplyLiveEye()
		{
			auto*       camera = RE::PlayerCamera::GetSingleton();
			auto*       player = RE::PlayerCharacter::GetSingleton();
			const auto& link = Link::Current();
			static float eye = 0.0f;  // MORDHAU's eye height, eased (it bobs with the body)
			if (!camera || !camera->cameraRoot || !player || !link.live || !link.firstPerson || g_skyrimOwns.load() || CameraValue("first_person_eye") < 0.5f || link.cameraPos[2] < 120.0f ||
				link.cameraPos[2] > 230.0f) {
				eye = 0.0f;
				return;
			}
			constexpr float kCm = 1.42875f;
			eye = eye <= 0.0f ? link.cameraPos[2] : eye + (link.cameraPos[2] - eye) * 0.15f;
			auto*       node = camera->cameraRoot.get();
			const float raise = std::clamp(eye / kCm - (node->world.translate.z - player->GetPositionZ()), 0.0f, 25.0f);
			if (raise < 0.2f) {
				return;
			}
			static Clock::time_point said{};
			if (Seconds(said) > 60.0f) {
				said = Clock::now();
				logger::info("camera: first person raised {:.1f} units to MORDHAU's eye height ({:.0f} cm above the feet)", raise, eye);
			}
			const RE::NiPoint3 shown = node->world.translate + RE::NiPoint3{ 0.0f, 0.0f, raise };
			if (node->parent) {
				const auto& parent = node->parent->world;
				node->local.translate = (parent.rotate.Transpose() * (shown - parent.translate)) / std::max(parent.scale, 1e-4f);
			} else {
				node->local.translate = shown;
			}
			RE::NiUpdateData update;
			node->UpdateDownwardPass(update, 0);
		}
	}
}
