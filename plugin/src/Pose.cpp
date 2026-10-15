#include "Pose.h"

#include "gen/Sheets.h"

namespace dm
{
	namespace
	{
		void Walk(RE::NiAVObject* a_obj, const RE::NiMatrix3& a_parentRot, const RE::NiPoint3& a_parentPos, std::unordered_map<std::string, RestPose::Node>& a_out)
		{
			const RE::NiMatrix3 rot = a_parentRot * a_obj->local.rotate;
			const RE::NiPoint3  pos = a_parentPos + a_parentRot * (a_obj->local.translate);
			if (!a_obj->name.empty()) {
				a_out.try_emplace(a_obj->name.c_str(), RestPose::Node{ ToQuat(rot), pos });
			}
			if (auto* node = a_obj->AsNode()) {
				for (auto& child : node->GetChildren()) {
					if (child) {
						Walk(child.get(), rot, pos, a_out);
					}
				}
			}
		}

		const sheets::Bone* FindBone(std::string_view a_skyrimNode)
		{
			for (const auto& bone : sheets::kBones) {
				if (a_skyrimNode == bone.skyrimNode) {
					return &bone;
				}
			}
			return nullptr;
		}
	}

	bool RestPose::Load(RE::Actor* a_actor)
	{
		_nodes.clear();
		auto* race = a_actor ? a_actor->GetRace() : nullptr;
		auto* base = a_actor ? a_actor->GetActorBase() : nullptr;
		if (!race || !base) {
			return false;
		}
		const char* path = race->skeletonModels[base->IsFemale() ? 1 : 0].GetModel();
		if (!path || !*path) {
			return false;
		}
		RE::NiPointer<RE::NiNode>     model;
		RE::BSModelDB::DBTraits::ArgsType args;
		const auto                    result = RE::BSModelDB::Demand(path, model, args);
		if (!model) {
			logger::error("rest pose: could not load '{}' (code {})", path, static_cast<int>(result));
			return false;
		}
		// The model's own root is the model space origin, so start below it.
		RE::NiMatrix3 identity;
		for (auto& child : model->GetChildren()) {
			if (child) {
				Walk(child.get(), identity, RE::NiPoint3{}, _nodes);
			}
		}
		logger::info("rest pose: '{}' -> {} nodes", path, _nodes.size());
		for (const auto& row : sheets::kTwistBones) {
			if (!Find(row.skyrimNode)) {
				logger::warn("rest pose: twist bone '{}' is not in this skeleton", row.skyrimNode);
			}
		}
		return !_nodes.empty();
	}

	const RestPose::Node* RestPose::Find(std::string_view a_name) const
	{
		const auto it = _nodes.find(std::string(a_name));
		return it != _nodes.end() ? &it->second : nullptr;
	}

	namespace
	{
		void SetWorldRotation(RE::NiAVObject* a_node, const Quat& a_world)
		{
			const RE::NiMatrix3 worldRot = ToMatrix(a_world);
			auto*               parent = a_node->parent;
			a_node->local.rotate = parent->world.rotate.Transpose() * worldRot;
			a_node->world.rotate = worldRot;
			a_node->world.translate = parent->world.translate + parent->world.rotate * (a_node->local.translate * parent->world.scale);
		}

		RE::BSFlattenedBoneTree* FindBoneTree(RE::NiAVObject* a_obj, int a_depth = 0)
		{
			if (!a_obj || a_depth > 4) {
				return nullptr;
			}
			if (auto* tree = netimmerse_cast<RE::BSFlattenedBoneTree*>(a_obj)) {
				return tree;
			}
			if (auto* node = a_obj->AsNode()) {
				for (auto& child : node->GetChildren()) {
					if (auto* tree = FindBoneTree(child.get(), a_depth + 1)) {
						return tree;
					}
				}
			}
			return nullptr;
		}

