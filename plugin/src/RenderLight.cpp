#include "CharacterRenderInternal.h"

namespace dm::CharacterRender
{
	inline namespace internal
	{
		const Person& PersonOf(RE::Actor* a_actor)
		{
			static const Person plain;
			if (!a_actor) {
				return plain;
			}
			const auto [it, fresh] = g_people.try_emplace(a_actor->GetFormID());
			auto& person = it->second;
			if (!fresh) {
				return person;
			}
			auto* base = a_actor->GetActorBase();
			if (!base) {
				return person;
			}
			// Skyrim's skin tone for this person against the light skin MORDHAU's maps are painted in
			const auto  tint = base->bodyTintColor;
			const float reference[3]{ 0.86f, 0.74f, 0.67f };
			const float tone[3]{ tint.red / 255.0f, tint.green / 255.0f, tint.blue / 255.0f };
			if (tone[0] + tone[1] + tone[2] > 0.15f) {
				for (int i = 0; i < 3; ++i) {
					person.skin[i] = std::clamp(tone[i] / reference[i], 0.22f, 1.15f);
				}
			}
			if (base->headRelatedData && base->headRelatedData->hairColor) {
				const auto colour = base->headRelatedData->hairColor->color;
				person.hair[0] = colour.red / 255.0f, person.hair[1] = colour.green / 255.0f, person.hair[2] = colour.blue / 255.0f;
			}
			if (base->headParts && base->numHeadParts > 0) {
				bool hair = false, beard = false;
				for (int i = 0; i < base->numHeadParts; ++i) {
					if (auto* part = base->headParts[i]) {
						// (a "hair" part that is the bald one has no model)
						hair = hair || (part->type == RE::BGSHeadPart::HeadPartType::kHair && !part->model.empty());
						beard = beard || part->type == RE::BGSHeadPart::HeadPartType::kFacialHair;
					}
				}
				person.hairAmount = hair ? 1.0f : 0.0f;
				person.beardAmount = beard && !base->IsFemale() ? 1.0f : 0.0f;
			}
			return person;
		}

		void ReadSceneLight()
		{
			SceneLight  out;
			const auto  lin = [](float c) { return std::pow(std::clamp(c, 0.0f, 4.0f), 2.2f); };
			auto*       player = RE::PlayerCharacter::GetSingleton();
			auto*       cell = player ? player->GetParentCell() : nullptr;
			const bool  interior = cell && cell->IsInteriorCell();
			auto*       sky = RE::Sky::GetSingleton();
			static bool logged = false;
			if (!interior && sky) {
				auto* shadowScene = RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];
				auto* sunLight = shadowScene ? shadowScene->GetRuntimeData().sunLight : nullptr;
				auto* light = sunLight ? sunLight->light.get() : nullptr;
				if (light) {
					// a directional light shines along its local X axis: the sun is the other way
					const RE::NiPoint3 along = light->world.rotate * RE::NiPoint3{ 1.0f, 0.0f, 0.0f };
					const float        len = along.Length();
					if (len > 1e-3f) {
						out.toSun[0] = -along.x / len, out.toSun[1] = -along.y / len, out.toSun[2] = -along.z / len;
					}
					const auto& data = light->GetLightRuntimeData();
					out.sun[0] = lin(data.diffuse.red), out.sun[1] = lin(data.diffuse.green), out.sun[2] = lin(data.diffuse.blue);
				}
				// directional ambient: [axis][+ / -]; up and down set the sky and ground light, the sides are shared out
				const auto& amb = sky->directionalAmbientColors;
				const float side[3]{ (amb[0][0].red + amb[0][1].red + amb[1][0].red + amb[1][1].red) * 0.25f,
					(amb[0][0].green + amb[0][1].green + amb[1][0].green + amb[1][1].green) * 0.25f,
					(amb[0][0].blue + amb[0][1].blue + amb[1][0].blue + amb[1][1].blue) * 0.25f };
				out.sky[0] = lin((amb[2][1].red + side[0]) * 0.5f), out.sky[1] = lin((amb[2][1].green + side[1]) * 0.5f), out.sky[2] = lin((amb[2][1].blue + side[2]) * 0.5f);
				out.ground[0] = lin((amb[2][0].red + side[0]) * 0.5f), out.ground[1] = lin((amb[2][0].green + side[1]) * 0.5f), out.ground[2] = lin((amb[2][0].blue + side[2]) * 0.5f);
				if (!logged) {
					logged = true;
					logger::info("character: lit by Skyrim's sun: to sun ({:.2f}, {:.2f}, {:.2f}), sun ({:.2f}, {:.2f}, {:.2f}), sky ({:.2f}, {:.2f}, {:.2f}), ground ({:.2f}, {:.2f}, {:.2f}) (linear)",
						out.toSun[0], out.toSun[1], out.toSun[2], out.sun[0], out.sun[1], out.sun[2], out.sky[0], out.sky[1], out.sky[2], out.ground[0], out.ground[1], out.ground[2]);
				}
			} else if (interior) {
				if (auto* lighting = cell->GetLighting()) {
					out.toSun[0] = 0.25f, out.toSun[1] = -0.25f, out.toSun[2] = 0.935f;   // indoors: mostly from above
					out.sun[0] = lin(lighting->directional.red / 255.0f), out.sun[1] = lin(lighting->directional.green / 255.0f), out.sun[2] = lin(lighting->directional.blue / 255.0f);
					const float ar = lighting->ambient.red / 255.0f, ag = lighting->ambient.green / 255.0f, ab = lighting->ambient.blue / 255.0f;
					out.sky[0] = lin(ar), out.sky[1] = lin(ag), out.sky[2] = lin(ab);
					out.ground[0] = lin(ar * 0.7f), out.ground[1] = lin(ag * 0.7f), out.ground[2] = lin(ab * 0.7f);
				}
			}
			// never pitch black (caves, bad data): a floor under the ambient light
			for (int i = 0; i < 3; ++i) {
				out.sky[i] = std::max(out.sky[i], 0.02f);
				out.ground[i] = std::max(out.ground[i], 0.012f);
			}
			g_sceneLight = out;
		}

