#pragma once

// Reads MORDHAU's own files from the player's install and writes this mod's local cache (models as
// character.dmchar, textures as .dds copied block for block): what tools/*.py did before the plugin could.
// Nothing is authored here and nothing leaves the player's PC. Standard C++ only (no Skyrim, no Windows types),
// so the same code could also build into a small stand-alone program.
//
// Ported from the first Python versions of these readers (PakIndex, Package, Texture, the mesh readers, Bake).

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mbake
{
	// Every file in MORDHAU's .pak archives (pak v11), by path. Only uncompressed entries can be read (all of
	// MORDHAU's cooked assets are stored that way).
	class PakIndex
	{
	public:
		// a_paks: <MORDHAU>/Mordhau/Content/Paks. Returns the number of files found (0 = nothing usable).
		std::size_t Open(const std::filesystem::path& a_paks);
		// The .pak files in that folder that could not be read as MORDHAU's own (name and why): left out.
		[[nodiscard]] const std::vector<std::string>& Skipped() const { return _skipped; }
		// The file whose path ends with a_suffix (e.g. "Mordhau/Assets/Weapons/Longbow/Arrow/LongbowArrow.uasset"); throws when
		// there is none or it is stored compressed.
		[[nodiscard]] std::vector<char> Blob(std::string_view a_suffix) const;
		[[nodiscard]] bool              Has(std::string_view a_suffix) const;
		// The package holding an asset known only by its own name ("BP_KettleHelmet"): the first file called
		// <name>.uasset, as a path under Content without the extension; "" when there is none.
		[[nodiscard]] std::string FindByName(std::string_view a_name) const;

	private:
		struct Entry
		{
			std::uint32_t pak = 0;
			std::uint64_t offset = 0;
		};
		[[nodiscard]] const Entry* Find(std::string_view a_suffix) const;

		std::vector<std::filesystem::path>     _paks;
		std::vector<std::string>               _skipped;
		std::vector<std::string>               _paths;    // in archive order (the first match wins, as in the tools)
		std::vector<Entry>                     _entries;
		std::unordered_map<std::string, std::size_t> _byContentPath;  // the part after ".../Content/"
		std::unordered_map<std::string, std::size_t> _byLeaf;         // file name without .uasset -> first such file
	};

	// One tagged property of an object, undecoded.
	struct Property
	{
		std::string name, type, extra, extra2;  // extra: struct name / enum / inner type; extra2: a map's value type
		std::uint8_t      flag = 0;             // a BoolProperty's value
		std::vector<char> raw;                  // its bytes
	};

	// A decoded property value, as far as the bakers need.
	struct Value
	{
		enum class Kind { kNone, kNumber, kBool, kText, kList, kMap };
		Kind                                       kind = Kind::kNone;
		double                                     number = 0.0;
		bool                                       flag = false;
		std::string                                text;
		std::vector<Value>                         list;
		std::vector<std::pair<std::string, Value>> map;

		[[nodiscard]] const Value* Get(std::string_view a_key) const
		{
			for (const auto& [key, value] : map) {
				if (key == a_key) {
					return &value;
				}
			}
			return nullptr;
		}
		[[nodiscard]] bool IsText() const { return kind == Kind::kText; }
		[[nodiscard]] bool IsList() const { return kind == Kind::kList; }
		// "truthy" as the Python tools used it
		[[nodiscard]] bool Truthy() const
		{
			return kind == Kind::kBool ? flag : kind == Kind::kNumber ? number != 0.0 : kind == Kind::kText ? !text.empty() : kind == Kind::kList ? !list.empty() : kind == Kind::kMap ? !map.empty() : false;
		}
	};

	// A cooked, unversioned UE4 package: <base>.uasset (names, imports, exports) + <base>.uexp (the objects' data).
	class Package
	{
	public:
		Package(const PakIndex& a_paks, std::string_view a_base);

		struct Export
		{
			std::string  name;
			std::int32_t classIndex = 0, superIndex = 0, outer = 0;
			std::int64_t size = 0, offset = 0;  // offset into data()
		};
		struct Import
		{
			std::string  className, name;
			std::int32_t outer = 0;
		};

		[[nodiscard]] const std::vector<char>&        data() const { return _uexp; }
		[[nodiscard]] const std::vector<std::string>& names() const { return _names; }
		[[nodiscard]] const std::vector<Export>&      exports() const { return _exports; }
		[[nodiscard]] const std::vector<Import>&      imports() const { return _imports; }
		[[nodiscard]] std::int32_t                    headerSize() const { return _headerSize; }

		// An FName at a_offset of the export data (advances a_offset by 8).
		[[nodiscard]] std::string Name(std::size_t& a_offset) const;
		// The name an object reference stands for: > 0 an export, < 0 an import, 0 nothing ("").
		[[nodiscard]] std::string Object(std::int32_t a_index) const;
		// The tagged properties starting at a_offset; a_offset ends up after their "None" terminator.
		[[nodiscard]] std::vector<Property> Properties(std::size_t& a_offset) const;
		// The same inside any block of this package's bytes (a struct's or an array element's own properties).
		[[nodiscard]] std::vector<Property> Properties(const std::vector<char>& a_bytes, std::size_t& a_offset) const;
		[[nodiscard]] std::string           Name(const std::vector<char>& a_bytes, std::size_t& a_offset) const;
		[[nodiscard]] const Export*         FindExport(std::string_view a_name) const;
		// A property's value, decoded.
		[[nodiscard]] Value Decode(const Property& a_property) const;
		// The properties of the export with this name (throws when there is none).
		[[nodiscard]] std::vector<Property> Defaults(std::string_view a_export) const;
		// The package path of import number a_index (0-based): its outermost object, e.g. /Game/UMA/.../T_Foo.
		[[nodiscard]] std::string ImportPath(std::size_t a_index) const;
		// The package path of the Texture2D import called a_object, without the leading /Game/; "" when there is none.
		[[nodiscard]] std::string TexturePath(std::string_view a_object) const;

	private:
		std::vector<char>        _uexp;
		std::vector<std::string> _names;
		std::vector<Export>      _exports;
		std::vector<Import>      _imports;
		std::int32_t             _headerSize = 0;
	};

	// A Texture2D as stored: block-compressed mips, exactly what a graphics card takes.
	struct Texture
	{
		struct Mip
		{
			std::int32_t      width = 0, height = 0;
			std::vector<char> data;
		};
		std::string      name, format;  // format: MORDHAU's pixel format name ("PF_DXT1", "PF_DXT5", "PF_BC5", ...)
		std::int32_t     width = 0, height = 0;
		std::vector<Mip> mips;
	};
	[[nodiscard]] Texture ReadTexture(const PakIndex& a_paks, std::string_view a_base);
	// Copies the texture's mips no larger than a_maxSide into <a_out>/<name>_<crc of path>.dds; returns the file name.
	std::string WriteDds(const PakIndex& a_paks, std::string_view a_base, const std::filesystem::path& a_out, int a_maxSide);

	// A model's LOD 0, rigged (SkeletalMesh) or solid (StaticMesh: one bone "root", everything on it).
	struct Mesh
	{
		struct Section
		{
			std::int32_t  material = 0;
			std::uint32_t baseIndex = 0, triangles = 0, baseVertex = 0;
			std::int32_t  vertices = 0;
		};
		struct Bone
		{
			std::string  name;
			std::int32_t parent = -1;
			float        rotation[4]{ 0, 0, 0, 1 };
			float        translation[3]{};
		};
		std::string                                      name;
		std::vector<std::pair<std::string, std::string>> materials;  // (material, slot)
		std::vector<Bone>                                bones;
		std::vector<float>                               positions;  // x 3
		std::vector<float>                               normals;    // x 3
		std::vector<float>                               uvs;        // x 2 (the first set)
		std::vector<std::int32_t>                        boneIds;    // x 4, indices into bones
		std::vector<double>                              weights;    // x 4, summing to 1
		std::vector<std::int64_t>                        indices;    // x 3
		std::vector<Section>                             sections;
		[[nodiscard]] std::size_t                        vertexCount() const { return positions.size() / 3; }
	};
	[[nodiscard]] Mesh ReadSkeletalMesh(const PakIndex& a_paks, std::string_view a_base);
	[[nodiscard]] Mesh ReadStaticMesh(const PakIndex& a_paks, std::string_view a_base);
	// Rigged if it reads as one, else solid.
	[[nodiscard]] Mesh ReadAnyMesh(const PakIndex& a_paks, std::string_view a_base);

	// One model with the maps to draw it with.
	struct Piece
	{
		std::string model;
		std::string colourMap;                         // for every section...
		std::map<std::string, std::string> byMaterial; // ...unless given per material here (then colourMap is unused)
		bool        perMaterial = false;
		std::string patternMask, normalMap, surfaceMap;
		float       tints[9]{ 1, 1, 1, 1, 1, 1, 1, 1, 1 };
		bool        weapon = false;  // vertices stay in the weapon's own space, on the slot after the bones
		// the human body under the clothes: skin / head / eye sections are marked, MORDHAU's hair and beard layers are
		// written, and triangles of body parts an item covers ("Chest", "LeftArm", "Hair", ...) are left out
		bool                     body = false;
		std::vector<std::string> hide;
		// the mercenary's emblem on this piece ("" = none): where it sits in the piece's texture space, its two colours
		std::string emblem;
		float       emblemValues[11]{};  // centre x y, size x y, rotation (turns), colour 1 rgb, colour 2 rgb
	};

	// Writes character.dmchar (version 6) and the textures it names into a_out. a_cap: largest texture side.
	// a_log gets the same lines the Python baker printed. Throws on a model that cannot be read at all.
	void BakePieces(const PakIndex& a_paks, const std::vector<Piece>& a_pieces, const std::filesystem::path& a_out, int a_cap, std::string& a_log);
}
