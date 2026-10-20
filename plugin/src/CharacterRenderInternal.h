// What the files of CharacterRender share among themselves: its types, its state and the prototypes of its functions.
// Not for use outside them - the public interface is CharacterRender.h.
#pragma once

#include "InventoryModels.h"
#include "Items.h"
#include "CharacterRender.h"
#include "Link.h"
#include "Pose.h"
#include "Quat.h"
#include "gen/Sheets.h"
#include <array>
#include <deque>
#include <map>
#include <unordered_set>
#include <DirectXMath.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxtk/DDSTextureLoader.h>
#include <chrono>
#include <mutex>
#include <thread>
#include <fstream>

namespace dm::CharacterRender
{
	inline namespace internal
	{
		using namespace DirectX;

		constexpr std::uint32_t kMaxBones = 192;

		constexpr float         kCmToUnits = 1.0f / 1.42875f;  // MORDHAU works in centimetres

		// Row-vector convention throughout (v * M), matrices sent as stored (row_major in the shader).
		constexpr char kShader[] = R"(
#pragma pack_matrix(row_major)
cbuffer Frame : register(b0)
{
	float4x4 world;      // MORDHAU component space -> camera-relative Skyrim world
	float4x4 viewProj;   // camera-relative world -> clip
	float4   light;      // xyz: direction to the sun (world), w: brightness
	float4   sunColour;  // Skyrim's own sun (or the cell's directional light indoors), linear; w: how much of it reaches this character (0 = in shadow)
	float4   skyColour;  // ambient light from above, linear
	float4   groundColour;  // ambient light from below, linear
	float4   skinTone;      // this person's skin, as a tint on MORDHAU's skin maps (1,1,1 = as painted)
	float4   hairColour;    // rgb; a: how much head hair (0 = bald)
	float4   eyeColour;     // rgb; a: how much beard
	float4   bloodInfo;     // x: how bloodied this character or weapon is (0..1); y: 1 when MORDHAU's blood pattern is bound; z: brightness of what is drawn (0 = as it is; under an ENB less)
};
cbuffer Bones : register(b1)
{
	float4x4 bones[192];
};
cbuffer Piece : register(b2)
{
	float4 tintRed;     // colour for the pattern mask's red, green and blue regions (1,1,1 = leave as is)
	float4 tintGreen;
	float4 tintBlue;
	float4 pieceFlags;  // x: 1 when this piece has a pattern mask, y: 1 when it carries the emblem, z: 1 with a normal map, w: 1 with a surface map
	float4 emblemRect;  // xy: centre, zw: size, in the piece's texture space
	float4 emblemTurn;  // x: cos, y: sin of its rotation
	float4 emblemColour1;
	float4 emblemColour2;
	float4 skinInfo;    // x: 0 clothing, 1 body skin, 2 head, 3 eyes; y: hair allowed; z: beard allowed; w: 1 = wood (a bow)
};
Texture2D    albedo : register(t0);
Texture2D    patternMask : register(t1);
Texture2D    emblemMap : register(t2);
Texture2D    normalMap : register(t3);
Texture2D    surfaceMap : register(t4);
Texture2D    hairLayer : register(t5);    // MORDHAU paints hair and beards onto the head: white = hair
Texture2D    beardLayer : register(t6);
Texture2D    bloodMap : register(t7);     // MORDHAU's blood grunge   // MORDHAU's roughness (r), metal (g), ambient occlusion (b)
SamplerState samp : register(s0);

struct VSIn { float3 pos : POSITION; float3 nrm : NORMAL; float2 uv : TEXCOORD0; uint4 ids : BLENDINDICES; float4 wts : BLENDWEIGHT; };
struct VSOut { float4 pos : SV_Position; float3 nrm : NORMAL; float2 uv : TEXCOORD0; float3 wpos : TEXCOORD1; };

VSOut VS(VSIn i)
{
	float4x4 skin = bones[i.ids.x] * i.wts.x + bones[i.ids.y] * i.wts.y + bones[i.ids.z] * i.wts.z + bones[i.ids.w] * i.wts.w;
	float4 p = mul(mul(float4(i.pos, 1.0), skin), world);
	VSOut o;
	o.pos = mul(p, viewProj);
	o.nrm = mul(mul(float4(i.nrm, 0.0), skin), world).xyz;
	o.uv = i.uv;
	o.wpos = p.xyz;
	return o;
}

// a Skyrim mesh as Skyrim stores it (position; uv; normal packed into bytes), drawn rigidly
struct VSSkyIn { float4 pos : POSITION; float2 uv : TEXCOORD0; float4 nrm : NORMAL; };
VSOut VSSkyrim(VSSkyIn i)
{
	float4 p = mul(float4(i.pos.xyz, 1.0), world);
	VSOut o;
	o.pos = mul(p, viewProj);
	o.nrm = mul(float4(i.nrm.xyz * 2.0 - 1.0, 0.0), world).xyz;
	o.uv = i.uv;
	o.wpos = p.xyz;
	return o;
}

float4 PS(VSOut i, bool front : SV_IsFrontFace) : SV_Target
{
	float4 texel = albedo.Sample(samp, i.uv);
	clip(texel.a - 0.333);   // cut-out parts (fur and feather cards, torn cloth), MORDHAU's own threshold
	// MORDHAU's maps hold display (gamma) colour, as Skyrim's own do; light is added up in linear and turned back
	float3 base = pow(max(texel.rgb, 0.0), 2.2);
	if (skinInfo.w > 0.5) {
		base *= float3(0.52, 0.30, 0.15) * 2.2;   // a bow: MORDHAU's bow texture is grey, its wood colour comes from the material
	}
	if (skinInfo.x > 2.5) {
		// an eye, drawn from its own texture coordinates (the iris is at the middle): white, iris, pupil
		float r = length(i.uv - 0.5);
		base = lerp(pow(max(eyeColour.rgb, 0.0), 2.2), float3(0.62, 0.60, 0.58), smoothstep(0.15, 0.17, r));
		base = lerp(float3(0.01, 0.01, 0.01), base, smoothstep(0.045, 0.06, r));
	} else if (skinInfo.x > 0.5) {
		// skin: this person's own tone; on the head, hair and beard are painted on in the person's hair colour
		base *= pow(max(skinTone.rgb, 0.0), 2.2);
		if (skinInfo.x > 1.5) {
			float3 hair = pow(max(hairColour.rgb, 0.0), 2.2);
			float  h = hairLayer.Sample(samp, i.uv).r * hairColour.a * skinInfo.y;
			float  b = beardLayer.Sample(samp, i.uv).r * eyeColour.a * skinInfo.z;
			base = lerp(base, hair * (0.55 + 0.45 * h), saturate(h * 1.3));
			base = lerp(base, hair * (0.55 + 0.45 * b), saturate(b * 1.3));
		}
	}
	if (pieceFlags.x > 0.5) {
		// MORDHAU's dye: its own colour map, tinted where the item's pattern mask says, with the profile's colours
		float3 m = patternMask.Sample(samp, i.uv).rgb;
		float3 t1 = pow(max(tintRed.rgb, 0.0), 2.2), t2 = pow(max(tintGreen.rgb, 0.0), 2.2), t3 = pow(max(tintBlue.rgb, 0.0), 2.2);
		base *= lerp(float3(1, 1, 1), t1, m.r) * lerp(float3(1, 1, 1), t2, m.g) * lerp(float3(1, 1, 1), t3, m.b);
	}
	if (pieceFlags.y > 0.5) {
		// the player's own emblem, where the item says it goes (MORDHAU's emblem texture: red = first colour, blue = second)
		float2 d = (i.uv - emblemRect.xy) / emblemRect.zw;
		float2 e = float2(d.x * emblemTurn.x - d.y * emblemTurn.y, d.x * emblemTurn.y + d.y * emblemTurn.x) + 0.5;
		if (e.x > 0.0 && e.x < 1.0 && e.y > 0.0 && e.y < 1.0) {
			float3 m = emblemMap.Sample(samp, e).rgb;
			base = lerp(base, pow(max(emblemColour1.rgb, 0.0), 2.2), saturate(m.r));
			base = lerp(base, pow(max(emblemColour2.rgb, 0.0), 2.2), saturate(m.b));
		}
	}
	float3 n = normalize(i.nrm);
	float3 v = normalize(-i.wpos);   // positions are relative to the camera
	if (dot(n, v) < 0.0) {
		n = -n;                       // cloth and hair cards are seen from both sides
	}
	if (pieceFlags.z > 0.5) {
		// MORDHAU's own surface detail (weave, rivets, dents): its two-channel normal map, on a frame built from the
		// surface itself (no stored tangents needed)
		float3 dp1 = ddx(i.wpos), dp2 = ddy(i.wpos);
		float2 du1 = ddx(i.uv), du2 = ddy(i.uv);
		float3 dp2perp = cross(dp2, n), dp1perp = cross(n, dp1);
		float3 t = dp2perp * du1.x + dp1perp * du2.x;
		float3 b = dp2perp * du1.y + dp1perp * du2.y;
		float  scale = rsqrt(max(max(dot(t, t), dot(b, b)), 1e-12));
		float2 xy = normalMap.Sample(samp, i.uv).rg * 2.0 - 1.0;
		float  z = sqrt(saturate(1.0 - dot(xy, xy)));
		n = normalize(t * scale * xy.x + b * scale * xy.y + n * z);
	}
	// what the surface is: MORDHAU's own roughness, metal and occlusion (plain cloth-like values without the map)
	float rough = 0.85, metal = 0.0, occlusion = 1.0;
	if (pieceFlags.w > 0.5) {
		float3 s = surfaceMap.Sample(samp, i.uv).rgb;
		rough = clamp(s.r, 0.08, 1.0);
		metal = s.g * 0.8;   // (kept a little short of full metal: without real reflections a pure metal turns black)
		occlusion = s.b;
	}
	if (bloodInfo.x > 0.004 && skinInfo.x < 2.5) {
		// blood: more of the pattern shows the more there is; dark, wet, not metallic
		float g1, g2;
		if (bloodInfo.y > 0.5) {
			g1 = dot(bloodMap.Sample(samp, i.uv * 2.0).rgb, float3(0.34, 0.33, 0.33));
			g2 = dot(bloodMap.Sample(samp, i.uv * 5.3 + 0.37).rgb, float3(0.34, 0.33, 0.33));
		} else {
			g1 = frac(sin(dot(floor(i.uv * 48.0), float2(12.9898, 78.233))) * 43758.5453);
			g2 = frac(sin(dot(floor(i.uv * 131.0), float2(39.346, 11.135))) * 24634.6345);
		}
		float pattern = g1 * 0.7 + g2 * 0.3;
		float edge = 1.0 - saturate(bloodInfo.x) * 0.85;
		float wet = smoothstep(edge, edge + 0.16, pattern);
		base = lerp(base, float3(0.17, 0.005, 0.004) * (0.55 + 0.45 * g2), wet * 0.92);
		rough = lerp(rough, 0.3, wet);
		metal = lerp(metal, 0.0, wet);
	}
	float3 diffuse = base * (1.0 - metal);
	float3 f0 = lerp(float3(0.04, 0.04, 0.04), base, metal);
	float3 l = light.xyz;
	float  nl = saturate(dot(n, l));
	float  nv = saturate(dot(n, v)) + 1e-4;
	// the sun: diffuse plus a highlight (GGX), by the usual metal/roughness rules
	float3 h = normalize(l + v);
	float  nh = saturate(dot(n, h)), vh = saturate(dot(v, h));
	float  a = rough * rough, a2 = a * a;
	float  dTerm = a2 / (3.14159 * pow(nh * nh * (a2 - 1.0) + 1.0, 2.0));
	float  k = (rough + 1.0) * (rough + 1.0) / 8.0;
	float  gTerm = (nl / (nl * (1.0 - k) + k)) * (nv / (nv * (1.0 - k) + k));
	float3 fTerm = f0 + (1.0 - f0) * pow(1.0 - vh, 5.0);
	float3 highlight = min(dTerm * gTerm * fTerm / max(4.0 * nl * nv, 1e-3), 8.0);
	float3 colour = (diffuse + highlight) * sunColour.rgb * (nl * sunColour.w);
	// the sky and the ground: soft light from all round, and what a shiny surface mirrors of them
	float3 around = lerp(groundColour.rgb, skyColour.rgb, n.z * 0.5 + 0.5);
	float3 r = reflect(-v, n);
	float3 mirrored = lerp(groundColour.rgb, skyColour.rgb, saturate(r.z * 0.5 + 0.5));
	mirrored = lerp(mirrored * (1.5 + metal), around, rough);   // (a real sky has brighter patches than its average)
	float3 fAmbient = f0 + (max(float3(1.0 - rough, 1.0 - rough, 1.0 - rough), f0) - f0) * pow(1.0 - nv, 5.0);
	colour += (diffuse * around + mirrored * fAmbient) * occlusion;
	return float4(pow(max(colour, 0.0), 1.0 / 2.2) * light.w * (bloodInfo.z > 0.0 ? bloodInfo.z : 1.0), 1.0);
}

// MORDHAU-style bars: position.xy is already in clip space, the colour is in the vertex normal
VSOut VSHud(VSIn i)
{
	VSOut o;
	o.pos = float4(i.pos.xy, 0.5, 1.0);
	o.nrm = i.nrm;
	o.uv = i.uv;
	o.wpos = float3(0, 0, 0);
	return o;
}
float4 PSHud(VSOut i) : SV_Target
{
	return float4(i.nrm, 1.0);
}
// MORDHAU's voice / emote menu from its window: as PSHudTex, on a dark backing that fades out at the edges (so the
// list reads over a bright sky)
float4 PSHudPicture(VSOut i) : SV_Target
{
	float3 c = albedo.Sample(samp, i.uv).rgb;
	float  edge = saturate(min(min(i.uv.x, 1.0 - i.uv.x) * 6.0, min(i.uv.y, 1.0 - i.uv.y) * 10.0));
	return float4(c, max(saturate(max(c.r, max(c.g, c.b)) * 8.0), 0.5 * edge));
}
// MORDHAU's own HUD, as captured from its window over a blacked-out view: black is see-through
float4 PSHudTex(VSOut i) : SV_Target
{
	float3 c = albedo.Sample(samp, i.uv).rgb;
	return float4(c, saturate(max(c.r, max(c.g, c.b)) * 8.0));
}

)";

