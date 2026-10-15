#pragma once

#include "Clip.h"

namespace dm
{
	// Skyrim's skeleton rest pose in model space, read once from the actor's skeleton model
	// (sheets/hooks.json: pose_override).
	class RestPose
	{
	public:
		struct Node
		{
			Quat         rot;  // model space
			RE::NiPoint3 pos;  // model space
		};

		// Loads the skeleton model the actor's race uses and records every node.
		bool Load(RE::Actor* a_actor);

		[[nodiscard]] const Node* Find(std::string_view a_name) const;
		[[nodiscard]] bool        Loaded() const { return !_nodes.empty(); }

	private:
		std::unordered_map<std::string, Node> _nodes;
	};

	// Writes one clip pose onto the actor's third-person skeleton. a_weight 0..1 blends from Skyrim's own pose.
	// Returns the number of bones driven.
	// a_twist false leaves the limb's twist bones on Skyrim's own animation (development comparison only).
	int ApplyClip(RE::Actor* a_actor, const RestPose& a_rest, const Clip& a_clip, float a_time, float a_weight, bool a_twist = true);

	// The twist bones of an already posed skeleton (sheets/twist_bones.json): the roll of the limbs is shared out
	// along them, so sleeves and gauntlets are not wrung. For skeletons posed by other means than ApplyClip.
	void ApplyTwist(RE::NiAVObject* a_root, const RestPose& a_rest);
	// Skinned meshes read the skeleton's flattened bone list, and the fingers have no nodes: they are only in that list,
	// which Skyrim fills before this plugin moves anything. After a bone (a hand) was moved from a_was to where it now
	// is, the node-less bones below it are carried along as they stood against it (a finger stays curled as Skyrim's
	// animation had it), and every bone with a node is copied from its node.
	void CarryBones(RE::NiAVObject* a_root, RE::NiAVObject* a_moved, const RE::NiTransform& a_was);
	// ... several moved bones (both hands) in one pass over the list.
	void CarryBones(RE::NiAVObject* a_root, RE::NiAVObject* const* a_moved, const RE::NiTransform* a_was, std::size_t a_count);
}
