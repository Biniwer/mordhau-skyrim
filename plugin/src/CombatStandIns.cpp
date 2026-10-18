#include "CombatInternal.h"

namespace dm::Combat
{
	inline namespace internal
	{
		bool IsMirrored(const RE::Actor* a_actor)
		{
			const auto link = Link::Current();
			if (!a_actor || !link.live) {
				return false;
			}
			// only once MORDHAU has actually given it a stand-in (otherwise its attacks would be decided by nobody)
			for (const auto& g : link.ghosts) {
				if (g.id != 0 && g.id == a_actor->GetFormID()) {
					return true;
				}
			}
			return false;
		}

		bool IsFought(const RE::Actor* a_actor)
		{
			if (!IsMirrored(a_actor)) {
				return false;
			}
			for (const auto& m : g_mirrored) {
				if (m.id == a_actor->GetFormID()) {
					return !m.dummy;
				}
			}
			return true;
		}

		// How tall a creature's body stands (its own bounds; Skyrim units).
		float CreatureTall(RE::Actor* a_actor)
		{
			return (a_actor->GetBoundMax().z - a_actor->GetBoundMin().z) * std::max(a_actor->GetScale(), 0.1f);
		}

		// Skyrim's own stagger, away from whoever caused it.
		bool StaggerFrom(RE::Actor* a_target, RE::Actor* a_from, float a_magnitude)
		{
			const RE::NiPoint3 to = a_from->GetPosition() - a_target->GetPosition();
			float              turn = (std::atan2(to.x, to.y) - a_target->GetAngleZ()) / (2.0f * std::numbers::pi_v<float>);
			turn -= std::floor(turn);
			a_target->SetGraphVariableFloat("staggerDirection", turn);
			a_target->SetGraphVariableFloat("staggerMagnitude", a_magnitude);
			return a_target->NotifyAnimationGraph("staggerStart");
		}

		// A creature's stand-in (sheets/builds.json creature_standin): a passive MORDHAU character at the creature's front,
		// there only so that MORDHAU judges the player's swings and kicks at it. The creature itself stays Skyrim's.
		void MirrorCreature(Mirrored& m, RE::PlayerCharacter* a_player, const Link::State& a_link, const float kCm, const float facing, std::vector<Link::Ghost>& ghosts)
		{
			auto actor = m.handle.get();
			if (!actor) {
				return;
			}
			const RE::NiPoint3 lo = actor->GetBoundMin(), hi = actor->GetBoundMax();
			const float        halfLength = std::max(std::abs(lo.y), std::abs(hi.y)) * std::max(actor->GetScale(), 0.1f);
			const float        turned = actor->GetAngleZ();
			const float        front = std::max(halfLength - BuildValue("creature_standin_front_inset"), 0.0f);
			const RE::NiPoint3 to = actor->GetPosition() + RE::NiPoint3{ std::sin(turned), std::cos(turned), 0.0f } * front - a_player->GetPosition();
			Link::Ghost        g;
			g.id = m.id;
			g.forward = (to.x * std::sin(facing) + to.y * std::cos(facing)) * kCm;
			g.right = (to.x * std::cos(facing) - to.y * std::sin(facing)) * kCm;
			g.up = to.z * kCm;
			g.yaw = (turned - facing) * 57.29578f;
			g.passive = true;
			g.dummy = true;
			if (a_player->GetHeight() > 1.0f) {
				g.size = std::clamp(CreatureTall(actor.get()) / a_player->GetHeight(), BuildValue("creature_standin_smallest"), 1.25f);
			}
			{
				auto* av = actor->AsActorValueOwner();
				g.health = std::clamp(av->GetActorValue(RE::ActorValue::kHealth) / std::max(av->GetPermanentActorValue(RE::ActorValue::kHealth), 1.0f), 0.0f, 1.0f) * 100.0f;
			}
			// (its body is Skyrim's alone: never posed from the stand-in, never walked after it)
			CharacterRender::SetSkyrimHasBody(m.id, true);
			g_skyrimHasBody.insert(m.id);
			ghosts.push_back(g);
			for (const auto& gs : a_link.ghosts) {
				if (gs.id != m.id) {
					continue;
				}
				if (!m.hitsKnown || gs.hits < m.seenHits) {
					m.hitsKnown = true;
					m.seenHits = gs.hits;
				} else if (gs.hits != m.seenHits) {
					m.seenHits = gs.hits;
					const bool kick = std::string_view(a_link.motion).find("Kick") != std::string_view::npos || g_lastRelease.find("Kick") != std::string::npos;
					// (one hit per swing: the blade against the whole body may have counted this swing already)
					if (g_creatureHitRelease[m.id] == g_releaseNumber) {
						logger::info("live: MORDHAU says the player's {} hit {}'s stand-in - already counted by the blade for this swing", a_link.motion, actor->GetDisplayFullName());
						continue;
					}
					g_creatureHitRelease[m.id] = g_releaseNumber;
					if (kick) {
						const bool taken = SuperKick(a_player, actor.get()) || StaggerFrom(actor.get(), a_player, BuildValue("kick_stagger_magnitude"));
						logger::info("live: MORDHAU says the player's kick hit {}'s stand-in: staggered ({})", actor->GetDisplayFullName(),
							taken ? "taken" : "not taken: no stagger in its animations, or it is in one already");
						if (BuildValue("hit_marker") > 0.5f) {
							CharacterRender::ShowHitMarker(false);
						}
						if (!actor->IsDead() && !actor->IsInCombat() && actor->IsHostileToActor(a_player)) {
							actor->StartCombat(a_player);
						}
					} else {
						ActiveSwing swing;
						swing.swing = g_liveSwing ? g_liveSwing : &sheets::kSwings[0];
						swing.family = FamilyOf(a_player);
						if (!swing.family) {
							swing.family = &sheets::kWeaponFamilies[0];
						}
						logger::info("live: MORDHAU says the player's {} hit {}'s stand-in ({:.0f} of 100 there): a creature's clean hit", a_link.motion, actor->GetDisplayFullName(), gs.damage);
						ApplyHit(a_player, actor.get(), swing, 1.0f, Progress("clean_hit") * CreatureScale(actor.get()));
					}
				}
			}
		}

		void SpawnDemon(RE::PlayerCharacter* a_player)
		{
			auto*       bandits = RE::TESForm::LookupByID<RE::TESFaction>(0x0001BCC0);
			RE::TESNPC* base = nullptr;
			RE::TESNPC* byName = nullptr;
			for (auto* npc : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESNPC>()) {
				if (npc && !npc->IsUnique() && !npc->IsFemale() && npc->GetName() && std::string_view(npc->GetName()) == "Bandit Chief") {
					byName = byName ? byName : npc;
					if (bandits && npc->IsInFaction(bandits)) {
						base = npc;
						break;
					}
				}
			}
			base = base ? base : byName;
			if (!base) {
				logger::info("demon: no body found for it in Skyrim (no bandit chief)");
				return;
			}
			auto  placed = a_player->PlaceObjectAtMe(base, false);
			auto* demon = placed ? placed->As<RE::Actor>() : nullptr;
			if (!demon) {
				logger::info("demon: Skyrim did not place its body");
				return;
			}
			const float        facing = a_player->GetAngleZ();
			const RE::NiPoint3 at = a_player->GetPosition() + RE::NiPoint3{ std::sin(facing) * 900.0f, std::cos(facing) * 900.0f, 30.0f };
			demon->SetPosition(at, true);
			demon->SetScale(2.0f);
			auto* av = demon->AsActorValueOwner();
			av->SetActorValue(RE::ActorValue::kHealth, 900.0f);
			av->SetActorValue(RE::ActorValue::kAggression, 2.0f);
			av->SetActorValue(RE::ActorValue::kConfidence, 4.0f);
			g_demonId = demon->GetFormID();
			CharacterRender::SetCreature(g_demonId, "Troll");
			logger::info("demon: a Troll is in Skyrim (body {:08X} from {:08X} {}, in the bandit faction: {})", g_demonId, base->GetFormID(), base->GetName(),
				bandits && base->IsInFaction(bandits));
		}

		// What the creature is up to in Skyrim's world, for the log (test runs): where, whom it is fighting, its health
		void ReportDemon(RE::PlayerCharacter* a_player)
		{
			auto* form = g_demonId ? RE::TESForm::LookupByID<RE::Actor>(g_demonId) : nullptr;
			if (!form) {
				return;
			}
			std::string target = "nobody";
			if (auto handle = form->GetActorRuntimeData().currentCombatTarget; handle) {
				if (auto who = handle.get()) {
					target = who->GetDisplayFullName();
				}
			}
			auto* av = form->AsActorValueOwner();
			logger::info("demon: {} at {:.0f} units from the player, in combat {}, target {}, health {:.0f}, 3D {}", form->IsDead() ? "DEAD" : "alive",
				form->GetPosition().GetDistance(a_player->GetPosition()), form->IsInCombat(), target, av->GetActorValue(RE::ActorValue::kHealth), form->Is3DLoaded());
		}

		// One fought character for this frame: where it stands goes to MORDHAU (or MORDHAU's AI walks it), its stand-in's hits on the player and the player's hits on it are applied, and what its stand-in may do is set.
		// Hybrid movement: while MORDHAU's AI has this one's stand-in free, the Skyrim character is walked after it, a step at a time - never through a wall, never over a ledge - and turned the way the stand-in faces.
		void WalkAfterStandIn(Mirrored& m, RE::PlayerCharacter* a_player, const Link::State& a_link, const float kCm, RE::NiPointer<RE::Actor>& actor, Link::Ghost& g)
		{
		{
			// Hybrid movement (sheets/builds.json hybrid_movement): MORDHAU's AI has set this one's stand-in free
			// near its opponent. The Skyrim character is walked after it - a step at a time, never through a wall
			// (a line-of-sight ray at chest height) and never over a ledge (the ground under the step may differ by
			// hybrid_step_units at most) - and turned the way the stand-in faces. Skyrim's own AI is kept from
			// walking it meanwhile.
			const Link::State::GhostMove* move = nullptr;
			for (const auto& candidate : a_link.moves) {
				if (candidate.id == m.id && candidate.free) {
					move = &candidate;
				}
			}
			const bool hybrid = move && BuildValue("hybrid_movement") > 0.5f && !a_player->IsDead() && !(RE::UI::GetSingleton() && RE::UI::GetSingleton()->GameIsPaused()) && !actor->IsDead() && !actor->IsInRagdollState() && !actor->AsActorState()->IsBleedingOut() &&
				!g_skyrimHasBody.contains(m.id);  // (Skyrim's alone - yielding, fleeing: Skyrim walks it; sheets/builds.json yield_after_seconds)
			auto&      flags = actor->GetActorRuntimeData().boolFlags;
			static std::unordered_set<std::uint32_t> walked;
			if (hybrid) {
				if (walked.insert(m.id).second) {
					logger::info("hybrid: {} is moved by MORDHAU's AI now", actor->GetDisplayFullName());
				}
				flags.set(RE::Actor::BOOL_FLAGS::kMovementBlocked);
				const RE::NiPoint3 here = actor->GetPosition();
				RE::NiPoint3       want{ move->right / kCm, move->forward / kCm, here.z };  // (told in Skyrim's own world coordinates)
				static std::unordered_map<std::uint32_t, Clock::time_point> lastStep;
				auto&        when = lastStep[m.id];
				const float  dt = std::clamp(Seconds(when), 0.0f, 0.1f);
				when = Clock::now();
				// (sheets/builds.json hybrid_smooth: MORDHAU's frames are of uneven length and not in step with Skyrim's,
				// so a stand-in walking at an even pace is reported in uneven steps, and each report was shown for exactly
				// one Skyrim frame - a jerky walk. Every report carries MORDHAU's own game time. From two reports the
				// stand-in's true speed is known; the character is walked to where the stand-in is at the MORDHAU time
				// that belongs to this Skyrim frame.)
				struct Track
				{
					RE::NiPoint3 place, speed;             // the last report (Skyrim units) and the speed between the last two (units/s)
					float        yaw = 0.0f, turn = 0.0f;  // the same for its heading (degrees, degrees/s)
					float        clock = 0.0f;             // MORDHAU's game time of that report
					bool         known = false;
					RE::NiPoint3 mine;                     // where the plugin last put the character (x, y)
					bool         mineKnown = false;
				};
				static std::unordered_map<std::uint32_t, Track> tracks;
				static const auto began = Clock::now();
				static float      ahead = 0.0f;       // Skyrim's clock less MORDHAU's, eased (the two run at the same rate)
				static bool       aheadKnown = false;
				static float      aheadClock = -1.0f;
				const bool  smooth = BuildValue("hybrid_smooth") > 0.5f;
				const float skyNow = Seconds(began);
				float       headingNow = move->yaw;
				if (smooth) {
					if (a_link.clock != aheadClock) {  // (a new report: once per report, whoever is asked first)
						aheadClock = a_link.clock;
						const float sample = skyNow - a_link.clock;
						ahead = aheadKnown && std::abs(sample - ahead) < 0.25f ? ahead + (sample - ahead) * 0.03f : sample;
						aheadKnown = true;
					}
					auto&              track = tracks[m.id];
					const RE::NiPoint3 told{ want.x, want.y, 0.0f };
					const float        passed = a_link.clock - track.clock;  // (MORDHAU's time between the two reports)
					if (!track.known || passed < 0.0f || passed > 0.25f) {
						track = { told, {}, move->yaw, 0.0f, a_link.clock, true };  // (the first report, or after a gap: no speed yet)
					} else if (passed > 0.002f) {
						const RE::NiPoint3 fresh = (told - track.place) / passed;
						float              turned = move->yaw - track.yaw;
						turned -= 360.0f * std::round(turned / 360.0f);
						// (a jump - the script set the stand-in somewhere - is not a speed)
						track.speed = fresh.Length() < 900.0f ? fresh : RE::NiPoint3{};
						track.turn = std::abs(turned / passed) < 720.0f ? turned / passed : 0.0f;
						track.place = told, track.yaw = move->yaw, track.clock = a_link.clock;
					}
					const float lead = std::clamp((skyNow - ahead) - track.clock, -0.03f, BuildValue("hybrid_lead_seconds"));
					want.x = track.place.x + track.speed.x * lead;
					want.y = track.place.y + track.speed.y * lead;
					headingNow = track.yaw + track.turn * lead;
				}
				if (g_dev.measure) {
					// (dev, measure=1: every frame's numbers of the walk, for looking at them afterwards)
					static std::ofstream raw(std::filesystem::temp_directory_path() / "mordhauskyrim_steps.csv");
					static const auto    began = Clock::now();
					raw << Seconds(began) << ',' << m.id << ',' << move->right / kCm << ',' << move->forward / kCm << ',' << want.x << ',' << want.y << ',' << here.x << ',' << here.y << ',' << a_player->GetPosition().x << ','
						<< a_player->GetPosition().y << ',' << move->yaw << ',' << a_link.clock << '\n';
				}
				// (the walk goes on from where the PLUGIN last put the character, not from where Skyrim has it now: between two
				// frames Skyrim shifts it a little by itself (measured: 0.7 units in the middle, against steps of 3), and
				// reckoned from there each frame's step took that up - the character stepped back against its own walk in
				// one frame of nine)
				// (sheets/builds.json hybrid_player_gap: never put into the player - Skyrim pushes it out again, it falls behind its
				// stand-in and the script sets the stand-in back: a fighter that lunges and is snapped back, over and over)
				{
					const RE::NiPoint3 fromPlayer{ want.x - a_player->GetPosition().x, want.y - a_player->GetPosition().y, 0.0f };
					const float        gap = BuildValue("hybrid_player_gap");
					if (const float nearNow = fromPlayer.Length(); nearNow < gap && nearNow > 1.0f) {
						want.x = a_player->GetPosition().x + fromPlayer.x * (gap / nearNow);
						want.y = a_player->GetPosition().y + fromPlayer.y * (gap / nearNow);
					}
				}
				RE::NiPoint3 from = here;
				if (smooth) {
					const auto& track = tracks[m.id];
					if (track.mineKnown && std::abs(track.mine.x - here.x) < 10.0f && std::abs(track.mine.y - here.y) < 10.0f) {  // (further off: Skyrim really has it elsewhere)
						from.x = track.mine.x, from.y = track.mine.y;
					}
				}
				RE::NiPoint3 step = want - from;
				float        length = step.Length();
				const float reachNow = std::max(700.0f * dt, 2.0f);  // (MORDHAU's sprint is about 540 cm/s = 380 units/s; some slack to catch up)
				if (length > reachNow) {
					step = step * (reachNow / length);
					length = reachNow;
				}
				static std::unordered_map<std::uint32_t, int> refused;
				if (length > 0.5f) {
					auto* cell = actor->GetParentCell();
					auto* world = cell ? cell->GetbhkWorld() : nullptr;
					bool  ok = world != nullptr;
					float ground = here.z;
					if (world) {
						const float scale = RE::bhkWorld::GetWorldScale();
						const auto  ray = [&](const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, float& a_fraction, RE::COL_LAYER a_layer = RE::COL_LAYER::kLOS) {
							RE::bhkPickData pick;
							pick.rayInput.from = RE::hkVector4(a_from.x * scale, a_from.y * scale, a_from.z * scale, 0.0f);
							pick.rayInput.to = RE::hkVector4(a_to.x * scale, a_to.y * scale, a_to.z * scale, 0.0f);
							RE::CFilter filter;
							actor->GetCollisionFilterInfo(filter);
							filter.SetCollisionLayer(a_layer);
							pick.rayInput.filterInfo = filter;
							RE::BSReadLockGuard lock(world->worldLock);
							world->PickObject(pick);
							a_fraction = pick.rayOutput.hitFraction;
							return pick.rayOutput.HasHit();
						};
						const RE::NiPoint3 next = from + step;
						const RE::NiPoint3 chest{ 0.0f, 0.0f, actor->GetHeight() * 0.55f };
						const RE::NiPoint3 ahead = step * ((length + 18.0f) / length);  // (and a body's half width beyond)
						float              fraction = 1.0f;
						int                why = 0;  // (dev, measure=1: what refused the step - 1 something in the way, 2 the ground's height, 3 no ground)
						if (ray(from + chest, from + ahead + chest, fraction)) {
							ok = false;  // a wall, a pillar, a door
							why = 1;
						} else {
							const float allowed = BuildValue("hybrid_step_units");
							if (ray(next + RE::NiPoint3{ 0.0f, 0.0f, allowed + 30.0f }, next - RE::NiPoint3{ 0.0f, 0.0f, allowed + 30.0f }, fraction)) {
								ground = next.z + allowed + 30.0f - fraction * 2.0f * (allowed + 30.0f);
								ok = std::abs(ground - here.z) <= allowed;
								why = ok ? 0 : 2;
							} else {
								ok = false;  // nothing to stand on there
								why = 3;
							}
						}
						if (g_dev.measure && !ok) {
							static int told = 0;
							if (++told <= 400) {
								const RE::NiPoint3 toPlayer = a_player->GetPosition() - from;
								const float        away = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);
								// (the same ground probe with other layers, and a longer one: which of them finds what the first did not)
								const RE::NiPoint3 top = next + RE::NiPoint3{ 0.0f, 0.0f, 60.0f }, bottom = next - RE::NiPoint3{ 0.0f, 0.0f, 60.0f };
								float              f1 = 1.0f, f2 = 1.0f, f3 = 1.0f, f4 = 1.0f;
								const bool         h1 = ray(top, bottom, f1, RE::COL_LAYER::kCharController), h2 = ray(top, bottom, f2, RE::COL_LAYER::kUnidentified);
								const bool         h3 = ray(next + RE::NiPoint3{ 0.0f, 0.0f, 150.0f }, next - RE::NiPoint3{ 0.0f, 0.0f, 300.0f }, f3), h4 = ray(here + RE::NiPoint3{ 0.0f, 0.0f, 60.0f }, here - RE::NiPoint3{ 0.0f, 0.0f, 60.0f }, f4);
								logger::info("refused: why={} away={:.0f} towards={:.2f} step={:.1f} fraction={:.2f} ground={:.1f} here={:.1f} mine={} at {:.0f},{:.0f} charctl={} {:.1f} unid={} {:.1f} long={} {:.1f} underfeet={} {:.1f}", why, away,
									away > 1.0f ? (step.x * toPlayer.x + step.y * toPlayer.y) / (length * away) : 0.0f, length, fraction, ground, here.z, tracks[m.id].mineKnown ? 1 : 0, here.x, here.y,
									h1, 60.0f - f1 * 120.0f, h2, 60.0f - f2 * 120.0f, h3, 150.0f - f3 * 450.0f, h4, 60.0f - f4 * 120.0f);
							}
						}
						if (ok) {
							// (the height follows the ground at a rate instead of jumping to it: a small jolt at every step on uneven ground)
							const float climb = BuildValue("hybrid_height_rate") * dt;
							const float height = smooth ? here.z + std::clamp(ground - here.z, -climb, climb) : ground;
							actor->SetPosition(RE::NiPoint3{ next.x, next.y, height }, true);
							if (smooth) {
								tracks[m.id].mine = RE::NiPoint3{ next.x, next.y, 0.0f }, tracks[m.id].mineKnown = true;
							}
							refused[m.id] = 0;
						} else {
							g.refused = true;  // (told to the script: the stand-in is set back and held for a moment)
							if (smooth) {
								tracks[m.id].mineKnown = false;
							}
						}
						if (!ok && ++refused[m.id] == 30) {
							logger::info("hybrid: {}'s steps after its stand-in are refused (a wall or a ledge in the way)", actor->GetDisplayFullName());
						}
					}
				}
				if (g_dev.measure) {
					// (dev, measure=1: how even the steps are - once a second per walked character)
					struct Steps
					{
						int               frames = 0, still = 0;
						float             sum = 0.0f, squares = 0.0f, most = 0.0f;
						Clock::time_point since = Clock::now();
					};
					static std::unordered_map<std::uint32_t, Steps> measured;
					auto&       steps = measured[m.id];
					const RE::NiPoint3 went = actor->GetPosition() - here;
					const float        moved = std::sqrt(went.x * went.x + went.y * went.y);
					const float        speedNow = dt > 0.0005f ? moved / dt : 0.0f;
					++steps.frames, steps.sum += speedNow, steps.squares += speedNow * speedNow, steps.most = std::max(steps.most, speedNow);
					steps.still += moved < 0.05f ? 1 : 0;
					if (Seconds(steps.since) > 1.0f) {
						const float mean = steps.sum / static_cast<float>(steps.frames);
						const float spread = std::sqrt(std::max(steps.squares / static_cast<float>(steps.frames) - mean * mean, 0.0f));
						logger::info("steps: {} walked {} frames, speed {:.0f} units/s on average, spread {:.0f} ({:.0f}% of it), fastest {:.0f}, {} frames without a step; smoothing {}", actor->GetDisplayFullName(),
							steps.frames, mean, spread, mean > 1.0f ? 100.0f * spread / mean : 0.0f, steps.most, steps.still, smooth ? "on" : "off");
						steps = {};
					}
				}
				actor->SetHeading(headingNow / 57.29578f);
			} else if (walked.erase(m.id) > 0) {
				flags.reset(RE::Actor::BOOL_FLAGS::kMovementBlocked);
				logger::info("hybrid: {} is moved by Skyrim again", actor->GetDisplayFullName());
			}
		}
		}