		struct Vertex
		{
			float        pos[3];
			float        nrm[3];
			float        uv[2];
			std::uint8_t ids[4];
			std::uint8_t wts[4];
		};

		static_assert(sizeof(Vertex) == 40);

		struct Bone
		{
			std::string name;
			int         parent = -1;
			XMFLOAT4    restRot{};
			XMFLOAT3    restPos{};
			XMFLOAT4X4  invRest{};  // inverse of the rest pose in component space
		};

		struct Section
		{
			std::uint32_t             first = 0, count = 0;
			ID3D11ShaderResourceView* srv = nullptr;
			ID3D11ShaderResourceView* mask = nullptr;
			float                     tints[9]{ 1, 1, 1, 1, 1, 1, 1, 1, 1 };
			ID3D11ShaderResourceView* emblem = nullptr;
			float                     emblemValues[11]{};  // centre xy, size xy, rotation (turns), colour 1 rgb, colour 2 rgb
			ID3D11ShaderResourceView* normal = nullptr;
			ID3D11ShaderResourceView* surface = nullptr;
			bool                      weapon = false;  // part of the held weapon (rides the weapon slot, not the body)
			ID3D11ShaderResourceView* hair = nullptr;
			ID3D11ShaderResourceView* beard = nullptr;
			float                     skin[4]{};       // kind (0 clothing, 1 body skin, 2 head, 3 eyes), hair allowed, beard allowed
			float                     zMin = 0.0f, zMax = 0.0f;  // how far the section reaches along the model's z (a weapon prop: grip to tip, cm)
		};

