#include "PCH.h"

#include "InventoryModels.h"
#include "Items.h"

#include <fstream>

namespace dm::InventoryModels
{
	namespace
	{
		std::atomic_bool g_on{ true };

		constexpr float kCmToUnits = 1.0f / 1.42875f;
		constexpr char  kFolder[] = "MordhauSkyrim";
		constexpr char  kMarker[] = "MordhauInventoryModel";

		struct Shape
		{
			std::vector<float>         positions, normals, uvs;  // Skyrim's weapon-model space, units
			std::vector<std::uint16_t> triangles;
			std::string                colour, mask;  // texture files in the cache folder of the prop: colour map, dye mask
			float                      tints[9]{ 1, 1, 1, 1, 1, 1, 1, 1, 1 };  // the dye for the mask's red, green, blue regions
		};
		struct Baked
		{
			std::vector<Shape> shapes;
			RE::NiPoint3       centre;
			float              radius = 0.0f;
		};

		// --- the prepared model (character.dmchar, written at the first start)
		bool ReadBaked(const std::filesystem::path& a_dir, Baked& a_out)
		{
			std::ifstream in(a_dir / "character.dmchar", std::ios::binary);
			char          magic[4]{};
			std::int32_t  head[5]{};  // version, bones, vertices, indices, sections
			in.read(magic, 4);
			in.read(reinterpret_cast<char*>(head), sizeof(head));
			if (!in || std::memcmp(magic, "DMCH", 4) != 0 || head[0] != 6 || head[1] < 0 || head[2] <= 0 || head[3] <= 0 || head[4] <= 0 || head[2] > 2000000 || head[3] > 6000000 || head[4] > 64) {
				return false;
			}
			in.seekg(static_cast<std::streamoff>(head[1]) * 96, std::ios::cur);  // bones: char[64], int32, float[4], float[3]
#pragma pack(push, 1)
			struct RawSection
			{
				std::int32_t first, count;
				char         texture[64], mask[64];
				float        tints[9];
				char         emblem[64];
				float        emblemValues[11];
				char         normal[64], surface[64], hair[64], beard[64];
				float        skin[4];
			};
			struct RawVertex
			{
				float        position[3], normal[3], uv[2];
				std::uint8_t bones[4], weights[4];
			};
#pragma pack(pop)
			static_assert(sizeof(RawSection) == 552 && sizeof(RawVertex) == 40);
			std::vector<RawSection>    sections(static_cast<std::size_t>(head[4]));
			std::vector<RawVertex>     vertices(static_cast<std::size_t>(head[2]));
			std::vector<std::uint32_t> indices(static_cast<std::size_t>(head[3]));
			in.read(reinterpret_cast<char*>(sections.data()), static_cast<std::streamsize>(sections.size() * sizeof(RawSection)));
			in.read(reinterpret_cast<char*>(vertices.data()), static_cast<std::streamsize>(vertices.size() * sizeof(RawVertex)));
			in.read(reinterpret_cast<char*>(indices.data()), static_cast<std::streamsize>(indices.size() * 4));
			if (!in) {
				return false;
			}
			RE::NiPoint3 low{ 1e9f, 1e9f, 1e9f }, high{ -1e9f, -1e9f, -1e9f };
			for (const auto& section : sections) {
				if (section.first < 0 || section.count <= 0 || static_cast<std::size_t>(section.first) + static_cast<std::size_t>(section.count) > indices.size()) {
					continue;
				}
				Shape                                           shape;
				std::unordered_map<std::uint32_t, std::uint16_t> seen;
				bool                                             full = false;
				for (std::int32_t i = 0; i + 2 < section.count && !full; i += 3) {
					std::uint16_t corner[3]{};
					for (int c = 0; c < 3; ++c) {
						const auto index = indices[static_cast<std::size_t>(section.first + i + c)];
						if (index >= vertices.size()) {
							return false;
						}
						auto found = seen.find(index);
						if (found == seen.end()) {
							if (seen.size() >= 65000) {
								full = true;  // (a Skyrim shape holds 65535 vertices: the rest of such a section is left out)
								break;
							}
							const auto& v = vertices[index];
							// MORDHAU's weapon space (x thin, y edge, z along; left-handed; cm) -> a Skyrim weapon model's
							// (y along, like Skyrim's own weapon models; right-handed; units)
							const float p[3] = { v.position[1] * kCmToUnits, v.position[2] * kCmToUnits, -v.position[0] * kCmToUnits };
							const float n[3] = { v.normal[1], v.normal[2], -v.normal[0] };
							shape.positions.insert(shape.positions.end(), p, p + 3);
							shape.normals.insert(shape.normals.end(), n, n + 3);
							shape.uvs.insert(shape.uvs.end(), v.uv, v.uv + 2);
							low = RE::NiPoint3{ std::min(low.x, p[0]), std::min(low.y, p[1]), std::min(low.z, p[2]) };
							high = RE::NiPoint3{ std::max(high.x, p[0]), std::max(high.y, p[1]), std::max(high.z, p[2]) };
							found = seen.emplace(index, static_cast<std::uint16_t>(seen.size())).first;
						}
						corner[c] = found->second;
					}
					if (!full) {
						// (the change of handedness turns every triangle over: two corners swapped)
						shape.triangles.insert(shape.triangles.end(), { corner[0], corner[2], corner[1] });
					}
				}
				shape.colour.assign(section.texture, strnlen(section.texture, 64));
				shape.mask.assign(section.mask, strnlen(section.mask, 64));
				std::memcpy(shape.tints, section.tints, sizeof(shape.tints));
				if (!shape.triangles.empty()) {
					a_out.shapes.push_back(std::move(shape));
				}
			}
			if (a_out.shapes.empty()) {
				return false;
			}
			a_out.centre = (low + high) * 0.5f;
			for (const auto& shape : a_out.shapes) {
				for (std::size_t i = 0; i + 2 < shape.positions.size(); i += 3) {
					a_out.radius = std::max(a_out.radius, (RE::NiPoint3{ shape.positions[i], shape.positions[i + 1], shape.positions[i + 2] } - a_out.centre).Length());
				}
			}
			return a_out.radius > 0.01f;
		}

