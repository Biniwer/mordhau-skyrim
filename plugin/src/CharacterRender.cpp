#include "CharacterRenderInternal.h"

namespace dm::CharacterRender
{
	inline namespace internal
	{
		// Skyrim's ground under a point (a ray down from above it); the point's own height when nothing is hit.
		float GroundBelow(RE::Actor* a_near, float a_x, float a_y, float a_z)
		{
			auto* cell = a_near ? a_near->GetParentCell() : nullptr;
			auto* world = cell ? cell->GetbhkWorld() : nullptr;
			if (!world) {
				return a_z;
			}
			const float     scale = RE::bhkWorld::GetWorldScale(), above = 150.0f, below = 400.0f;
			RE::bhkPickData pick;
			pick.rayInput.from = RE::hkVector4(a_x * scale, a_y * scale, (a_z + above) * scale, 0.0f);
			pick.rayInput.to = RE::hkVector4(a_x * scale, a_y * scale, (a_z - below) * scale, 0.0f);
			RE::CFilter filter;
			a_near->GetCollisionFilterInfo(filter);
			filter.SetCollisionLayer(RE::COL_LAYER::kLOS);
			pick.rayInput.filterInfo = filter;
			{
				RE::BSReadLockGuard lock(world->worldLock);
				world->PickObject(pick);
			}
			return pick.rayOutput.HasHit() ? a_z + above - (above + below) * pick.rayOutput.hitFraction : a_z;
		}

		                                                                  // sharing that one threw "resource deadlock would occur" and took the game down)
		bool AsSkyrim(std::uint32_t a_owner)
		{
			std::scoped_lock lock(g_droppedLock);
			return g_droppedAsSkyrim.contains(a_owner);
		}

		bool                              SkyrimHasBody(std::uint32_t a_id)
		{
			std::scoped_lock lock(g_skyrimHasLock);
			return g_skyrimHas.contains(a_id);
		}

		std::string MordhauHeldBy(std::uint32_t a_id)
		{
			std::scoped_lock lock(g_mordhauHeldLock);
			const auto       found = g_mordhauHeld.find(a_id);
			return found == g_mordhauHeld.end() ? std::string{} : found->second;
		}

		XMMATRIX LocalMatrix(const float* a_rot, const float* a_pos)
		{
			return XMMatrixRotationQuaternion(XMVectorSet(a_rot[0], a_rot[1], a_rot[2], a_rot[3])) * XMMatrixTranslation(a_pos[0], a_pos[1], a_pos[2]);
		}

		bool Compile(const char* a_entry, const char* a_target, ID3DBlob** a_out)
		{
			ID3DBlob*  errors = nullptr;
			const auto hr = D3DCompile(kShader, sizeof(kShader) - 1, "dm_character", nullptr, nullptr, a_entry, a_target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, a_out, &errors);
			if (FAILED(hr)) {
				logger::error("character: shader {} failed: {}", a_entry, errors ? static_cast<const char*>(errors->GetBufferPointer()) : "no message");
			}
			if (errors) {
				errors->Release();
			}
			return SUCCEEDED(hr);
		}

		// One baked character (models, weights, maps): the player's, or a look for MORDHAU's stand-ins.
		bool LoadMesh(ID3D11Device* a_device, const std::filesystem::path& a_dir, std::vector<Bone>& a_bones, std::vector<Section>& a_sections, ID3D11Buffer** a_vb, ID3D11Buffer** a_ib, int* a_weaponBone, int* a_headBone)
		{
			const auto&   dir = a_dir;
			std::ifstream in(dir / "character.dmchar", std::ios::binary);
			char          magic[4]{};
			std::int32_t  version = 0, bones = 0, vertices = 0, indices = 0, sections = 0;
			in.read(magic, 4);
			in.read(reinterpret_cast<char*>(&version), 4);
			in.read(reinterpret_cast<char*>(&bones), 4);
			in.read(reinterpret_cast<char*>(&vertices), 4);
			in.read(reinterpret_cast<char*>(&indices), 4);
			in.read(reinterpret_cast<char*>(&sections), 4);
			if (!in || std::memcmp(magic, "DMCH", 4) != 0 || version != 6 || bones <= 0 || bones > static_cast<std::int32_t>(kMaxBones) || vertices <= 0 || indices <= 0 || sections <= 0 || sections > 64) {
				logger::warn("character: no usable character.dmchar in '{}'", dir.string());
				return false;
			}
			a_bones.resize(static_cast<std::size_t>(bones));
			std::vector<XMMATRIX> rest(a_bones.size());
			for (std::size_t i = 0; i < a_bones.size(); ++i) {
				auto& bone = a_bones[i];
				char  name[64]{};
				in.read(name, 64);
				name[63] = '\0';
				bone.name = name;
				if (bone.name == "RightWeapon") {
					*a_weaponBone = static_cast<int>(i);
				}
				if (bone.name == "Head") {
					*a_headBone = static_cast<int>(i);
				}
				in.read(reinterpret_cast<char*>(&bone.parent), 4);
				in.read(reinterpret_cast<char*>(&bone.restRot), 16);
				in.read(reinterpret_cast<char*>(&bone.restPos), 12);
				const XMMATRIX local = LocalMatrix(&bone.restRot.x, &bone.restPos.x);
				rest[i] = bone.parent >= 0 && static_cast<std::size_t>(bone.parent) < i ? local * rest[static_cast<std::size_t>(bone.parent)] : local;
				XMStoreFloat4x4(&bone.invRest, XMMatrixInverse(nullptr, rest[i]));
			}
			struct RawSection
			{
				std::int32_t first, count;
				char         texture[64];
				char         mask[64];
				float        tints[9];
				char         emblem[64];
				float        emblemValues[11];
				char         normal[64];
				char         surface[64];
				char         hair[64];
				char         beard[64];
				float        skin[4];
			};
			std::vector<RawSection> raw(static_cast<std::size_t>(sections));
			in.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(raw.size() * sizeof(RawSection)));
			std::vector<Vertex>        verts(static_cast<std::size_t>(vertices));
			std::vector<std::uint32_t> idx(static_cast<std::size_t>(indices));
			in.read(reinterpret_cast<char*>(verts.data()), static_cast<std::streamsize>(verts.size() * sizeof(Vertex)));
			in.read(reinterpret_cast<char*>(idx.data()), static_cast<std::streamsize>(idx.size() * 4));
			if (!in) {
				logger::warn("character: character.dmchar is cut short");
				return false;
			}

			D3D11_BUFFER_DESC bd{};
			bd.Usage = D3D11_USAGE_IMMUTABLE;
			bd.ByteWidth = static_cast<UINT>(verts.size() * sizeof(Vertex));
			bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			D3D11_SUBRESOURCE_DATA init{ verts.data(), 0, 0 };
			if (FAILED(a_device->CreateBuffer(&bd, &init, a_vb))) {
				return false;
			}
			bd.ByteWidth = static_cast<UINT>(idx.size() * 4);
			bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
			init.pSysMem = idx.data();
			if (FAILED(a_device->CreateBuffer(&bd, &init, a_ib))) {
				return false;
			}
			std::unordered_map<std::string, ID3D11ShaderResourceView*> textures;
			// MORDHAU's maps as stored (block-compressed), sampled without sRGB conversion: that is how Skyrim samples its
			// own textures, so its lighting and colour grading treat both alike
			const auto texture = [&](const char* a_field) -> ID3D11ShaderResourceView* {
				char name[65]{};
				std::memcpy(name, a_field, 64);
				if (!*name) {
					return nullptr;
				}
				auto& srv = textures[name];
				if (!srv) {
					const auto hr = CreateDDSTextureFromFileEx(a_device, (dir / name).wstring().c_str(), 0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
						DDS_LOADER_DEFAULT, nullptr, &srv);
					logger::info("character: map '{}' {}", name, SUCCEEDED(hr) ? "loaded" : "FAILED");
				}
				return srv;
			};
			int dyed = 0;
			for (const auto& s : raw) {
				if (auto* srv = texture(s.texture)) {
					Section section{ static_cast<std::uint32_t>(s.first), static_cast<std::uint32_t>(s.count), srv, texture(s.mask) };
					std::memcpy(section.tints, s.tints, sizeof(section.tints));
					if (s.count > 0 && static_cast<std::size_t>(s.first) < idx.size() && idx[static_cast<std::size_t>(s.first)] < verts.size()) {
						section.weapon = verts[idx[static_cast<std::size_t>(s.first)]].ids[0] == static_cast<std::uint8_t>(bones);
					}
					section.emblem = texture(s.emblem);
					section.normal = texture(s.normal);
					section.surface = texture(s.surface);
					section.hair = texture(s.hair);
					section.beard = texture(s.beard);
					std::memcpy(section.skin, s.skin, sizeof(section.skin));
					std::memcpy(section.emblemValues, s.emblemValues, sizeof(section.emblemValues));
					section.zMin = FLT_MAX, section.zMax = -FLT_MAX;
					for (std::int32_t k = s.first; k < s.first + s.count && static_cast<std::size_t>(k) < idx.size(); ++k) {
						if (idx[static_cast<std::size_t>(k)] < verts.size()) {
							const float z = verts[idx[static_cast<std::size_t>(k)]].pos[2];
							section.zMin = std::min(section.zMin, z), section.zMax = std::max(section.zMax, z);
						}
					}
					if (section.zMin > section.zMax) {
						section.zMin = section.zMax = 0.0f;
					}
					dyed += section.mask ? 1 : 0;
					a_sections.push_back(section);
				}
			}
			logger::info("character: {} sections dyed through their pattern masks", dyed);
			logger::info("character: {} bones, {} vertices, {} triangles, {} of {} sections with a colour map", bones, vertices, indices / 3, a_sections.size(), sections);
			return !a_sections.empty();
		}