		struct FullClip
		{
			float              length = 0.0f;
			std::int32_t       frames = 0, bones = 0;
			std::vector<float> data;  // frames x bones x 7: rotation xyzw, translation xyz (local)
		};

		struct FrameConstants
		{
			XMFLOAT4X4 world;
			XMFLOAT4X4 viewProj;
			float      light[4];
			float      sunColour[4];
			float      skyColour[4];
			float      groundColour[4];
			float      skinTone[4];
			float      hairColour[4];
			float      eyeColour[4];
			float      blood[4];
		};

		// What a person looks like under the clothes, from Skyrim: skin tone, hair colour, whether there is hair and a
		// beard. Read once per character.
		struct Person
		{
			float skin[3]{ 1.0f, 1.0f, 1.0f };   // tint on MORDHAU's (light) skin maps
			float hair[3]{ 0.25f, 0.17f, 0.10f };
			float eyes[3]{ 0.22f, 0.14f, 0.08f };
			float hairAmount = 1.0f, beardAmount = 0.0f;
		};

		inline std::unordered_map<std::uint32_t, Person> g_people;

		const Person& PersonOf(RE::Actor* a_actor);

		// Skyrim's own light where the player is: the sun (direction and colour) and the ambient light from above and
		// below outdoors, the cell's lighting indoors. Colours come out linear (Skyrim's are display values).
		struct SceneLight
		{
			float toSun[3]{ 0.3f, -0.4f, 0.85f };
			float sun[3]{ 0.9f, 0.85f, 0.75f };
			float sky[3]{ 0.25f, 0.27f, 0.30f };
			float ground[3]{ 0.10f, 0.09f, 0.08f };
		};

		inline SceneLight g_sceneLight;

		void ReadSceneLight();

		float LightScale();  // the dev setting for overall brightness (defined below)

		// Is this character standing in the sun? One ray from its chest towards the sun through Skyrim's own collision
		// world, a few times a second, eased, so it darkens under a roof or behind a wall like Skyrim's own objects do
		// (which are shadowed properly; ours has no shadow map). Indoors there is no sun to block.
		struct SunShade
		{
			float                                 lit = 1.0f;   // 0 = in shadow, 1 = in the sun (eased)
			float                                 target = 1.0f;
			int                                   countdown = 0;
			std::chrono::steady_clock::time_point last = std::chrono::steady_clock::now();
		};

		inline std::unordered_map<std::uint32_t, SunShade> g_sunShade;

		float SunReaching(RE::Actor* a_actor);

		// blood: how much each character carries (0 = the player), and what the next thing drawn shows
		inline std::unordered_map<std::uint32_t, float> g_blood;