		// --- a Skyrim Special Edition model file (NIF 20.2.0.7, user version 12, stream 100)
		struct Bytes
		{
			std::string data;
			template <class T>
			void Put(T a_value) { data.append(reinterpret_cast<const char*>(&a_value), sizeof(T)); }
			void Text(std::string_view a_text)  // (a string with its length ahead)
			{
				Put(static_cast<std::uint32_t>(a_text.size()));
				data.append(a_text);
			}
		};

		std::uint16_t Half(float a_value)
		{
			std::uint32_t bits = 0;
			std::memcpy(&bits, &a_value, 4);
			const std::uint32_t sign = (bits >> 16) & 0x8000u;
			const std::int32_t  exponent = static_cast<std::int32_t>((bits >> 23) & 0xFF) - 127 + 15;
			const std::uint32_t mantissa = bits & 0x7FFFFFu;
			if (exponent <= 0) {
				return static_cast<std::uint16_t>(sign);  // (too small: nought)
			}
			if (exponent >= 31) {
				return static_cast<std::uint16_t>(sign | 0x7BFFu);  // (too large: the largest)
			}
			return static_cast<std::uint16_t>(sign | (static_cast<std::uint32_t>(exponent) << 10) | (mantissa >> 13));
		}

		std::uint8_t AsByte(float a_value) { return static_cast<std::uint8_t>(std::clamp(std::lround((a_value + 1.0f) * 127.5f), 0l, 255l)); }

		void Object(Bytes& a_out, std::uint32_t a_name)  // what every scene object starts with
		{
			a_out.Put(a_name);                    // name (index into the file's strings)
			a_out.Put(std::uint32_t{ 0 });        // no extra data
			a_out.Put(std::int32_t{ -1 });        // no controller
			a_out.Put(std::uint32_t{ 0x0008000E });  // flags, as Skyrim's own models have them
			for (float v : { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f }) {
				a_out.Put(v);  // place, turn (none), scale
			}
			a_out.Put(std::int32_t{ -1 });  // no collision
		}