		// Skinned meshes read bone transforms from the skeleton's flattened bone list, not from the nodes. Bones with
		// a node are copied from it; bones without one (fingers, for a start) exist only in the list and are
		// recomputed here from their parents, or gauntlets stay behind at the hand's old place.
		void SyncBoneTree(RE::NiAVObject* a_root)
		{
			auto* tree = FindBoneTree(a_root);
			if (!tree) {
				static bool warned = false;
				if (!warned) {
					warned = true;
					logger::warn("pose: no flattened bone list found; node-less bones will not follow");
				}
				return;
			}
			auto&         data = tree->GetRuntimeData();
			std::uint32_t nodeless = 0, unordered = 0;
			for (std::uint32_t i = 0; i < data.numBones; ++i) {
				auto& entry = data.boneEntries[i];
				if (entry.node) {
					entry.world = entry.node->world;
				} else if (entry.parentIndex >= 0 && static_cast<std::uint32_t>(entry.parentIndex) < i) {
					entry.world = data.boneEntries[entry.parentIndex].world * entry.local;
					++nodeless;
				} else if (entry.parentIndex >= 0) {
					++unordered;
				}
			}
			static bool logged = false;
			if (!logged) {
				logged = true;
				logger::info("pose: flattened bone list has {} bones, {} without a node (recomputed), {} out of order (left alone)", data.numBones, nodeless, unordered);
			}
		}

		// sheets/twist_bones.json: share the roll out along the limb so gauntlets and sleeves are not wrung.
		void ApplyTwistBones(RE::NiAVObject* a_root, const RestPose& a_rest, float a_weight)
		{
			for (const auto& row : sheets::kTwistBones) {
				auto*       twistNode = a_root->GetObjectByName(row.skyrimNode);
				auto*       ownerNode = a_root->GetObjectByName(row.ownerNode);
				auto*       relNode = a_root->GetObjectByName(row.relativeToNode);
				const auto* twistRest = a_rest.Find(row.skyrimNode);
				const auto* ownerRest = a_rest.Find(row.ownerNode);
				const auto* towardRest = a_rest.Find(row.towardNode);
				const auto* relRest = a_rest.Find(row.relativeToNode);
				if (!twistNode || !twistNode->parent || !ownerNode || !relNode || !twistRest || !ownerRest || !towardRest || !relRest) {
					continue;
				}
				// the limb's axis and the twist bone's place along it, in the owner's own frame (rest pose)
				const Quat         toOwner = Inverse(ownerRest->rot);
				RE::NiPoint3       axis = Rotate(toOwner, towardRest->pos - ownerRest->pos);
				const float        limb = axis.Length();
				if (limb < 1e-3f) {
					continue;
				}
				axis = axis / limb;
				const float share = std::clamp(Rotate(toOwner, twistRest->pos - ownerRest->pos).Dot(axis) / limb, 0.0f, 1.0f);

				const Quat owner = ToQuat(ownerNode->world.rotate);
				const Quat other = ToQuat(relNode->world.rotate);
				Quat       roll;
				if (row.followChild) {
					// how far the hand has rolled about the forearm since the rest pose
					const Quat now = Mul(Inverse(owner), other);
					const Quat rest = Mul(toOwner, relRest->rot);
					roll = Scaled(TwistAbout(Mul(now, Inverse(rest)), axis), share);
				} else {
					// how far the upper arm has rolled about itself against the shoulder; undo the part this bone should not take
					const Quat now = Mul(Inverse(other), owner);
					const Quat rest = Mul(Inverse(relRest->rot), ownerRest->rot);
					roll = Scaled(TwistAbout(Mul(Inverse(rest), now), axis), -(1.0f - share));
				}
				Quat world = Normalized(Mul(owner, Mul(roll, Mul(toOwner, twistRest->rot))));
				if (a_weight < 1.0f) {
					world = Nlerp(ToQuat(twistNode->world.rotate), world, a_weight);
				}
				SetWorldRotation(twistNode, world);
			}
		}
	}