		float SunReaching(RE::Actor* a_actor)
		{
			if (!a_actor) {
				return 1.0f;
			}
			auto&       shade = g_sunShade[a_actor->GetFormID()];
			const auto  now = std::chrono::steady_clock::now();
			const float dt = std::clamp(std::chrono::duration<float>(now - shade.last).count(), 0.0f, 0.2f);
			shade.last = now;
			if (--shade.countdown <= 0) {
				shade.countdown = 8;
				shade.target = 1.0f;
				auto* cell = a_actor->GetParentCell();
				auto* world = cell && !cell->IsInteriorCell() ? cell->GetbhkWorld() : nullptr;
				if (world && g_sceneLight.toSun[2] > 0.05f) {
					const float        scale = RE::bhkWorld::GetWorldScale();
					const RE::NiPoint3 from = a_actor->GetPosition() + RE::NiPoint3{ 0.0f, 0.0f, std::max(a_actor->GetHeight(), 60.0f) * 0.85f };
					const RE::NiPoint3 to = from + RE::NiPoint3{ g_sceneLight.toSun[0], g_sceneLight.toSun[1], g_sceneLight.toSun[2] } * 5000.0f;
					RE::bhkPickData    pick;
					pick.rayInput.from = RE::hkVector4(from.x * scale, from.y * scale, from.z * scale, 0.0f);
					pick.rayInput.to = RE::hkVector4(to.x * scale, to.y * scale, to.z * scale, 0.0f);
					RE::CFilter filter;
					a_actor->GetCollisionFilterInfo(filter);
					filter.SetCollisionLayer(RE::COL_LAYER::kLOS);   // what blocks sight blocks the sun; the character's own group is skipped
					pick.rayInput.filterInfo = filter;
					{
						RE::BSReadLockGuard lock(world->worldLock);
						world->PickObject(pick);
					}
					if (pick.rayOutput.HasHit()) {
						shade.target = 0.0f;
					}
				} else if (!cell || cell->IsInteriorCell()) {
					shade.target = 1.0f;
				} else if (g_sceneLight.toSun[2] <= 0.05f) {
					shade.target = 0.0f;   // sun below the horizon
				}
			}
			shade.lit += (shade.target - shade.lit) * std::clamp(dt * 6.0f, 0.0f, 1.0f);
			return shade.lit;
		}

		float BloodOf(std::uint32_t a_id)
		{
			std::scoped_lock lock(g_bloodLock);
			const auto       it = g_blood.find(a_id);
			return it == g_blood.end() ? 0.0f : it->second;
		}

		void FillLight(FrameConstants* a_fc, float a_sunReaching, const Person& a_person)
		{
			a_fc->blood[0] = std::clamp(g_bloodNow, 0.0f, 1.0f);
			a_fc->blood[1] = g_bloodSrv ? 1.0f : 0.0f;
			a_fc->blood[2] = g_outputScale.load();  // (sheets/builds.json enb_light_scale while an ENB is installed)
			a_fc->blood[3] = 0.0f;
			g_bloodNow = 0.0f;
			for (int i = 0; i < 3; ++i) {
				a_fc->skinTone[i] = a_person.skin[i];
				a_fc->hairColour[i] = a_person.hair[i];
				a_fc->eyeColour[i] = a_person.eyes[i];
			}
			a_fc->skinTone[3] = 1.0f;
			a_fc->hairColour[3] = a_person.hairAmount;
			a_fc->eyeColour[3] = a_person.beardAmount;
			for (int i = 0; i < 3; ++i) {
				a_fc->light[i] = g_sceneLight.toSun[i];
				a_fc->sunColour[i] = g_sceneLight.sun[i];
				a_fc->skyColour[i] = g_sceneLight.sky[i];
				a_fc->groundColour[i] = g_sceneLight.ground[i];
			}
			a_fc->light[3] = LightScale();
			a_fc->sunColour[3] = a_sunReaching;
			a_fc->skyColour[3] = a_fc->groundColour[3] = 1.0f;
		}
	}
}