		std::string ModelFile(const std::string& a_name, const Baked& a_baked)
		{
			std::vector<std::string> strings{ a_name };
			std::vector<std::string> kinds{ "BSFadeNode", "BSTriShape", "BSLightingShaderProperty", "BSShaderTextureSet" };
			std::vector<std::uint16_t> kindOf;
			std::vector<Bytes>         blocks;
			const auto                 count = static_cast<std::int32_t>(a_baked.shapes.size());
			{
				Bytes root;
				Object(root, 0);
				root.Put(static_cast<std::uint32_t>(count));
				for (std::int32_t i = 0; i < count; ++i) {
					root.Put(1 + i * 3);  // its children: the shapes
				}
				root.Put(std::uint32_t{ 0 });  // no effects
				blocks.push_back(std::move(root));
				kindOf.push_back(0);
			}
			for (std::int32_t i = 0; i < count; ++i) {
				const auto& shape = a_baked.shapes[static_cast<std::size_t>(i)];
				const auto  vertices = shape.positions.size() / 3;
				strings.push_back(a_name + ":" + std::to_string(i));
				// a frame for the surface at each corner, from how the picture lies on the triangles
				std::vector<RE::NiPoint3> across(vertices), down(vertices);
				for (std::size_t t = 0; t + 2 < shape.triangles.size(); t += 3) {
					const std::size_t  a = shape.triangles[t], b = shape.triangles[t + 1], c = shape.triangles[t + 2];
					const auto         at = [&](std::size_t v) { return RE::NiPoint3{ shape.positions[v * 3], shape.positions[v * 3 + 1], shape.positions[v * 3 + 2] }; };
					const RE::NiPoint3 e1 = at(b) - at(a), e2 = at(c) - at(a);
					const float        u1 = shape.uvs[b * 2] - shape.uvs[a * 2], v1 = shape.uvs[b * 2 + 1] - shape.uvs[a * 2 + 1];
					const float        u2 = shape.uvs[c * 2] - shape.uvs[a * 2], v2 = shape.uvs[c * 2 + 1] - shape.uvs[a * 2 + 1];
					const float        area = u1 * v2 - u2 * v1;
					if (std::abs(area) < 1e-12f) {
						continue;
					}
					const RE::NiPoint3 alongU = (e1 * v2 - e2 * v1) / area, alongV = (e2 * u1 - e1 * u2) / area;
					for (const auto v : { a, b, c }) {
						across[v] = across[v] + alongU;
						down[v] = down[v] + alongV;
					}
				}
				Bytes mesh;
				Object(mesh, static_cast<std::uint32_t>(1 + i));
				mesh.Put(a_baked.centre.x), mesh.Put(a_baked.centre.y), mesh.Put(a_baked.centre.z), mesh.Put(a_baked.radius);
				mesh.Put(std::int32_t{ -1 });  // no skin
				mesh.Put(2 + i * 3);           // its shader
				mesh.Put(std::int32_t{ -1 });  // no alpha
				// what a corner holds: place (full floats), texture place, normal, tangent - 28 bytes
				mesh.Put(std::uint64_t{ 7ull | (4ull << 8) | (5ull << 16) | (6ull << 20) | (0x01Bull << 44) });
				mesh.Put(static_cast<std::uint16_t>(shape.triangles.size() / 3));
				mesh.Put(static_cast<std::uint16_t>(vertices));
				mesh.Put(static_cast<std::uint32_t>(vertices * 28 + shape.triangles.size() * 2));
				for (std::size_t v = 0; v < vertices; ++v) {
					RE::NiPoint3 normal{ shape.normals[v * 3], shape.normals[v * 3 + 1], shape.normals[v * 3 + 2] };
					normal = normal / std::max(normal.Length(), 1e-6f);
					RE::NiPoint3 u = across[v] - normal * across[v].Dot(normal);
					u = u.Length() > 1e-6f ? u / u.Length() : (std::abs(normal.z) < 0.9f ? normal.Cross(RE::NiPoint3{ 0.0f, 0.0f, 1.0f }) : normal.Cross(RE::NiPoint3{ 1.0f, 0.0f, 0.0f }));
					u = u / std::max(u.Length(), 1e-6f);
					RE::NiPoint3 w = normal.Cross(u);
					if (w.Dot(down[v]) < 0.0f) {
						w = RE::NiPoint3{ -w.x, -w.y, -w.z };
					}
					mesh.Put(shape.positions[v * 3]), mesh.Put(shape.positions[v * 3 + 1]), mesh.Put(shape.positions[v * 3 + 2]);
					mesh.Put(u.x);
					mesh.Put(Half(shape.uvs[v * 2])), mesh.Put(Half(shape.uvs[v * 2 + 1]));
					mesh.Put(AsByte(normal.x)), mesh.Put(AsByte(normal.y)), mesh.Put(AsByte(normal.z)), mesh.Put(AsByte(u.y));
					mesh.Put(AsByte(w.x)), mesh.Put(AsByte(w.y)), mesh.Put(AsByte(w.z)), mesh.Put(AsByte(u.z));
				}
				mesh.data.append(reinterpret_cast<const char*>(shape.triangles.data()), shape.triangles.size() * 2);
				mesh.Put(std::uint32_t{ 0 });  // no particle data
				blocks.push_back(std::move(mesh));
				kindOf.push_back(1);

				Bytes shader;
				shader.Put(std::uint32_t{ 0 });           // the plain kind
				shader.Put(std::uint32_t{ 0xFFFFFFFF });  // no name
				shader.Put(std::uint32_t{ 0 });
				shader.Put(std::int32_t{ -1 });
				shader.Put(std::uint32_t{ 0x82400301 });  // specular, takes and casts shadows, own emission, depth test
				shader.Put(std::uint32_t{ 0x00008011 });  // depth write, both sides drawn
				for (float v : { 0.0f, 0.0f, 1.0f, 1.0f }) {
					shader.Put(v);  // the picture as it is
				}
				shader.Put(3 + i * 3);  // its textures
				for (float v : { 0.0f, 0.0f, 0.0f, 1.0f }) {
					shader.Put(v);  // no glow
				}
				shader.Put(std::uint32_t{ 3 });  // the picture repeats
				for (float v : { 1.0f, 0.0f, 40.0f, 0.6f, 0.6f, 0.6f, 0.3f, 0.3f, 2.0f }) {
					shader.Put(v);  // solid, no refraction, gloss, shine colour and strength, the two lighting effects
				}
				blocks.push_back(std::move(shader));
				kindOf.push_back(2);

				Bytes textures;
				textures.Put(std::uint32_t{ 9 });
				textures.Text(std::string("textures\\") + kFolder + "\\" + a_name + "_" + std::to_string(i) + "_d.dds");
				textures.Text(std::string("textures\\") + kFolder + "\\flat_n.dds");
				for (int empty = 0; empty < 7; ++empty) {
					textures.Text("");
				}
				blocks.push_back(std::move(textures));
				kindOf.push_back(3);
			}
			Bytes file;
			file.data = "Gamebryo File Format, Version 20.2.0.7\n";
			file.Put(std::uint32_t{ 0x14020007 });
			file.Put(std::uint8_t{ 1 });
			file.Put(std::uint32_t{ 12 });
			file.Put(static_cast<std::uint32_t>(blocks.size()));
			file.Put(std::uint32_t{ 100 });
			for (int text = 0; text < 3; ++text) {
				file.Put(std::uint8_t{ 1 });  // author, and the two script names: empty
				file.Put(std::uint8_t{ 0 });
			}
			file.Put(static_cast<std::uint16_t>(kinds.size()));
			for (const auto& kind : kinds) {
				file.Text(kind);
			}
			for (const auto kind : kindOf) {
				file.Put(kind);
			}
			for (const auto& block : blocks) {
				file.Put(static_cast<std::uint32_t>(block.data.size()));
			}
			file.Put(static_cast<std::uint32_t>(strings.size()));
			std::size_t longest = 0;
			for (const auto& text : strings) {
				longest = std::max(longest, text.size());
			}
			file.Put(static_cast<std::uint32_t>(longest));
			for (const auto& text : strings) {
				file.Text(text);
			}
			file.Put(std::uint32_t{ 0 });  // no groups
			for (const auto& block : blocks) {
				file.data.append(block.data);
			}
			file.Put(std::uint32_t{ 1 });  // the footer: one root,
			file.Put(std::int32_t{ 0 });   // the first block (without it Skyrim does not load the file)
			return file.data;
		}