	void CarryBones(RE::NiAVObject* a_root, RE::NiAVObject* const* a_moved, const RE::NiTransform* a_was, std::size_t a_count)
	{
		auto* tree = a_root && a_moved && a_was && a_count ? FindBoneTree(a_root) : nullptr;
		if (!tree) {
			return;
		}
		auto& data = tree->GetRuntimeData();
		// (which moved bone each entry hangs under: 0 = none, else its number + 1)
		static thread_local std::vector<std::uint8_t> under;
		under.assign(data.numBones, 0);
		for (std::uint32_t i = 0; i < data.numBones; ++i) {
			auto&              entry = data.boneEntries[i];
			const std::uint8_t above = entry.parentIndex >= 0 && static_cast<std::uint32_t>(entry.parentIndex) < i ? under[entry.parentIndex] : std::uint8_t{ 0 };
			if (entry.node) {
				under[i] = above;
				for (std::size_t m = 0; m < a_count; ++m) {
					if (entry.node == a_moved[m]) {
						under[i] = static_cast<std::uint8_t>(m + 1);
					}
				}
				entry.world = entry.node->world;
			} else if (above) {
				// (as Skyrim's animation left it against the moved bone: a finger stays curled as it was)
				under[i] = above;
				const auto&         moved = *a_moved[above - 1];
				const auto&         was = a_was[above - 1];
				const RE::NiMatrix3 turn = moved.world.rotate * was.rotate.Transpose();
				entry.world.translate = moved.world.translate + turn * (entry.world.translate - was.translate);
				entry.world.rotate = turn * entry.world.rotate;
			}
		}
	}

	void CarryBones(RE::NiAVObject* a_root, RE::NiAVObject* a_moved, const RE::NiTransform& a_was)
	{
		CarryBones(a_root, &a_moved, &a_was, 1);
	}

	void ApplyTwist(RE::NiAVObject* a_root, const RestPose& a_rest)
	{
		if (a_root) {
			ApplyTwistBones(a_root, a_rest, 1.0f);
		}
	}

	int ApplyClip(RE::Actor* a_actor, const RestPose& a_rest, const Clip& a_clip, float a_time, float a_weight, bool a_twist)
	{
		auto* root = a_actor ? a_actor->Get3D(false) : nullptr;
		if (!root || !a_rest.Loaded()) {
			return 0;
		}
		const Quat      actorRot = ToQuat(root->world.rotate);
		int             driven = 0;
		RE::NiAVObject* top = nullptr;

		// Tracks follow sheets/bone_map.json order: parent before child.
		for (const auto& track : a_clip.tracks) {
			const auto* bone = FindBone(track.skyrimNode);
			if (!bone || bone->layer != sheets::Layer::kUpper) {
				continue;
			}
			auto*       node = root->GetObjectByName(track.skyrimNode);
			const auto* rest = a_rest.Find(track.skyrimNode);
			if (!node || !node->parent || !rest) {
				continue;
			}
			// Turn Skyrim's rest pose so this bone points where MORDHAU's rest pose points, then apply the clip.
			Quat align;
			if (!track.dirSkyrimNode.empty()) {
				if (const auto* child = a_rest.Find(track.dirSkyrimNode)) {
					RE::NiPoint3 dir = child->pos - rest->pos;
					const float  len = dir.Length();
					if (len > 1e-4f) {
						align = Between(dir / len, track.restDir);
					}
				}
			}
			const Quat model = Mul(a_clip.Sample(track, a_time), Mul(align, rest->rot));
			Quat       world = Normalized(Mul(actorRot, model));
			if (a_weight < 1.0f) {
				world = Nlerp(ToQuat(node->world.rotate), world, a_weight);
			}
			const RE::NiMatrix3 worldRot = ToMatrix(world);
			auto*               parent = node->parent;
			node->local.rotate = parent->world.rotate.Transpose() * worldRot;
			node->world.rotate = worldRot;
			node->world.translate = parent->world.translate + parent->world.rotate * (node->local.translate * parent->world.scale);
			if (!top) {
				top = node;
			}
			++driven;
		}
		if (top && a_twist) {
			ApplyTwistBones(root, a_rest, a_weight);
		}
		if (top) {
			// Carry the new transforms down to everything below (fingers, weapon, twist bones).
			RE::NiUpdateData data{};
			top->UpdateDownwardPass(data, 0);
			SyncBoneTree(root);
		}
		return driven;
	}
}
