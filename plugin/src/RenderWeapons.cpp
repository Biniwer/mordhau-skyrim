#include "CharacterRenderInternal.h"

namespace dm::CharacterRender
{
	inline namespace internal
	{
		void PlaceWeaponNode(RE::NiNode* node, const EnemyWeapon& held, bool note)
		{
			// which way the Skyrim model lies on its node: from the node to the middle of the biggest piece under it
			const RE::NiAVObject* model = nullptr;
			for (auto& child : node->GetChildren()) {
				if (child && child->worldBound.radius > 1.0f && (!model || child->worldBound.radius > model->worldBound.radius)) {
					model = child.get();
				}
			}
			if (note) {
				logger::info("enemy weapon: model {} (radius {:.1f})", model ? model->name.c_str() : "none", model ? model->worldBound.radius : 0.0f);
			}
			if (!model) {
				return;
			}
			const RE::NiPoint3 lie = node->world.rotate.Transpose() * (model->worldBound.center - node->world.translate);
			int                axis = 0;
			if (std::abs(lie.y) > std::abs(lie.x) && std::abs(lie.y) >= std::abs(lie.z)) {
				axis = 1;
			} else if (std::abs(lie.z) > std::abs(lie.x) && std::abs(lie.z) > std::abs(lie.y)) {
				axis = 2;
			}
			const float sign = (axis == 0 ? lie.x : axis == 1 ? lie.y : lie.z) < 0.0f ? -1.0f : 1.0f;
			// a proper rotation whose `axis` column is the weapon's direction and whose next column is its thin side
			RE::NiPoint3 a = held.along * sign;
			RE::NiPoint3 t = held.thin - a * held.thin.Dot(a);
			if (t.Length() < 1e-3f) {
				return;
			}
			// (sheets/builds.json skyrim_weapon_half_turn: with the thin side the other way a Skyrim axe had its edge facing
			// backwards - its model has the head on its -x side, and that side goes where MORDHAU's edge leads)
			// (not for a MORDHAU item: MORDHAU's own model is drawn on this node, by the node's turn, and must lie as
			// MORDHAU has it - turned along with the hidden Skyrim model the Falx had its edge the wrong way)
			const bool halfTurn = g_halfTurn.load() && MordhauHeldBy(held.id == 1 ? 0 : held.id).empty();
			t = t / (halfTurn ? -t.Length() : t.Length());
			const RE::NiPoint3 third = a.Cross(t);
			RE::NiPoint3       columns[3];
			columns[axis] = a;
			columns[(axis + 1) % 3] = t;
			columns[(axis + 2) % 3] = third;
			RE::NiMatrix3 wanted;
			for (int r = 0; r < 3; ++r) {
				wanted.entry[r][0] = (&columns[0].x)[r];
				wanted.entry[r][1] = (&columns[1].x)[r];
				wanted.entry[r][2] = (&columns[2].x)[r];
			}
			const auto&        parent = node->parent->world;
			const RE::NiMatrix3 parentInverse = parent.rotate.Transpose();
			node->local.rotate = parentInverse * wanted;
			node->local.translate = (parentInverse * (held.grip - parent.translate)) / std::max(parent.scale, 1e-4f);
			node->local.scale = 1.0f / std::max(parent.scale, 1e-4f);
			RE::NiUpdateData update;
			node->UpdateDownwardPass(update, 0);
		}

		void PlaceEnemyWeaponsNow()
		{
			if (g_playerSkyrim.id != 0 && Link::Current().weaponState == 2) {
				auto* player = RE::PlayerCharacter::GetSingleton();
				auto* root = player ? player->Get3D(g_playerSkyrimFirst) : nullptr;
				auto* object = root ? root->GetObjectByName("WEAPON") : nullptr;
				if (auto* node = object ? object->AsNode() : nullptr; node && node->parent) {
					object->CullGeometry(false);
					PlaceWeaponNode(node, g_playerSkyrim, false);
				}
			}
			for (auto& held : g_enemyWeapons) {
				if (held.id == 0 || !g_hiddenActors.contains(held.id)) {
					continue;
				}
				auto* actor = RE::TESForm::LookupByID<RE::Actor>(held.id);
				auto* root = actor ? actor->Get3D(false) : nullptr;
				auto* object = root ? root->GetObjectByName("WEAPON") : nullptr;
				auto* node = object ? object->AsNode() : nullptr;
				static auto lastNote = std::chrono::steady_clock::now();
				const bool  note = std::chrono::duration<float>(std::chrono::steady_clock::now() - lastNote).count() > 3.0f;
				if (note) {
					lastNote = std::chrono::steady_clock::now();
					logger::info("enemy weapon: {} root {} WEAPON node {} children {} grip ({:.0f}, {:.0f}, {:.0f}) actor at ({:.0f}, {:.0f}, {:.0f})", actor ? actor->GetDisplayFullName() : "?",
						root ? "yes" : "no", node ? "yes" : (object ? "not a node" : "no"), node ? node->GetChildren().size() : 0, held.grip.x, held.grip.y, held.grip.z,
						actor ? actor->GetPosition().x : 0.0f, actor ? actor->GetPosition().y : 0.0f, actor ? actor->GetPosition().z : 0.0f);
				}
				if (!node || !node->parent) {
					continue;
				}
				PlaceWeaponNode(node, held, note);
			}
		}

		// The turn that takes one direction to another, the short way.
		RE::NiMatrix3 TurnFromTo(RE::NiPoint3 a_from, RE::NiPoint3 a_to)
		{
			RE::NiMatrix3 out;
			a_from = a_from / std::max(a_from.Length(), 1e-5f);
			a_to = a_to / std::max(a_to.Length(), 1e-5f);
			RE::NiPoint3 axis = a_from.Cross(a_to);
			const float  sine = axis.Length(), cosine = a_from.Dot(a_to);
			if (sine < 1e-5f) {
				return out;  // (the same way already)
			}
			axis = axis / sine;
			const float t = 1.0f - cosine, x = axis.x, y = axis.y, z = axis.z;
			out.entry[0][0] = t * x * x + cosine, out.entry[0][1] = t * x * y - sine * z, out.entry[0][2] = t * x * z + sine * y;
			out.entry[1][0] = t * x * y + sine * z, out.entry[1][1] = t * y * y + cosine, out.entry[1][2] = t * y * z - sine * x;
			out.entry[2][0] = t * x * z - sine * y, out.entry[2][1] = t * y * z + sine * x, out.entry[2][2] = t * z * z + cosine;
			return out;
		}

		// Both hands of the player's own Skyrim body to where MORDHAU's hands are (sheets/builds.json
		// arms_to_mordhau_hands). The body's bones were turned like MORDHAU's before this; a Skyrim body's arms are not as
		// long and its shoulders not as wide, so its hands ended somewhere else than MORDHAU's - beside the weapon.
		void ArmsToMordhauHands(RE::NiAVObject* a_body, const XMMATRIX& a_toWorld, float a_k)
		{
			static const int   mordhau[2][3] = { { BoneNamed(g_bones, "LeftArm"), BoneNamed(g_bones, "LeftForeArm"), BoneNamed(g_bones, "LeftHand") },
				  { BoneNamed(g_bones, "RightArm"), BoneNamed(g_bones, "RightForeArm"), BoneNamed(g_bones, "RightHand") } };
			static const char* skyrim[2][3] = { { "NPC L UpperArm [LUar]", "NPC L Forearm [LLar]", "NPC L Hand [LHnd]" }, { "NPC R UpperArm [RUar]", "NPC R Forearm [RLar]", "NPC R Hand [RHnd]" } };
			// (the six arm nodes are kept for as long as the body is the same one - told by looking one of them up afresh
			// each frame, so a body that was let go is never used; the rest skeleton's values are the same every frame
			// and worked out once)
			static RE::NiAVObject* keptBody = nullptr;
			static RE::NiAVObject* kept[2][3]{};
			static RE::NiPoint3                  foreAt[2], handAt[2];
			static bool                          restKnown = false;
			RE::NiAVObject*                      node[2][3]{};
			float                                upperLength[2]{}, foreLength[2]{}, mordhauLength[2]{};
			if (auto* first = a_body->GetObjectByName(skyrim[0][0]); keptBody != a_body || kept[0][0] != first) {
				keptBody = a_body;
				for (int side = 0; side < 2; ++side) {
					for (int part = 0; part < 3; ++part) {
						kept[side][part] = side == 0 && part == 0 ? first : a_body->GetObjectByName(skyrim[side][part]);
					}
				}
			}
			if (!restKnown) {
				for (int side = 0; side < 2; ++side) {
					const dm::RestPose::Node* rest[3]{};
					for (int part = 0; part < 3; ++part) {
						rest[part] = g_skyrimRest.Find(skyrim[side][part]);
						if (!rest[part]) {
							return;
						}
					}
					foreAt[side] = ToMatrix(rest[0]->rot).Transpose() * (rest[1]->pos - rest[0]->pos);
					handAt[side] = ToMatrix(rest[1]->rot).Transpose() * (rest[2]->pos - rest[1]->pos);
				}
				restKnown = true;
			}
			const auto         at = [&](int a_bone) { return g_playerComp[static_cast<std::size_t>(a_bone)].r[3]; };
			const auto         world = [&](XMVECTOR a_step) {
				XMFLOAT3 f{};
				XMStoreFloat3(&f, XMVector3TransformNormal(a_step, a_toWorld));
				return RE::NiPoint3{ f.x, f.y, f.z };
			};
			for (int side = 0; side < 2; ++side) {
				for (int part = 0; part < 3; ++part) {
					if (mordhau[side][part] < 0 || static_cast<std::size_t>(mordhau[side][part]) >= g_playerComp.size()) {
						return;
					}
					node[side][part] = kept[side][part];
					if (!node[side][part] || !node[side][part]->parent) {
						return;
					}
				}
				// (the arm's own lengths from the rest skeleton: set outright below, so nothing adds up)
				upperLength[side] = foreAt[side].Length() * node[side][0]->world.scale;
				foreLength[side] = handAt[side].Length() * node[side][1]->world.scale;
				mordhauLength[side] = (XMVectorGetX(XMVector3Length(XMVectorSubtract(at(mordhau[side][1]), at(mordhau[side][0])))) +
										  XMVectorGetX(XMVector3Length(XMVectorSubtract(at(mordhau[side][2]), at(mordhau[side][1]))))) *
									  a_k;
			}
			// how long the Skyrim arms are against MORDHAU's: MORDHAU's reach is brought to this body's size
			const float           ratio = std::clamp((upperLength[0] + foreLength[0] + upperLength[1] + foreLength[1]) / std::max(mordhauLength[0] + mordhauLength[1], 1.0f), 0.6f, 1.4f);
			const XMVECTOR        mordhauMiddle = XMVectorScale(XMVectorAdd(at(mordhau[0][0]), at(mordhau[1][0])), 0.5f);
			const RE::NiPoint3    middle = (node[0][0]->world.translate + node[1][0]->world.translate) * 0.5f;
			const RE::NiTransform handWas[2] = { node[0][2]->world, node[1][2]->world };
			float                 stretched[2]{ 1.0f, 1.0f }, moved[2]{};
			RE::NiUpdateData      update;
			for (int side = 0; side < 2; ++side) {
				if (side == 0 && g_leftCasting.load()) {
					continue;  // (the left hand casting: that arm is Skyrim's own casting animation, sheets/builds.json cast_arm_skyrim)
				}
				auto *             upper = node[side][0], *fore = node[side][1], *hand = node[side][2];
				const RE::NiPoint3 shoulder = upper->world.translate, elbow = fore->world.translate, palm = hand->world.translate;
				RE::NiPoint3       target = middle + world(XMVectorSubtract(at(mordhau[side][2]), mordhauMiddle)) * ratio;
				moved[side] = (target - palm).Length();
				RE::NiPoint3 reach = target - shoulder;
				float        length = reach.Length();
				if (upperLength[side] < 1.0f || foreLength[side] < 1.0f || length < 1.0f) {
					continue;
				}
				const float stretch = std::clamp(length / (upperLength[side] + foreLength[side] - 0.5f), 1.0f, std::max(g_armsStretch.load(), 1.0f));
				stretched[side] = stretch;
				fore->local.translate = foreAt[side] * stretch;
				hand->local.translate = handAt[side] * stretch;
				const float a = upperLength[side] * stretch, b = foreLength[side] * stretch;
				// (no further than the arm goes, no nearer than it folds)
				const float clamped = std::clamp(length, std::abs(a - b) + 0.5f, a + b - 0.5f);
				reach = reach * (clamped / length);
				length = clamped;
				target = shoulder + reach;
				const RE::NiPoint3 line = reach / length;
				// the elbow to the side MORDHAU's upper arm points; failing that, where it is now
				const RE::NiPoint3 mordhauUpper = world(XMVectorSubtract(at(mordhau[side][1]), at(mordhau[side][0])));
				RE::NiPoint3       bend = mordhauUpper - line * mordhauUpper.Dot(line);
				if (bend.Length() < 0.5f) {
					bend = (elbow - shoulder) - line * (elbow - shoulder).Dot(line);
				}
				if (bend.Length() < 0.5f) {
					bend = RE::NiPoint3{ 0.0f, 0.0f, -1.0f } - line * (-line.z);
				}
				bend = bend / std::max(bend.Length(), 1e-4f);
				const float         along = (a * a - b * b + length * length) / (2.0f * length);
				const float         out = std::sqrt(std::max(a * a - along * along, 0.0f));
				const RE::NiPoint3  newElbow = shoulder + line * along + bend * out;
				const RE::NiMatrix3 worlds[3] = { TurnFromTo(elbow - shoulder, newElbow - shoulder) * upper->world.rotate, TurnFromTo(palm - elbow, target - newElbow) * fore->world.rotate,
					hand->world.rotate };
				// (each against the parent it has, as that parent now stands; the hand keeps the turn MORDHAU's pose gave it)
				for (int part = 0; part < 3; ++part) {
					node[side][part]->local.rotate = node[side][part]->parent->world.rotate.Transpose() * worlds[part];
					node[side][part]->UpdateDownwardPass(update, 0);
				}
			}
			// the twist bones along the arms (sleeves, bracers) for the arms as they now are; the fingers, which have no
			// nodes, carried along with their hands
			dm::ApplyTwist(a_body, g_skyrimRest);
			RE::NiAVObject* const hands[2] = { node[0][2], node[1][2] };
			dm::CarryBones(a_body, hands, handWas, 2);
			static int  told = 0;
			static auto last = std::chrono::steady_clock::now();
			if (told < 40 && std::chrono::steady_clock::now() - last > std::chrono::seconds(2)) {
				++told;
				last = std::chrono::steady_clock::now();
				logger::info("arms: hands put where MORDHAU's are - Skyrim arms {:.2f} of MORDHAU's; left hand moved {:.1f} units (arm set out by {:.2f}), right hand {:.1f} ({:.2f}); now {:.1f} units apart, MORDHAU's {:.1f} cm", ratio,
					moved[0], stretched[0], moved[1], stretched[1], (node[0][2]->world.translate - node[1][2]->world.translate).Length(),
					XMVectorGetX(XMVector3Length(XMVectorSubtract(at(mordhau[0][2]), at(mordhau[1][2])))));
			}
		}

		void PlacePlayerWeaponThirdNow()
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* playerCamera = RE::PlayerCamera::GetSingleton();
			if (!g_thirdWeaponMordhau.load() || !g_skyrimBodies.load() || !player || !playerCamera || playerCamera->IsInFirstPerson() || player->IsInRagdollState() || g_skyrimOwns.load() ||
				!g_playerCompValid || g_bones.empty()) {
				return;
			}
			const auto link = Link::Current();
			if (!link.live || link.weaponState != 1) {
				return;
			}
			static const int hand = BoneNamed(g_bones, "RightHand");
			auto*            body = player->Get3D(false);
			auto*            object = body ? body->GetObjectByName("WEAPON") : nullptr;
			auto*            node = object ? object->AsNode() : nullptr;
			auto*            handNode = body ? body->GetObjectByName("NPC R Hand [RHnd]") : nullptr;
			if (hand < 0 || static_cast<std::size_t>(hand) >= g_playerComp.size() || !node || !node->parent || !handNode) {
				return;
			}
			const float k = body->world.scale / 1.42875f;
			XMFLOAT4X4  turn{};
			for (int r = 0; r < 3; ++r) {
				for (int c = 0; c < 3; ++c) {
					turn.m[r][c] = body->world.rotate.entry[c][r];
				}
			}
			turn.m[3][3] = 1.0f;
			const XMMATRIX toWorld = XMMatrixScaling(-k, k, k) * XMLoadFloat4x4(&turn);  // MORDHAU's character space (cm) -> Skyrim's world directions (units)
			const XMMATRIX weapon = XMMatrixRotationQuaternion(XMVectorSet(link.weaponRot[0], link.weaponRot[1], link.weaponRot[2], link.weaponRot[3])) *
									XMMatrixTranslation(link.weaponPos[0], link.weaponPos[1], link.weaponPos[2]);
			XMFLOAT3 origin{}, palm{}, along{}, thin{};
			XMStoreFloat3(&origin, XMVector3TransformCoord(XMVectorZero(), weapon * toWorld));
			XMStoreFloat3(&palm, XMVector3TransformCoord(XMVectorSetW(g_playerComp[static_cast<std::size_t>(hand)].r[3], 1.0f), toWorld));
			XMStoreFloat3(&along, XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), weapon * toWorld)));
			XMStoreFloat3(&thin, XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), weapon * toWorld)));
			// (the Skyrim model's size and how much of it is on the pommel side of its grip point: read before anything is
			// moved, while its place and its bound are both as Skyrim's own update left them)
			const float modelRadius = node->worldBound.radius;
			const float modelHandle = modelRadius - (node->worldBound.center - node->world.translate).Length();
			if (g_armsFollow.load()) {
				ArmsToMordhauHands(body, toWorld, k);
			}
			EnemyWeapon held;
			held.id = 1;
			held.along = RE::NiPoint3{ along.x, along.y, along.z };
			RE::NiPoint3 offset{ origin.x - palm.x, origin.y - palm.y, origin.z - palm.z };
			const float  mordhauAlong = offset.Dot(held.along);  // where MORDHAU's weapon is along its own line, from MORDHAU's hand
			const auto*  inRight = player->GetEquippedObject(false);
			const auto*  handRest = g_skyrimRest.Find("NPC R Hand [RHnd]");
			const auto*  weaponRest = g_skyrimRest.Find("WEAPON");
			if (g_gripInFist.load() && inRight && handRest && weaponRest && !dm::Items::MordhauWeaponOf(inRight)) {
				// (sheets/builds.json skyrim_weapon_grip_in_fist: on MORDHAU's line, moved along it so that the Skyrim
				// model's own grip point is in the fist - MORDHAU's holding point is wherever each MORDHAU model has it)
				const RE::NiPoint3 fist = (handNode->world.rotate * (ToMatrix(handRest->rot).Transpose() * (weaponRest->pos - handRest->pos))) * body->world.scale;
				offset = offset + held.along * (fist.Dot(held.along) - offset.Dot(held.along));
			}
			if (link.altMode && inRight && !dm::Items::MordhauWeaponOf(inRight)) {
				// (sheets/builds.json alt_slide_...: MORDHAU's other grip of this weapon is the hand somewhere else along it)
				offset = offset + held.along * (g_altSlide.load() * k);
			}
			if (inRight && !dm::Items::MordhauWeaponOf(inRight)) {
				// What MORDHAU's own animation moves the weapon by along itself in the other grip (the grip-in-fist rule
				// above took MORDHAU's place along the line out altogether): measured against the usual grip, which is
				// noted per weapon while it is held, and put back on top.
				static std::unordered_map<RE::FormID, float> usual;
				static auto                                  since = std::chrono::steady_clock::now();
				static bool                                  wasAlt = false, told = true;
				if (link.altMode != wasAlt) {
					wasAlt = link.altMode;
					since = std::chrono::steady_clock::now();
					told = false;
				}
				const auto known = usual.find(inRight->GetFormID());
				if (!link.altMode) {
					// (not while the weapon is still on its way back from the other grip)
					if (std::chrono::steady_clock::now() - since > std::chrono::milliseconds(700) || known == usual.end()) {
						usual[inRight->GetFormID()] = mordhauAlong;
					}
				} else if (known != usual.end()) {
					offset = offset + held.along * (mordhauAlong - known->second);
				}
				if (!told && std::chrono::steady_clock::now() - since > std::chrono::seconds(1)) {
					told = true;
					logger::info("weapon: MORDHAU's other grip {} for a second: MORDHAU's weapon is {:.1f} units along its line from the hand, {:.1f} in the usual grip; sheet slide {:.1f} cm", link.altMode ? "on" : "off",
						mordhauAlong, known != usual.end() ? known->second : 0.0f, g_altSlide.load());
				}
			}
			if (g_altOnBlade.load() && inRight && !dm::Items::MordhauWeaponOf(inRight)) {
				// (sheets/builds.json alt_grip_hands_on_blade)
				static std::unordered_map<RE::FormID, RE::NiPoint3> usual;  // the weapon's direction in the hand's own frame, usual grip
				static bool                                         wasAlt = false;
				const RE::NiPoint3                                  inHand = handNode->world.rotate.Transpose() * held.along;
				const auto                                          known = usual.find(inRight->GetFormID());
				const bool                                          turnedRound = link.altMode && known != usual.end() && known->second.Dot(inHand) < 0.0f;
				auto*                                               leftNode = body->GetObjectByName("NPC L Hand [LHnd]");
				if (!link.altMode) {
					usual[inRight->GetFormID()] = inHand;
				} else if (turnedRound && leftNode && modelRadius > 5.0f && modelHandle > 0.0f) {
					const float blade = 2.0f * modelRadius - modelHandle;
					const float toLeft = (leftNode->world.translate - handNode->world.translate).Dot(held.along);
					const float onBlade = std::clamp((blade - toLeft) * 0.5f, blade * 0.1f, blade * 0.9f);  // the right hand, from the grip point up the blade
					offset = offset + held.along * (-onBlade - offset.Dot(held.along));
					if (!wasAlt) {
						logger::info("weapon: MORDHAU's other grip, the weapon turned round in the hand: Skyrim model with {:.1f} units of blade, hands {:.1f} apart along it, right hand {:.1f} up the blade", blade, toLeft, onBlade);
					}
				} else if (link.altMode && !wasAlt) {
					logger::info("weapon: MORDHAU's other grip, the weapon not turned round in the hand ({}): the grip point stays in the fist", known != usual.end() ? "measured" : "usual grip not seen yet");
				}
				wasAlt = link.altMode;
			}
			held.grip = handNode->world.translate + offset;
			held.thin = RE::NiPoint3{ thin.x, thin.y, thin.z };
			PlaceWeaponNode(node, held, false);
			static bool told = false;
			if (!told) {
				told = true;
				logger::info("weapon: third person - the player's weapon is held as MORDHAU holds it ({:.1f} units from the Skyrim hand's own place)", (held.grip - handNode->world.translate).Length());
			}
		}
	}
}
