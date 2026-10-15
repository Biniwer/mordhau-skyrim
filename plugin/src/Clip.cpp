#include "Clip.h"

#include <fstream>

namespace dm
{
	Quat Clip::Sample(const Track& a_track, float a_time) const
	{
		if (a_track.keys.empty()) {
			return {};
		}
		if (a_track.keys.size() == 1 || length <= 0.0f) {
			return a_track.keys.front();
		}
		const float f = std::clamp(a_time / length, 0.0f, 1.0f) * static_cast<float>(a_track.keys.size() - 1);
		const auto  i = std::min(static_cast<std::size_t>(f), a_track.keys.size() - 2);
		return Nlerp(a_track.keys[i], a_track.keys[i + 1], f - static_cast<float>(i));
	}

	std::shared_ptr<Clip> Clip::Load(const std::filesystem::path& a_path)
	{
		std::ifstream in(a_path, std::ios::binary);
		if (!in) {
			return nullptr;
		}
		char         magic[4]{};
		std::int32_t bones = 0;
		auto         clip = std::make_shared<Clip>();
		in.read(magic, 4);
		in.read(reinterpret_cast<char*>(&clip->length), 4);
		in.read(reinterpret_cast<char*>(&clip->frames), 4);
		in.read(reinterpret_cast<char*>(&bones), 4);
		if (!in || std::memcmp(magic, "DMC1", 4) != 0 || clip->frames <= 0 || clip->frames > 100000 || bones <= 0 || bones > 512) {
			return nullptr;
		}
		clip->tracks.resize(static_cast<std::size_t>(bones));
		for (auto& track : clip->tracks) {
			char  node[64]{}, dirNode[64]{};
			float dir[3]{};
			in.read(node, 64);
			in.read(dirNode, 64);
			in.read(reinterpret_cast<char*>(dir), 12);
			node[63] = dirNode[63] = '\0';
			track.skyrimNode = node;
			track.dirSkyrimNode = dirNode;
			track.restDir = { dir[0], dir[1], dir[2] };
			track.keys.resize(static_cast<std::size_t>(clip->frames));
			in.read(reinterpret_cast<char*>(track.keys.data()), static_cast<std::streamsize>(track.keys.size() * sizeof(Quat)));
		}
		return in ? clip : nullptr;
	}
}