		// a 4 x 4 picture that says "the surface is as the model has it" (no relief), with a low shine
		std::string FlatNormalMap()
		{
			Bytes file;
			file.data = "DDS ";
			std::uint32_t head[31]{};
			head[0] = 124;                               // size of this header
			head[1] = 0x1 | 0x2 | 0x4 | 0x8 | 0x1000;    // has: caps, height, width, pitch, pixel format
			head[2] = 4, head[3] = 4;                    // height, width
			head[4] = 16;                                // bytes a row
			head[6] = 1;                                 // one size only
			head[18] = 32;                               // size of the pixel format
			head[19] = 0x41;                             // red-green-blue with alpha
			head[21] = 32;                               // bits a pixel
			head[22] = 0x00FF0000, head[23] = 0x0000FF00, head[24] = 0x000000FF, head[25] = 0xFF000000;
			head[26] = 0x1000;                           // a texture
			file.data.append(reinterpret_cast<const char*>(head), sizeof(head));
			for (int pixel = 0; pixel < 16; ++pixel) {
				file.data.append("\xFF\x80\x80\x40", 4);  // blue 255, green 128, red 128, shine 64
			}
			return file.data;
		}

		bool Write(const std::filesystem::path& a_file, const std::string& a_data)
		{
			std::error_code ec;
			std::filesystem::create_directories(a_file.parent_path(), ec);
			std::ofstream out(a_file, std::ios::binary | std::ios::trunc);
			out.write(a_data.data(), static_cast<std::streamsize>(a_data.size()));
			return static_cast<bool>(out);
		}