		inline std::mutex                               g_bloodLock;

		inline float                                    g_bloodNow = 0.0f;   // used by the next FillLight, then back to 0

		inline ID3D11ShaderResourceView*                g_bloodSrv = nullptr;

		float BloodOf(std::uint32_t a_id);

		inline std::atomic<float> g_outputScale{ 0.0f };         // brightness of everything this renderer draws (0 = unchanged)

		void FillLight(FrameConstants* a_fc, float a_sunReaching, const Person& a_person);

		inline std::filesystem::path g_cache;

		inline float                 g_sideOffset = 0.0f;

		inline float                 g_light = 1.0f;

		inline float LightScale() { return g_light; }
		inline Pose (*g_pose)() = nullptr;

		inline bool                     g_failed = false, g_ready = false;

		inline std::vector<Bone>        g_bones;

		inline std::vector<Section>     g_sections;

		inline ID3D11Buffer*            g_vb = nullptr;

		inline ID3D11Buffer*            g_ib = nullptr;

		inline ID3D11Buffer*            g_frameCb = nullptr;

		inline ID3D11Buffer*            g_boneCb = nullptr;

		inline ID3D11Buffer*            g_pieceCb = nullptr;

		inline ID3D11VertexShader*      g_vs = nullptr;

		inline ID3D11PixelShader*       g_ps = nullptr;

		inline ID3D11InputLayout*       g_layout = nullptr;

		inline ID3D11SamplerState*      g_sampler = nullptr;

		inline ID3D11RasterizerState*   g_raster = nullptr;

		inline ID3D11BlendState*        g_blend = nullptr;

		inline ID3D11DepthStencilState* g_depthLess = nullptr;

		inline ID3D11DepthStencilState* g_depthGreater = nullptr;

		inline ID3D11DepthStencilState* g_depthOff = nullptr;

		inline ID3D11VertexShader*      g_hudVs = nullptr;

		inline ID3D11PixelShader*       g_hudPs = nullptr;


		inline ID3D11Buffer*            g_hudVb = nullptr;

		inline ID3D11PixelShader*       g_hudTexPs = nullptr;

		inline ID3D11VertexShader*      g_skyVs = nullptr;        // Skyrim meshes (the player's Skyrim weapon)

		inline ID3DBlob*                g_skyVsBlob = nullptr;    // (kept: input layouts are made per vertex format)

		inline std::unordered_map<std::uint64_t, ID3D11InputLayout*> g_skyLayouts;

		inline ID3D11PixelShader*       g_hudPicturePs = nullptr;

		inline ID3D11BlendState*        g_blendAlpha = nullptr;

		// Parts of MORDHAU's own HUD, read from its window (which sits behind Skyrim) about 25 times a second on a
		// worker thread. The MORDHAU side blacks out its 3D view, so only the HUD is in the picture. Three parts:
		// [0] the strip at the bottom centre with the health / stamina bars and their numbers (not shown since
		// sheets/builds.json mordhau_bars is 0: Skyrim's own bars are), [1] the middle of the screen with MORDHAU's
		// crosshair and its attack-direction ("240") indicator, [2] MORDHAU's voice / emote menu, top left.
		struct HudPart
		{
			float left, right, top, bottom;  // of MORDHAU's window
		};

		// (the third's place: sheets/menus.json)
		constexpr int     kHudPartCount = 3;

		constexpr HudPart kHudParts[kHudPartCount]{ { 0.285f, 0.71f, 0.89f, 0.955f }, { 0.44f, 0.56f, 0.393f, 0.607f },
			{ sheets::kMenuPart[0].left, sheets::kMenuPart[0].right, sheets::kMenuPart[0].top, sheets::kMenuPart[0].bottom } };  // of the picture MORDHAU actually draws

		struct HudCapture
		{
			std::mutex                            lock;
			std::vector<std::uint8_t>             pixels[kHudPartCount];  // BGRA, top row first
			int                                   width[kHudPartCount]{}, height[kHudPartCount]{};
			std::uint32_t                         version = 0;
			std::chrono::steady_clock::time_point when{};
			bool                                  started = false;
		};

		inline HudCapture                g_hud;

		inline ID3D11Texture2D*          g_hudTex[kHudPartCount]{};

		inline ID3D11ShaderResourceView* g_hudSrv[kHudPartCount]{};

		inline int                       g_hudTexW[kHudPartCount]{}, g_hudTexH[kHudPartCount]{};

		inline std::uint32_t             g_hudTexVersion = 0;

		void HudCaptureLoop();

		bool UpdateHudTexture(ID3D11Device* a_device, ID3D11DeviceContext* a_context);

		inline std::unordered_map<std::string, std::shared_ptr<FullClip>> g_clips;

		inline int                                                        g_headBone = -1;    // first person folds this bone and all under it away

		inline std::vector<bool>                                          g_underHead;

		inline XMFLOAT3                                                   g_headAt{};         // the head joint this frame, character space

		inline int                                                        g_weaponBone = -1;  // the hand's weapon bone (where the weapon rides without a live stream)

		// A look MORDHAU's stand-ins are drawn with (cache/enemies/<n>), loaded the first time one is needed.
		struct EnemyLook
		{
			bool                 tried = false, ready = false;
			std::vector<Bone>    bones;
			std::vector<Section> sections;
			ID3D11Buffer*        vb = nullptr;
			ID3D11Buffer*        ib = nullptr;
			int                  weaponBone = -1, headBone = -1;
		};

		// Props in Skyrim's world (Combat.cpp owns them): baked models by folder name, loaded when first drawn
		inline std::vector<Prop> g_props;

		// the old built-in mode's clips are in the cache (a development machine): without them, and while MORDHAU is not
		// linked, nothing of ours is drawn - Skyrim shows its own characters and plays as plain Skyrim
		inline bool g_builtIn = false;

		inline bool g_unlinked = false;

		struct PropModel
		{
			bool                               tried = false, ready = false;
			EnemyLook                          look;
		};

		inline std::unordered_map<std::string, PropModel> g_propModels;

		inline std::unordered_map<std::uint32_t, std::string> g_standinWeapons;

		constexpr int kMaxLooks = 128;  // sheets/looks.json