		bool Load(ID3D11Device* a_device)
		{
			if (!LoadMesh(a_device, g_cache / "character", g_bones, g_sections, &g_vb, &g_ib, &g_weaponBone, &g_headBone)) {
				return false;
			}
			D3D11_BUFFER_DESC bd{};
			bd.Usage = D3D11_USAGE_DYNAMIC;
			bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			bd.ByteWidth = sizeof(FrameConstants);
			if (FAILED(a_device->CreateBuffer(&bd, nullptr, &g_frameCb))) {
				return false;
			}
			bd.ByteWidth = sizeof(XMFLOAT4X4) * kMaxBones;
			if (FAILED(a_device->CreateBuffer(&bd, nullptr, &g_boneCb))) {
				return false;
			}
			bd.ByteWidth = 144;
			if (FAILED(a_device->CreateBuffer(&bd, nullptr, &g_pieceCb))) {
				return false;
			}

			ID3DBlob* vsBlob = nullptr;
			ID3DBlob* psBlob = nullptr;
			if (!Compile("VS", "vs_5_0", &vsBlob) || !Compile("PS", "ps_5_0", &psBlob)) {
				return false;
			}
			a_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_vs);
			a_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_ps);
			const D3D11_INPUT_ELEMENT_DESC layout[] = {
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "BLENDWEIGHT", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			};
			a_device->CreateInputLayout(layout, 5, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_layout);
			vsBlob->Release();
			psBlob->Release();
			if (!g_vs || !g_ps || !g_layout) {
				return false;
			}

			D3D11_SAMPLER_DESC sd{};
			sd.Filter = D3D11_FILTER_ANISOTROPIC;
			sd.MaxAnisotropy = 8;
			sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
			sd.MaxLOD = D3D11_FLOAT32_MAX;
			a_device->CreateSamplerState(&sd, &g_sampler);
			D3D11_RASTERIZER_DESC rs{};
			rs.FillMode = D3D11_FILL_SOLID;
			rs.CullMode = D3D11_CULL_NONE;  // the left-handed model is mirrored into Skyrim's space, and cloth is two-sided
			rs.DepthClipEnable = TRUE;
			a_device->CreateRasterizerState(&rs, &g_raster);
			D3D11_BLEND_DESC bs{};
			bs.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			a_device->CreateBlendState(&bs, &g_blend);
			D3D11_DEPTH_STENCIL_DESC ds{};
			ds.DepthEnable = TRUE;
			ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			ds.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
			a_device->CreateDepthStencilState(&ds, &g_depthLess);
			ds.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
			a_device->CreateDepthStencilState(&ds, &g_depthGreater);
			ds.DepthEnable = FALSE;
			ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
			a_device->CreateDepthStencilState(&ds, &g_depthOff);
			{
				ID3DBlob* hudVs = nullptr;
				ID3DBlob* hudPs = nullptr;
				if (Compile("VSSkyrim", "vs_5_0", &g_skyVsBlob)) {
					a_device->CreateVertexShader(g_skyVsBlob->GetBufferPointer(), g_skyVsBlob->GetBufferSize(), nullptr, &g_skyVs);
				}
				ID3DBlob* hudPicturePs = nullptr;
				if (Compile("PSHudPicture", "ps_5_0", &hudPicturePs)) {
					a_device->CreatePixelShader(hudPicturePs->GetBufferPointer(), hudPicturePs->GetBufferSize(), nullptr, &g_hudPicturePs);
					hudPicturePs->Release();
				}
				ID3DBlob* hudTexPs = nullptr;
				if (Compile("PSHudTex", "ps_5_0", &hudTexPs)) {
					a_device->CreatePixelShader(hudTexPs->GetBufferPointer(), hudTexPs->GetBufferSize(), nullptr, &g_hudTexPs);
					hudTexPs->Release();
				}
				D3D11_BLEND_DESC alpha{};
				alpha.RenderTarget[0].BlendEnable = TRUE;
				alpha.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
				alpha.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
				alpha.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
				alpha.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
				alpha.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
				alpha.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
				alpha.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
				a_device->CreateBlendState(&alpha, &g_blendAlpha);
				if (Compile("VSHud", "vs_5_0", &hudVs) && Compile("PSHud", "ps_5_0", &hudPs)) {
					a_device->CreateVertexShader(hudVs->GetBufferPointer(), hudVs->GetBufferSize(), nullptr, &g_hudVs);
					a_device->CreatePixelShader(hudPs->GetBufferPointer(), hudPs->GetBufferSize(), nullptr, &g_hudPs);
				}
				if (hudVs) {
					hudVs->Release();
				}
				if (hudPs) {
					hudPs->Release();
				}
				D3D11_BUFFER_DESC hb{};
				hb.Usage = D3D11_USAGE_DYNAMIC;
				hb.BindFlags = D3D11_BIND_VERTEX_BUFFER;
				hb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
				hb.ByteWidth = sizeof(Vertex) * 6 * 16;
				a_device->CreateBuffer(&hb, nullptr, &g_hudVb);
			}

			return !g_sections.empty() && g_sampler && g_raster && g_blend && g_depthLess && g_depthGreater;
		}
	}

	void SetCreature(std::uint32_t a_formId, const char* a_name)
	{
		g_creatureId = a_formId;
		g_creatureName = a_name ? a_name : "";
	}

	void SetStandinWeapons(const std::unordered_map<std::uint32_t, std::string>& a_weapons)
	{
		g_standinWeapons = a_weapons;
	}

	void SetMenu(int a_menu, int a_page)
	{
		g_menuShown.store(a_menu > 0 ? 1 : 0);
		(void)a_page;
	}

	bool WeaponOnGroundNear(const RE::NiPoint3& a_from, float a_reach, std::uint32_t& a_owner, std::string& a_model, bool a_take)
	{
		float best = a_reach;
		bool  found = false;
		int   leftIndex = -1;
		std::scoped_lock lock(g_leftLock);
		for (std::size_t i = 0; i < g_leftWeapons.size(); ++i) {
			const RE::NiPoint3 at{ g_leftWeapons[i].lying.m[3][0], g_leftWeapons[i].lying.m[3][1], g_leftWeapons[i].lying.m[3][2] };
			if (at.GetDistance(a_from) <= best) {
				best = at.GetDistance(a_from);
				leftIndex = static_cast<int>(i);
				found = true;
			}
		}
		if (g_playerWeaponDown.load() && g_playerWeaponAt.GetDistance(a_from) <= best) {
			best = g_playerWeaponAt.GetDistance(a_from);
			a_owner = 0;
			found = true;
			leftIndex = -1;
		}
		int fallenIndex = -1;
		for (std::size_t i = 0; i < std::size(g_fallen); ++i) {
			const auto&        fallen = g_fallen[i];
			const RE::NiPoint3 at{ fallen.lying.m[3][0], fallen.lying.m[3][1], fallen.lying.m[3][2] };
			if (fallen.id != 0 && at.GetDistance(a_from) <= best) {
				best = at.GetDistance(a_from);
				a_owner = fallen.id;
				found = true;
				leftIndex = -1;
				fallenIndex = static_cast<int>(i);
			}
		}
		if (a_take && fallenIndex >= 0) {
			// (taken up: it no longer lies there - it stayed on the ground, model and Skyrim copy, after the player had
			// picked a follower's sword up into the inventory)
			g_fallen[static_cast<std::size_t>(fallenIndex)] = {};
		}
		if (found && a_owner == 0 && leftIndex >= 0 && !(g_playerWeaponDown.load() && g_playerWeaponAt.GetDistance(a_from) <= best)) {
			leftIndex = leftIndex;  // (nobody's is nearest)
		}
		if (leftIndex >= 0) {
			a_owner = 0xFFFFFFFF;
			a_model = g_leftWeapons[static_cast<std::size_t>(leftIndex)].model;
			if (a_take) {
				g_leftWeapons.erase(g_leftWeapons.begin() + leftIndex);
			}
		}
		return found;
	}

	void AddBlood(std::uint32_t a_formId, float a_amount)
	{
		std::scoped_lock lock(g_bloodLock);
		auto&            blood = g_blood[a_formId];
		blood = std::clamp(blood + a_amount, 0.0f, 1.0f);
	}

	void ExplodeHead(std::uint32_t a_formId)
	{
		g_headless[a_formId] = 1;
	}

	void SeverHead(std::uint32_t a_formId, const RE::NiPoint3& a_away, bool a_mordhauHead)
	{
		if (!a_mordhauHead) {
			g_headless.erase(a_formId);  // (Skyrim's own head and neck wound: nothing of MORDHAU's is put on the body)
			return;
		}
		g_headless[a_formId] = 2;
		if (g_looseHeads.size() > 24) {
			g_looseHeads.erase(g_looseHeads.begin());
		}
		LooseHead head;
		head.id = a_formId;
		head.away = a_away;
		g_looseHeads.push_back(head);
	}

	void SetCastArm(bool a_on, float a_linger)
	{
		g_castArmSkyrim.store(a_on);
		g_castArmLinger.store(a_linger);
	}

	void SetLeftCasting(bool a_on)
	{
		g_leftCasting.store(a_on);
	}

	void SetSkyrimBodies(bool a_on)
	{
		g_skyrimBodies.store(a_on);
	}

	void DroppedWeapons(std::vector<DroppedWeapon>& a_out)
	{
		a_out.clear();
		if (g_playerRested && g_playerWeaponDown.load()) {
			a_out.push_back({ 0, g_playerWeaponAt });
		}
		for (const auto& fallen : g_fallen) {
			if (fallen.id != 0 && fallen.settled) {
				a_out.push_back({ fallen.id, RE::NiPoint3{ fallen.lying.m[3][0], fallen.lying.m[3][1], fallen.lying.m[3][2] } });
			}
		}
	}

	void DroppedAsSkyrim(std::uint32_t a_owner, bool a_shown, const RE::NiPoint3& a_at)
	{
		std::scoped_lock lock(g_droppedLock);
		if (a_shown) {
			g_droppedAsSkyrim[a_owner] = a_at;
			for (auto& fallen : g_fallen) {
				if (fallen.id == a_owner && a_owner != 0) {
					fallen.lying.m[3][0] = a_at.x, fallen.lying.m[3][1] = a_at.y, fallen.lying.m[3][2] = a_at.z;
				}
			}
		} else {
			g_droppedAsSkyrim.erase(a_owner);
		}
	}

	void SetMordhauHeld(const std::unordered_map<std::uint32_t, std::string>& a_models)
	{
		std::scoped_lock lock(g_mordhauHeldLock);
		g_mordhauHeld = a_models;
	}

	void SetDrawSmoothing(float a_perSecond, float a_most, bool a_measure)
	{
		g_drawSmooth.store(a_perSecond);
		g_drawSmoothMost.store(a_most);
		g_drawMeasure.store(a_measure);
	}

	void SetUnsheatheWait(float a_seconds)
	{
		g_unsheatheWait.store(a_seconds);
	}


	void ShowHitMarker(bool a_kill)
	{
		g_hitMarkerKill.store(a_kill);
		g_hitMarkerAt.store(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
	}

	void SetOutputScale(float a_scale)
	{
		g_outputScale.store(a_scale);
	}

	void SetSkyrimOwnsPlayer(bool a_owns)
	{
		g_skyrimOwns.store(a_owns);
	}

	void SetPlayerSkyrimWeapon(bool a_show)
	{
		g_skyrimWeaponWanted.store(a_show);
	}

	void SetAltSlide(float a_cm)
	{
		g_altSlide.store(a_cm);
	}

	void SetArms(bool a_follow, float a_stretch, bool a_gripInFist, bool a_halfTurn, bool a_altOnBlade)
	{
		g_altOnBlade.store(a_altOnBlade);
		g_armsFollow.store(a_follow);
		g_armsStretch.store(a_stretch);
		g_gripInFist.store(a_gripInFist);
		g_halfTurn.store(a_halfTurn);
	}


	void SetSkyrimHasBody(std::uint32_t a_id, bool a_on)
	{
		std::scoped_lock lock(g_skyrimHasLock);
		if (a_on) {
			g_skyrimHas.insert(a_id);
		} else {
			g_skyrimHas.erase(a_id);
		}
	}

	void SetMordhauBars(bool a_on)
	{
		g_mordhauBars.store(a_on);
	}

	void SetSkyrimShieldLook(bool a_on)
	{
		g_skyrimShieldLook.store(a_on);
	}
	void SetProps(const std::vector<Prop>& a_props)
	{
		g_props = a_props;
	}


	void PlaceEnemyWeapons()
	{
		PoseSkyrimBodiesNow();
		PlacePlayerWeaponThirdNow();
		PlaceEnemyWeaponsNow();
	}

	void SetStandinWeaponMordhau(bool a_on)
	{
		g_standinWeaponMordhau.store(a_on);
	}

	void SetThirdWeaponMordhau(bool a_on)
	{
		g_thirdWeaponMordhau.store(a_on);
	}

	void Install(const std::filesystem::path& a_cache, float a_sideOffset, float a_light, Pose (*a_pose)())
	{
		if (auto* ui = RE::UI::GetSingleton()) {
			ui->AddEventSink<RE::MenuOpenCloseEvent>(&g_wearWatch);
		}
		g_cache = a_cache;
		{
			std::error_code ec;
			g_builtIn = std::filesystem::exists(g_cache / "locomotion.txt", ec);
		}
		g_sideOffset = a_sideOffset;
		g_light = a_light;
		g_pose = a_pose;
		g_logMovement = std::filesystem::exists(a_cache / "log_movement.txt");
		if (!REL::Module::IsAE()) {
			logger::info("character: not an AE build; the MORDHAU character is not drawn");
			return;
		}
		// The game's own call to its world render, inside its frame function: the hook goes onto that call.
		// Where it is differs by game build (+0x85E on 1.7.104, +0x841 on 1.6.1170). It is looked for in two ways:
		//  - a call from that function to the world render itself (nobody else has hooked it);
		//  - failing that, a call instruction at the place known for this build. Another mod that hooks the same call
		//    has already pointed it at its own code - found on a user's 1.7.104 with the Anniversary content
		//    ("0 candidates"). The hook then goes after that mod's: ours calls theirs, theirs calls the game.
		const auto begin = REL::ID(36559).address();
		const auto target = REL::ID(107142).address();
		const auto isCall = [](std::uintptr_t a_at) { return *reinterpret_cast<const std::uint8_t*>(a_at) == 0xE8; };
		const auto calls = [&](std::uintptr_t a_at) {
			std::int32_t rel = 0;
			std::memcpy(&rel, reinterpret_cast<const std::uint8_t*>(a_at) + 1, 4);
			return isCall(a_at) && a_at + 5 + rel == target;
		};
		const auto known = begin + (REL::Module::get().version() >= REL::Version(1, 7, 0, 0) ? 0x85E : 0x841);
		if (std::filesystem::exists(a_cache / "prehook_test.txt") && calls(known)) {
			// (dev: stands in for another mod that got to the call first)
			static REL::Relocation<void(bool)> other;
			other = SKSE::GetTrampoline().write_call<5>(known, +[](bool a_unk) { other(a_unk); });
			logger::info("character: dev - the world render call was hooked by a stand-in for another mod first");
		}
		std::uintptr_t site = 0;
		if (calls(known)) {
			site = known;
		} else {
			int count = 0;
			for (std::uintptr_t at = begin; at < begin + 0x1800; ++at) {
				if (calls(at)) {
					site = at;
					++count;
				}
			}
			if (count == 1) {
				logger::info("character: on this game build the world render call is at +{:#x} of its function", site - begin);
			} else if (isCall(known)) {
				site = known;
				logger::info("character: the world render call at +{:#x} already goes to another mod's code ({} direct calls found): hooked after it", known - begin, count);
			} else {
				logger::warn("character: the world render call was not found on this game build ({} candidates, no call at +{:#x}); the MORDHAU character is not drawn", count, known - begin);
				return;
			}
		}
		RenderWorldHook::func = SKSE::GetTrampoline().write_call<5>(site, RenderWorldHook::thunk);
		logger::info("character: render hook installed, cache '{}', side offset {:.0f}", g_cache.string(), g_sideOffset);
	}
}