		// --- the colour map with MORDHAU's dye in it (the plugin's own shader dyes while it draws; Skyrim's cannot)
		struct Picture
		{
			int                       width = 0, height = 0;
			std::vector<std::uint8_t> pixels;  // red, green, blue, alpha
		};

		// A block-compressed picture as the cache holds it (the common kind only: DXT1); its largest size.
		bool ReadPicture(const std::filesystem::path& a_file, Picture& a_out)
		{
			std::ifstream in(a_file, std::ios::binary);
			std::string   data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
			if (data.size() < 128 || data.compare(0, 4, "DDS ") != 0 || data.compare(84, 4, "DXT1") != 0) {
				return false;
			}
			std::uint32_t height = 0, width = 0;
			std::memcpy(&height, data.data() + 12, 4);
			std::memcpy(&width, data.data() + 16, 4);
			if (width == 0 || height == 0 || width > 8192 || height > 8192 || (width & 3) || (height & 3) || data.size() < 128 + static_cast<std::size_t>(width / 4) * (height / 4) * 8) {
				return false;
			}
			a_out.width = static_cast<int>(width);
			a_out.height = static_cast<int>(height);
			a_out.pixels.assign(static_cast<std::size_t>(width) * height * 4, 255);
			const auto* block = reinterpret_cast<const std::uint8_t*>(data.data()) + 128;
			for (std::uint32_t by = 0; by < height; by += 4) {
				for (std::uint32_t bx = 0; bx < width; bx += 4, block += 8) {
					const std::uint16_t c0 = static_cast<std::uint16_t>(block[0] | (block[1] << 8)), c1 = static_cast<std::uint16_t>(block[2] | (block[3] << 8));
					std::uint8_t        colour[4][3]{};
					const auto          wide = [](std::uint16_t c, std::uint8_t* o) {
						o[0] = static_cast<std::uint8_t>(((c >> 11) & 31) * 255 / 31), o[1] = static_cast<std::uint8_t>(((c >> 5) & 63) * 255 / 63), o[2] = static_cast<std::uint8_t>((c & 31) * 255 / 31);
					};
					wide(c0, colour[0]);
					wide(c1, colour[1]);
					for (int k = 0; k < 3; ++k) {
						colour[2][k] = static_cast<std::uint8_t>(c0 > c1 ? (2 * colour[0][k] + colour[1][k]) / 3 : (colour[0][k] + colour[1][k]) / 2);
						colour[3][k] = static_cast<std::uint8_t>(c0 > c1 ? (colour[0][k] + 2 * colour[1][k]) / 3 : 0);
					}
					const std::uint32_t picks = block[4] | (block[5] << 8) | (block[6] << 16) | (static_cast<std::uint32_t>(block[7]) << 24);
					for (int y = 0; y < 4; ++y) {
						for (int x = 0; x < 4; ++x) {
							const auto pick = (picks >> (2 * (y * 4 + x))) & 3;
							auto*      out = &a_out.pixels[(static_cast<std::size_t>(by + y) * width + bx + x) * 4];
							out[0] = colour[pick][0], out[1] = colour[pick][1], out[2] = colour[pick][2];
						}
					}
				}
			}
			return true;
		}