		inline int           g_nakedLook = 0;  // the look of class "naked"

		inline EnemyLook     g_looks[kMaxLooks];

		// One stand-in being drawn: its pose is eased between the samples MORDHAU sends.
		struct EnemyShown
		{
			std::uint32_t         id = 0;
			std::vector<XMFLOAT4> rot;
			std::vector<XMFLOAT3> pos;
			std::vector<float>    last;                // the pose as last received, to see whether a new one came with this frame
			bool                  wasChanged = false;
		};

		float GroundBelow(RE::Actor* a_near, float a_x, float a_y, float a_z);

		inline std::unordered_map<std::uint32_t, int> g_headless;  // characters without a head: 1 crushed, 2 cut off

		// a head that was cut off: thrown, then lying in Skyrim's world
		struct LooseHead
		{
			std::uint32_t id = 0;        // whose
			int           look = 0;      // 0 = not in the world yet (waits for the body to be drawn once more)
			RE::NiPoint3  at, speed;     // Skyrim world, units and units/s
			RE::NiPoint3  away;
			float         spin = 0.0f, ground = 0.0f, size = 1.0f, sun = 1.0f;
			bool          landed = false;
			Person        person{};
		};

		inline std::vector<LooseHead> g_looseHeads;

		inline EnemyShown                        g_shown[Link::kGhosts];

		inline std::atomic<bool>                 g_playerWeaponDown{ false };

		inline RE::NiPoint3                      g_playerWeaponAt;

		// a stand-in's dropped weapon: where it lies in Skyrim's world
		struct Fallen
		{
			std::uint32_t                         id = 0;
			bool                                  settled = false;
			std::chrono::steady_clock::time_point since{};
			XMFLOAT4X4                            lying{};
			std::string                           model;        // its MORDHAU model (cache/props)
			bool                                  seen = false;  // its owner was drawn with it on the ground this frame
		};

		inline Fallen g_fallen[Link::kGhosts];

		// Weapons left lying whose owner is dead or gone, or which the player swapped away: they stay in the world.
		struct LeftWeapon
		{
			std::string model;
			XMFLOAT4X4  lying{};
		};

		inline std::vector<LeftWeapon> g_leftWeapons;

		inline std::mutex              g_leftLock;

		// dropped weapons that have come to rest (for Combat, which sets Skyrim's own object down there), and the
		// owners for whom that object is there
		inline bool                                              g_playerRested = false;

		inline std::unordered_map<std::uint32_t, RE::NiPoint3>   g_droppedAsSkyrim;

		inline std::mutex                                        g_droppedLock;  // (its own lock: AsSkyrim is asked from inside g_leftLock too -

		bool AsSkyrim(std::uint32_t a_owner);

		inline std::unordered_set<std::uint32_t> g_hiddenActors;  // Skyrim characters whose own model is hidden while their stand-in is drawn

		// Where each drawn stand-in's weapon hand is (Skyrim world), from the last frame drawn.
		struct EnemyWeapon
		{
			std::uint32_t id = 0;   // the Skyrim character; 0 = nothing held
			RE::NiPoint3  grip, along, thin;
		};

		inline EnemyWeapon g_enemyWeapons[Link::kGhosts];

		// After Skyrim's animation for the frame and before it is drawn (called from the camera update, see Combat.cpp):
		// each mirrored enemy's own Skyrim weapon (the model on its WEAPON node) is made
		// visible again and moved into its stand-in's weapon hand, pointing the way MORDHAU's weapon points. The rest
		// of the Skyrim body stays hidden. Skyrim's own animation puts the node back every frame, so nothing sticks.
		inline std::unordered_map<std::uint32_t, std::string> g_mordhauHeld;  // who holds a MORDHAU weapon as a Skyrim item, and its model

		inline std::mutex                                     g_mordhauHeldLock;

		// Fought characters that are Skyrim's alone right now (yielding, fleeing): told by Combat every frame.
		inline std::mutex                        g_skyrimHasLock;

		inline std::unordered_set<std::uint32_t> g_skyrimHas;

		bool                              SkyrimHasBody(std::uint32_t a_id);

		std::string MordhauHeldBy(std::uint32_t a_id);

		inline float             g_arrowTurn = 1.5707963f;       // an arrow's model lies along its +Z: turned to lie along the way it flies

		inline std::atomic<float> g_unsheatheWait{ 3.0f };       // sheets/builds.json unsheathe_wait
		inline std::atomic<float> g_drawSmooth{ 0.0f };          // sheets/builds.json standin_draw_smooth (per second; 0 = off)
		inline std::atomic<float> g_drawSmoothMost{ 40.0f };     // ... standin_draw_smooth_most
		inline std::atomic<bool>  g_drawMeasure{ false };        // dev measure=1: drawn and physics steps logged
		inline std::atomic<bool>  g_leftCasting{ false };        // the player's left hand casts: its arm is Skyrim's (SetLeftCasting)
		inline std::atomic<bool>  g_castArmSkyrim{ true };       // anyone else's left hand casting: its arm Skyrim's too (SetCastArm)
		inline std::atomic<float> g_castArmLinger{ 0.5f };       // ...until this long after the cast

		// a bone of the left arm or hand (shoulder to fingertips), by its Skyrim node name
		inline bool LeftArmBone(std::string_view a_node)
		{
			if (a_node.rfind("NPC L ", 0) != 0) {
				return false;
			}
			for (const char* part : { "UpperArm", "Upperarm", "Forearm", "ForeArm", "Hand", "Finger", "Magic" }) {
				if (a_node.find(part) != std::string_view::npos) {
					return true;
				}
			}
			return false;
		}

		inline std::atomic<std::int64_t> g_hitMarkerAt{ 0 };     // steady-clock milliseconds of the last hit to mark (0: none)

		inline std::atomic<bool>         g_hitMarkerKill{ false };


		inline std::atomic<bool> g_skyrimOwns{ false };          // a Skyrim scene has the player's body (sitting, riding, a scripted scene...)

		inline std::atomic<bool> g_skyrimWeaponWanted{ false };  // the player's weapon is shown as the equipped Skyrim one

		inline EnemyWeapon       g_playerSkyrim;                 // where it goes (id 1 = valid), from the last frame drawn

