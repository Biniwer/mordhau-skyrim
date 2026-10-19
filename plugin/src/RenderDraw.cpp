#include "CharacterRenderInternal.h"

namespace dm::CharacterRender
{
	inline namespace internal
	{
		// Dropped or disarmed in MORDHAU: the weapon lies in Skyrim's world. It follows MORDHAU's fall for a moment (relative to the player, as MORDHAU has it), then stays at that spot while the player walks on.
		void PlaceDroppedPlayerWeapon(RE::PlayerCharacter* player, const bool skyrimBody, const RE::NiPoint3& cam, const XMMATRIX& world, std::vector<XMFLOAT4X4>& boneMatrices, XMMATRIX& placement, const Link::State& link, const std::string*& playerDropModel, XMMATRIX& playerDropWorld)
		{
		{
			static bool                                  down = false, settled = false;
			static std::chrono::steady_clock::time_point since{};
			static XMFLOAT4X4                            lying{};  // weapon -> Skyrim world (absolute)
			static std::string                           droppedModel;
			if (link.live && link.weaponState == 2 && g_bones.size() < kMaxBones) {
				if (!down) {
					down = true, settled = false;
					since = std::chrono::steady_clock::now();
				}
				if (!settled) {
					const XMMATRIX falling = XMMatrixRotationQuaternion(XMVectorSet(link.weaponRot[0], link.weaponRot[1], link.weaponRot[2], link.weaponRot[3])) *
											 XMMatrixTranslation(link.weaponPos[0], link.weaponPos[1], link.weaponPos[2]) * world;
					XMStoreFloat4x4(&lying, falling);
					lying.m[3][0] += cam.x, lying.m[3][1] += cam.y, lying.m[3][2] += cam.z;
					settled = std::chrono::duration<float>(std::chrono::steady_clock::now() - since).count() > 1.5f;
					if (settled) {
						lying.m[3][2] = GroundBelow(player, lying.m[3][0], lying.m[3][1], lying.m[3][2]) + 3.0f;  // (MORDHAU's floor is not Skyrim's)
					}
				}
				g_playerRested = settled;
				if (settled && AsSkyrim(0)) {
					// (Skyrim's own object lies there now: where it came to rest is where one reaches for it)
					std::scoped_lock lock(g_droppedLock);
					const auto&      at = g_droppedAsSkyrim[0];
					lying.m[3][0] = at.x, lying.m[3][1] = at.y, lying.m[3][2] = at.z;
				}
				XMFLOAT4X4 seen = lying;
				seen.m[3][0] -= cam.x, seen.m[3][1] -= cam.y, seen.m[3][2] -= cam.z;
				XMVECTOR det{};
				XMStoreFloat4x4(&boneMatrices[g_bones.size()], settled && AsSkyrim(0) ? XMMatrixScaling(0.0f, 0.0f, 0.0f) : XMLoadFloat4x4(&seen) * XMMatrixInverse(&det, placement));
				g_playerWeaponAt = { lying.m[3][0], lying.m[3][1], lying.m[3][2] };
				g_playerWeaponDown = true;
				if (skyrimBody) {
					std::string name = link.weaponClass;
					static std::string model;
					model = name.rfind("BP_", 0) == 0 && name.ends_with("_C") ? "w_" + name.substr(3, name.size() - 5) : "";
					if (!model.empty() && !(settled && AsSkyrim(0))) {
						playerDropModel = &model;
						playerDropWorld = XMLoadFloat4x4(&seen);
					}
				}
				std::string name = link.weaponClass;
				droppedModel = name.rfind("BP_", 0) == 0 && name.ends_with("_C") ? "w_" + name.substr(3, name.size() - 5) : "";
			} else {
				g_playerRested = false;
				if (down && settled && !droppedModel.empty() && !AsSkyrim(0)) {
					// (back in the hand = picked up; anything else - another weapon was handed over - leaves it lying)
					std::string name = link.weaponClass;
					const std::string now = name.rfind("BP_", 0) == 0 && name.ends_with("_C") ? "w_" + name.substr(3, name.size() - 5) : "";
					if (link.live && !(link.weaponState == 1 && now == droppedModel) && std::filesystem::exists(g_cache / "props" / droppedModel / "character.dmchar")) {
						std::scoped_lock lock(g_leftLock);
						g_leftWeapons.push_back({ droppedModel, lying });
					}
				}
				down = false;
				g_playerWeaponDown = false;
			}
		}
		}

		// Where the player's own Skyrim weapon goes: where MORDHAU's character holds its weapon.
		void PlaceSkyrimWeaponInHand(RE::PlayerCharacter* player, const bool skyrimBody, std::vector<XMFLOAT4X4>& boneMatrices, XMMATRIX& placement, const Link::State& link, bool& skyrimWeapon, XMMATRIX& skyWeaponWorld)
		{
		if (link.live && link.weaponState >= 1 && g_skyrimWeaponWanted.load() && !skyrimBody && g_bones.size() < kMaxBones && !player->IsInRagdollState()) {
			// where MORDHAU has its weapon (weapon space: x thin, y edge, z along, origin at the grip -> the view):
			// Skyrim's own mesh is drawn there after the body
			// (the Skyrim model's own origin is its grip; MORDHAU's is not always - its knife's is at mid-length, and the
			// Skyrim dagger was held by the pommel. So the model is slid along the weapon to where MORDHAU's palm is:
			// the middle between the wrist and the root of the middle finger, measured in the weapon's own space.)
			XMMATRIX slide = XMMatrixIdentity();
			{
				static const int hand = BoneNamed(g_bones, "RightHand"), finger = BoneNamed(g_bones, "RightHandMiddle1");
				const XMMATRIX   weaponNow = XMLoadFloat4x4(&boneMatrices[g_bones.size()]);
				XMVECTOR         det{};
				const XMMATRIX   back = XMMatrixInverse(&det, weaponNow);
				if (hand >= 0 && g_playerCompValid && static_cast<std::size_t>(hand) < g_playerComp.size() && std::abs(XMVectorGetX(det)) > 1e-6f) {
					XMVECTOR palm = g_playerComp[static_cast<std::size_t>(hand)].r[3];
					if (finger >= 0 && static_cast<std::size_t>(finger) < g_playerComp.size()) {
						palm = XMVectorScale(XMVectorAdd(palm, g_playerComp[static_cast<std::size_t>(finger)].r[3]), 0.5f);
					}
					const float along = std::clamp(XMVectorGetZ(XMVector3TransformCoord(XMVectorSetW(palm, 1.0f), back)), -30.0f, 30.0f);
					// (measured ONCE per kind of weapon, standing ready in its ordinary grip, and kept: measured every
					// frame it undid MORDHAU's other grips - with the longsword held by the blade the sword was slid
					// back until its hilt sat in the hand again.)
					static std::unordered_map<std::string, float>       kept;
					static std::string                                  holding;
					static std::chrono::steady_clock::time_point        since{};
					if (holding != link.weaponClass) {
						holding = link.weaponClass;
						since = std::chrono::steady_clock::now();
					}
					auto found = kept.find(holding);
					if (found == kept.end() && link.weaponState == 1 && !link.altMode && std::string_view(link.motion).find("Idle") != std::string_view::npos &&
						std::chrono::steady_clock::now() - since > std::chrono::milliseconds(700)) {
						found = kept.emplace(holding, along).first;
						logger::info("skyrim weapon: MORDHAU's palm is {:.1f} cm along {} from its origin in its ordinary grip: the Skyrim model's grip is kept there", along, link.weaponClass);
					}
					// (not measured yet and already in the other grip: no slide at all, rather than one taken from that grip)
					slide = XMMatrixTranslation(0.0f, 0.0f, found != kept.end() ? found->second : link.altMode ? 0.0f : along);
				}
			}
			skyWeaponWorld = slide * XMLoadFloat4x4(&boneMatrices[g_bones.size()]) * placement;
			XMStoreFloat4x4(&boneMatrices[g_bones.size()], XMMatrixScaling(0.0f, 0.0f, 0.0f));
			skyrimWeapon = true;
		}
		}

		// MORDHAU's own model of the weapon in the player's hand, for when Skyrim's is not shown.
		void PickHandWeaponModel(ID3D11Device* device, std::vector<XMFLOAT4X4>& boneMatrices, const Link::State& link, PropModel*& handModel, XMMATRIX& handWeapon, bool& skyrimWeapon)
		{
		if (!skyrimWeapon && link.live && link.weaponState >= 1 && g_bones.size() < kMaxBones) {
			std::string name = link.weaponClass;
			if (name.rfind("BP_", 0) == 0 && name.size() > 5 && name.ends_with("_C")) {
				name = "w_" + name.substr(3, name.size() - 5);
				auto& model = g_propModels[name];
				if (!model.tried) {
					model.tried = true;
					int unusedWeapon = -1, unusedHead = -1;
					model.ready = LoadMesh(device, g_cache / "props" / name, model.look.bones, model.look.sections, &model.look.vb, &model.look.ib, &unusedWeapon, &unusedHead);
					logger::info("props: {} {}", name, model.ready ? "loaded (the player's weapon)" : "not in the cache (the mercenary's own weapon model is shown)");
				}
				if (model.ready && model.look.bones.size() + 1 <= kMaxBones) {
					handModel = &model;
					handWeapon = XMLoadFloat4x4(&boneMatrices[g_bones.size()]);
					XMStoreFloat4x4(&boneMatrices[g_bones.size()], XMMatrixScaling(0.0f, 0.0f, 0.0f));
				}
			}
		}
		}

		// MORDHAU's blood pattern (cache/props/blood_mask), loaded once.
		void LoadBloodPattern(ID3D11Device* device)
		{
		if (!g_bloodSrv) {
			static bool tried = false;
			if (!tried) {
				tried = true;
				auto& model = g_propModels["blood_mask"];
				int   unusedWeapon = -1, unusedHead = -1;
				model.tried = true;
				model.ready = LoadMesh(device, g_cache / "props" / "blood_mask", model.look.bones, model.look.sections, &model.look.vb, &model.look.ib, &unusedWeapon, &unusedHead);
				if (model.ready && !model.look.sections.empty()) {
					g_bloodSrv = model.look.sections[0].srv;
				}
				logger::info("blood: MORDHAU's blood pattern {}", g_bloodSrv ? "loaded" : "NOT available (a plain speckle is used)");
			}
		}
		}

		// Skyrim's own weapon mesh in MORDHAU's hand.
		void DrawSkyrimWeapon(RE::PlayerCharacter* player, RE::NiAVObject* root, ID3D11Device* device, ID3D11DeviceContext* context, const XMMATRIX& viewProj, const Link::State& link, bool& skyrimWeapon, XMMATRIX& skyWeaponWorld, D3D11_MAPPED_SUBRESOURCE& map)
		{
		if (skyrimWeapon && g_skyVs && g_skyVsBlob && !(link.weaponState == 2 && AsSkyrim(0))) {
			// Skyrim's own weapon mesh (the third-person copy: it is there in both views), each of its shapes with
			// its own vertex and index buffers and diffuse texture, taken from Skyrim's weapon node into MORDHAU's
			// weapon space and from there to where MORDHAU has the weapon.
			auto* object = root->GetObjectByName("WEAPON");
			auto* node = object ? object->AsNode() : nullptr;
			const RE::NiAVObject* biggest = nullptr;
			if (node) {
				for (auto& child : node->GetChildren()) {
					if (child && child->worldBound.radius > 1.0f && (!biggest || child->worldBound.radius > biggest->worldBound.radius)) {
						biggest = child.get();
					}
				}
			}
			if (node && biggest) {
				const auto rowsOf = [](const RE::NiTransform& a_t) {
					XMFLOAT4X4 m{};
					for (int r = 0; r < 3; ++r) {
						for (int c = 0; c < 3; ++c) {
							m.m[r][c] = a_t.rotate.entry[c][r] * a_t.scale;
						}
					}
					m.m[3][0] = a_t.translate.x, m.m[3][1] = a_t.translate.y, m.m[3][2] = a_t.translate.z;
					m.m[3][3] = 1.0f;
					return XMLoadFloat4x4(&m);
				};
				// which way the model lies on its node (as in PlaceWeaponNode), and from that: node space (Skyrim units)
				// -> MORDHAU weapon space (cm): along -> z, the next axis -> x (thin); the third chosen so that the
				// whole chain mirrors as MORDHAU's own models do
				const RE::NiPoint3 lie = node->world.rotate.Transpose() * (biggest->worldBound.center - node->world.translate);
				int                axis = 0;
				if (std::abs(lie.y) > std::abs(lie.x) && std::abs(lie.y) >= std::abs(lie.z)) {
					axis = 1;
				} else if (std::abs(lie.z) > std::abs(lie.x) && std::abs(lie.z) > std::abs(lie.y)) {
					axis = 2;
				}
				const float sign = (axis == 0 ? lie.x : axis == 1 ? lie.y : lie.z) < 0.0f ? -1.0f : 1.0f;
				const float cm = 1.42875f;
				XMFLOAT4X4  a{};
				a.m[axis][2] = sign * cm;
				a.m[(axis + 1) % 3][0] = cm;
				a.m[(axis + 2) % 3][1] = sign * cm;   // (x cross y = -z here for sign > 0: a mirror, undone by the placement's own)
				a.m[3][3] = 1.0f;
				XMVECTOR       det{};
				const XMMATRIX nodeInverse = XMMatrixInverse(&det, rowsOf(node->world));
				const XMMATRIX toView = XMLoadFloat4x4(&a) * skyWeaponWorld;
				std::vector<RE::NiAVObject*> pending{ node };
				bool                         drew = false;
				while (!pending.empty()) {
					auto* at = pending.back();
					pending.pop_back();
					if (auto* inner = at->AsNode()) {
						for (auto& child : inner->GetChildren()) {
							if (child) {
								pending.push_back(child.get());
							}
						}
						continue;
					}
					auto* geometry = at->AsGeometry();
					auto* shape = geometry ? netimmerse_cast<RE::BSTriShape*>(geometry) : nullptr;
					if (!shape) {
						continue;
					}
					std::string name = at->name.c_str();
					std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
					if (name.find("blood") != std::string::npos || name.find("scb") != std::string::npos) {
						continue;  // (the blood layer and the scabbard are not the weapon)
					}
					auto& data = geometry->GetGeometryRuntimeData();
					auto* buffers = data.rendererData;
					auto* property = netimmerse_cast<RE::BSLightingShaderProperty*>(data.shaderProperty.get());
					auto* material = property ? static_cast<RE::BSLightingShaderMaterialBase*>(property->material) : nullptr;
					auto* texture = material && material->diffuseTexture && material->diffuseTexture->rendererTexture ? material->diffuseTexture->rendererTexture->resourceView : nullptr;
					if (!buffers || !buffers->vertexBuffer || !buffers->indexBuffer || !texture) {
						continue;
					}
					auto        desc = data.vertexDesc;
					const bool  full = desc.HasFlag(RE::BSGraphics::Vertex::VF_FULLPREC);
					const UINT  uvAt = desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_TEXCOORD0), normalAt = desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_NORMAL);
					const UINT  stride = desc.GetSize();
					if (!desc.HasFlag(RE::BSGraphics::Vertex::VF_UV) || !desc.HasFlag(RE::BSGraphics::Vertex::VF_NORMAL) || stride == 0) {
						continue;
					}
					const std::uint64_t key = (static_cast<std::uint64_t>(full) << 32) | (uvAt << 8) | normalAt;
					auto&               layout = g_skyLayouts[key];
					if (!layout) {
						const D3D11_INPUT_ELEMENT_DESC elements[] = {
							{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT /* (always whole floats in this Skyrim, whatever the flag says) */, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
							{ "TEXCOORD", 0, DXGI_FORMAT_R16G16_FLOAT, 0, uvAt, D3D11_INPUT_PER_VERTEX_DATA, 0 },
							{ "NORMAL", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, normalAt, D3D11_INPUT_PER_VERTEX_DATA, 0 },
						};
						device->CreateInputLayout(elements, 3, g_skyVsBlob->GetBufferPointer(), g_skyVsBlob->GetBufferSize(), &layout);
						logger::info("skyrim weapon: vertex layout {} bytes (full precision {}, uv at {}, normal at {}) {}", stride, full, uvAt, normalAt, layout ? "ready" : "FAILED");
					}
					if (!layout) {
						continue;
					}
					if (SUCCEEDED(context->Map(g_frameCb, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
						auto* fc = static_cast<FrameConstants*>(map.pData);
						XMStoreFloat4x4(&fc->world, rowsOf(at->world) * nodeInverse * toView);
						XMStoreFloat4x4(&fc->viewProj, viewProj);
						g_bloodNow = BloodOf(0) * 1.7f;
						FillLight(fc, SunReaching(player), PersonOf(nullptr));
						context->Unmap(g_frameCb, 0);
					}
					if (SUCCEEDED(context->Map(g_pieceCb, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
						auto* piece = static_cast<float*>(map.pData);
						std::memset(piece, 0, sizeof(float) * 36);
						for (int i = 0; i < 12; ++i) {
							piece[i] = 1.0f;  // (no tinting)
						}
						context->Unmap(g_pieceCb, 0);
					}
					auto* vb = reinterpret_cast<ID3D11Buffer*>(buffers->vertexBuffer);
					auto* srv = reinterpret_cast<ID3D11ShaderResourceView*>(texture);
					ID3D11ShaderResourceView* maps[7]{ srv, srv, srv, srv, srv, srv, srv };
					const UINT zero = 0;
					context->IASetInputLayout(layout);
					context->IASetVertexBuffers(0, 1, &vb, &stride, &zero);
					context->IASetIndexBuffer(reinterpret_cast<ID3D11Buffer*>(buffers->indexBuffer), DXGI_FORMAT_R16_UINT, 0);
					context->VSSetShader(g_skyVs, nullptr, 0);
					context->PSSetShaderResources(0, 7, maps);
					context->DrawIndexed(static_cast<UINT>(shape->GetTrishapeRuntimeData().triangleCount) * 3, 0, 0);
					drew = true;
				}
				// back to our own meshes' layout and shader for what is drawn after this
				context->IASetInputLayout(g_layout);
				context->VSSetShader(g_vs, nullptr, 0);
				static bool told = false;
				if (!told) {
					told = true;
					logger::info("skyrim weapon: the player's Skyrim weapon is drawn from Skyrim's own mesh{}", drew ? "" : " - NO shape of it could be drawn");
				}
			}
		}
		}

		// MORDHAU's shield, where MORDHAU's character holds it.
		void DrawPlayerShield(RE::PlayerCharacter* player, const bool skyrimBody, XMMATRIX& placement, const Link::State& link, const auto& fromLink, const auto& drawProp)
		{
		if (link.live && link.shieldHas >= 1 && !player->IsInRagdollState() && !g_skyrimOwns.load() && !(skyrimBody && g_skyrimShieldLook.load())) {
			// MORDHAU's shield, where MORDHAU's character holds it (both views; Skyrim's own shield model is hidden).
			// Which one: 1 kite, 2 heater, 3 targe, 4 buckler, 5 round (the script reports it)
			static const char* models[]{ "w_KiteShield", "w_KiteShield", "w_HeaterShield", "w_TargeShield", "w_BucklerShield", "w_RoundShield" };
			drawProp(models[std::clamp(link.shieldHas, 1, 5)], fromLink(link.shield) * placement, SunReaching(player));
			static bool told = false;
			if (!told) {
				told = true;
				logger::info("shield: MORDHAU's shield is drawn on the player's arm");
			}
		}
		}

		// MORDHAU's model of the weapon in the player's hand.
		void DrawHandWeaponModel(RE::PlayerCharacter* player, ID3D11DeviceContext* context, const bool skyrimBody, const XMMATRIX& viewProj, XMMATRIX& placement, PropModel*& handModel, XMMATRIX& handWeapon, D3D11_MAPPED_SUBRESOURCE& map, const UINT stride, const UINT offset, const auto& drawSections)
		{
		if (handModel && !skyrimBody) {
			const std::size_t              bones = handModel->look.bones.size();
			static std::vector<XMFLOAT4X4> still(kMaxBones);
			for (std::size_t i = 0; i <= bones; ++i) {
				XMStoreFloat4x4(&still[i], XMMatrixIdentity());
			}
			if (SUCCEEDED(context->Map(g_frameCb, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
				auto* fc = static_cast<FrameConstants*>(map.pData);
				XMStoreFloat4x4(&fc->world, handWeapon * placement);
				XMStoreFloat4x4(&fc->viewProj, viewProj);
				g_bloodNow = BloodOf(0) * 1.7f;
				FillLight(fc, SunReaching(player), PersonOf(nullptr));
				context->Unmap(g_frameCb, 0);
			}
			if (SUCCEEDED(context->Map(g_boneCb, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
				std::memcpy(map.pData, still.data(), sizeof(XMFLOAT4X4) * (bones + 1));
				context->Unmap(g_boneCb, 0);
			}
			context->IASetVertexBuffers(0, 1, &handModel->look.vb, &stride, &offset);
			context->IASetIndexBuffer(handModel->look.ib, DXGI_FORMAT_R32_UINT, 0);
			drawSections(handModel->look.sections);
		}
		}

		// MORDHAU's stand-ins, drawn where their Skyrim characters stand (whose own models are hidden meanwhile), and what lies or flies around them: dropped weapons, cut-off heads, arrows.
		// Who may be drawn as a MORDHAU fighter: every grown humanoid near the player, nearest first.
		void ListNearFigures(RE::PlayerCharacter* player, const Link::State& link, std::vector<Figure>& figures, const int lookCount)
		{
		if (link.live && lookCount > 0 && !g_skyrimBodies.load()) {
			RE::ProcessLists::GetSingleton()->ForEachHighActor([&](RE::Actor* a_actor) {
				if (a_actor && a_actor != player && a_actor->Is3DLoaded() && a_actor->HasKeywordString("ActorTypeNPC") && !a_actor->IsChild()) {
					const float d = a_actor->GetPosition().GetDistance(player->GetPosition());
					if (d < 4500.0f) {
						int slot = -1;
						for (int k = 0; k < Link::kGhosts; ++k) {
							if (link.ghosts[k].id == a_actor->GetFormID()) {
								slot = k;
							}
						}
						figures.push_back({ a_actor, d, slot });
					}
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
			std::sort(figures.begin(), figures.end(), [](const Figure& x, const Figure& y) { return x.distance < y.distance; });
			if (figures.size() > 16) {
				figures.resize(16);
			}
		}
		}

		// Each listed character as its MORDHAU fighter, in the pose of its stand-in inside MORDHAU.
		// The MORDHAU weapon a drawn stand-in holds, in its hand or lying where it fell.
		void DrawStandInWeapon(ID3D11Device* device, ID3D11DeviceContext* context, const RE::NiPoint3& cam, const XMMATRIX& viewProj, D3D11_MAPPED_SUBRESOURCE& map, const UINT stride, const UINT offset, const auto& drawSections, Link::GhostPose& ghost, RE::Actor* actor, const auto& id, const int slot, const bool fought, const XMMATRIX& enemyWorld, bool& mordhauWeapon)
		{
		if (fought) {
			if (const auto wanted = g_standinWeapons.find(id); wanted != g_standinWeapons.end() && !wanted->second.empty()) {
				auto& model = g_propModels[wanted->second];
				if (!model.tried) {
					model.tried = true;
					int unusedWeapon = -1, unusedHead = -1;
					model.ready = LoadMesh(device, g_cache / "props" / wanted->second, model.look.bones, model.look.sections, &model.look.vb, &model.look.ib, &unusedWeapon, &unusedHead);
					logger::info("props: {} {}", wanted->second, model.ready ? "loaded" : "NOT available (that enemy keeps its Skyrim weapon)");
				}
				const std::size_t bones = model.look.bones.size();
				// (with a MORDHAU model the Skyrim weapon stays hidden, also while the stand-in is disarmed)
				mordhauWeapon = model.ready && bones + 1 <= kMaxBones;
				if (mordhauWeapon && ghost.weaponState >= 1) {
					static std::vector<XMFLOAT4X4> still(kMaxBones);
					for (std::size_t i = 0; i <= bones; ++i) {
						XMStoreFloat4x4(&still[i], XMMatrixIdentity());
					}
					XMMATRIX weaponWorld = XMMatrixRotationQuaternion(XMVectorSet(ghost.weaponRot[0], ghost.weaponRot[1], ghost.weaponRot[2], ghost.weaponRot[3])) *
										   XMMatrixTranslation(ghost.weaponPos[0], ghost.weaponPos[1], ghost.weaponPos[2]) * enemyWorld;
					{
						std::scoped_lock lock(g_bodyLiftLock);
						if (const auto lift = g_bodyLift.find(id); lift != g_bodyLift.end() && g_skyrimBodies.load()) {
							weaponWorld = weaponWorld * XMMatrixTranslation(lift->second.x, lift->second.y, lift->second.z);
						}
					}
					bool asSkyrim = false;
					if (ghost.weaponState == 2) {
						// disarmed: it lies where it fell while the character moves on
						auto& fallen = g_fallen[slot];
						fallen.seen = true;
						if (fallen.id != id) {
							fallen = {};
							fallen.seen = true;
							fallen.model = wanted->second;
							fallen.id = id;
							fallen.since = std::chrono::steady_clock::now();
						}
						if (!fallen.settled) {
							XMStoreFloat4x4(&fallen.lying, weaponWorld);
							fallen.lying.m[3][0] += cam.x, fallen.lying.m[3][1] += cam.y, fallen.lying.m[3][2] += cam.z;
							fallen.settled = std::chrono::duration<float>(std::chrono::steady_clock::now() - fallen.since).count() > 1.5f;
							if (fallen.settled) {
								fallen.lying.m[3][2] = GroundBelow(actor, fallen.lying.m[3][0], fallen.lying.m[3][1], fallen.lying.m[3][2]) + 3.0f;
							}
						}
						XMFLOAT4X4 seen = fallen.lying;
						seen.m[3][0] -= cam.x, seen.m[3][1] -= cam.y, seen.m[3][2] -= cam.z;
						weaponWorld = XMLoadFloat4x4(&seen);
						asSkyrim = fallen.settled && AsSkyrim(id);
					}
					if (asSkyrim) {
						weaponWorld = XMMatrixScaling(0.0f, 0.0f, 0.0f);  // (Skyrim's own object lies there)
					}
					if (SUCCEEDED(context->Map(g_frameCb, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
						auto* fc = static_cast<FrameConstants*>(map.pData);
						XMStoreFloat4x4(&fc->world, weaponWorld);
						XMStoreFloat4x4(&fc->viewProj, viewProj);
						g_bloodNow = BloodOf(id) * 1.7f;
						FillLight(fc, SunReaching(actor), PersonOf(nullptr));
						context->Unmap(g_frameCb, 0);
					}
					if (SUCCEEDED(context->Map(g_boneCb, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
						std::memcpy(map.pData, still.data(), sizeof(XMFLOAT4X4) * (bones + 1));
						context->Unmap(g_boneCb, 0);
					}
					context->IASetVertexBuffers(0, 1, &model.look.vb, &stride, &offset);
					context->IASetIndexBuffer(model.look.ib, DXGI_FORMAT_R32_UINT, 0);
					drawSections(model.look.sections);
				}
			}
		}
		}

		void DrawStandIns(RE::PlayerCharacter* player, ID3D11Device* device, ID3D11DeviceContext* context, const RE::NiPoint3& cam, const XMMATRIX& viewProj, D3D11_MAPPED_SUBRESOURCE& map, const UINT stride, const UINT offset, const auto& drawSections, const auto& drawProp, const float edt, std::unordered_set<std::uint32_t>& hiddenNow, Link::GhostPose& ghost, std::vector<XMFLOAT4X4>& enemyMatrices, std::vector<Figure>& figures)
		{
		for (const auto& figure : figures) {
			auto*      actor = figure.actor;
			const auto id = actor->GetFormID();
			const int  slot = figure.slot;
			// which look: always the same one for the same character
			const int lookNumber = LookFor(actor);
			if (lookNumber < 1 || lookNumber > kMaxLooks) {
				continue;
			}
			auto& look = g_looks[lookNumber - 1];
			if (!look.tried) {
				look.tried = true;
				look.ready = LoadMesh(device, g_cache / "enemies" / std::to_string(lookNumber), look.bones, look.sections, &look.vb, &look.ib, &look.weaponBone, &look.headBone);
				logger::info("everyone: look {} {}", lookNumber, look.ready ? "loaded" : "NOT available (those characters stay as they are)");
			}
			auto*             actorRoot = actor->Get3D(false);
			const std::size_t n = look.bones.size();
			if (!look.ready || !actorRoot || n + 1 > kMaxBones) {
				continue;
			}
			if (!g_retarget.tried) {
				PrepareRetarget(look.bones, player);
			}
			// (thrown or knocked down by Skyrim - a shout, a giant's club: Skyrim's own fall is shown, carried onto the
			// MORDHAU body like any other Skyrim animation, instead of the stand-in standing there)
			const bool fought = slot >= 0 && !actor->IsInRagdollState() && !actor->AsActorState()->IsBleedingOut() && !SkyrimHasBody(actor->GetFormID()) && Link::GhostPoseAt(slot, ghost) && ghost.bones.size() == n * 7;
			if (fought) {
			// pose: MORDHAU's rotations on the look's own proportions (only the root bones take MORDHAU's offsets)
			auto& shown = g_shown[slot];
			const bool fresh = shown.id != id || shown.rot.size() != n;
			if (fresh) {
				shown.id = id;
				shown.rot.resize(n);
				shown.pos.resize(n);
			}
			// (only a light smoothing: a heavier one showed an enemy's swing some 40 ms behind where MORDHAU had it,
			// so a parry timed by eye came late and the weapon went through)
			bool changed = false;
			for (std::size_t i = 0; i < n * 7 && !changed; ++i) {
				changed = shown.last.size() != n * 7 || shown.last[i] != ghost.bones[i];
			}
			if (changed) {
				shown.last = ghost.bones;
			}
			const bool everyFrame = changed && shown.wasChanged;
			shown.wasChanged = changed;
			// (a pose that arrives with every frame is shown as it is; a slower one - an idle stand-in among several - is eased)
			const float ease = fresh || everyFrame ? 1.0f : 1.0f - std::exp(-edt * 70.0f);
			static std::vector<XMMATRIX> comp;
			comp.resize(n);
			for (std::size_t i = 0; i < n; ++i) {
				const float* b = &ghost.bones[i * 7];
				const bool   streamed = b[0] != 0.0f || b[1] != 0.0f || b[2] != 0.0f || b[3] != 0.0f;
				const auto&  bone = look.bones[i];
				const XMVECTOR wantRot = streamed ? XMVectorSet(b[0], b[1], b[2], b[3]) : XMLoadFloat4(&bone.restRot);
				const XMVECTOR wantPos = streamed && i <= 2 ? XMVectorSet(b[4], b[5], b[6], 0.0f) : XMLoadFloat3(&bone.restPos);
				if (fresh) {
					XMStoreFloat4(&shown.rot[i], wantRot);
					XMStoreFloat3(&shown.pos[i], wantPos);
				} else {
					XMStoreFloat4(&shown.rot[i], XMQuaternionSlerp(XMLoadFloat4(&shown.rot[i]), wantRot, ease));
					XMStoreFloat3(&shown.pos[i], XMVectorLerp(XMLoadFloat3(&shown.pos[i]), wantPos, ease));
				}
				const XMMATRIX local = XMMatrixRotationQuaternion(XMLoadFloat4(&shown.rot[i])) * XMMatrixTranslationFromVector(XMLoadFloat3(&shown.pos[i]));
				comp[i] = bone.parent >= 0 && static_cast<std::size_t>(bone.parent) < i ? local * comp[static_cast<std::size_t>(bone.parent)] : local;
				XMStoreFloat4x4(&enemyMatrices[i], XMLoadFloat4x4(&bone.invRest) * comp[i]);
			}
			} else if (!BuildFromSkyrim(actor, actorRoot, look.bones, enemyMatrices.data())) {
				continue;  // (neither source of a pose: the Skyrim character stays as it is)
			}
			// no MORDHAU weapon model is drawn: the Skyrim character's own weapon is kept (PlaceEnemyWeapons)
			XMStoreFloat4x4(&enemyMatrices[n], XMMatrixScaling(0.0f, 0.0f, 0.0f));
			// a crushed head: the head bone and everything on it (face, hair, helmet) shrinks to nothing at the neck
			XMVECTOR neckAt = XMVectorZero();
			bool     neckKnown = false;
			XMMATRIX neckFrame = XMMatrixIdentity();  // the head joint as modelled -> where the body has it now (character space)
			if (look.headBone >= 0 && g_headless.contains(id)) {
				static std::vector<char> under;
				under.assign(n, 0);
				XMFLOAT4X4 neck = enemyMatrices[static_cast<std::size_t>(look.headBone)];
				XMFLOAT4X4 rest{};
				XMStoreFloat4x4(&rest, XMMatrixInverse(nullptr, XMLoadFloat4x4(&look.bones[static_cast<std::size_t>(look.headBone)].invRest)));
				const XMVECTOR at = XMVector3TransformCoord(XMVectorSet(rest.m[3][0], rest.m[3][1], rest.m[3][2], 1.0f), XMLoadFloat4x4(&neck));
				for (std::size_t i = 0; i < n; ++i) {
					const int parent = look.bones[i].parent;
					under[i] = static_cast<int>(i) == look.headBone || (parent >= 0 && static_cast<std::size_t>(parent) < i && under[static_cast<std::size_t>(parent)]);
					if (under[i]) {
						XMStoreFloat4x4(&enemyMatrices[i], XMMatrixScaling(0.0f, 0.0f, 0.0f) * XMMatrixTranslationFromVector(at));
					}
				}
				neckAt = at;
				neckKnown = true;
				neckFrame = XMMatrixTranslation(rest.m[3][0], rest.m[3][1], rest.m[3][2]) * XMLoadFloat4x4(&neck);
			}

			XMFLOAT4X4 enemyRot{};
			for (int r = 0; r < 3; ++r) {
				for (int c = 0; c < 3; ++c) {
					enemyRot.m[r][c] = actorRoot->world.rotate.entry[c][r];
				}
			}
			enemyRot.m[3][3] = 1.0f;
			const RE::NiPoint3 enemyAt = actorRoot->world.translate - cam;
			const float        size = kCmToUnits * actorRoot->world.scale;
			const XMMATRIX     enemyWorld = XMMatrixScaling(-size, size, size) * XMLoadFloat4x4(&enemyRot) * XMMatrixTranslation(enemyAt.x, enemyAt.y, enemyAt.z);
			// a head just cut off starts its flight at this body's neck
			if (neckKnown) {
				for (auto& head : g_looseHeads) {
					if (head.id == id && head.look == 0) {
						XMFLOAT3 start{};
						XMStoreFloat3(&start, XMVector3TransformCoord(neckAt, enemyWorld));
						head.look = lookNumber;
						head.at = RE::NiPoint3{ start.x + cam.x, start.y + cam.y, start.z + cam.z };
						head.speed = head.away * 170.0f + RE::NiPoint3{ 0.0f, 0.0f, 260.0f };
						head.ground = GroundBelow(actor, head.at.x + head.away.x * 120.0f, head.at.y + head.away.y * 120.0f, head.at.z) + 9.0f;
						head.size = size;
						head.sun = SunReaching(actor);
						head.person = PersonOf(actor);
					}
				}
			}
			if (fought) {
				auto& held = g_enemyWeapons[slot];
				held.id = ghost.hasWeapon ? id : 0;
				if (ghost.hasWeapon) {
					// MORDHAU's weapon space (x thin, y edge, z along the weapon, origin at the grip) -> Skyrim world
					const XMMATRIX toWorld = XMMatrixRotationQuaternion(XMVectorSet(ghost.weaponRot[0], ghost.weaponRot[1], ghost.weaponRot[2], ghost.weaponRot[3])) *
											 XMMatrixTranslation(ghost.weaponPos[0], ghost.weaponPos[1], ghost.weaponPos[2]) * enemyWorld;
					XMFLOAT3 origin{}, along{}, thin{};
					XMStoreFloat3(&origin, XMVector3TransformCoord(XMVectorZero(), toWorld));
					XMStoreFloat3(&along, XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), toWorld)));
					XMStoreFloat3(&thin, XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), toWorld)));
					held.grip = RE::NiPoint3{ origin.x + cam.x, origin.y + cam.y, origin.z + cam.z };
					held.along = RE::NiPoint3{ along.x, along.y, along.z };
					held.thin = RE::NiPoint3{ thin.x, thin.y, thin.z };
				}
			}
			if (SUCCEEDED(context->Map(g_frameCb, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
				auto* fc = static_cast<FrameConstants*>(map.pData);
				XMStoreFloat4x4(&fc->world, enemyWorld);
				XMStoreFloat4x4(&fc->viewProj, viewProj);
				g_bloodNow = BloodOf(id);
				FillLight(fc, SunReaching(actor), PersonOf(actor));
				context->Unmap(g_frameCb, 0);
			}
			if (SUCCEEDED(context->Map(g_boneCb, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
				std::memcpy(map.pData, enemyMatrices.data(), sizeof(XMFLOAT4X4) * (n + 1));
				context->Unmap(g_boneCb, 0);
			}
			context->IASetVertexBuffers(0, 1, &look.vb, &stride, &offset);
			context->IASetIndexBuffer(look.ib, DXGI_FORMAT_R32_UINT, 0);
			drawSections(look.sections);
			// MORDHAU's own gore piece closes the neck: the cut one, or the jutting bone of a crushed head
			if (neckKnown) {
				drawProp(g_headless[id] == 1 ? "gore_stump" : "gore_neck", neckFrame * enemyWorld, SunReaching(actor));
			}
			// a fought character's weapon: MORDHAU's own model, where MORDHAU's stand-in holds it
			bool mordhauWeapon = false;
			if (fought && slot >= 0 && (ghost.weaponState != 2 || g_fallen[slot].id != id)) {
				g_fallen[slot] = {};
			}
			DrawStandInWeapon(device, context, cam, viewProj, map, stride, offset, drawSections, ghost, actor, id, slot, fought, enemyWorld, mordhauWeapon);
			actorRoot->CullGeometry(true);
			// ...except what it holds: its own Skyrim weapon (moved into the stand-in's hand when it is fought,
			// see PlaceEnemyWeapons) and its shield
			for (const char* keep : { "WEAPON", "SHIELD" }) {
				// (a fought one's Skyrim weapon only when MORDHAU's model is not there to be drawn)
				const bool show = !fought || (keep[0] == 'W' && !mordhauWeapon);
				if (auto* kept = actorRoot->GetObjectByName(keep)) {
					if (show) {
						kept->CullGeometry(false);
					}
					if (keep[0] == 'W') {
						kept->SetAppCulled(!show);  // (the node too: Skyrim shows a bloodied weapon's blood layer by itself)
					}
				}
			}
			hiddenNow.insert(id);
		}
		}

		// Skyrim-bodies mode: what MORDHAU's models still add to Skyrim's own bodies - gore at the neck, MORDHAU weapons in Skyrim hands, dropped weapons.
		void DrawOnSkyrimBodies(RE::PlayerCharacter* player, ID3D11Device* device, const bool skyrimBody, const RE::NiPoint3& cam, const Link::State& link, const std::string* playerDropModel, XMMATRIX& playerDropWorld, const auto& drawProp, Link::GhostPose& ghost, const int lookCount)
		{
		if (g_skyrimBodies.load()) {
			// MORDHAU's gore on Skyrim's own bodies: the Skyrim head (with hair, helmet - everything weighted to the
			// head bone) is folded away into the neck, MORDHAU's neck piece (cut) or stump (crushed) stands on the
			// neck, and a cut-off head flies as MORDHAU's own head model in the character's skin and hair colours.
			// (someone on the list who is alive - a save from before the blow loaded, someone brought back, or a new character
			// that got a dead one's id - gets the head back and is taken off: a head lost once stayed lost through
			// reloads and cell changes, seen in a player's video)
			std::vector<std::uint32_t> alive;
			for (const auto& [goreId, goreKind] : g_headless) {
				auto* victim = RE::TESForm::LookupByID<RE::Actor>(goreId);
				auto* victimRoot = victim ? victim->Get3D(false) : nullptr;
				auto* headNode = victimRoot ? victimRoot->GetObjectByName("NPC Head [Head]") : nullptr;
				auto* chestNode = victimRoot ? victimRoot->GetObjectByName("NPC Spine2 [Spn2]") : nullptr;
				if (victim && !victim->IsDead() && victim->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth) > 0.0f) {
					alive.push_back(goreId);
					if (headNode) {
						headNode->local.scale = 1.0f;
					}
					continue;
				}
				if (!headNode || !chestNode) {
					continue;
				}
				headNode->local.scale = 0.002f;
				headNode->world.scale = 0.002f;
				const RE::NiPoint3 neck = headNode->world.translate;
				RE::NiPoint3       up = neck - chestNode->world.translate;
				up = up.Length() > 1e-3f ? up / up.Length() : RE::NiPoint3{ 0.0f, 0.0f, 1.0f };
				RE::NiPoint3 side = up.Cross(RE::NiPoint3{ 0.0f, 0.0f, 1.0f });
				if (side.Length() < 1e-3f) {
					side = { 1.0f, 0.0f, 0.0f };
				}
				side = side / side.Length();
				const RE::NiPoint3 fore = up.Cross(side);
				const float        k = kCmToUnits * std::max(victimRoot->world.scale, 0.1f);
				XMFLOAT4X4         m{};
				m.m[0][0] = side.x * k, m.m[0][1] = side.y * k, m.m[0][2] = side.z * k;
				m.m[1][0] = fore.x * k, m.m[1][1] = fore.y * k, m.m[1][2] = fore.z * k;
				m.m[2][0] = up.x * k, m.m[2][1] = up.y * k, m.m[2][2] = up.z * k;
				m.m[3][0] = neck.x - cam.x, m.m[3][1] = neck.y - cam.y, m.m[3][2] = neck.z - cam.z;
				m.m[3][3] = 1.0f;
				drawProp(goreKind == 1 ? "gore_stump" : "gore_neck", XMLoadFloat4x4(&m), SunReaching(victim));
				for (auto& head : g_looseHeads) {
					if (head.id == goreId && head.look == 0 && lookCount > 0) {
						const int lookNumber = 1 + static_cast<int>(goreId % static_cast<std::uint32_t>(lookCount));
						auto& headLook = g_looks[lookNumber - 1];
						if (!headLook.tried) {
							headLook.tried = true;
							headLook.ready = LoadMesh(device, g_cache / "enemies" / std::to_string(lookNumber), headLook.bones, headLook.sections, &headLook.vb, &headLook.ib, &headLook.weaponBone, &headLook.headBone);
						}
						if (headLook.ready && lookNumber <= kMaxLooks) {
							head.look = lookNumber;
							head.at = neck;
							head.speed = head.away * 170.0f + RE::NiPoint3{ 0.0f, 0.0f, 260.0f };
							head.ground = GroundBelow(victim, head.at.x + head.away.x * 120.0f, head.at.y + head.away.y * 120.0f, head.at.z) + 9.0f;
							head.size = k;
							head.sun = SunReaching(victim);
							head.person = PersonOf(victim);
						} else {
							head.look = -1;  // (no MORDHAU head model to throw)
						}
					}
				}
			}
			for (const auto id : alive) {
				g_headless.erase(id);
				std::erase_if(g_looseHeads, [id](const auto& a_head) { return a_head.id == id; });
				logger::info("gore: {:08X} is alive again: its head is back", id);
			}
			// A MORDHAU weapon held as a Skyrim item by a Skyrim body: MORDHAU's model, in Skyrim's own weapon
			// hand (the WEAPON node; which way a weapon lies on it is read from the hidden Skyrim model there).
			const auto drawInHand = [&](RE::Actor* a_holder, std::uint32_t a_key, const char* a_node = "WEAPON", bool a_skyrimsTurn = false) -> bool {
				const std::string name = MordhauHeldBy(a_key);
				auto*             holderRoot = a_holder && !name.empty() ? a_holder->Get3D(false) : nullptr;
				auto*             object = holderRoot ? holderRoot->GetObjectByName(a_node) : nullptr;
				auto*             node = object ? object->AsNode() : nullptr;
				if (!node) {
					return false;
				}
				const RE::NiAVObject* model = nullptr;
				for (auto& child : node->GetChildren()) {
					if (child && child->worldBound.radius > 1.0f && (!model || child->worldBound.radius > model->worldBound.radius)) {
						model = child.get();
					}
				}
				if (!model) {
					return false;
				}
				const RE::NiPoint3 lie = node->world.rotate.Transpose() * (model->worldBound.center - node->world.translate);
				int                axis = 0;
				if (std::abs(lie.y) > std::abs(lie.x) && std::abs(lie.y) >= std::abs(lie.z)) {
					axis = 1;
				} else if (std::abs(lie.z) > std::abs(lie.x) && std::abs(lie.z) > std::abs(lie.y)) {
					axis = 2;
				}
				const float sign = (axis == 0 ? lie.x : axis == 1 ? lie.y : lie.z) < 0.0f ? -1.0f : 1.0f;
				const auto  column = [&](int a_j) { return RE::NiPoint3{ node->world.rotate.entry[0][a_j], node->world.rotate.entry[1][a_j], node->world.rotate.entry[2][a_j] }; };
				// (a_skyrimsTurn: the body is in Skyrim's own animation, so the node lies as Skyrim has it - not turned to MORDHAU's
				// weapon by PlaceWeaponNode. A Skyrim model has its edge on the other side of the node than MORDHAU's model
				// (sheets/builds.json skyrim_weapon_half_turn): the half turn the other way round. Seen: a Falx backwards.)
				const RE::NiPoint3 along = column(axis) * sign, thin = column((axis + 1) % 3) * (a_skyrimsTurn && g_halfTurn.load() ? -1.0f : 1.0f);
				const RE::NiPoint3 edge = thin.Cross(along);
				const float        k = kCmToUnits * std::max(holderRoot->world.scale, 0.1f);
				XMFLOAT4X4         m{};
				m.m[0][0] = thin.x * k, m.m[0][1] = thin.y * k, m.m[0][2] = thin.z * k;
				m.m[1][0] = edge.x * k, m.m[1][1] = edge.y * k, m.m[1][2] = edge.z * k;
				m.m[2][0] = along.x * k, m.m[2][1] = along.y * k, m.m[2][2] = along.z * k;
				m.m[3][0] = node->world.translate.x - cam.x, m.m[3][1] = node->world.translate.y - cam.y, m.m[3][2] = node->world.translate.z - cam.z;
				m.m[3][3] = 1.0f;
				drawProp(name, XMLoadFloat4x4(&m), SunReaching(a_holder));
				return true;
			};
			const auto skyrimWeaponState = player->AsActorState()->GetWeaponState();
			const bool skyrimDrawsOrSheathes = skyrimWeaponState == RE::WEAPON_STATE::kWantToDraw || skyrimWeaponState == RE::WEAPON_STATE::kDrawing ||
			                                   skyrimWeaponState == RE::WEAPON_STATE::kWantToSheathe || skyrimWeaponState == RE::WEAPON_STATE::kSheathing;
			if (skyrimBody && skyrimDrawsOrSheathes) {
				// Skyrim's own draw or put-away: MORDHAU's model where Skyrim's own model is at that moment - in the hand
				// while it is there, at the back or hip once Skyrim has moved it. (Seen in play: drawn only in the hand,
				// the weapon vanished halfway through putting it away.)
				if (!drawInHand(player, 0)) {
					for (const char* sheath : kSheathNodes) {
						drawInHand(player, 0, sheath);
					}
				}
			} else if (skyrimBody && link.weaponState == 1) {
				drawInHand(player, 0);
			} else if (skyrimBody && !player->AsActorState()->IsWeaponDrawn()) {
				// put away: MORDHAU's model where Skyrim carries the weapon (back or hip) - Skyrim's own model of a
				// MORDHAU item is a borrowed one (a messer showed as a steel greatsword on the back) and is hidden
				for (const char* sheath : kSheathNodes) {
					drawInHand(player, 0, sheath);
				}
			}
			for (int k = 0; k < Link::kGhosts; ++k) {
				if (link.ghosts[k].id != 0 && Link::GhostPoseAt(k, ghost) && ghost.weaponState == 1) {
					auto* holder = RE::TESForm::LookupByID<RE::Actor>(link.ghosts[k].id);
					drawInHand(holder, link.ghosts[k].id, "WEAPON", holder && (holder->AsActorState()->IsBleedingOut() || SkyrimHasBody(link.ghosts[k].id)));
				}
			}
			for (auto& fallen : g_fallen) {
				if (fallen.id != 0 && !fallen.model.empty() && !(fallen.settled && AsSkyrim(fallen.id))) {
					XMFLOAT4X4 seen = fallen.lying;
					seen.m[3][0] -= cam.x, seen.m[3][1] -= cam.y, seen.m[3][2] -= cam.z;
					drawProp(fallen.model, XMLoadFloat4x4(&seen), 1.0f);
				}
			}
			if (playerDropModel) {
				drawProp(*playerDropModel, playerDropWorld, 1.0f);
			}
		}
		}

		// A weapon on the ground whose owner was not drawn this frame (dead, gone): it stays, as nobody's.
		void DrawOwnerlessWeapons(const RE::NiPoint3& cam, const auto& drawProp)
		{
		{
			std::scoped_lock lock(g_leftLock);
			for (auto& fallen : g_fallen) {
				if (fallen.id != 0 && !fallen.seen && fallen.settled && !fallen.model.empty() && !AsSkyrim(fallen.id)) {
					if (g_leftWeapons.size() >= 24) {
						g_leftWeapons.erase(g_leftWeapons.begin());
					}
					g_leftWeapons.push_back({ fallen.model, fallen.lying });
					fallen = {};
				}
				fallen.seen = false;
			}
			for (const auto& left : g_leftWeapons) {
				XMFLOAT4X4 seen = left.lying;
				seen.m[3][0] -= cam.x, seen.m[3][1] -= cam.y, seen.m[3][2] -= cam.z;
				drawProp(left.model, XMLoadFloat4x4(&seen), 1.0f);
			}
		}
		}

		// Heads that were cut off: thrown in an arc, then lying where they came down.
		void DrawLooseHeads(ID3D11DeviceContext* context, const RE::NiPoint3& cam, const XMMATRIX& viewProj, D3D11_MAPPED_SUBRESOURCE& map, const UINT stride, const UINT offset, const auto& drawSections, const auto& drawProp, const float edt, std::vector<XMFLOAT4X4>& enemyMatrices)
		{
		for (auto& head : g_looseHeads) {
			if (head.look < 1 || head.look > kMaxLooks) {
				continue;
			}
			auto& look = g_looks[head.look - 1];
			const std::size_t n = look.bones.size();
			if (!look.ready || look.headBone < 0 || n + 1 > kMaxBones) {
				continue;
			}
			if (!head.landed) {
				head.speed.z -= 700.0f * edt;
				head.at += head.speed * edt;
				head.spin += 7.0f * edt;
				if (head.at.z <= head.ground && head.speed.z < 0.0f) {
					head.at.z = head.ground;
					head.landed = true;
				}
			}
			// only the head bone and what hangs on it is drawn, as modelled, turned about the head's own middle
			XMFLOAT4X4 rest{};
			XMStoreFloat4x4(&rest, XMMatrixInverse(nullptr, XMLoadFloat4x4(&look.bones[static_cast<std::size_t>(look.headBone)].invRest)));
			static std::vector<char> under;
			under.assign(n, 0);
			for (std::size_t i = 0; i < n; ++i) {
				const int parent = look.bones[i].parent;
				under[i] = static_cast<int>(i) == look.headBone || (parent >= 0 && static_cast<std::size_t>(parent) < i && under[static_cast<std::size_t>(parent)]);
				XMStoreFloat4x4(&enemyMatrices[i], under[i] ? XMMatrixIdentity() : XMMatrixScaling(0.0f, 0.0f, 0.0f) * XMMatrixTranslation(rest.m[3][0], rest.m[3][1], rest.m[3][2] + 4.0f));
			}
			XMStoreFloat4x4(&enemyMatrices[n], XMMatrixScaling(0.0f, 0.0f, 0.0f));
			const XMMATRIX headWorld = XMMatrixTranslation(-rest.m[3][0], -rest.m[3][1], -rest.m[3][2] - 8.0f) * XMMatrixRotationRollPitchYaw(head.spin, head.landed ? 1.2f : head.spin * 0.6f, 0.0f) *
									   XMMatrixScaling(-head.size, head.size, head.size) * XMMatrixTranslation(head.at.x - cam.x, head.at.y - cam.y, head.at.z - cam.z);
			if (SUCCEEDED(context->Map(g_frameCb, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
				auto* fc = static_cast<FrameConstants*>(map.pData);
				XMStoreFloat4x4(&fc->world, headWorld);
				XMStoreFloat4x4(&fc->viewProj, viewProj);
				FillLight(fc, head.sun, head.person);
				context->Unmap(g_frameCb, 0);
			}
			if (SUCCEEDED(context->Map(g_boneCb, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
				std::memcpy(map.pData, enemyMatrices.data(), sizeof(XMFLOAT4X4) * (n + 1));
				context->Unmap(g_boneCb, 0);
			}
			context->IASetVertexBuffers(0, 1, &look.vb, &stride, &offset);
			context->IASetIndexBuffer(look.ib, DXGI_FORMAT_R32_UINT, 0);
			drawSections(look.sections);
			// MORDHAU's neck piece under the head, facing down out of it
			drawProp("gore_neck", XMMatrixRotationX(3.14159265f) * XMMatrixTranslation(rest.m[3][0], rest.m[3][1], rest.m[3][2]) * headWorld, head.sun);
		}
		}

		// Arrows, bolts and thrown weapons, flying or stuck in the world.
		void DrawProps(RE::PlayerCharacter* player, ID3D11Device* device, ID3D11DeviceContext* context, const RE::NiPoint3& cam, const XMMATRIX& viewProj, D3D11_MAPPED_SUBRESOURCE& map, const UINT stride, const UINT offset, const auto& drawSections)
		{
		for (const auto& prop : g_props) {
			auto& model = g_propModels[prop.model];
			if (!model.tried) {
				model.tried = true;
				int unusedWeapon = -1, unusedHead = -1;
				model.ready = LoadMesh(device, g_cache / "props" / prop.model, model.look.bones, model.look.sections, &model.look.vb, &model.look.ib, &unusedWeapon, &unusedHead);
				logger::info("props: {} {} ({} bones)", prop.model, model.ready ? "loaded" : "NOT available", model.look.bones.size());
			}
			const std::size_t n = model.look.bones.size();
			if (!model.ready || n + 1 > kMaxBones) {
				continue;
			}
			static std::vector<XMFLOAT4X4> propMatrices(kMaxBones);
			static std::vector<XMMATRIX>   comp;
			comp.resize(n);
			for (std::size_t i = 0; i < n; ++i) {
				const auto& bone = model.look.bones[i];
				const XMMATRIX local = XMMatrixRotationQuaternion(XMLoadFloat4(&bone.restRot)) * XMMatrixTranslationFromVector(XMLoadFloat3(&bone.restPos));
				comp[i] = bone.parent >= 0 && static_cast<std::size_t>(bone.parent) < i ? local * comp[static_cast<std::size_t>(bone.parent)] : local;
				XMStoreFloat4x4(&propMatrices[i], XMLoadFloat4x4(&bone.invRest) * comp[i]);
			}
			// (a model without bones - a weapon - is rigid: its vertices use the spare matrix, which then leaves them be)
			XMStoreFloat4x4(&propMatrices[n], XMMatrixScaling(0.0f, 0.0f, 0.0f));
			if (prop.model[0] == 'w' && prop.model[1] == '_') {
				// (a weapon is rigid, and drawn as it is in the hand: every matrix leaves its vertices be)
				for (std::size_t i = 0; i <= n; ++i) {
					XMStoreFloat4x4(&propMatrices[i], XMMatrixIdentity());
				}
			}
			const RE::NiPoint3 at = prop.at - cam;
			// (pitch lifts its nose)
			const XMMATRIX propWorld = (prop.lengthUp ? XMMatrixRotationX(g_arrowTurn) : XMMatrixIdentity()) * XMMatrixScaling(-kCmToUnits, kCmToUnits, kCmToUnits) *
									   XMMatrixRotationX(prop.pitch) * XMMatrixRotationZ(-prop.yaw) * XMMatrixTranslation(at.x, at.y, at.z);
			if (SUCCEEDED(context->Map(g_frameCb, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
				auto* fc = static_cast<FrameConstants*>(map.pData);
				XMStoreFloat4x4(&fc->world, propWorld);
				XMStoreFloat4x4(&fc->viewProj, viewProj);
				g_bloodNow = BloodOf(0) * 1.7f;
			FillLight(fc, SunReaching(player), PersonOf(nullptr));
				context->Unmap(g_frameCb, 0);
			}
			if (SUCCEEDED(context->Map(g_boneCb, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
				std::memcpy(map.pData, propMatrices.data(), sizeof(XMFLOAT4X4) * (n + 1));
				context->Unmap(g_boneCb, 0);
			}
			context->IASetVertexBuffers(0, 1, &model.look.vb, &stride, &offset);
			context->IASetIndexBuffer(model.look.ib, DXGI_FORMAT_R32_UINT, 0);
			drawSections(model.look.sections);
		}
		}

		void DrawWorldFigures(RE::PlayerCharacter* player, ID3D11Device* device, ID3D11DeviceContext* context, const bool skyrimBody, const RE::NiPoint3& cam, const XMMATRIX& viewProj, const Link::State& link, const std::string* playerDropModel, XMMATRIX& playerDropWorld, D3D11_MAPPED_SUBRESOURCE& map, const UINT stride, const UINT offset, const auto& drawSections, const auto& drawProp)
		{
		{
			static auto  lastEnemyFrame = std::chrono::steady_clock::now();
			const auto   nowEnemy = std::chrono::steady_clock::now();
			const float  edt = std::clamp(std::chrono::duration<float>(nowEnemy - lastEnemyFrame).count(), 0.0f, 0.1f);
			lastEnemyFrame = nowEnemy;
			std::unordered_set<std::uint32_t> hiddenNow;
			static Link::GhostPose            ghost;
			static std::vector<XMFLOAT4X4>    enemyMatrices(kMaxBones);
			// who: every grown humanoid near the player, nearest first (the fought ones have a stand-in inside MORDHAU)
			std::vector<Figure> figures;
			const int           lookCount = LookCount();
			ListArmourMatches();
			ListNearFigures(player, link, figures, lookCount);
			for (auto& held : g_enemyWeapons) {
				held.id = 0;
			}
			DrawStandIns(player, device, context, cam, viewProj, map, stride, offset, drawSections, drawProp, edt, hiddenNow, ghost, enemyMatrices, figures);
			DrawOnSkyrimBodies(player, device, skyrimBody, cam, link, playerDropModel, playerDropWorld, drawProp, ghost, lookCount);
			// a weapon on the ground whose owner was not drawn this frame (dead, gone): it stays, as nobody's
			DrawOwnerlessWeapons(cam, drawProp);
			// heads that were cut off: thrown in an arc, then lying where they came down
			DrawLooseHeads(context, cam, viewProj, map, stride, offset, drawSections, drawProp, edt, enemyMatrices);
			// props lying, standing or flying in the world
			DrawProps(player, device, context, cam, viewProj, map, stride, offset, drawSections);
			// characters no longer drawn as stand-ins get their own model back
			for (const auto hiddenId : g_hiddenActors) {
				if (!hiddenNow.contains(hiddenId)) {
					if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(hiddenId)) {
						if (auto* actorRoot = actor->Get3D(false)) {
							actorRoot->CullGeometry(false);
						}
					}
				}
			}
			g_hiddenActors = std::move(hiddenNow);
		}
		}


		// The eye view and Skyrim's own first person: Skyrim's first-person arms are hidden and MORDHAU's view of the character is laid onto Skyrim's camera. Returns true when nothing more is to be drawn this frame.
		bool ShowFirstPersonView(RE::PlayerCharacter* player, const bool firstPerson, const RE::NiPoint3& forward, std::vector<XMFLOAT4X4>& boneMatrices, XMMATRIX& placement, const Link::State& link, bool& armsHidden, const auto& hideArms)
		{
		if (firstPerson) {
			auto* arms = player->Get3D(true);
			if (!link.live) {
				if (arms) {
					hideArms(arms, false);
					arms->CullGeometry(false);
				}
				return true;
			}
			if (!link.firstPerson) {
				// MORDHAU has not switched its own view yet (a fraction of a second): nothing rather than Skyrim's arms
				if (arms) {
					arms->CullGeometry(true);
					hideArms(arms, true);
				}
				return true;
			}
			if (arms) {
				arms->CullGeometry(true);
				hideArms(arms, true);
			}
			// everything above the neck folds into the head joint (the camera sits inside the head)
			if (g_headBone >= 0) {
				XMFLOAT4X4 fold{};
				fold.m[3][0] = g_headAt.x;
				fold.m[3][1] = g_headAt.y;
				fold.m[3][2] = g_headAt.z;
				fold.m[3][3] = 1.0f;
				for (std::size_t i = 0; i < g_bones.size(); ++i) {
					if (g_underHead[i]) {
						boneMatrices[i] = fold;
					}
				}
			}
			// character space -> MORDHAU camera space (x forward, y right, z up) -> Skyrim's camera axes
			const XMMATRIX toCamera = XMMatrixTranslation(-link.cameraPos[0], -link.cameraPos[1], -link.cameraPos[2]) *
									  XMMatrixRotationQuaternion(XMQuaternionConjugate(XMVectorSet(link.cameraRot[0], link.cameraRot[1], link.cameraRot[2], link.cameraRot[3])));
			RE::NiPoint3 up{ 0.0f, 0.0f, 1.0f };
			// (right from the player's heading, not from forward x up: looking straight down that product vanishes,
			// and the body jumped a quarter turn to the left of the view)
			const float  heading = player->GetAngleZ();
			RE::NiPoint3 right{ std::cos(heading), -std::sin(heading), 0.0f };
			up = right.Cross(forward);
			if (up.Length() > 1e-3f) {
				up = up / up.Length();
			}
			XMFLOAT4X4 axes{};
			axes.m[0][0] = forward.x * kCmToUnits, axes.m[0][1] = forward.y * kCmToUnits, axes.m[0][2] = forward.z * kCmToUnits;
			axes.m[1][0] = right.x * kCmToUnits, axes.m[1][1] = right.y * kCmToUnits, axes.m[1][2] = right.z * kCmToUnits;
			axes.m[2][0] = up.x * kCmToUnits, axes.m[2][1] = up.y * kCmToUnits, axes.m[2][2] = up.z * kCmToUnits;
			axes.m[3][3] = 1.0f;
			// (bare hands are held up close: pushed a little forward, or Skyrim's nearer edge of view cuts them -
			// sheets/camera.json fists_forward)
			float push = 0.0f;
			if (std::string_view(link.weaponClass).empty() || std::string_view(link.weaponClass).find("Fists") != std::string_view::npos || link.weaponState != 1) {
				for (const auto& row : sheets::kCamera) {
					if (std::string_view(row.id) == "fists_forward") {
						push = row.value;
					}
				}
			}
			placement = toCamera * XMMatrixTranslation(push, 0.0f, 0.0f) * XMLoadFloat4x4(&axes);  // (positions are already relative to Skyrim's camera)
		} else if (auto* arms = player->Get3D(true)) {
			armsHidden = false;  // (in third person Skyrim keeps that node hidden itself: it is left alone)
			arms->CullGeometry(link.live);  // (their shapes stay hidden while MORDHAU is linked: they flashed up during the switch)
		}
		return false;
		}

		void DrawInFrame()
		{
			if (g_failed) {
				return;
			}
			auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
			auto* camera = RE::Main::WorldRootCamera();
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* root = player ? player->Get3D(false) : nullptr;
			auto* ui = RE::UI::GetSingleton();
			if (!renderer || !camera || !root || !ui || ui->IsMenuOpen(RE::MainMenu::MENU_NAME) || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) {
				return;
			}
			auto& rd = renderer->GetRuntimeData();
			auto* device = reinterpret_cast<ID3D11Device*>(rd.forwarder);
			auto* context = reinterpret_cast<ID3D11DeviceContext*>(rd.context);
			if (!device || !context) {
				return;
			}
			if (!g_ready) {
				if (!Load(device)) {
					g_failed = true;
					logger::warn("character: not drawn");
					return;
				}
				g_ready = true;
			}
			// (a Skyrim scene that has the player's body is treated as MORDHAU not being linked: Skyrim's own body and arms)
			const bool mine = Link::Current().live && !g_skyrimOwns.load();
			// (with this plugin's own clips in the cache - g_builtIn - the two branches below were skipped, and during a
			// Skyrim scene in first person MORDHAU's arms were still drawn: the wagon ride at the start of the game)
			// (only once MORDHAU's character is loaded: its bone count is what the link is read with, and it is loaded
			// further down - a scene at the very start of a session returned here first, the link then read as dead, the
			// scene was never looked at again, and MORDHAU never took over)
			const bool skyrimScene = Link::Current().live && g_skyrimOwns.load() && !g_bones.empty() && g_pose;
			if ((!g_builtIn || skyrimScene) && !mine) {
				// (still listening for MORDHAU: the link is read here while nothing of ours is drawn)
				static std::vector<float> unused;
				Link::Poll(g_bones.size(), unused);
				// (and the game side's own work for the frame goes on - it is done from the pose callback, which is not
				// reached below: without it nothing was sent to MORDHAU any more and the scene was never seen to end.
				// Seen in play: Skyrim's own combat came back after sneaking and stealing.)
				if (g_pose) {
					g_pose();
				}
			}
			if ((!g_builtIn || skyrimScene) && !mine) {
				// MORDHAU is not linked (not started yet, or restarting), or a Skyrim scene has the player: everyone gets Skyrim's own body back
				if (!g_unlinked) {
					root->CullGeometry(false);
					if (auto* arms = player->Get3D(true)) {
						arms->CullGeometry(false);
					}
					for (const auto hiddenId : g_hiddenActors) {
						if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(hiddenId)) {
							if (auto* actorRoot = actor->Get3D(false)) {
								actorRoot->CullGeometry(false);
							}
						}
					}
					g_hiddenActors.clear();
					logger::info("character: MORDHAU is not linked; Skyrim's own characters are shown");
				}
				g_unlinked = true;
				return;
			}
			if (g_unlinked) {
				g_unlinked = false;
				logger::info("character: MORDHAU is linked; its characters are drawn");
			}
			auto*      playerCamera = RE::PlayerCamera::GetSingleton();
			const bool firstPerson = playerCamera && playerCamera->IsInFirstPerson();
			g_playerSkyrim.id = 0;
			// Standing on the player: Skyrim's own character is not drawn, but its bones stay, so position, collision
			// and the blade trace carry on underneath. (Takes effect from the next frame.)
			const bool skyrimBody = g_skyrimBodies.load() && Link::Current().live && !firstPerson;  // the player is Skyrim's own model
			if (g_sideOffset == 0.0f) {
				root->CullGeometry(!skyrimBody);

				static bool logged = false;
				if (!logged) {
					logged = true;
					logger::info("character: standing on the player; Skyrim's own character model is hidden");
				}
			}
			const auto& scene = rd.renderTargets[RE::RENDER_TARGETS::kMAIN];
			const auto& depth = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kMAIN];
			auto*       sceneTex = reinterpret_cast<ID3D11Texture2D*>(scene.texture);
			auto*       rtv = reinterpret_cast<ID3D11RenderTargetView*>(scene.RTV);
			auto*       dsv = reinterpret_cast<ID3D11DepthStencilView*>(depth.views[0]);
			if (!sceneTex || !rtv || !dsv) {
				return;
			}
			D3D11_TEXTURE2D_DESC sd{};
			sceneTex->GetDesc(&sd);

			// Skyrim's own world -> clip matrix, re-based on the camera so the GPU sees small numbers.
			const auto&        w2c = camera->GetRuntimeData().worldToCam;
			const RE::NiPoint3 cam = camera->world.translate;
			XMFLOAT4X4         vpColumn{};  // column-vector form: clip = M * p
			for (int r = 0; r < 4; ++r) {
				for (int c = 0; c < 3; ++c) {
					vpColumn.m[r][c] = w2c[r][c];
				}
				vpColumn.m[r][3] = static_cast<float>(double(w2c[r][3]) + double(w2c[r][0]) * cam.x + double(w2c[r][1]) * cam.y + double(w2c[r][2]) * cam.z);
			}
			const XMMATRIX viewProj = XMMatrixTranspose(XMLoadFloat4x4(&vpColumn));  // row-vector form

			// Which way Skyrim's depth runs: compare clip depth 100 and 10000 units in front of the camera.
			const RE::NiPoint3 forward = camera->world.rotate * RE::NiPoint3{ 1.0f, 0.0f, 0.0f };  // NiCamera looks along its local X
			const auto         depthAt = [&](float a_distance) {
				const XMVECTOR p = XMVector4Transform(XMVectorSet(forward.x * a_distance, forward.y * a_distance, forward.z * a_distance, 1.0f), viewProj);
				return XMVectorGetZ(p) / XMVectorGetW(p);
			};
			const bool reversed = depthAt(10000.0f) < depthAt(100.0f);

			// MORDHAU component space (cm, left-handed, facing +Y, its right is -X) -> Skyrim: mirror X, scale to units,
			// turn with the actor, move to the actor (plus a side step), all relative to the camera.
			XMFLOAT4X4 actorRot{};
			for (int r = 0; r < 3; ++r) {
				for (int c = 0; c < 3; ++c) {
					actorRot.m[r][c] = root->world.rotate.entry[c][r];  // transpose: row-vector form
				}
			}
			actorRot.m[3][3] = 1.0f;
			const RE::NiPoint3 side = root->world.rotate * RE::NiPoint3{ g_sideOffset, 0.0f, 0.0f };
			const RE::NiPoint3 at = root->world.translate + side - cam;
			const XMMATRIX     world = XMMatrixScaling(-kCmToUnits, kCmToUnits, kCmToUnits) * XMLoadFloat4x4(&actorRot) * XMMatrixTranslation(at.x, at.y, at.z);

			static std::vector<XMFLOAT4X4> boneMatrices(kMaxBones);
			BuildBones(g_pose ? g_pose() : Pose{}, boneMatrices.data());
			// the player thrown by Skyrim (a dragon's shout, a giant): the MORDHAU body takes Skyrim's fall until the player is up again
			if (player->IsInRagdollState() && g_bones.size() + 1 <= kMaxBones) {
				if (!g_retarget.tried) {
					PrepareRetarget(g_bones, player);
				}
				if (BuildFromSkyrim(player, root, g_bones, boneMatrices.data())) {
					XMStoreFloat4x4(&boneMatrices[g_bones.size()], XMMatrixScaling(0.0f, 0.0f, 0.0f));  // (the weapon is not in a thrown hand)
				}
			}

			// First person. With MORDHAU live and in first person too: draw its body from its own camera, by laying that
			// camera exactly onto Skyrim's (MORDHAU is a true first-person game: the body below the neck is the view).
			// Otherwise Skyrim's own first-person arms stay and nothing of ours is drawn.
			XMMATRIX   placement = world;
			const auto link = Link::Current();
			// (Skyrim's own first-person arms and weapon are hidden at their top node as well as shape by shape: Skyrim
			// itself switches a bloodied weapon's blood layer back on, which then hung alone in front of the camera)
			static bool armsHidden = false;
			const auto  hideArms = [&](RE::NiAVObject* a_arms, bool a_hide) {
				if (a_arms && (a_hide || armsHidden)) {
					a_arms->SetAppCulled(a_hide);
					armsHidden = a_hide;
				}
			};
			if (ShowFirstPersonView(player, firstPerson, forward, boneMatrices, placement, link, armsHidden, hideArms)) {
				return;
			}

			const std::string* playerDropModel = nullptr;   // (Skyrim-bodies mode: MORDHAU's model of the player's dropped weapon)
			XMMATRIX           playerDropWorld = XMMatrixIdentity();
			if (skyrimBody) {
				if (auto* skyrimShield = root->GetObjectByName("SHIELD")) {
					skyrimShield->CullGeometry(link.live && link.shieldHas >= 1 && !g_skyrimOwns.load() && !g_skyrimShieldLook.load());  // (MORDHAU's model is drawn instead)
				}
				if (auto* weapon = root->GetObjectByName("WEAPON")) {
					weapon->CullGeometry(link.live && (link.weaponState != 1 || !MordhauHeldBy(0).empty()));  // (seen only in the hand, and not when it is a MORDHAU item)
				}
				for (const char* sheath : kSheathNodes) {
					if (auto* carried = root->GetObjectByName(sheath)) {
						carried->CullGeometry(link.live && !MordhauHeldBy(0).empty());  // (a MORDHAU item put away: its own model is drawn there)
					}
				}
			}
			// Dropped or disarmed in MORDHAU: the weapon lies in Skyrim's world. It follows MORDHAU's fall for a moment
			// (relative to the player, as MORDHAU has it), then stays at that spot while the player walks on.
			PlaceDroppedPlayerWeapon(player, skyrimBody, cam, world, boneMatrices, placement, link, playerDropModel, playerDropWorld);

			// The weapon in MORDHAU's hand is not the mercenary's own (the Skyrim weapon's counterpart, or one picked up):
			// its own model is drawn, the baked one is left out.
			PropModel* handModel = nullptr;
			XMMATRIX   handWeapon = XMMatrixIdentity();
			bool       skyrimWeapon = false;
			XMMATRIX   skyWeaponWorld = XMMatrixIdentity();
			PlaceSkyrimWeaponInHand(player, skyrimBody, boneMatrices, placement, link, skyrimWeapon, skyWeaponWorld);
			PickHandWeaponModel(device, boneMatrices, link, handModel, handWeapon, skyrimWeapon);

			ReadSceneLight();
			SavedState saved;
			saved.Save(context);

			D3D11_MAPPED_SUBRESOURCE map{};
			if (SUCCEEDED(context->Map(g_frameCb, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
				auto* fc = static_cast<FrameConstants*>(map.pData);
				XMStoreFloat4x4(&fc->world, placement);
				XMStoreFloat4x4(&fc->viewProj, viewProj);
				g_bloodNow = BloodOf(0);
				FillLight(fc, SunReaching(player), PersonOf(player));
				context->Unmap(g_frameCb, 0);
			}
			if (SUCCEEDED(context->Map(g_boneCb, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
				std::memcpy(map.pData, boneMatrices.data(), sizeof(XMFLOAT4X4) * std::min<std::size_t>(g_bones.size() + 1, kMaxBones));
				context->Unmap(g_boneCb, 0);
			}

			D3D11_VIEWPORT vp{ 0.0f, 0.0f, static_cast<float>(sd.Width), static_cast<float>(sd.Height), 0.0f, 1.0f };
			const UINT     stride = sizeof(Vertex), offset = 0;
			const FLOAT    factor[4]{ 0, 0, 0, 0 };
			context->OMSetRenderTargets(1, &rtv, dsv);
			context->RSSetViewports(1, &vp);
			context->RSSetState(g_raster);
			context->OMSetBlendState(g_blend, factor, 0xFFFFFFFF);
			context->OMSetDepthStencilState(reversed ? g_depthGreater : g_depthLess, 0);
			context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			context->IASetInputLayout(g_layout);
			context->IASetVertexBuffers(0, 1, &g_vb, &stride, &offset);
			context->IASetIndexBuffer(g_ib, DXGI_FORMAT_R32_UINT, 0);
			context->VSSetShader(g_vs, nullptr, 0);
			context->PSSetShader(g_ps, nullptr, 0);
			ID3D11Buffer* cbs[2]{ g_frameCb, g_boneCb };
			context->VSSetConstantBuffers(0, 2, cbs);
			ID3D11Buffer* psCbs[3]{ g_frameCb, g_boneCb, g_pieceCb };
			context->PSSetConstantBuffers(0, 3, psCbs);
			context->PSSetSamplers(0, 1, &g_sampler);
			const auto drawSections = [&](const std::vector<Section>& a_sections) {
				for (const auto& section : a_sections) {
					if (SUCCEEDED(context->Map(g_pieceCb, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
						auto* piece = static_cast<float*>(map.pData);
						for (int channel = 0; channel < 3; ++channel) {
							piece[channel * 4 + 0] = section.tints[channel * 3 + 0];
							piece[channel * 4 + 1] = section.tints[channel * 3 + 1];
							piece[channel * 4 + 2] = section.tints[channel * 3 + 2];
							piece[channel * 4 + 3] = 1.0f;
						}
						piece[12] = section.mask ? 1.0f : 0.0f;
						piece[13] = section.emblem ? 1.0f : 0.0f;
						piece[14] = section.normal ? 1.0f : 0.0f;
						piece[15] = section.surface ? 1.0f : 0.0f;
						const float* ev = section.emblemValues;
						piece[16] = ev[0], piece[17] = ev[1], piece[18] = std::max(ev[2], 1e-4f), piece[19] = std::max(ev[3], 1e-4f);
						piece[20] = std::cos(ev[4] * 6.2831853f), piece[21] = std::sin(ev[4] * 6.2831853f), piece[22] = piece[23] = 0.0f;
						piece[24] = ev[5], piece[25] = ev[6], piece[26] = ev[7], piece[27] = 1.0f;
						piece[28] = ev[8], piece[29] = ev[9], piece[30] = ev[10], piece[31] = 1.0f;
						piece[32] = section.skin[0], piece[33] = section.skin[1], piece[34] = section.skin[2], piece[35] = 0.0f;
						context->Unmap(g_pieceCb, 0);
					}
					ID3D11ShaderResourceView* maps[7]{ section.srv, section.mask ? section.mask : section.srv, section.emblem ? section.emblem : section.srv,
						section.normal ? section.normal : section.srv, section.surface ? section.surface : section.srv, section.hair ? section.hair : section.srv,
						section.beard ? section.beard : section.srv };
					context->PSSetShaderResources(0, 7, maps);
					context->DrawIndexed(section.count, section.first, 0);
				}
			};
			LoadBloodPattern(device);
			if (g_bloodSrv) {
				context->PSSetShaderResources(7, 1, &g_bloodSrv);
			}
			// (a prop that is posed - the arrow on a bow's string: its bone matrices, else the prop is drawn rigid)
			const XMFLOAT4X4* propPose = nullptr;
			std::size_t       propPoseBones = 0;
			const auto        fromLink = [](const float* a_seven) {
				return XMMatrixRotationQuaternion(XMVectorSet(a_seven[0], a_seven[1], a_seven[2], a_seven[3])) * XMMatrixTranslation(a_seven[4], a_seven[5], a_seven[6]);
			};
			const auto drawProp = [&](const std::string& a_name, const XMMATRIX& a_world, float a_sun) {
				auto& model = g_propModels[a_name];
				if (!model.tried) {
					model.tried = true;
					int unusedWeapon = -1, unusedHead = -1;
					model.ready = LoadMesh(device, g_cache / "props" / a_name, model.look.bones, model.look.sections, &model.look.vb, &model.look.ib, &unusedWeapon, &unusedHead);
					logger::info("props: {} {}", a_name, model.ready ? "loaded" : "NOT available");
				}
				const std::size_t bones = model.look.bones.size();
				if (!model.ready || bones + 1 > kMaxBones) {
					return;
				}
				static std::vector<XMFLOAT4X4> still(kMaxBones);
				for (std::size_t i = 0; i <= bones; ++i) {
					if (propPose && propPoseBones == bones && i < bones) {
						XMStoreFloat4x4(&still[i], XMLoadFloat4x4(&model.look.bones[i].invRest) * XMLoadFloat4x4(&propPose[i]));
					} else {
						XMStoreFloat4x4(&still[i], XMMatrixIdentity());
					}
				}
				if (SUCCEEDED(context->Map(g_frameCb, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
					auto* fc = static_cast<FrameConstants*>(map.pData);
					XMStoreFloat4x4(&fc->world, a_world);
					XMStoreFloat4x4(&fc->viewProj, viewProj);
					FillLight(fc, a_sun, PersonOf(nullptr));
					context->Unmap(g_frameCb, 0);
				}
				if (SUCCEEDED(context->Map(g_boneCb, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
					std::memcpy(map.pData, still.data(), sizeof(XMFLOAT4X4) * (bones + 1));
					context->Unmap(g_boneCb, 0);
				}
				context->IASetVertexBuffers(0, 1, &model.look.vb, &stride, &offset);
				context->IASetIndexBuffer(model.look.ib, DXGI_FORMAT_R32_UINT, 0);
				drawSections(model.look.sections);
			};
			if (skyrimBody) {
				// (nothing of MORDHAU's is drawn for the player: Skyrim's own body is posed instead, see PoseSkyrimBody)
			} else if (auto* outfit = PlayerOutfit(device, player)) {
				// the outfit for what is worn in Skyrim, and the mercenary's own weapon in the hand
				context->IASetVertexBuffers(0, 1, &outfit->vb, &stride, &offset);
				context->IASetIndexBuffer(outfit->ib, DXGI_FORMAT_R32_UINT, 0);
				drawSections(outfit->sections);
				static std::vector<Section> weaponOnly;
				if (weaponOnly.empty()) {
					for (const auto& section : g_sections) {
						if (section.weapon) {
							weaponOnly.push_back(section);
						}
					}
				}
				context->IASetVertexBuffers(0, 1, &g_vb, &stride, &offset);
				context->IASetIndexBuffer(g_ib, DXGI_FORMAT_R32_UINT, 0);
				drawSections(weaponOnly);
			} else {
				drawSections(g_sections);
			}
			DrawSkyrimWeapon(player, root, device, context, viewProj, link, skyrimWeapon, skyWeaponWorld, map);
			DrawPlayerShield(player, skyrimBody, placement, link, fromLink, drawProp);
			DrawHandWeaponModel(player, context, skyrimBody, viewProj, placement, handModel, handWeapon, map, stride, offset, drawSections);

			// MORDHAU's stand-ins, drawn where their Skyrim characters stand (whose own models are hidden meanwhile):
			// the enemy you fight is the MORDHAU fighter whose attacks MORDHAU is actually resolving.
			DrawWorldFigures(player, device, context, skyrimBody, cam, viewProj, link, playerDropModel, playerDropWorld, map, stride, offset, drawSections, drawProp);
			// The parts of MORDHAU's own HUD streamed from its window (drawn in PresentHook) - and, only while MORDHAU's
			// health / stamina bars are shown (sheets/builds.json mordhau_bars), plain stand-in bars for when no picture
			// of MORDHAU's window can be had: stamina (MORDHAU's own value) on the left, health (Skyrim's) on the right.
			// MORDHAU's hit marker: four short slanted ticks around the crosshair for a quarter of a second (for hits Skyrim
			// judges - creatures; MORDHAU shows its own for the fighters it judges). sheets/builds.json hit_marker_*
			DrawHitMarker(context, sd, map, stride, offset);
			bool hudStreamed = false;
			if (link.live && g_hudVs && g_hudTexPs && g_hudVb && g_depthOff && g_blendAlpha && player->AsActorState()->IsWeaponDrawn()) {
				// drawn after Skyrim's post-processing (so its blur and colour grading leave it alone): see PresentHook
				EnsurePresentHook(renderer, device, context);
				g_hudWantedAt = std::chrono::steady_clock::now();
				std::scoped_lock lock(g_hud.lock);
				hudStreamed = g_presentHooked && g_hud.width[0] > 0 && std::chrono::duration<float>(g_hudWantedAt - g_hud.when).count() < 1.5f;
			}
			DrawFallbackBars(player, context, sd, link, map, stride, offset, hudStreamed);
			saved.Restore(context);

			static bool logged = false;
			if (!logged) {
				logged = true;
				logger::info("character: drawing into Skyrim's scene {}x{} (format {}), depth {}, {} sections", sd.Width, sd.Height, static_cast<int>(sd.Format),
					reversed ? "reversed" : "standard", g_sections.size());
			}
		}
	}
}