		// The colour map of one part with its dye, at half its size, as a plain picture file Skyrim reads.
		bool WriteDyed(const std::filesystem::path& a_source, const Shape& a_shape, const std::filesystem::path& a_out)
		{
			Picture colour, mask;
			if (!ReadPicture(a_source / a_shape.colour, colour)) {
				return false;
			}
			const bool dyed = !a_shape.mask.empty() && ReadPicture(a_source / a_shape.mask, mask);
			float      tint[9];
			for (int k = 0; k < 9; ++k) {
				tint[k] = std::pow(std::max(a_shape.tints[k], 0.0f), 2.2f);
			}
			const int   width = std::max(colour.width / 2, 1), height = std::max(colour.height / 2, 1);
			std::uint32_t head[31]{};
			head[0] = 124, head[1] = 0x1 | 0x2 | 0x4 | 0x8 | 0x1000, head[2] = static_cast<std::uint32_t>(height), head[3] = static_cast<std::uint32_t>(width), head[4] = static_cast<std::uint32_t>(width) * 4, head[6] = 1;
			head[18] = 32, head[19] = 0x41, head[21] = 32, head[22] = 0x00FF0000, head[23] = 0x0000FF00, head[24] = 0x000000FF, head[25] = 0xFF000000, head[26] = 0x1000;
			std::string file = "DDS ";
			file.append(reinterpret_cast<const char*>(head), sizeof(head));
			file.reserve(file.size() + static_cast<std::size_t>(width) * height * 4);
			for (int y = 0; y < height; ++y) {
				for (int x = 0; x < width; ++x) {
					float sum[3]{};
					for (int dy = 0; dy < 2; ++dy) {
						for (int dx = 0; dx < 2; ++dx) {
							const int   sx = std::min(x * 2 + dx, colour.width - 1), sy = std::min(y * 2 + dy, colour.height - 1);
							const auto* c = &colour.pixels[(static_cast<std::size_t>(sy) * colour.width + sx) * 4];
							float       dye[3]{ 1.0f, 1.0f, 1.0f };
							if (dyed) {
								const int   mx = std::min(sx * mask.width / colour.width, mask.width - 1), my = std::min(sy * mask.height / colour.height, mask.height - 1);
								const auto* m = &mask.pixels[(static_cast<std::size_t>(my) * mask.width + mx) * 4];
								for (int region = 0; region < 3; ++region) {
									const float amount = m[region] / 255.0f;
									for (int k = 0; k < 3; ++k) {
										dye[k] *= 1.0f + (tint[region * 3 + k] - 1.0f) * amount;
									}
								}
							}
							for (int k = 0; k < 3; ++k) {
								// (the dye is in linear light, the picture is not: the factor is brought to the picture's scale)
								sum[k] += c[k] * std::pow(std::max(dye[k], 0.0f), 1.0f / 2.2f);
							}
						}
					}
					const char pixel[4] = { static_cast<char>(static_cast<std::uint8_t>(std::clamp(sum[2] * 0.25f, 0.0f, 255.0f))), static_cast<char>(static_cast<std::uint8_t>(std::clamp(sum[1] * 0.25f, 0.0f, 255.0f))),
						static_cast<char>(static_cast<std::uint8_t>(std::clamp(sum[0] * 0.25f, 0.0f, 255.0f))), static_cast<char>(-1) };
					file.append(pixel, 4);
				}
			}
			return Write(a_out, file);
		}

		std::filesystem::path DataFolder()
		{
			wchar_t exe[MAX_PATH]{};
			GetModuleFileNameW(nullptr, exe, MAX_PATH);
			return std::filesystem::path(exe).parent_path() / "Data";
		}

		// The files of one MORDHAU weapon ("w_Falx"): written if they are not there or older than the prepared model.
		// File work only - nothing of the game is touched, so this may run beside it.
		void WriteFiles(const std::string& a_prop, const std::filesystem::path& a_cache, const std::filesystem::path& a_data)
		{
			const auto      source = a_cache / "props" / a_prop;
			const auto      file = a_data / "meshes" / kFolder / (a_prop + ".nif");
			const auto      pictures = a_data / "textures" / kFolder;
			std::error_code ec;
			const auto      made = std::filesystem::last_write_time(source / "character.dmchar", ec);
			if (ec || (std::filesystem::exists(file, ec) && std::filesystem::last_write_time(file, ec) >= made)) {
				return;
			}
			Baked baked;
			if (!ReadBaked(source, baked)) {
				return;
			}
			if (!std::filesystem::exists(pictures / "flat_n.dds", ec)) {
				Write(pictures / "flat_n.dds", FlatNormalMap());
			}
			for (std::size_t i = 0; i < baked.shapes.size(); ++i) {
				const auto out = pictures / (a_prop + "_" + std::to_string(i) + "_d.dds");
				if (!WriteDyed(source, baked.shapes[i], out)) {
					// (a colour map of another kind than this reads: as it is, undyed)
					std::filesystem::create_directories(pictures, ec);
					std::filesystem::copy_file(source / baked.shapes[i].colour, out, std::filesystem::copy_options::overwrite_existing, ec);
				}
			}
			// (the model file last: it is what says "these files are complete")
			if (Write(file, ModelFile(a_prop, baked))) {
				logger::info("inventory: MORDHAU's model of {} written as a Skyrim model file ({} part(s)); Skyrim shows it from its next start", a_prop, baked.shapes.size());
			}
		}