		inline bool              g_playerSkyrimFirst = false;    // ... on the first-person skeleton

		inline RE::NiPoint3      g_lastCam, g_lastPlayerAt;      // the camera and the player where the last frame was drawn

		constexpr float   kCmToUnitsEarly = 1.0f / 1.42875f;

		inline std::atomic_bool g_halfTurn{ true };  // sheets/builds.json skyrim_weapon_half_turn

		inline std::atomic_bool g_standinWeaponMordhau{ true };  // sheets/builds.json standin_weapon_as_mordhau

		void PlaceWeaponNode(RE::NiNode* node, const EnemyWeapon& held, bool note);

		void PlaceEnemyWeaponsNow();

		XMMATRIX LocalMatrix(const float* a_rot, const float* a_pos);

		bool Compile(const char* a_entry, const char* a_target, ID3DBlob** a_out);

		bool LoadMesh(ID3D11Device* a_device, const std::filesystem::path& a_dir, std::vector<Bone>& a_bones, std::vector<Section>& a_sections, ID3D11Buffer** a_vb, ID3D11Buffer** a_ib, int* a_weaponBone, int* a_headBone);

		bool Load(ID3D11Device* a_device);

		std::shared_ptr<FullClip> GetClip(const std::string& a_name);

		// ---- movement (MORDHAU's own layering, read from its blends at the first start) ----
		// lower: the shared leg blend, by direction and gait (0 walk .. 1 run). upper: the weapon's upper-body blend, by
		// direction and its speed value. still / jump / fall / land: single clips.
		struct Sample
		{
			std::shared_ptr<FullClip> clip;
			float                     direction = 0.0f, axis = 0.0f, travel = 0.0f;  // travel: cm/s the clip is authored for
		};

		inline std::vector<Sample>       g_lower, g_upper;

		inline std::shared_ptr<FullClip> g_still, g_fall;

		inline std::vector<bool>         g_upperBone;  // bones the upper layer owns

		inline bool                      g_locomotionLoaded = false;

		inline bool                      g_logMovement = false;

		inline float                     g_phase = 0.0f;    // 0..1 through the walk cycle, shared by all movement clips

		inline float                     g_idleTime = 0.0f;  // seconds, for clips that just loop

		inline float                     g_walkSpeed = 175.0f, g_runSpeed = 280.0f;  // cm/s, replaced by the measured clip speeds

		inline std::chrono::steady_clock::time_point g_lastFrame = std::chrono::steady_clock::now();

		struct LocalPose
		{
			std::vector<XMFLOAT4> rot;
			std::vector<XMFLOAT3> pos;
		};

		void RestPose(LocalPose& a_out);

		void SampleClip(const FullClip& a_clip, float a_time, bool a_loop, LocalPose& a_out);

		void Blend(LocalPose& a_io, const LocalPose& a_other, float a_t, const std::vector<bool>* a_mask = nullptr);

		float AngleBetween(float a, float b);

		bool SampleRow(const std::vector<Sample>& a_samples, float a_axis, float a_direction, float a_phase, LocalPose& a_out, float* a_travel);

		void LoadLocomotion();

		// The pose MORDHAU itself is producing right now, when it runs alongside (see Link.h).
		// MORDHAU's newest pose as last taken from the link. One buffer for everyone who takes it: a second reader with
		// its own buffer got the new poses instead of the body (the body then moved only now and then, in jerks,
		// and the weapon, which did get them, left the hand).
		inline std::vector<float> g_liveRaw;



		bool LivePose(LocalPose& a_io, float a_dt);

		// Skin matrices for this frame: inverse rest pose, then the posed bone, both in MORDHAU's component space.
		inline std::atomic<bool>     g_skyrimBodies{ false };

		inline std::vector<XMMATRIX> g_playerComp;   // the player's MORDHAU pose as last built (character space)

		inline bool                  g_playerCompValid = false;

		void BuildBones(const Pose& a_pose, XMFLOAT4X4* a_out);

		struct SavedState
		{
			ID3D11RenderTargetView*   rtv[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
			ID3D11DepthStencilView*   dsv = nullptr;
			ID3D11BlendState*         blend = nullptr;
			FLOAT                     factor[4]{};
			UINT                      mask = 0, stencil = 0, vpCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
			ID3D11RasterizerState*    raster = nullptr;
			ID3D11DepthStencilState*  depth = nullptr;
			D3D11_VIEWPORT            vps[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
			D3D11_PRIMITIVE_TOPOLOGY  topo{};
			ID3D11InputLayout*        layout = nullptr;
			ID3D11Buffer*             vb = nullptr;
			ID3D11Buffer*             ib = nullptr;
			DXGI_FORMAT               ibFormat{};
			UINT                      stride = 0, offset = 0, ibOffset = 0;
			ID3D11VertexShader*       vs = nullptr;
			ID3D11PixelShader*        ps = nullptr;
			ID3D11Buffer*             vsCb[2]{};
			ID3D11Buffer*             psCb[3]{};
			ID3D11ShaderResourceView* srv[7]{};
			ID3D11SamplerState*       sampler = nullptr;

			void Save(ID3D11DeviceContext* c)
			{
				c->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtv, &dsv);
				c->OMGetBlendState(&blend, factor, &mask);
				c->RSGetState(&raster);
				c->OMGetDepthStencilState(&depth, &stencil);
				c->RSGetViewports(&vpCount, vps);
				c->IAGetPrimitiveTopology(&topo);
				c->IAGetInputLayout(&layout);
				c->IAGetVertexBuffers(0, 1, &vb, &stride, &offset);
				c->IAGetIndexBuffer(&ib, &ibFormat, &ibOffset);
				c->VSGetShader(&vs, nullptr, nullptr);
				c->PSGetShader(&ps, nullptr, nullptr);
				c->VSGetConstantBuffers(0, 2, vsCb);
				c->PSGetConstantBuffers(0, 3, psCb);
				c->PSGetShaderResources(0, 7, srv);
				c->PSGetSamplers(0, 1, &sampler);
			}

			void Restore(ID3D11DeviceContext* c)
			{
				c->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtv, dsv);
				c->OMSetBlendState(blend, factor, mask);
				c->RSSetState(raster);
				c->OMSetDepthStencilState(depth, stencil);
				c->RSSetViewports(vpCount, vps);
				c->IASetPrimitiveTopology(topo);
				c->IASetInputLayout(layout);
				c->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
				c->IASetIndexBuffer(ib, ibFormat, ibOffset);
				c->VSSetShader(vs, nullptr, 0);
				c->PSSetShader(ps, nullptr, 0);
				c->VSSetConstantBuffers(0, 2, vsCb);
				c->PSSetConstantBuffers(0, 3, psCb);
				c->PSSetShaderResources(0, 7, srv);
				c->PSSetSamplers(0, 1, &sampler);
				const auto release = [](IUnknown* p) {
					if (p) {
						p->Release();
					}
				};
				for (auto* p : rtv) {
					release(p);
				}
				release(dsv), release(blend), release(raster), release(depth), release(layout), release(vb), release(ib), release(vs), release(ps);
				release(vsCb[0]), release(vsCb[1]), release(psCb[0]), release(psCb[1]), release(psCb[2]), release(srv[0]), release(srv[1]), release(srv[2]), release(srv[3]), release(srv[4]), release(srv[5]), release(srv[6]), release(sampler);
			}
		};