		// A character that Skyrim has fleeing, kneeling or out of the fight is Skyrim's alone for that time: its stand-in holds its attacks and its body keeps Skyrim's own animation.
		void LeaveYieldingBodyToSkyrim(Mirrored& m, RE::NiPointer<RE::Actor>& actor, Link::Ghost& g)
		{
		{
			// Skyrim's own retreat: at low health its AI can make a character run (slowly, wounded) or kneel. A MORDHAU
			// fighter does neither - and its stand-in went on attacking from a body that was walking away. So the
			// retreat is called off (confidence in the character's own data set to "never flees", the fleeing
			// state cleared), and for as long as Skyrim still has it running or kneeling, the stand-in holds its
			// attacks (sheets/builds.json fought_no_retreat).
			auto*      controller = actor->GetActorRuntimeData().combatController;
			const bool fleeing = controller && controller->state && controller->state->isFleeing;
			const bool kneeling = actor->AsActorState()->IsBleedingOut();
			if (BuildValue("fought_no_retreat") > 0.5f) {
				if (auto* base = actor->GetActorBase()) {
					auto& ai = base->aiData;
					if (!(ai.confidence3 && !ai.confidence2 && !ai.confidence1)) {
						ai.confidence1 = false, ai.confidence2 = false, ai.confidence3 = true;  // 4 = foolhardy
						logger::info("fought: {}'s kind never flees now (confidence in its own data set to 4)", actor->GetDisplayFullName());
					}
				}
				if (fleeing) {
					controller->state->isFleeing = false;
				}
			}
			// (yielding: Skyrim has taken it out of the fight - no longer in combat - though it stays an enemy by faction)
			// (only once it has lasted: Skyrim drops and retakes its combat state for moments during a fight - sheets/builds.json yield_after_seconds)
			static std::unordered_map<std::uint32_t, Clock::time_point> inCombatAt;
			if (actor->IsInCombat() || !inCombatAt.contains(m.id)) {
				inCombatAt[m.id] = Clock::now();
			}
			const bool yielded = !actor->IsInCombat() && Seconds(inCombatAt[m.id]) > BuildValue("yield_after_seconds");
			const bool skyrimHas = fleeing || kneeling || yielded;
			CharacterRender::SetSkyrimHasBody(actor->GetFormID(), skyrimHas);
			if (skyrimHas != g_skyrimHasBody.contains(m.id)) {
				logger::info("fought: {}'s body is {} ({})", actor->GetDisplayFullName(), skyrimHas ? "Skyrim's alone now" : "in MORDHAU's movement again",
					kneeling ? "kneeling" : fleeing ? "running away" : yielded ? "out of combat" : "fighting");
				if (skyrimHas) {
					g_skyrimHasBody.insert(m.id);
				} else {
					g_skyrimHasBody.erase(m.id);
				}
			}
			if (yielded) {
				g.passive = true;
			}
			if (fleeing || kneeling) {
				g.passive = true;
				static Clock::time_point told{};
				if (Seconds(told) > 3.0f) {
					told = Clock::now();
					logger::info("fought: {} is {} in Skyrim (health {:.0f} of {:.0f}): its stand-in holds its attacks", actor->GetDisplayFullName(), kneeling ? "kneeling" : "running away",
						actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth), actor->AsActorValueOwner()->GetPermanentActorValue(RE::ActorValue::kHealth));
				}
			}
		}
		}

		// What MORDHAU decided between this character's stand-in and the others this frame: its hits on the player, the player's hits on it, and their gore.
		void ApplyStandInVerdicts(Mirrored& m, RE::PlayerCharacter* a_player, const Link::State& a_link, RE::NiPointer<RE::Actor>& actor)
		{
		for (const auto& gs : a_link.ghosts) {
			if (gs.id != m.id) {
				continue;
			}
			// its stand-in's swing begins: Skyrim's own swing sound too (its Skyrim attack animation, which carries one, does not play)
			if (gs.stage == 1 && m.swingStage != 1 && actor) {
				SoundMix::PlaySkyrimSwing(actor.get(), false);
			}
			// its swing ended while the player held a parry and the player was not hit: parried - Skyrim's block sound, by
			// what the player parried with (MORDHAU tells only who was hit, not who parried)
			if (m.swingStage == 1 && gs.stage != 1 && a_link.kind == 2 && a_link.playerHits == m.playerHitsSeen) {
				SoundMix::PlaySkyrimBlock(a_player);
			}
			m.playerHitsSeen = a_link.playerHits;
			m.swingStage = gs.stage;
			if (!m.hitsKnown) {
				m.seenOther = gs.otherHits;
			} else if (gs.otherHits < m.seenOther) {
				m.seenOther = gs.otherHits;
			} else if (gs.otherHits != m.seenOther) {
				m.seenOther = gs.otherHits;
				// another stand-in's blade reached this one in MORDHAU: that Skyrim character's hit lands
				RE::Actor* attacker = nullptr;
				for (auto& other : g_mirrored) {
					if (other.id == gs.otherBy) {
						attacker = other.handle.get().get();
					}
				}
				// (only between two who are really against each other: allies' stand-ins also clip each other in MORDHAU)
				if (attacker && attacker != actor.get() && !attacker->IsDead() && (attacker->IsHostileToActor(actor.get()) || actor->IsHostileToActor(attacker))) {
					ActiveSwing swing;
					swing.swing = &sheets::kSwings[0];
					swing.family = FamilyOf(attacker);
					if (!swing.family) {
						swing.family = &sheets::kWeaponFamilies[0];
					}
					const float scale = std::clamp(gs.otherDamage / 50.0f, 0.4f, 2.5f);
					logger::info("live: MORDHAU says {}'s stand-in hit {}'s stand-in for {:.0f} of 100 (scale {:.2f})", attacker->GetDisplayFullName(), actor->GetDisplayFullName(),
						gs.otherDamage, scale);
					ApplyHit(attacker, actor.get(), swing, scale, gs.otherDamage);
				}
			}
			if (!m.hitsKnown) {
				m.hitsKnown = true;
				m.seenHits = gs.hits;
			} else if (gs.hits < m.seenHits) {
				m.seenHits = gs.hits;  // MORDHAU's side started counting afresh
			} else if (gs.hits != m.seenHits) {
				m.seenHits = gs.hits;
				ActiveSwing swing;
				swing.swing = g_liveSwing ? g_liveSwing : &sheets::kSwings[0];
				swing.family = FamilyOf(a_player);
				if (!swing.family) {
					swing.family = &sheets::kWeaponFamilies[0];
				}
				// MORDHAU's damage (head, torso, legs, armour) scales Skyrim's: 50 of 100 = an ordinary hit
				// (MORDHAU's side adds 1000 when the blow landed on the head)
				// (and 2000 when it was a headshot beside the neck and skull: enough for a blunt weapon to crush the head - sheets/gore.json head_any_cm)
				const bool  headshot = gs.damage >= 1000.0f || g_dev.goreTest;
				const bool  onHead = (gs.damage >= 1000.0f && gs.damage < 2000.0f) || g_dev.goreTest;
				const float hitDamage = gs.damage >= 2000.0f ? gs.damage - 2000.0f : gs.damage >= 1000.0f ? gs.damage - 1000.0f : gs.damage;
				const float scale = std::clamp(hitDamage / 50.0f, 0.4f, 2.5f);
				logger::info("live: MORDHAU says the player's {} hit {}'s stand-in for {:.0f} of 100{} (scale {:.2f})", a_link.motion, actor->GetDisplayFullName(), hitDamage,
					onHead ? ", on the head" : headshot ? ", a headshot" : "", scale);
				{
					// (looking into torso hits that counted as head hits: the heights Skyrim shows, to set
					// against what MORDHAU's side notes for the same hit)
					constexpr float kToCm = 1.42875f;
					const auto*     camera = RE::PlayerCamera::GetSingleton();
					const auto*     root = camera ? camera->cameraRoot.get() : nullptr;
					const auto*     head = actor->GetNodeByName("NPC Head [Head]");
					const auto*     chest = actor->GetNodeByName("NPC Spine2 [Spn2]");
					logger::info("heights (cm): Skyrim's camera {:.0f} above the player's feet ({}), MORDHAU's camera {:.0f} above its feet; {}: feet {:.0f} against the player's, head {:.0f} and chest {:.0f} above its own feet, scale {:.2f}; player scale {:.2f}",
						root ? (root->world.translate.z - a_player->GetPositionZ()) * kToCm : -1.0f, camera && camera->IsInFirstPerson() ? "first person" : "third person", a_link.cameraPos[2],
						actor->GetDisplayFullName(), (actor->GetPositionZ() - a_player->GetPositionZ()) * kToCm, head ? (head->world.translate.z - actor->GetPositionZ()) * kToCm : -1.0f,
						chest ? (chest->world.translate.z - actor->GetPositionZ()) * kToCm : -1.0f, actor->GetHeight() / 128.0f, a_player->GetHeight() / 128.0f);
				}
				const bool wasAlive = !actor->IsDead();
				ApplyHit(a_player, actor.get(), swing, scale, hitDamage);
				// a kick MORDHAU judged on a stand-in: the settings menu's super kick throws the Skyrim character too
				if (std::string_view(a_link.motion).find("Kick") != std::string_view::npos || g_lastRelease.find("Kick") != std::string::npos) {
					SuperKick(a_player, actor.get());
				}
				CharacterRender::AddBlood(actor->GetFormID(), 0.22f * scale);  // (sheets/gore.json blood_layer)
				CharacterRender::AddBlood(0, 0.06f);
				// MORDHAU's head explosion: the killing blow on the head with a blunt weapon (sheets/gore.json)
				// (not someone Skyrim will not let die: an essential character goes to its knees at no health and gets
				// up again - Jaree-Ra walked on without a head, seen in play)
				if (headshot && wasAlive && !actor->IsEssential() && (actor->IsDead() || actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth) <= 0.0f)) {
					std::string weapon = a_link.weaponClass;  // "BP_Maul_C"
					if (weapon.rfind("BP_", 0) == 0 && weapon.ends_with("_C")) {
						weapon = weapon.substr(3, weapon.size() - 5);
					}
					const std::string list = std::string("|") + sheets::kGoreBlunt + "|";
					const bool blunt = !weapon.empty() && list.find("|" + weapon + "|") != std::string::npos;
					const bool strike = g_lastRelease.find("Strike") != std::string::npos;  // (by now the motion is a recovery)
					if (blunt || (onHead && strike && !weapon.empty() && weapon.find("Fists") == std::string::npos)) {
							Link::Request("GoreSound");  // (MORDHAU's own sound of it)
						if (blunt) {
							CharacterRender::ExplodeHead(actor->GetFormID());
						} else {
							RE::NiPoint3 away = actor->GetPosition() - a_player->GetPosition();
							away.z = 0.0f;
							away = away.Length() > 1e-3f ? away / away.Length() : RE::NiPoint3{ 0.0f, 1.0f, 0.0f };
							// the head that comes off is the Skyrim character's own (Skyrim's decapitation makes it: its face, hair
							// and helmet, as an object in the world); MORDHAU's own head model is not shown. MORDHAU's neck piece
							// still goes on the body. (sheets/builds.json gore_skyrim_head)
							const bool own = BuildValue("gore_skyrim_head") > 0.5f && actor->Decapitate();
							if (own) {
								g_thrownHeads.push_back({ actor->GetHandle(), away, Clock::now(), 0 });  // (thrown on MORDHAU's arc: see ThrowHeads)
							}
							if (own || BuildValue("gore_skyrim_head") < 0.5f) {
								logger::info("gore: {}", own ? "Skyrim's own head came off" : "gore_skyrim_head is 0: MORDHAU's head model flies");
								CharacterRender::SeverHead(actor->GetFormID(), away, !own);
							} else {
								logger::info("gore: Skyrim did not take this head off at the blow: asked again while the body falls");
								g_pendingHeads.push_back({ actor->GetHandle(), away, Clock::now(), 0 });
							}
						}
						int played = 0;
						if (auto* race = actor->GetRace(); race && race->impactDataSet) {
							if (auto* impacts = RE::BGSImpactManager::GetSingleton()) {
								for (int k = 0; k < 8; ++k) {
									const float  turn = 0.785398f * static_cast<float>(k);
									RE::NiPoint3 way{ std::cos(turn), std::sin(turn), k % 2 ? -0.9f : -0.35f };
									played += impacts->PlayImpactEffect(actor.get(), race->impactDataSet, "NPC Head [Head]", way, 400.0f, false, false) ? 1 : 0;
								}
							}
						}
						logger::info("gore: {}'s head is {} by the {} ({} blood effects played)", actor->GetDisplayFullName(), blunt ? "crushed" : "cut off", weapon, played);
					}
				}
			}
		}
		}

		// A fought character's stamina (sheets/builds.json standin_stamina): its Skyrim bar shows its stand-in's MORDHAU stamina,
		// as the player's does; whatever Skyrim takes off it beyond that since the last frame - a frost spell, a stamina poison,
		// any mod's effect, whatever its source - is counted, and its stand-in loses as much in MORDHAU (a running total, Link
		// " sd:"). Skyrim's own spending is left out: sprinting, a blocked hit (ApplyHit), and all of it while Skyrim has the body.
		void MirrorStamina(Mirrored& m, const Link::State& a_link, RE::Actor* a_actor, Link::Ghost& g)
		{
			if (BuildValue("standin_stamina") < 0.5f) {
				m.staminaSet = -1.0f;
				return;
			}
			float mordhau = -1.0f;
			for (const auto& s : a_link.standinStamina) {
				if (s.id == m.id) {
					mordhau = s.stamina;
				}
			}
			auto*       values = a_actor->AsActorValueOwner();
			const float full = std::max(values->GetPermanentActorValue(RE::ActorValue::kStamina), 1.0f);
			const float have = values->GetActorValue(RE::ActorValue::kStamina);
			const bool  skyrims = g_skyrimHasBody.contains(m.id) || a_actor->IsDead() || a_actor->AsActorState()->IsSprinting() ||
			                     (RE::UI::GetSingleton() && RE::UI::GetSingleton()->GameIsPaused());
			if (!skyrims && m.staminaSet >= 0.0f && m.staminaSet - have > BuildValue("standin_stamina_drain_step")) {
				const float share = (m.staminaSet - have) / full * 100.0f;
				m.staminaDrained += share;
				if (Seconds(m.drainLogged) > 1.0f) {  // (a concentration spell drains every frame: at most a line a second)
					m.drainLogged = Clock::now();
					logger::info("stamina: Skyrim is draining {} ({:.1f}% of full so far): passed on to its stand-in (MORDHAU's stamina {:.0f})", a_actor->GetDisplayFullName(), m.staminaDrained,
						mordhau);
				}
			}
			g.staminaDrained = m.staminaDrained;
			if (skyrims || mordhau < 0.0f) {
				m.staminaSet = -1.0f;
				return;
			}
			const float want = full * std::clamp(mordhau, 0.0f, 100.0f) / 100.0f;
			if (std::abs(want - have) > 0.5f) {
				values->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina, want - have);
			}
			m.staminaSet = values->GetActorValue(RE::ActorValue::kStamina);
		}

		void MirrorOneEnemy(Mirrored& m, RE::PlayerCharacter* a_player, const Link::State& a_link, const float kCm, const float facing, std::vector<Link::Ghost>& ghosts)
		{
			if (m.dummy) {
				MirrorCreature(m, a_player, a_link, kCm, facing, ghosts);
				return;
			}
			auto actor = m.handle.get();
			if (!actor) {
				return;
			}
			const RE::NiPoint3 to = actor->GetPosition() - a_player->GetPosition();
			Link::Ghost        g;
			g.id = m.id;
			g.forward = (to.x * std::sin(facing) + to.y * std::cos(facing)) * kCm;
			g.right = (to.x * std::cos(facing) - to.y * std::sin(facing)) * kCm;
			g.up = to.z * kCm;
			g.yaw = (actor->GetAngleZ() - facing) * 57.29578f;
			// (sheets/builds.json standin_scale: Skyrim's people come in sizes, MORDHAU's fighters in one)
			if (BuildValue("standin_scale") > 0.5f && a_player->GetHeight() > 1.0f) {
				g.size = std::clamp(actor->GetHeight() / a_player->GetHeight(), 0.8f, 1.25f);
			}
			WalkAfterStandIn(m, a_player, a_link, kCm, actor, g);
			MirrorStamina(m, a_link, actor.get(), g);
			if (BuildValue("standin_slow") > 0.5f && !g_skyrimHasBody.contains(m.id)) {
				// (sheets/builds.json standin_slow: Skyrim's slows reach the stand-in - its speed against its normal one, a
				// frost spell's slow or any mod's, and the world's time, Slow Time; the player's own character keeps its pace)
				auto*       values = actor->AsActorValueOwner();
				const float base = values->GetBaseActorValue(RE::ActorValue::kSpeedMult);
				g.moveScale = base > 1.0f ? std::clamp(values->GetActorValue(RE::ActorValue::kSpeedMult) / base, BuildValue("standin_slow_least"), 1.0f) : 1.0f;
				g.timeScale = std::clamp(RE::BSTimer::QGlobalTimeMultiplier(), BuildValue("standin_slow_least"), 1.0f);
			}
			{
				static Clock::time_point lastMirror = Clock::now();
				static float             mirrorDt = 0.016f;
				if (&m == &g_mirrored.front()) {
					mirrorDt = std::clamp(Seconds(lastMirror), 0.001f, 0.1f);
					lastMirror = Clock::now();
				}
				const RE::NiPoint3 pos = actor->GetPosition();
				if (m.posKnown) {
					RE::NiPoint3 step = pos - m.lastPos;
					step.z = 0.0f;
					const float k = std::clamp(mirrorDt * 10.0f, 0.0f, 1.0f);
					m.velocity = step.Length() < 150.0f ? m.velocity * (1.0f - k) + (step / mirrorDt) * k : RE::NiPoint3{};
				}
				m.lastPos = pos;
				m.posKnown = true;
				g.velForward = (m.velocity.x * std::sin(facing) + m.velocity.y * std::cos(facing)) * kCm;
				g.velRight = (m.velocity.x * std::cos(facing) - m.velocity.y * std::sin(facing)) * kCm;
			}
			const auto state = actor->AsActorState()->GetAttackState();
			const bool attacking = state == RE::ATTACK_STATE_ENUM::kDraw || state == RE::ATTACK_STATE_ENUM::kSwing;
			const auto* inHand = actor->GetEquippedObject(false);
			const auto* inHandWeapon = inHand ? inHand->As<RE::TESObjectWEAP>() : nullptr;
			const bool  shoots = inHandWeapon && (inHandWeapon->IsBow() || inHandWeapon->IsCrossbow());
			// (an archer; or someone who is only being attacked and does not fight: its stand-in stands there to be hit
			// - with its brain on it would go for the player's character)
			g.passive = shoots || (!actor->IsInCombat() && !actor->IsHostileToActor(a_player));
			// (one whose weapon is put away in Skyrim does not fight: its stand-in only stands to be hit. Arvel sheathed
			// his sword and ran, and his stand-in went on and killed the player)
			if (g_dev.duelPassive) {
				g.passive = true;
			}
			if (!actor->AsActorState()->IsWeaponDrawn()) {
				g.passive = true;
			}
			if (shoots) {
				// (a bow in Skyrim = a bow in MORDHAU's hands too: with a bow out MORDHAU's character cannot parry - the
				// user: "just make them hold a bow in mordhau too when they hold a bow in skyrim")
				g.weapon = inHandWeapon->IsCrossbow() ? "Ranged/BP_Crossbow" : "Ranged/BP_RecurveBow";
			}
			LeaveYieldingBodyToSkyrim(m, actor, g);
			if (BuildValue("fought_press") > 0.5f) {
				// A fought character presses the attack: its way of fighting (shared by everyone of its kind) is set to
				// never fall back, hardly circle, never avoid a stronger opponent. Skyrim's AI backed weak enemies away
				// from a strong player (Daedric armour) - and a stand-in only strikes where its Skyrim character stands.
				auto* base = actor->GetActorBase();
				auto* style = base ? base->GetCombatStyle() : nullptr;
				static std::unordered_set<std::uint32_t> pressed;
				if (style && pressed.insert(style->GetFormID()).second) {
					style->closeRangeData.fallbackMult = 0.0f;
					style->closeRangeData.circleMult = std::min(style->closeRangeData.circleMult, 0.2f);
					style->closeRangeData.stalkTimeMult = 0.0f;
					style->generalData.avoidThreatChance = 0.0f;
					style->generalData.offensiveMult = std::max(style->generalData.offensiveMult, 1.0f);
					style->generalData.defensiveMult = std::min(style->generalData.defensiveMult, 0.2f);
					logger::info("fought: {}'s way of fighting set to press the attack (no falling back)", actor->GetDisplayFullName());
				}
			}
			if (BuildValue("fought_confidence") > 0.5f) {
				// (no cowering or running off in the middle of a MORDHAU fight: Skyrim's confidence is put at exactly 4,
				// "foolhardy" - never flees. The first version added 4 to whatever it was, which could leave the scale.)
				const float confidence = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kConfidence);
				if (std::abs(confidence - 4.0f) > 0.01f) {
					actor->AsActorValueOwner()->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kConfidence, 4.0f - confidence);
					static int told = 0;
					if (told++ < 8) {
						logger::info("fought: {}'s confidence {:.0f} -> 4 (no cowering or fleeing while it is fought)", actor->GetDisplayFullName(), confidence);
					}
				}
			}
			if (attacking && !m.wasAttacking && !shoots) {
				if (Seconds(m.lastAttack) > 0.4f) {
					m.lastAttack = Clock::now();
					++m.attacks;
					logger::info("live: {} starts an attack: its MORDHAU stand-in attacks", actor->GetDisplayFullName());
				}
				// (the Skyrim character itself is not seen; its own swing would only be heard: the whoosh is a sound event
				// inside its attack animation, so the animation is thrown back to standing before it gets there -
				// "attackStop" alone let the swing play on)
				actor->NotifyAnimationGraph("attackStop");
				actor->NotifyAnimationGraph("IdleForceDefaultState");
			}
			m.wasAttacking = attacking;
			g.attacks = m.attacks;
			g.blocking = actor->IsBlocking();
			g.weapon = MordhauWeaponFor(actor.get());
			if (auto foe = actor->GetActorRuntimeData().currentCombatTarget.get(); foe && foe.get() != a_player && !foe->IsDead()) {
				g.target = foe->GetFormID();  // (it fights someone else: its stand-in aims there, not at the player)
			}
			if (g.target == 0 && actor->IsPlayerTeammate()) {
				// A follower between opponents: Skyrim's AI took 6-7 s to choose the next one. The
				// nearest fought enemy of the player is its opponent at once - for its stand-in, and (asked once a
				// second) for Skyrim's own combat too.
				RE::Actor* next = nullptr;
				float      best = 900.0f;
				for (const auto& other : g_mirrored) {
					if (other.id == m.id) {
						continue;
					}
					if (auto* enemy = RE::TESForm::LookupByID<RE::Actor>(other.id); enemy && !enemy->IsDead() && enemy->IsHostileToActor(a_player)) {
						if (const float d = enemy->GetPosition().GetDistance(actor->GetPosition()); d < best) {
							best = d, next = enemy;
						}
					}
				}
				if (next) {
					g.target = next->GetFormID();
					g.passive = shoots;  // (it has an opponent: not one that only stands to be hit)
					static std::unordered_map<std::uint32_t, Clock::time_point> asked;
					if (auto& when = asked[m.id]; Seconds(when) > 1.0f) {
						when = Clock::now();
						actor->StartCombat(next);
						logger::info("follower: {} is set on {} ({:.0f} units away)", actor->GetDisplayFullName(), next->GetDisplayFullName(), best);
					}
				}
			}
			g.shield = BuildValue("mordhau_shield") > 0.5f ? MordhauShieldOf(actor->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kShield)) : "";
			if (m.id == g_demonId) {
				g.weapon = "Demon/Troll";  // not a weapon: its stand-in is MORDHAU's Troll itself
			} else if (const auto* fighter = FighterFor(actor.get())) {
				// (sheets/creature_fighters.json: its stand-in is that Demon Horde creature, with its own brain and moves)
				// ("own": the MORDHAU weapon for the Skyrim weapon it holds, as for a person - so per creature, not per kind)
				static std::unordered_map<std::uint32_t, std::string> names;
				const bool  own = std::string_view(fighter->weapon) == "own";
				const char* held = own ? MordhauWeaponFor(actor.get()) : fighter->weapon;
				auto&       name = names[m.id];
				name = std::string("Demon/") + fighter->demon + (held && held[0] ? std::string("/") + held : std::string());
				g.weapon = name.c_str();
				g.shield = "";
				CharacterRender::SetOwnHips(m.id, fighter->skyrimBones);
				g.passive = !actor->IsInCombat() && !actor->IsHostileToActor(a_player);  // (a creature has no weapon to put away)
				// as tall in MORDHAU as the creature stands in Skyrim (its head at nine tenths of its height)
				g.size = fighter->headCm > 1.0f ? std::clamp(actor->GetHeight() * kCm * 0.9f / fighter->headCm, 0.3f, 1.5f) : 1.0f;
			}
			{
				auto* av = actor->AsActorValueOwner();
				{
					// skill by the level gap (sheets/enemy_ai.json level_skill): the level chosen (F8 / the settings menu's "Enemy skill
					// at equal level") at equal levels, and from there smoothly all the way to Duelist (5) at 19 levels above the
					// player and to Peasant (1) at 19 below, whatever level is chosen (a level 1 player against a level 20 enemy: the
					// hardest) - plus one for a named enemy; between 1 and 5, in tenths. (First made as two levels either way: from
					// Recruit a level 19 marauder came to 3.9 - "he didnt seem much better at dueling".)
					const auto* base = actor->GetActorBase();
					const float chosen = static_cast<float>(g_aiLevel);
					const float t = std::clamp((static_cast<float>(actor->GetLevel()) - static_cast<float>(a_player->GetLevel())) / 19.0f, -1.0f, 1.0f);
					const float byGap = Settings::Value("level_skill", 1.0f) > 0.5f ? (t >= 0.0f ? (5.0f - chosen) * t : (chosen - 1.0f) * t) : 0.0f;  // (settings menu "Enemy skill follows the level gap")
					float       skill = chosen + byGap + (base && base->IsUnique() ? 1.0f : 0.0f);
					g.skill10 = static_cast<int>(std::lround(std::clamp(skill, 1.0f, 5.0f) * 10.0f));
				}
				g.health = std::clamp(av->GetActorValue(RE::ActorValue::kHealth) / std::max(av->GetPermanentActorValue(RE::ActorValue::kHealth), 1.0f), 0.0f, 1.0f) * 100.0f;
			}
			MuteSkyrimWeapon(actor.get());
			ghosts.push_back(g);

			// what MORDHAU decided about the player's attacks on it
			ApplyStandInVerdicts(m, a_player, a_link, actor);
		}

		void MirrorTick(RE::PlayerCharacter* a_player, const Link::State& a_link)
		{
			auto* root = a_player->Get3D(false);
			if (!root) {
				return;
			}
			// the nearest living characters around the player (10 m), at most three; ones already mirrored keep their place
			struct Candidate
			{
				RE::Actor* actor;
				float      distance;
			};
			std::vector<Candidate> nearby;
			// (whoever a fighter near the player has as its Skyrim target counts too, fighting back or not: without a
			// stand-in of its own nothing can hit it - a bandit swung at a follower who stood there, over and in front of her)
			std::unordered_set<std::uint32_t> attacked;
			RE::ProcessLists::GetSingleton()->ForEachHighActor([&](RE::Actor* a_actor) {
				if (a_actor && a_actor != a_player && !a_actor->IsDead() && a_actor->IsInCombat()) {
					if (auto foe = a_actor->GetActorRuntimeData().currentCombatTarget.get(); foe && foe.get() != a_player && !foe->IsDead()) {
						attacked.insert(foe->GetFormID());
					}
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
			RE::ProcessLists::GetSingleton()->ForEachHighActor([&](RE::Actor* a_actor) {
				// only people (MORDHAU has no wolves or dragons: creatures keep Skyrim's own attacks, which a MORDHAU parry
				// can still stop) and only ones actually fighting the player (bystanders stay themselves)
				// ...or each other: two people fighting near the player fight by MORDHAU's rules too
				if (a_actor && a_actor != a_player && !a_actor->IsDead() && a_actor->Is3DLoaded() && (Humanoid(a_actor) || FighterFor(a_actor)) &&
					(a_actor->IsHostileToActor(a_player) || a_actor->IsInCombat() || attacked.contains(a_actor->GetFormID()) ||
						(a_actor->IsPlayerTeammate() && a_player->IsInCombat()))) {  // (a follower keeps its stand-in between opponents)
					// melee fighters only: an archer or a mage keeps fighting Skyrim's way
					const auto* held = a_actor->GetEquippedObject(false);
					const auto* heldWeapon = held ? held->As<RE::TESObjectWEAP>() : nullptr;
					// (a spell or a staff in either hand: that one is Skyrim's own altogether - its moves, its path, its fight.
					// With a spell in one hand and a blade in the other its stand-in pointed at the ground. It is still hit:
					// the player's blade is checked against it as against a creature.)
					const auto* off = a_actor->GetEquippedObject(true);
					const auto* offWeapon = off ? off->As<RE::TESObjectWEAP>() : nullptr;
					const bool  casts = (held && !heldWeapon && held->Is(RE::FormType::Spell)) || (off && off->Is(RE::FormType::Spell)) || (heldWeapon && heldWeapon->IsStaff()) ||
										(offWeapon && offWeapon->IsStaff());
					// (nor one still lying, sitting or getting up - a draugr climbing out of its coffin: Skyrim's own animation
					// has it until it stands; with a stand-in already it swung and parried unseen through that)
					const bool  rising = a_actor->AsActorState()->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal;  // (not 'animation driven': a stagger is that too)
					const bool  melee = !casts && !rising && (!held || (heldWeapon && !heldWeapon->IsBow() && !heldWeapon->IsCrossbow() && !heldWeapon->IsStaff()));
					const float d = a_actor->GetPosition().GetDistance(a_player->GetPosition());
					// (the creature is twice a person's size: so is its reach)
					const bool archer = heldWeapon && (heldWeapon->IsBow() || heldWeapon->IsCrossbow()) && d < BuildValue("archer_standin_reach");
					if (a_actor->GetFormID() == g_demonId ? d < 1300.0f : ((melee && d < 700.0f) || archer)) {
						// (one that has a stand-in already counts as nearer: sheets/builds.json standin_keep_margin)
						const bool has = std::any_of(g_mirrored.begin(), g_mirrored.end(), [&](const Mirrored& a_m) { return a_m.id == a_actor->GetFormID(); });
						nearby.push_back({ a_actor, has ? d - BuildValue("standin_keep_margin") : d });
					}
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
			std::sort(nearby.begin(), nearby.end(), [](const Candidate& a, const Candidate& b) { return a.distance < b.distance; });
			if (nearby.size() > static_cast<std::size_t>(Link::kGhosts)) {
				nearby.resize(static_cast<std::size_t>(Link::kGhosts));
			}
			// (sheets/builds.json creature_standin: the places people leave over go to the nearest creatures that fight the
			// player - a stand-in that only stands to be hit)
			const std::size_t people = nearby.size();
			if (BuildValue("creature_standin") > 0.5f && people < static_cast<std::size_t>(Link::kGhosts)) {
				std::vector<Candidate> creatures;
				RE::ProcessLists::GetSingleton()->ForEachHighActor([&](RE::Actor* a_actor) {
					if (a_actor && a_actor != a_player && !a_actor->IsDead() && a_actor->Is3DLoaded() && !Humanoid(a_actor) && !FighterFor(a_actor) && !SkyrimsAlone(a_actor) && a_actor->GetFormID() != g_demonId && !a_actor->IsPlayerTeammate() &&
						a_actor->IsHostileToActor(a_player) && a_actor->IsInCombat() && CreatureTall(a_actor) <= BuildValue("creature_standin_tallest")) {
						const bool  has = std::any_of(g_mirrored.begin(), g_mirrored.end(), [&](const Mirrored& a_m) { return a_m.id == a_actor->GetFormID(); });
						const float d = a_actor->GetPosition().GetDistance(a_player->GetPosition());
						if (d < BuildValue("creature_standin_reach") + (has ? BuildValue("standin_keep_margin") : 0.0f)) {
							creatures.push_back({ a_actor, has ? d - BuildValue("standin_keep_margin") : d });
						}
					}
					return RE::BSContainer::ForEachResult::kContinue;
				});
				std::sort(creatures.begin(), creatures.end(), [](const Candidate& a, const Candidate& b) { return a.distance < b.distance; });
				for (const auto& c : creatures) {
					if (nearby.size() < static_cast<std::size_t>(Link::kGhosts)) {
						nearby.push_back(c);
					}
				}
			}
			std::vector<Mirrored> next;
			for (std::size_t k = 0; k < nearby.size(); ++k) {
				const auto& c = nearby[k];
				Mirrored    m;
				m.handle = c.actor->GetHandle();
				m.id = c.actor->GetFormID();
				bool had = false;
				for (const auto& old : g_mirrored) {
					if (old.id == m.id) {
						m = old;
						had = true;
					}
				}
				if (!had || m.dummy != (k >= people)) {
					m.dummy = k >= people;
					if (m.dummy) {
						logger::info("fought: {} (a creature, {:.0f} units tall) gets a stand-in that only stands to be hit", c.actor->GetDisplayFullName(), CreatureTall(c.actor));
					}
				}
				next.push_back(m);
			}
			g_mirrored = std::move(next);

			constexpr float           kCm = 1.42875f;
			const float               facing = a_player->GetAngleZ();
			Link::SetPlayerPlace(a_player->GetPosition().x * kCm, a_player->GetPosition().y * kCm, facing * 57.29578f);
			std::vector<Link::Ghost> ghosts;
			for (auto& m : g_mirrored) {
				MirrorOneEnemy(m, a_player, a_link, kCm, facing, ghosts);
			}
			Link::SetGhosts(ghosts);
			{
				std::unordered_map<std::uint32_t, std::string> held;
				for (const auto& g : ghosts) {
					const std::string_view path = g.weapon ? g.weapon : "";
					const auto             name = path.substr(path.find_last_of('/') == std::string_view::npos ? 0 : path.find_last_of('/') + 1);
					if (name.rfind("BP_", 0) == 0 && name != "BP_FistsWeapon") {
						held[g.id] = "w_" + std::string(name.substr(3));
					}
				}
				CharacterRender::SetStandinWeapons(held);
			}
			{
				std::vector<std::uint32_t> down;
				for (auto& m : g_mirrored) {
					auto actor = m.handle.get();
					if (!actor || m.dummy) {
						continue;  // (a creature falls by Skyrim's rules alone)
					}
					if (actor->IsInRagdollState()) {
						down.push_back(m.id);
					}
					bool fallen = false;
					for (const auto& gs : a_link.ghosts) {
						fallen = fallen || (gs.id == m.id && gs.stage == kFallen);
					}
					if (FallFromMordhau(g_falls[m.id], actor.get(), fallen)) {
						logger::info("fall: MORDHAU knocked {}'s stand-in down: Skyrim's body goes down too", actor->GetDisplayFullName());
						PushDown(a_player, actor.get(), 3.0f);
					}
				}
				bool playerFallen = a_link.stage == kFallen;
				{
					// (sheets/builds.json creature_knockdown_every: a MORDHAU creature's blows throw the player down only
					// now and then - the Centurion's every move did)
					static bool wasFallen = false, spared = false;
					static int  creatureFalls = 0;
					if (playerFallen && !wasFallen) {
						RE::Actor* by = nullptr;
						for (auto& m : g_mirrored) {
							if (m.id == a_link.playerAttacker) {
								by = m.handle.get().get();
							}
						}
						spared = false;
						if (by && FighterFor(by)) {
							const int every = std::max(static_cast<int>(BuildValue("creature_knockdown_every")), 1);
							spared = ++creatureFalls % every != 0;
							logger::info("fall: {} threw the player down in MORDHAU ({} so far): {}", by->GetDisplayFullName(), creatureFalls,
								spared ? "undone, the player stays on his feet" : "the player goes down");
							if (spared) {
								Link::Request("StandUp");
							}
						}
					}
					wasFallen = playerFallen;
					if (spared && playerFallen) {
						playerFallen = false;
					}
				}
				if (FallFromMordhau(g_falls[0x14], a_player, playerFallen)) {
					logger::info("fall: MORDHAU knocked the player down: Skyrim's body goes down too");
					PushDown(nullptr, a_player, 3.0f);
				}
				static bool wasDown = false;
				if (a_player->IsInRagdollState() != wasDown) {
					wasDown = !wasDown;
					logger::info("fall: the player is {} in Skyrim (MORDHAU motion {})", wasDown ? "limp" : "up again", a_link.motion);
				}
				Link::SetDown(a_player->IsInRagdollState(), down);
			}

			// what MORDHAU decided about attacks on the player
			if (!g_playerHitsKnown) {
				g_playerHitsKnown = true;
				g_seenPlayerHits = a_link.playerHits;
			} else if (a_link.playerHits < g_seenPlayerHits) {
				g_seenPlayerHits = a_link.playerHits;  // MORDHAU's side started counting afresh
			} else if (a_link.playerHits != g_seenPlayerHits) {
				g_seenPlayerHits = a_link.playerHits;
				RE::Actor* attacker = nullptr;
				for (auto& m : g_mirrored) {
					if (m.id == a_link.playerAttacker) {
						attacker = m.handle.get().get();
					}
				}
				if (!attacker && !g_mirrored.empty() && !g_mirrored.front().dummy) {
					attacker = g_mirrored.front().handle.get().get();
				}
				// (only someone actually against the player: a bystander's stand-in can clip the player in MORDHAU)
				if (attacker && attacker->IsHostileToActor(a_player)) {
					ApplyHitOnPlayer(attacker, a_player, a_link.playerDamage);
				}
			}
		}

		void SetAiLevel(int a_level, bool a_tell)
		{
			g_aiLevel = std::clamp(a_level, 1, 5);
			Link::SetAiLevel(g_aiLevel);
			std::ofstream out(g_dev.cache / "ai_level.txt");
			out << g_aiLevel;
			if (a_tell) {
				logger::info("ai: enemy skill level {} ({})", g_aiLevel, kAiNames[g_aiLevel - 1]);
			}
		}

		void LoadAiLevel()
		{
			if (g_aiLevel == 0) {
				int           level = 3;
				std::ifstream in(g_dev.cache / "ai_level.txt");
				in >> level;
				g_aiLevel = std::clamp(level, 1, 5);
				Link::SetAiLevel(g_aiLevel);
			}
		}

		void ShowAiMenu()
		{
			auto* factories = RE::MessageDataFactoryManager::GetSingleton();
			auto* strings = RE::InterfaceStrings::GetSingleton();
			auto* creator = factories && strings ? factories->GetCreator<RE::MessageBoxData>(strings->messageBoxData) : nullptr;
			auto* box = creator ? creator->Create() : nullptr;
			if (!box) {
				logger::info("ai: no message box could be made");
				return;
			}
			LoadAiLevel();
			box->bodyText = std::format("Enemy skill (now: {}). How well enemies parry, feint and how soon they attack.", kAiNames[g_aiLevel - 1]).c_str();
			for (const char* name : kAiNames) {
				box->buttonText.push_back(name);
			}
			box->buttonText.push_back("Cancel");
			box->cancelButtonIndex = 5;
			box->isCancellable = true;
			box->verticalButtons = true;
			box->callback = RE::make_smart<AiLevelPicked>();
			using func_t = void(RE::MessageBoxData*);
			REL::Relocation<func_t> queue{ RELOCATION_ID(51422, 52271) };  // MessageBoxData::QueueMessage
			queue(box);
		}
	}
}