		std::string PropOf(const char* a_weapon)
		{
			std::string_view leaf(a_weapon);
			if (const auto at = leaf.rfind("BP_"); at != std::string_view::npos) {
				leaf = leaf.substr(at + 3);
			}
			return "w_" + std::string(leaf);
		}

		struct Ready
		{
			bool                      thereAtStart = false, tried = false;
			RE::NiPointer<RE::NiNode> model;
			RE::NiPoint3              centre;
			float                     radius = 0.0f;
		};
		std::unordered_map<std::string, Ready> g_models;
		bool                                   g_prepared = false;

		// Once, when the MORDHAU items are known: which of their model files are on disk now (Skyrim only finds files
		// that were there when it started), and the missing or outdated ones written in the background for next time.
		void PrepareAll(const std::filesystem::path& a_cache)
		{
			const auto&              items = Items::All();
			std::error_code          ec;
			if (items.empty() || !std::filesystem::exists(a_cache / "baked.txt", ec)) {
				return;  // (not yet: the items, or the prepared models, are not there)
			}
			g_prepared = true;
			const auto               data = DataFolder();
			std::vector<std::string> props;
			int                      there = 0;
			for (const auto* item : items) {
				const char* weapon = Items::MordhauWeaponOf(item);
				if (!weapon) {
					continue;
				}
				const auto prop = PropOf(weapon);
				if (g_models.contains(prop)) {
					continue;
				}
				const auto file = data / "meshes" / kFolder / (prop + ".nif");
				const auto made = std::filesystem::last_write_time(a_cache / "props" / prop / "character.dmchar", ec);
				auto&      ready = g_models[prop];
				ready.thereAtStart = !ec && std::filesystem::exists(file, ec) && std::filesystem::last_write_time(file, ec) >= made;
				there += ready.thereAtStart ? 1 : 0;
				props.push_back(prop);
			}
			logger::info("inventory: {} of {} MORDHAU weapons have their own model file for the inventory; the others are written now and show from Skyrim's next start", there, props.size());
			std::thread([props, cache = a_cache, data] {
				for (const auto& prop : props) {
					try {
						WriteFiles(prop, cache, data);
					} catch (const std::exception&) {
					}
				}
			}).detach();
		}

		Ready* ModelOf(const std::string& a_prop, const std::filesystem::path& a_cache)
		{
			const auto found = g_models.find(a_prop);
			if (found == g_models.end() || !found->second.thereAtStart) {
				return nullptr;
			}
			auto& ready = found->second;
			if (ready.tried) {
				return &ready;
			}
			ready.tried = true;
			Baked baked;
			if (!ReadBaked(a_cache / "props" / a_prop, baked)) {
				return &ready;
			}
			RE::NiPointer<RE::NiNode>         model;
			RE::BSModelDB::DBTraits::ArgsType args;
			const auto                        path = std::string(kFolder) + "\\" + a_prop + ".nif";
			const auto                        result = RE::BSModelDB::Demand(path.c_str(), model, args);
			if (!model) {
				logger::warn("inventory: Skyrim did not load '{}' (code {}): the borrowed Skyrim model stays", path, static_cast<int>(result));
				return &ready;
			}
			ready.model = model;
			ready.centre = baked.centre;
			ready.radius = baked.radius;
			logger::info("inventory: MORDHAU's model of {} is shown in the inventory", a_prop);
			return &ready;
		}
	}

	void SetEnabled(bool a_on)
	{
		g_on.store(a_on);
	}