		// ---- Everyone is a MORDHAU character. A humanoid that is not being fought has no stand-in inside MORDHAU, so
		// its pose comes from its own Skyrim animation, carried onto MORDHAU's skeleton bone by bone (sheets/bone_map.json,
		// read the other way round): each mapped MORDHAU bone turns as its Skyrim bone turns, measured from rest poses
		// that are first lined up by the direction each bone points in. Fingers and face stay at rest.
		struct RetargetRow
		{
			int           bone = -1;       // MORDHAU bone
			const char*   node = "";       // Skyrim node
			RE::NiMatrix3 post;            // MORDHAU bone (Skyrim-handed) = Skyrim bone (model space) x post
		};

		struct Retarget
		{
			bool                     tried = false, ready = false;
			std::vector<RetargetRow> rows;
			std::vector<int>         rowOfBone;
			int                      hips = -1;
			const char*              hipsNode = "";
			float                    hipsScale = 1.0f;
		};

		inline Retarget g_retarget;

		// where Skyrim carries a weapon that is put away
		constexpr const char* kSheathNodes[]{ "WeaponBack", "WeaponSword", "WeaponAxe", "WeaponMace", "WeaponDagger" };

		inline dm::RestPose g_skyrimRest;

		// How far a posed Skyrim body stands off MORDHAU's own hip height (a draugr's longer legs): what is drawn from
		// MORDHAU's character space for it - its weapon - is moved by as much, or it leaves the hand.
		inline std::mutex                                      g_bodyLiftLock;

		inline std::unordered_map<std::uint32_t, RE::NiPoint3> g_bodyLift;


		inline std::atomic<bool> g_skyrimShieldLook{ true }; // sheets/builds.json skyrim_shield_look


		RE::NiMatrix3 MirrorX(RE::NiMatrix3 a_m);

		int BoneNamed(const std::vector<Bone>& a_bones, const char* a_name);

		void PrepareRetarget(const std::vector<Bone>& a_bones, RE::Actor* a_anyHuman);

		// The Skyrim nodes of one character, looked up once (a lookup walks its whole skeleton).
		struct FigureNodes
		{
			RE::NiAVObject*              root = nullptr;
			std::vector<RE::NiAVObject*> nodes;
		};

		inline std::unordered_map<std::uint32_t, FigureNodes> g_figureNodes;

		bool BuildFromSkyrim(RE::Actor* a_actor, RE::NiAVObject* a_root, const std::vector<Bone>& a_bones, XMFLOAT4X4* a_out);

		void PoseSkyrimBody(RE::Actor* a_actor, const std::vector<XMMATRIX>& a_comp);

		void PoseSkyrimBodiesNow();

		// The baked outfits (cache/enemies/looks.txt, written at the first start from sheets/looks.json): number and
		// which Skyrim people wear it. Read once.
		enum LookClass { kCivilian = 0, kRobe, kLight, kHeavy };

		struct LookInfo
		{
			int         number = 0;
			int         lookClass = kCivilian;
			std::vector<std::string> wears;  // not empty: the standard outfit for everyone whose body armour's name contains one of these (lower case)
			std::vector<std::string> who;  // not empty: the outfit of everyone whose NAME contains one of these (lower case)
			std::string creature;  // not empty: not an outfit but one of MORDHAU's Demon Horde creatures ("Troll")
		};

		inline std::vector<LookInfo> g_lookList;

		int LookCount();

		int OutfitFor(RE::TESObjectARMO* a_armour, std::uint32_t a_key, const char** a_kind = nullptr);

		void ListArmourMatches();

		// Which outfit a character wears: by what it has on its body in Skyrim (clothes, a robe, light or heavy armour),
		// and within that always the same one for the same character. Looked up now and then (it walks the inventory).
		// The Skyrim body spawned for a Demon Horde creature (Combat.cpp), and which creature it is
		inline std::uint32_t g_creatureId = 0;

		inline std::string   g_creatureName;

		int LookFor(RE::Actor* a_actor);

