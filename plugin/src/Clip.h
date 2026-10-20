#pragma once

#include "Quat.h"

namespace dm
{
	// One MORDHAU clip converted to Skyrim model space (the plugin's own reader replaces
	// that tool later). Per bone and frame: the rotation from MORDHAU's rest pose to the animated pose.
	struct Clip
	{
		struct Track
		{
			std::string       skyrimNode;
			std::string       dirSkyrimNode;
			RE::NiPoint3      restDir;  // MORDHAU rest direction to the child bone, zero when unused
			std::vector<Quat> keys;     // one per frame
		};

		float              length = 0.0f;  // seconds
		std::int32_t       frames = 0;
		std::vector<Track> tracks;

		[[nodiscard]] Quat Sample(const Track& a_track, float a_time) const;

		static std::shared_ptr<Clip> Load(const std::filesystem::path& a_path);
	};
}