	namespace
	{
		// What the item menu that is open has selected (inventory, container, barter, gift); nullptr when none.
		RE::TESBoundObject* Selected()
		{
			auto* ui = RE::UI::GetSingleton();
			if (!ui) {
				return nullptr;
			}
			RE::ItemList* list = nullptr;
			if (const auto menu = ui->GetMenu<RE::InventoryMenu>()) {
				list = menu->GetRuntimeData().itemList;
			} else if (const auto container = ui->GetMenu<RE::ContainerMenu>()) {
				list = container->GetRuntimeData().itemList;
			} else if (const auto barter = ui->GetMenu<RE::BarterMenu>()) {
				list = barter->GetRuntimeData().itemList;
			} else if (const auto gift = ui->GetMenu<RE::GiftMenu>()) {
				list = gift->GetRuntimeData().itemList;
			}
			auto* item = list ? list->GetSelectedItem() : nullptr;
			return item && item->data.objDesc ? item->data.objDesc->object : nullptr;
		}

		// A preview model of Skyrim's with MORDHAU's hung in it: what was hidden and what was added, to undo it.
		struct Changed
		{
			RE::NiPointer<RE::NiNode>                node;
			RE::NiPointer<RE::NiAVObject>            shown;
			std::vector<RE::NiPointer<RE::NiAVObject>> hidden;
			std::string                              prop;
		};
		std::vector<Changed> g_changed;

		void Undo(Changed& a_change)
		{
			a_change.node->DetachChild2(a_change.shown.get());
			for (auto& child : a_change.hidden) {
				child->SetAppCulled(false);
			}
			RE::NiUpdateData update;
			a_change.node->Update(update);
		}
	}

	void Update(const std::filesystem::path& a_cache)
	{
		if (!g_on.load() || a_cache.empty()) {
			return;
		}
		if (!g_prepared) {
			PrepareAll(a_cache);
			return;
		}
		auto* manager = RE::Inventory3DManager::GetSingleton();
		if (!manager) {
			return;
		}
		// (Skyrim keeps a loaded preview model and shows it again for every item with the same model file - and its
		// own note of which item it belongs to stays the first one's. So the item is taken from the menu's selection.)
		auto*             selected = Selected();
		const char*       weapon = selected ? Items::MordhauWeaponOf(selected) : nullptr;
		const std::string wanted = weapon ? PropOf(weapon) : std::string();
		auto&             loaded = manager->GetRuntimeData().loadedModels;
		const auto        showing = [&](const RE::NiNode* a_node) {
			for (auto& entry : loaded) {
				if (entry.spModel.get() == a_node) {
					return true;
				}
			}
			return false;
		};
		// whatever hangs somewhere it is no longer wanted comes off, and Skyrim's model is as it was
		for (auto it = g_changed.begin(); it != g_changed.end();) {
			if (it->prop != wanted || !showing(it->node.get())) {
				Undo(*it);
				it = g_changed.erase(it);
			} else {
				++it;
			}
		}
		if (wanted.empty()) {
			return;
		}
		auto* ready = ModelOf(wanted, a_cache);
		if (!ready || !ready->model || ready->radius <= 0.0f) {
			return;
		}
		for (auto& entry : loaded) {
			auto* node = entry.spModel ? entry.spModel->AsNode() : nullptr;
			if (!node || std::any_of(g_changed.begin(), g_changed.end(), [&](const Changed& a_change) { return a_change.node.get() == node; })) {
				continue;
			}
			auto* copy = ready->model->Clone();
			auto* shown = copy ? copy->AsNode() : nullptr;
			if (!shown) {
				continue;
			}
			// MORDHAU's model where the borrowed one is and as large as it is: the preview has already been set up for
			// that one's size
			RE::NiPoint3 centre{};
			float        radius = entry.boundRadius;
			if (node->worldBound.radius > 0.1f) {
				const float scale = std::max(node->world.scale, 1e-4f);
				centre = (node->world.rotate.Transpose() * (node->worldBound.center - node->world.translate)) / scale;
				radius = node->worldBound.radius / scale;
			}
			if (radius <= 0.1f) {
				continue;
			}
			Changed change;
			change.node.reset(node);
			change.shown.reset(shown);
			change.prop = wanted;
			for (auto& child : node->GetChildren()) {
				if (child && !child->GetAppCulled()) {
					child->SetAppCulled(true);
					change.hidden.emplace_back(child.get());
				}
			}
			const float fit = radius / ready->radius;
			shown->name = kMarker;
			shown->local.scale = fit;
			shown->local.translate = centre - ready->centre * fit;
			node->AttachChild(shown);
			RE::NiUpdateData update;
			node->Update(update);
			g_changed.push_back(std::move(change));
		}
	}
}