		// What the player wears in Skyrim decides the player's MORDHAU outfit: change armour in Skyrim's inventory and
		// the character changes. Each Skyrim body armour always gives the same outfit of its kind (clothes, robe, light,
		// heavy: sheets/looks.json); with nothing on the body the player is the mercenary as made in MORDHAU.
		// Returns the outfit to draw instead of the mercenary's own, or null.
		// When to look at what the player wears: once at the start, then only while Skyrim's inventory is open and for
		// five seconds after it closes (told by Skyrim's own menu events; nothing is polled in between).
		class WearWatch : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (a_event && a_event->menuName == RE::InventoryMenu::MENU_NAME) {
					open = a_event->opening;
					until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
				} else if (a_event && !a_event->opening && a_event->menuName == RE::LoadingMenu::MENU_NAME) {
					until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
				}
				return RE::BSEventNotifyControl::kContinue;
			}
			bool                                  open = false;
			std::chrono::steady_clock::time_point until = std::chrono::steady_clock::now() + std::chrono::seconds(5);  // (the start counts as one look)
		};

		inline WearWatch g_wearWatch;

		EnemyLook* PlayerOutfit(ID3D11Device* a_device, RE::PlayerCharacter* a_player);

		EnemyLook* PlayerOutfit(ID3D11Device* a_device, RE::PlayerCharacter* a_player);

		// MORDHAU's HUD strip goes onto the finished picture, right before it is shown.
		using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);

		inline PresentFn                             g_present = nullptr;

		inline bool                                  g_presentHooked = false;

		inline ID3D11Device*                         g_presentDevice = nullptr;

		inline ID3D11DeviceContext*                  g_presentContext = nullptr;

		inline std::chrono::steady_clock::time_point g_hudWantedAt{};

		inline std::atomic<int> g_menuShown{ 0 };  // MORDHAU's own voice / emote menu is open: its part of MORDHAU's window is shown

		inline std::atomic_bool g_mordhauBars{ false };  // MORDHAU's health / stamina strip drawn into Skyrim's picture (sheets/builds.json mordhau_bars)

		void DrawHudOnBackBuffer(IDXGISwapChain* a_chain);

		HRESULT STDMETHODCALLTYPE PresentHook(IDXGISwapChain* a_chain, UINT a_sync, UINT a_flags);

		void EnsurePresentHook(RE::BSGraphics::Renderer* a_renderer, ID3D11Device* a_device, ID3D11DeviceContext* a_context);

		void PlaceDroppedPlayerWeapon(RE::PlayerCharacter* player, const bool skyrimBody, const RE::NiPoint3& cam, const XMMATRIX& world, std::vector<XMFLOAT4X4>& boneMatrices, XMMATRIX& placement, const Link::State& link, const std::string*& playerDropModel, XMMATRIX& playerDropWorld);

		void PlaceSkyrimWeaponInHand(RE::PlayerCharacter* player, const bool skyrimBody, std::vector<XMFLOAT4X4>& boneMatrices, XMMATRIX& placement, const Link::State& link, bool& skyrimWeapon, XMMATRIX& skyWeaponWorld);

		void PickHandWeaponModel(ID3D11Device* device, std::vector<XMFLOAT4X4>& boneMatrices, const Link::State& link, PropModel*& handModel, XMMATRIX& handWeapon, bool& skyrimWeapon);

		void LoadBloodPattern(ID3D11Device* device);

		void DrawSkyrimWeapon(RE::PlayerCharacter* player, RE::NiAVObject* root, ID3D11Device* device, ID3D11DeviceContext* context, const XMMATRIX& viewProj, const Link::State& link, bool& skyrimWeapon, XMMATRIX& skyWeaponWorld, D3D11_MAPPED_SUBRESOURCE& map);

		// One character near the player that may be drawn as a MORDHAU fighter (slot: its stand-in, -1 none).
		struct Figure
		{
			RE::Actor* actor;
			float      distance;
			int        slot;
		};

		void ListNearFigures(RE::PlayerCharacter* player, const Link::State& link, std::vector<Figure>& figures, const int lookCount);


		void DrawHitMarker(ID3D11DeviceContext* context, D3D11_TEXTURE2D_DESC& sd, D3D11_MAPPED_SUBRESOURCE& map, const UINT stride, const UINT offset);

		void DrawFallbackBars(RE::PlayerCharacter* player, ID3D11DeviceContext* context, D3D11_TEXTURE2D_DESC& sd, const Link::State& link, D3D11_MAPPED_SUBRESOURCE& map, const UINT stride, const UINT offset, bool& hudStreamed);

		void DrawInFrame();

		inline std::atomic<bool> g_thirdWeaponMordhau{ true };  // sheets/builds.json third_person_weapon_mordhau

		// Third person, Skyrim's own body: the weapon in the player's hand as MORDHAU holds it - turned as MORDHAU's
		// weapon is, and set where MORDHAU's weapon is relative to its right hand (the Skyrim hand stands in for that
		// hand: the two skeletons' arms are not the same length, so MORDHAU's place in the world would miss the hand).
		inline std::atomic_bool   g_armsFollow{ true };     // sheets/builds.json arms_to_mordhau_hands

		inline std::atomic<float> g_armsStretch{ 1.15f };   // sheets/builds.json arms_stretch_max

		inline std::atomic<float> g_altSlide{ 0.0f };       // cm; sheets/builds.json alt_slide_<weapon>_<view>, for the weapon now held

		inline std::atomic_bool   g_altOnBlade{ true };     // sheets/builds.json alt_grip_hands_on_blade

		inline std::atomic_bool   g_gripInFist{ true };     // sheets/builds.json skyrim_weapon_grip_in_fist

		inline std::atomic_bool   g_handOntoWeapon{ true };  // sheets/builds.json right_hand_onto_weapon

		inline std::atomic_bool   g_leftOntoWeapon{ true };  // sheets/builds.json left_hand_onto_weapon

		// Both hands on the weapon (left_hand_onto_weapon): what ArmsToMordhauHands needs to lay the Skyrim left fist on
		// the weapon as it is drawn. Directions and places in Skyrim's world units, from the right wrist.
		struct LeftOnWeapon
		{
			RE::NiPoint3 lineFromRight;  // a point of the drawn weapon's line, from where the right hand's place is
			RE::NiPoint3 along;          // the weapon's direction
			RE::NiPoint3 fist;           // the Skyrim left fist from the left wrist, as the left hand is turned now
			float        snapped = 0.0f;  // (out: how far the left hand was moved onto the line, for the log)
		};

		RE::NiMatrix3 TurnFromTo(RE::NiPoint3 a_from, RE::NiPoint3 a_to);

		void ArmsToMordhauHands(RE::NiAVObject* a_body, const XMMATRIX& a_toWorld, float a_k, const RE::NiPoint3& a_rightShift, LeftOnWeapon* a_left);

		void PlacePlayerWeaponThirdNow();

		// Right after Skyrim renders its 3D scene and before its post-processing, so fog, tone mapping and
		// anti-aliasing treat the character like any Skyrim object (technique from SkyCraft, chasmlol, MIT).
		struct RenderWorldHook
		{
			static void thunk(bool a_unk)
			{
				func(a_unk);
				DrawInFrame();
				dm::InventoryModels::Update(g_cache);  // (sheets/builds.json inventory_mordhau_models; this runs in menus too)
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};
	}
}
