#include "bake/Bake.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace mbake
{
	namespace
	{
		[[noreturn]] void Fail(const std::string& a_what)
		{
			throw std::runtime_error(a_what);
		}

		// A bounds-checked cursor over bytes (little endian).
		struct Reader
		{
			const std::vector<char>& bytes;
			std::size_t              at = 0;

			void Need(std::size_t a_count) const
			{
				if (at + a_count > bytes.size()) {
					Fail("read past the end of the data");
				}
			}
			template <class T>
			T Get()
			{
				Need(sizeof(T));
				T value;
				std::memcpy(&value, bytes.data() + at, sizeof(T));
				at += sizeof(T);
				return value;
			}
			template <class T>
			T Peek(std::size_t a_offset) const
			{
				if (a_offset + sizeof(T) > bytes.size()) {
					Fail("read past the end of the data");
				}
				T value;
				std::memcpy(&value, bytes.data() + a_offset, sizeof(T));
				return value;
			}
			void Skip(std::size_t a_count)
			{
				Need(a_count);
				at += a_count;
			}
			// An FString: int32 length (negative = UTF-16), the characters, a terminator.
			std::string String()
			{
				const auto n = Get<std::int32_t>();
				std::string out;
				if (n >= 0) {
					Need(static_cast<std::size_t>(n));
					out.assign(bytes.data() + at, static_cast<std::size_t>(std::max(n - 1, 0)));
					at += static_cast<std::size_t>(n);
				} else {
					const auto count = static_cast<std::size_t>(-n);
					Need(count * 2);
					for (std::size_t i = 0; i + 1 < count; ++i) {
						out.push_back(bytes[at + i * 2]);  // (names here are plain ASCII)
					}
					at += count * 2;
				}
				return out;
			}
		};

		std::vector<char> ReadAt(const std::filesystem::path& a_file, std::uint64_t a_offset, std::size_t a_size)
		{
			std::ifstream in(a_file, std::ios::binary);
			if (!in) {
				Fail("cannot open " + a_file.string());
			}
			in.seekg(static_cast<std::streamoff>(a_offset));
			std::vector<char> out(a_size);
			in.read(out.data(), static_cast<std::streamsize>(a_size));
			if (static_cast<std::size_t>(in.gcount()) != a_size) {
				Fail("short read from " + a_file.string());
			}
			return out;
		}

		std::uint32_t Crc32(std::string_view a_text)
		{
			std::uint32_t crc = 0xFFFFFFFFu;
			for (const unsigned char c : a_text) {
				crc ^= c;
				for (int k = 0; k < 8; ++k) {
					crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
				}
			}
			return ~crc;
		}

		float HalfToFloat(std::uint16_t a_half)
		{
			const std::uint32_t sign = (a_half >> 15) & 1u, exponent = (a_half >> 10) & 0x1Fu, mantissa = a_half & 0x3FFu;
			std::uint32_t       bits = 0;
			if (exponent == 0) {
				if (mantissa == 0) {
					bits = sign << 31;
				} else {
					int           e = -1;
					std::uint32_t m = mantissa;
					do {
						++e;
						m <<= 1;
					} while ((m & 0x400u) == 0);
					bits = (sign << 31) | (static_cast<std::uint32_t>(127 - 15 - e) << 23) | ((m & 0x3FFu) << 13);
				}
			} else if (exponent == 31) {
				bits = (sign << 31) | 0x7F800000u | (mantissa << 13);
			} else {
				bits = (sign << 31) | ((exponent + 127 - 15) << 23) | (mantissa << 13);
			}
			float out;
			std::memcpy(&out, &bits, 4);
			return out;
		}

		std::string LeafName(std::string_view a_base)
		{
			const auto slash = a_base.find_last_of('/');
			return std::string(slash == std::string_view::npos ? a_base : a_base.substr(slash + 1));
		}
	}

	// ------------------------------------------------------------------ the archives

	std::size_t PakIndex::Open(const std::filesystem::path& a_paks)
	{
		_paks.clear(), _paths.clear(), _entries.clear(), _byContentPath.clear(), _byLeaf.clear();
		std::vector<std::filesystem::path> files;
		std::error_code                    ec;
		for (const auto& entry : std::filesystem::directory_iterator(a_paks, ec)) {
			if (entry.is_regular_file() && entry.path().extension() == ".pak") {
				files.push_back(entry.path());
			}
		}
		std::sort(files.begin(), files.end());
		_skipped.clear();
		for (const auto& file : files) {
			// (one archive at a time: a file in the folder that is not one of MORDHAU's own - a mod's .pak in another
			// format - is left out with a note; before, the first such file ended the whole preparation with
			// 'read past the end of the data', seen in a player's log)
			const auto paksBefore = _paks.size();
			const auto pathsBefore = _paths.size();
			try {
				const auto size = std::filesystem::file_size(file, ec);
				if (ec || size < 400) {
					continue;
				}
				const auto tail = ReadAt(file, size - 400, 400);
				// the footer: magic 0x5A6F12E1, version, index offset, index size
				std::size_t magic = std::string::npos;
				for (std::size_t i = tail.size() - 4;; --i) {
					if (static_cast<unsigned char>(tail[i]) == 0xE1 && static_cast<unsigned char>(tail[i + 1]) == 0x12 && static_cast<unsigned char>(tail[i + 2]) == 0x6F &&
						static_cast<unsigned char>(tail[i + 3]) == 0x5A) {
						magic = i;
						break;
					}
					if (i == 0) {
						break;
					}
				}
				if (magic == std::string::npos) {
					continue;
				}
				std::uint64_t indexOffset = 0, indexSize = 0;
				std::memcpy(&indexOffset, tail.data() + magic + 8, 8);
				std::memcpy(&indexSize, tail.data() + magic + 16, 8);
				const auto index = ReadAt(file, indexOffset, static_cast<std::size_t>(indexSize));
				Reader     r{ index };
				const auto mount = r.String();
				r.Skip(4 + 8);  // file count, path hash seed
				if (r.Get<std::int32_t>()) {
					r.Skip(36);  // path hash index: offset, size, hash
				}
				r.Skip(4);  // has a full directory index
				const auto directoryOffset = r.Get<std::int64_t>();
				const auto directorySize = r.Get<std::int64_t>();
				r.Skip(20);
				const auto        encodedSize = r.Get<std::int32_t>();
				const std::size_t encoded = r.at;
				r.Need(static_cast<std::size_t>(encodedSize));
				const auto directory = ReadAt(file, static_cast<std::uint64_t>(directoryOffset), static_cast<std::size_t>(directorySize));
				Reader     d{ directory };
				const auto pakNumber = static_cast<std::uint32_t>(_paks.size());
				_paks.push_back(file);
				const auto directories = d.Get<std::int32_t>();
				for (std::int32_t i = 0; i < directories; ++i) {
					const auto folder = d.String();
					const auto count = d.Get<std::int32_t>();
					for (std::int32_t k = 0; k < count; ++k) {
						const auto name = d.String();
						const auto where = d.Get<std::int32_t>();
						if (where < 0) {
							continue;
						}
						// an encoded entry: flags, then (maybe) a block size, the offset (32 or 64 bit), sizes
						std::size_t q = encoded + static_cast<std::size_t>(where);
						const auto  flags = r.Peek<std::uint32_t>(q);
						q += 4;
						if ((flags & 0x3Fu) == 0x3Fu) {
							q += 4;
						}
						const std::uint64_t offset = (flags >> 31) ? r.Peek<std::uint32_t>(q) : r.Peek<std::uint64_t>(q);
						std::string         path = folder != "/" ? mount + folder + name : mount + name;
						if (const auto content = path.find("/Content/"); content != std::string::npos) {
							_byContentPath.try_emplace(path.substr(content + 9), _paths.size());
						}
						if (path.size() > 7 && path.compare(path.size() - 7, 7, ".uasset") == 0) {
							const auto slash = path.find_last_of('/');
							_byLeaf.try_emplace(path.substr(slash + 1, path.size() - 7 - slash - 1), _paths.size());
						}
						_paths.push_back(std::move(path));
						_entries.push_back({ pakNumber, offset });
					}
				}
			} catch (const std::exception& ex) {
				_paks.resize(paksBefore), _paths.resize(pathsBefore), _entries.resize(pathsBefore);
				std::erase_if(_byContentPath, [&](const auto& a_item) { return a_item.second >= pathsBefore; });
				std::erase_if(_byLeaf, [&](const auto& a_item) { return a_item.second >= pathsBefore; });
				_skipped.push_back(file.filename().string() + " (" + ex.what() + ")");
			}
		}
		return _paths.size();
	}

	const PakIndex::Entry* PakIndex::Find(std::string_view a_suffix) const
	{
		if (const auto it = _byContentPath.find(std::string(a_suffix)); it != _byContentPath.end()) {
			return &_entries[it->second];
		}
		for (std::size_t i = 0; i < _paths.size(); ++i) {
			const auto& path = _paths[i];
			if (path.size() >= a_suffix.size() && std::string_view(path).substr(path.size() - a_suffix.size()) == a_suffix) {
				return &_entries[i];
			}
		}
		return nullptr;
	}

	bool PakIndex::Has(std::string_view a_suffix) const
	{
		return Find(a_suffix) != nullptr;
	}

	std::string PakIndex::FindByName(std::string_view a_name) const
	{
		const auto it = _byLeaf.find(std::string(a_name));
		if (it == _byLeaf.end()) {
			return {};
		}
		// (the part after the LAST "Content/", without ".uasset")
		const auto& path = _paths[it->second];
		const auto  content = path.rfind("Content/");
		const auto  from = content == std::string::npos ? 0 : content + 8;
		return path.substr(from, path.size() - 7 - from);
	}

	std::vector<char> PakIndex::Blob(std::string_view a_suffix) const
	{
		const auto* entry = Find(a_suffix);
		if (!entry) {
			Fail("MORDHAU has no file " + std::string(a_suffix));
		}
		// the entry's own header: offset, size, uncompressed size, method, hash, ... then the data
		const auto&   file = _paks[entry->pak];
		const auto    header = ReadAt(file, entry->offset, 48);
		std::uint64_t size = 0;
		std::uint32_t method = 0;
		std::memcpy(&size, header.data() + 16, 8);
		std::memcpy(&method, header.data() + 24, 4);
		if (method != 0) {
			Fail(std::string(a_suffix) + " is stored compressed");
		}
		return ReadAt(file, entry->offset + 48 + 1 + 4, static_cast<std::size_t>(size));
	}

	// ------------------------------------------------------------------ packages

	Package::Package(const PakIndex& a_paks, std::string_view a_base)
	{
		const auto header = a_paks.Blob(std::string(a_base) + ".uasset");
		_uexp = a_paks.Blob(std::string(a_base) + ".uexp");
		Reader r{ header };
		if (r.Get<std::uint32_t>() != 0x9E2A83C1u) {
			Fail(std::string(a_base) + " is not a package");
		}
		r.Skip(16);  // legacy version, UE3, UE4, licensee
		if (r.Get<std::int32_t>() != 0) {
			Fail(std::string(a_base) + " has custom versions (not a cooked, unversioned package)");
		}
		_headerSize = r.Get<std::int32_t>();
		(void)r.String();  // folder name
		r.Skip(4);         // package flags
		const auto nameCount = r.Get<std::int32_t>();
		const auto nameOffset = r.Get<std::int32_t>();
		r.Skip(8);  // gatherable text
		const auto exportCount = r.Get<std::int32_t>();
		const auto exportOffset = r.Get<std::int32_t>();
		const auto importCount = r.Get<std::int32_t>();
		const auto importOffset = r.Get<std::int32_t>();
		r.at = static_cast<std::size_t>(nameOffset);
		for (std::int32_t i = 0; i < nameCount; ++i) {
			_names.push_back(r.String());
			r.Skip(4);  // hashes
		}
		const auto named = [&](std::int32_t a_index, std::int32_t a_number) {
			if (a_index < 0 || static_cast<std::size_t>(a_index) >= _names.size()) {
				Fail("name index out of range");
			}
			return a_number ? _names[static_cast<std::size_t>(a_index)] + "_" + std::to_string(a_number - 1) : _names[static_cast<std::size_t>(a_index)];
		};
		for (std::int32_t i = 0; i < exportCount; ++i) {
			const std::size_t q = static_cast<std::size_t>(exportOffset) + 104u * static_cast<std::size_t>(i);
			Export            ex;
			ex.classIndex = r.Peek<std::int32_t>(q);
			ex.superIndex = r.Peek<std::int32_t>(q + 4);
			ex.outer = r.Peek<std::int32_t>(q + 12);
			ex.name = named(r.Peek<std::int32_t>(q + 16), r.Peek<std::int32_t>(q + 20));
			ex.size = r.Peek<std::int64_t>(q + 28);
			ex.offset = r.Peek<std::int64_t>(q + 36) - _headerSize;
			_exports.push_back(std::move(ex));
		}
		for (std::int32_t i = 0; i < importCount; ++i) {
			const std::size_t q = static_cast<std::size_t>(importOffset) + 28u * static_cast<std::size_t>(i);
			Import            im;
			im.className = named(r.Peek<std::int32_t>(q + 8), 0);
			im.outer = r.Peek<std::int32_t>(q + 16);
			im.name = named(r.Peek<std::int32_t>(q + 20), r.Peek<std::int32_t>(q + 24));  // (with its number: "SK_Scimitar_Blade_1" is kept as SK_Scimitar_Blade + a number)
			_imports.push_back(std::move(im));
		}
	}

	std::string Package::Name(const std::vector<char>& a_bytes, std::size_t& a_offset) const
	{
		Reader     r{ a_bytes, a_offset };
		const auto index = r.Get<std::int32_t>();
		const auto number = r.Get<std::int32_t>();
		a_offset = r.at;
		if (index < 0 || static_cast<std::size_t>(index) >= _names.size()) {
			Fail("name index out of range");
		}
		return number ? _names[static_cast<std::size_t>(index)] + "_" + std::to_string(number - 1) : _names[static_cast<std::size_t>(index)];
	}

	std::string Package::Name(std::size_t& a_offset) const
	{
		return Name(_uexp, a_offset);
	}

	std::string Package::Object(std::int32_t a_index) const
	{
		if (a_index > 0 && static_cast<std::size_t>(a_index) <= _exports.size()) {
			return _exports[static_cast<std::size_t>(a_index) - 1].name;
		}
		if (a_index < 0 && static_cast<std::size_t>(-a_index) <= _imports.size()) {
			return _imports[static_cast<std::size_t>(-a_index) - 1].name;
		}
		return {};
	}

	const Package::Export* Package::FindExport(std::string_view a_name) const
	{
		for (const auto& ex : _exports) {
			if (ex.name == a_name) {
				return &ex;
			}
		}
		return nullptr;
	}

	std::vector<Property> Package::Properties(std::size_t& a_offset) const
	{
		return Properties(_uexp, a_offset);
	}

	std::vector<Property> Package::Properties(const std::vector<char>& a_bytes, std::size_t& a_offset) const
	{
		std::vector<Property> out;
		Reader                r{ a_bytes, a_offset };
		for (;;) {
			Property p;
			p.name = Name(a_bytes, r.at);
			if (p.name == "None") {
				a_offset = r.at;
				return out;
			}
			p.type = Name(a_bytes, r.at);
			const auto size = r.Get<std::int32_t>();
			r.Skip(4);  // array index
			if (p.type == "StructProperty") {
				p.extra = Name(a_bytes, r.at);
				r.Skip(16);  // struct guid
			} else if (p.type == "BoolProperty") {
				p.flag = r.Get<std::uint8_t>();
			} else if (p.type == "ByteProperty" || p.type == "EnumProperty" || p.type == "ArrayProperty" || p.type == "SetProperty") {
				p.extra = Name(a_bytes, r.at);
			} else if (p.type == "MapProperty") {
				p.extra = Name(a_bytes, r.at);
				p.extra2 = Name(a_bytes, r.at);
			}
			r.Skip(1);  // has a property guid
			if (size < 0) {
				Fail("property with a negative size");
			}
			r.Need(static_cast<std::size_t>(size));
			p.raw.assign(a_bytes.begin() + static_cast<std::ptrdiff_t>(r.at), a_bytes.begin() + static_cast<std::ptrdiff_t>(r.at + static_cast<std::size_t>(size)));
			r.Skip(static_cast<std::size_t>(size));
			// (a later property of the same name replaces an earlier one, as in the Python dict)
			bool replaced = false;
			for (auto& earlier : out) {
				if (earlier.name == p.name) {
					earlier = p;
					replaced = true;
					break;
				}
			}
			if (!replaced) {
				out.push_back(std::move(p));
			}
		}
	}

	std::vector<Property> Package::Defaults(std::string_view a_export) const
	{
		const auto* ex = FindExport(a_export);
		if (!ex) {
			Fail("no object named " + std::string(a_export));
		}
		std::size_t o = static_cast<std::size_t>(ex->offset);
		return Properties(o);
	}

	std::string Package::ImportPath(std::size_t a_index) const
	{
		const Import* im = &_imports.at(a_index);
		while (im->outer < 0) {
			im = &_imports.at(static_cast<std::size_t>(-im->outer) - 1);
		}
		return im->name;
	}

	std::string Package::TexturePath(std::string_view a_object) const
	{
		for (std::size_t i = 0; i < _imports.size(); ++i) {
			if (_imports[i].className == "Texture2D" && _imports[i].name == a_object) {
				auto path = ImportPath(i);
				if (const auto game = path.find("/Game/"); game != std::string::npos) {
					path.erase(game, 6);
				}
				return path;
			}
		}
		return {};
	}

	namespace
	{
		// Python's round(x, digits) on a 32-bit float: the nearest decimal with that many places, as a double.
		double Rounded(float a_value, int a_digits)
		{
			char text[64];
			std::snprintf(text, sizeof(text), "%.*f", a_digits, static_cast<double>(a_value));
			return std::strtod(text, nullptr);
		}

		Value Number(double a_number)
		{
			Value v;
			v.kind = Value::Kind::kNumber;
			v.number = a_number;
			return v;
		}
		Value Text(std::string a_text)
		{
			Value v;
			v.kind = Value::Kind::kText;
			v.text = std::move(a_text);
			return v;
		}
	}

	Value Package::Decode(const Property& a_property) const
	{
		const auto& t = a_property.type;
		const auto& raw = a_property.raw;
		const auto& extra = a_property.extra;
		Reader      r{ raw };
		const auto  opaque = [&] { return Text("<" + t + " " + std::to_string(raw.size()) + "b>"); };
		const auto  mapOf = [&](const std::vector<Property>& a_sub) {
			Value v;
			v.kind = Value::Kind::kMap;
			for (const auto& sub : a_sub) {
				v.map.emplace_back(sub.name, Decode(sub));
			}
			return v;
		};
		try {
			if (t == "FloatProperty") {
				return Number(Rounded(r.Get<float>(), 5));
			}
			if (t == "IntProperty") {
				return Number(r.Get<std::int32_t>());
			}
			if (t == "BoolProperty") {
				Value v;
				v.kind = Value::Kind::kBool;
				v.flag = a_property.flag != 0;
				return v;
			}
			if (t == "ObjectProperty") {
				const auto index = r.Get<std::int32_t>();
				return index == 0 ? Value{} : Text(Object(index));
			}
			if (t == "NameProperty" || ((t == "ByteProperty" || t == "EnumProperty") && raw.size() == 8)) {
				return Text(_names.at(static_cast<std::size_t>(r.Get<std::int32_t>())));
			}
			static const std::set<std::string> plain{ "Vector", "Rotator", "Quat", "Guid", "LinearColor", "Color", "Vector2D", "SoftObjectPath", "GameplayTagContainer" };
			if (t == "StructProperty" && !plain.contains(extra)) {
				try {
					std::size_t o = 0;
					return mapOf(Properties(raw, o));
				} catch (const std::exception&) {
					return Text("<" + extra + " " + std::to_string(raw.size()) + "b>");
				}
			}
			if (t == "StructProperty" && extra == "Vector") {
				Value v;
				v.kind = Value::Kind::kList;
				for (int k = 0; k < 3; ++k) {
					v.list.push_back(Number(Rounded(r.Get<float>(), 4)));
				}
				return v;
			}
			if (t == "ArrayProperty" && extra == "StructProperty" && raw.size() >= 4) {
				// count, one inner tag (name, type, size, array index, struct name, guid, flag), then the elements
				try {
					const auto  count = r.Get<std::int32_t>();
					std::size_t o = 4;
					(void)Name(raw, o);
					(void)Name(raw, o);
					o += 8;
					const auto inner = Name(raw, o);
					o += 17;
					Value v;
					v.kind = Value::Kind::kList;
					for (std::int32_t i = 0; i < count; ++i) {
						if (inner == "Vector") {
							Reader e{ raw, o };
							Value  vector;
							vector.kind = Value::Kind::kList;
							for (int k = 0; k < 3; ++k) {
								vector.list.push_back(Number(Rounded(e.Get<float>(), 4)));
							}
							o = e.at;
							v.list.push_back(std::move(vector));
							continue;
						}
						v.list.push_back(mapOf(Properties(raw, o)));
					}
					return v;
				} catch (const std::exception&) {
					return Text("<array of " + extra + " " + std::to_string(raw.size()) + "b>");
				}
			}
			if (t == "ArrayProperty" && (extra == "ObjectProperty" || extra == "IntProperty") && raw.size() >= 4) {
				const auto count = r.Get<std::int32_t>();
				if (count >= 0 && raw.size() == 4 + 4 * static_cast<std::size_t>(count)) {
					Value v;
					v.kind = Value::Kind::kList;
					for (std::int32_t i = 0; i < count; ++i) {
						const auto item = r.Get<std::int32_t>();
						if (extra == "ObjectProperty") {
							v.list.push_back(item == 0 ? Value{} : Text(Object(item)));
						} else {
							v.list.push_back(Number(item));
						}
					}
					return v;
				}
			}
			if (t == "ArrayProperty" && extra == "ByteProperty" && raw.size() >= 4) {
				const auto count = r.Get<std::int32_t>();
				if (count >= 0 && raw.size() == 4 + static_cast<std::size_t>(count)) {
					Value v;
					v.kind = Value::Kind::kList;
					for (std::int32_t i = 0; i < count; ++i) {
						v.list.push_back(Number(r.Get<std::uint8_t>()));
					}
					return v;
				}
			}
		} catch (const std::exception&) {
		}
		return opaque();
	}

	// ------------------------------------------------------------------ textures

	Texture ReadTexture(const PakIndex& a_paks, std::string_view a_base)
	{
		const Package p(a_paks, a_base);
		Texture       t;
		t.name = LeafName(a_base);
		const auto* ex = p.FindExport(t.name);
		if (!ex) {
			Fail("no texture named " + t.name + " in its package");
		}
		std::size_t o = static_cast<std::size_t>(ex->offset);
		(void)p.Properties(o);
		Reader r{ p.data(), o };
		r.Skip(4);      // no object guid
		r.Skip(2 + 2);  // strip flags: UTexture, UTexture2D
		if (r.Get<std::int32_t>() != 1) {
			Fail(t.name + " is not a cooked texture");
		}
		(void)p.Name(r.at);  // pixel format name, then the platform data
		r.Skip(8);           // skip offset
		t.width = r.Get<std::int32_t>();
		t.height = r.Get<std::int32_t>();
		r.Skip(4);  // packed data
		t.format = r.String();
		r.Skip(4);  // first mip
		const auto        mipCount = r.Get<std::int32_t>();
		std::vector<char> bulk;
		bool              bulkRead = false;
		for (std::int32_t i = 0; i < mipCount; ++i) {
			r.Skip(4);  // cooked
			const auto flags = r.Get<std::uint32_t>();
			r.Skip(4);  // element count
			const auto disk = r.Get<std::int32_t>();
			const auto offset = r.Get<std::int64_t>();
			if (disk < 0) {
				Fail(t.name + ": mip with a negative size");
			}
			Texture::Mip mip;
			const auto   take = [&](const std::vector<char>& a_from, std::size_t a_start) {
				// (as the Python slice did: what is there, even if short)
				const std::size_t end = std::min(a_from.size(), a_start + static_cast<std::size_t>(disk));
				if (a_start < end) {
					mip.data.assign(a_from.begin() + static_cast<std::ptrdiff_t>(a_start), a_from.begin() + static_cast<std::ptrdiff_t>(end));
				}
			};
			if (flags & 0x100u) {  // payload in the separate .ubulk file
				if (!bulkRead) {
					bulk = a_paks.Blob(std::string(a_base) + ".ubulk");
					bulkRead = true;
				}
				take(bulk, static_cast<std::size_t>(offset));
			} else if ((flags & 0x40u) || !(flags & 0x1u)) {  // inline
				take(p.data(), r.at);
				r.Skip(static_cast<std::size_t>(disk));
			} else {  // at the end of the file: the offset counts from the start of .uasset
				take(p.data(), static_cast<std::size_t>(offset - p.headerSize()));
			}
			mip.width = r.Get<std::int32_t>();
			mip.height = r.Get<std::int32_t>();
			r.Skip(4);  // depth
			t.mips.push_back(std::move(mip));
		}
		return t;
	}

	std::string WriteDds(const PakIndex& a_paks, std::string_view a_base, const std::filesystem::path& a_out, int a_maxSide)
	{
		const auto  t = ReadTexture(a_paks, a_base);
		const char* fourcc = t.format == "PF_DXT1" ? "DXT1" : t.format == "PF_DXT5" ? "DXT5" : t.format == "PF_BC5" ? "ATI2" : nullptr;
		if (!fourcc) {
			Fail(t.name + ": pixel format " + t.format + " is not handled");
		}
		std::vector<const Texture::Mip*> mips;
		for (const auto& mip : t.mips) {
			if (!mip.data.empty() && std::max(mip.width, mip.height) <= a_maxSide) {
				mips.push_back(&mip);
			}
		}
		if (mips.empty()) {
			Fail(t.name + ": no mip small enough");
		}
		std::vector<char> header(128, 0);
		const auto        put = [&](std::size_t a_at, std::uint32_t a_value) { std::memcpy(header.data() + a_at, &a_value, 4); };
		std::memcpy(header.data(), "DDS ", 4);
		put(4, 124);
		put(8, 0xA1007);
		put(12, static_cast<std::uint32_t>(mips[0]->height));
		put(16, static_cast<std::uint32_t>(mips[0]->width));
		put(20, static_cast<std::uint32_t>(mips[0]->data.size()));
		put(24, 0);
		put(28, static_cast<std::uint32_t>(mips.size()));
		put(76, 32);
		put(80, 0x4);
		std::memcpy(header.data() + 84, fourcc, 4);
		put(108, 0x401008);
		// (two different textures can share a name: the path decides)
		char tag[8]{};
		std::snprintf(tag, sizeof(tag), "%06x", Crc32(a_base) & 0xFFFFFFu);
		const std::string name = t.name.substr(0, 48) + "_" + tag + ".dds";
		std::ofstream     out(a_out / name, std::ios::binary);
		if (!out) {
			Fail("cannot write " + (a_out / name).string());
		}
		out.write(header.data(), static_cast<std::streamsize>(header.size()));
		for (const auto* mip : mips) {
			out.write(mip->data.data(), static_cast<std::streamsize>(mip->data.size()));
		}
		return name;
	}

	// ------------------------------------------------------------------ models

	Mesh ReadSkeletalMesh(const PakIndex& a_paks, std::string_view a_base)
	{
		const Package p(a_paks, a_base);
		Mesh          m;
		m.name = LeafName(a_base);
		// (a package can hold other objects of the same name, e.g. a clothing asset: take the mesh itself)
		const Package::Export* ex = nullptr;
		for (const auto& candidate : p.exports()) {
			if (candidate.name == m.name) {
				if (!ex) {
					ex = &candidate;
				}
				if (p.Object(candidate.classIndex) == "SkeletalMesh") {
					ex = &candidate;
					break;
				}
			}
		}
		if (!ex) {
			Fail("no model named " + m.name + " in its package");
		}
		std::size_t o = static_cast<std::size_t>(ex->offset);
		(void)p.Properties(o);
		Reader r{ p.data(), o };
		r.Skip(4);       // no object guid
		r.Skip(2);       // strip flags
		r.Skip(7 * 4);   // bounds
		for (auto count = r.Get<std::int32_t>(); count > 0; --count) {
			auto material = p.Object(r.Get<std::int32_t>());
			auto slot = p.Name(r.at);
			r.Skip(4 + 24);  // no imported slot name (cooked); UV channel info
			m.materials.emplace_back(std::move(material), std::move(slot));
		}
		// reference skeleton
		const auto boneCount = r.Get<std::int32_t>();
		if (boneCount < 0 || boneCount > 100000) {
			Fail(m.name + " is not a rigged model");
		}
		for (std::int32_t i = 0; i < boneCount; ++i) {
			Mesh::Bone bone;
			bone.name = p.Name(r.at);
			bone.parent = r.Get<std::int32_t>();
			m.bones.push_back(std::move(bone));
		}
		if (r.Get<std::int32_t>() != boneCount) {
			Fail(m.name + ": bone pose count differs");
		}
		for (auto& bone : m.bones) {
			for (float& v : bone.rotation) {
				v = r.Get<float>();
			}
			for (float& v : bone.translation) {
				v = r.Get<float>();
			}
			r.Skip(12);  // scale
		}
		for (auto count = r.Get<std::int32_t>(); count > 0; --count) {
			r.Skip(8 + 4);  // name -> index
		}
		if (r.Get<std::int32_t>() != 1) {
			Fail(m.name + " is not cooked");
		}
		r.Skip(4);  // number of LODs
		// ---- LOD 0 ----
		r.Skip(2);
		const auto cookedOut = r.Get<std::int32_t>();
		const auto inlined = r.Get<std::int32_t>();
		if (cookedOut || !inlined) {
			Fail(m.name + ": LOD 0 is not stored inline");
		}
		r.Skip(2 * static_cast<std::size_t>(r.Get<std::int32_t>()));  // required bones
		std::vector<std::vector<std::uint16_t>> boneMaps;
		for (auto count = r.Get<std::int32_t>(); count > 0; --count) {
			Mesh::Section s;
			r.Skip(2);
			s.material = r.Get<std::int16_t>();
			s.baseIndex = r.Get<std::uint32_t>();
			s.triangles = r.Get<std::uint32_t>();
			r.Skip(4);  // recompute tangent
			r.Skip(1);  // one byte the stock 4.25 layout lacks
			r.Skip(4);  // cast shadow
			s.baseVertex = r.Get<std::uint32_t>();
			r.Skip(64 * static_cast<std::size_t>(r.Get<std::int32_t>()));  // cloth mapping
			std::vector<std::uint16_t> map(static_cast<std::size_t>(r.Get<std::int32_t>()));
			for (auto& b : map) {
				b = r.Get<std::uint16_t>();
			}
			s.vertices = r.Get<std::int32_t>();
			r.Skip(4);       // max influences
			r.Skip(2 + 20);  // cloth asset index, clothing data
			r.Skip(4 * static_cast<std::size_t>(r.Get<std::int32_t>()));
			r.Skip(8 * static_cast<std::size_t>(r.Get<std::int32_t>()));  // duplicated vertices buffers
			r.Skip(4);                                                      // disabled
			m.sections.push_back(s);
			boneMaps.push_back(std::move(map));
		}
		r.Skip(2 * static_cast<std::size_t>(r.Get<std::int32_t>()));  // active bones
		r.Skip(4);                                                      // buffers size
		r.Skip(2);
		r.Skip(1);
		{
			const auto size = r.Get<std::int32_t>(), count = r.Get<std::int32_t>();
			if ((size != 2 && size != 4) || count < 0) {
				Fail(m.name + ": unexpected index buffer");
			}
			m.indices.resize(static_cast<std::size_t>(count));
			for (auto& index : m.indices) {
				index = size == 2 ? r.Get<std::uint16_t>() : r.Get<std::uint32_t>();
			}
		}
		r.Skip(4);
		const auto vertexCount = r.Get<std::uint32_t>();
		{
			const auto size = r.Get<std::int32_t>(), count = r.Get<std::int32_t>();
			if (size != 12 || static_cast<std::uint32_t>(count) != vertexCount) {
				Fail(m.name + ": unexpected position buffer");
			}
			m.positions.resize(static_cast<std::size_t>(vertexCount) * 3);
			r.Need(m.positions.size() * 4);
			std::memcpy(m.positions.data(), p.data().data() + r.at, m.positions.size() * 4);
			r.at += m.positions.size() * 4;
		}
		r.Skip(2);
		const auto uvSets = r.Get<std::int32_t>();
		if (r.Get<std::uint32_t>() != vertexCount) {
			Fail(m.name + ": vertex counts differ");
		}
		r.Skip(8);  // precision flags
		{
			// tangent basis: per vertex TangentX then TangentZ (the normal), 4 signed bytes each
			const auto size = r.Get<std::int32_t>(), count = r.Get<std::int32_t>();
			if (size != 8 || static_cast<std::uint32_t>(count) != vertexCount) {
				Fail(m.name + ": unexpected tangent buffer");
			}
			m.normals.resize(static_cast<std::size_t>(vertexCount) * 3);
			for (std::uint32_t v = 0; v < vertexCount; ++v) {
				r.Skip(4);
				for (int k = 0; k < 3; ++k) {
					m.normals[v * 3 + static_cast<std::size_t>(k)] = static_cast<float>(static_cast<double>(r.Get<std::int8_t>()) / 127.0);
				}
				r.Skip(1);
			}
		}
		{
			const auto size = r.Get<std::int32_t>(), count = r.Get<std::int32_t>();
			if (count && ((size != 4 && size != 8) || uvSets < 1 || static_cast<std::uint32_t>(count) != vertexCount * static_cast<std::uint32_t>(uvSets))) {
				Fail(m.name + ": unexpected texture-coordinate buffer");
			}
			m.uvs.assign(static_cast<std::size_t>(vertexCount) * 2, 0.0f);
			for (std::uint32_t v = 0; count && v < vertexCount; ++v) {
				for (std::int32_t set = 0; set < uvSets; ++set) {
					float u, w;
					if (size == 4) {
						u = HalfToFloat(r.Get<std::uint16_t>());
						w = HalfToFloat(r.Get<std::uint16_t>());
					} else {
						u = r.Get<float>();
						w = r.Get<float>();
					}
					if (set == 0) {
						m.uvs[v * 2] = u, m.uvs[v * 2 + 1] = w;
					}
				}
			}
		}
		// skin weights: per vertex max-influences bone bytes, then as many weight bytes
		r.Skip(2);
		if (r.Get<std::int32_t>()) {
			Fail(m.name + ": variable bone influences are not handled");
		}
		const auto influences = r.Get<std::int32_t>();
		r.Skip(4);
		if (r.Get<std::uint32_t>() != vertexCount) {
			Fail(m.name + ": vertex counts differ");
		}
		r.Skip(4);  // 16-bit bone index flag (false)
		{
			const auto size = r.Get<std::int32_t>(), count = r.Get<std::int32_t>();
			if (size != 1 || influences < 4 || static_cast<std::uint32_t>(count) != vertexCount * static_cast<std::uint32_t>(influences) * 2u) {
				Fail(m.name + ": unexpected skin weight buffer");
			}
		}
		std::vector<std::int32_t> local(static_cast<std::size_t>(vertexCount) * 4);
		m.weights.resize(static_cast<std::size_t>(vertexCount) * 4);
		for (std::uint32_t v = 0; v < vertexCount; ++v) {
			for (std::int32_t k = 0; k < influences; ++k) {
				const auto b = r.Get<std::uint8_t>();
				if (k < 4) {
					local[v * 4 + static_cast<std::size_t>(k)] = b;
				}
			}
			double sum = 0.0;
			for (std::int32_t k = 0; k < influences; ++k) {
				const auto w = r.Get<std::uint8_t>();
				if (k < 4) {
					m.weights[v * 4 + static_cast<std::size_t>(k)] = w;
					sum += w;
				}
			}
			sum = std::max(sum, 1.0);
			for (int k = 0; k < 4; ++k) {
				m.weights[v * 4 + static_cast<std::size_t>(k)] /= sum;
			}
		}
		m.boneIds.assign(static_cast<std::size_t>(vertexCount) * 4, 0);
		for (std::size_t s = 0; s < m.sections.size(); ++s) {
			const auto& section = m.sections[s];
			const auto& map = boneMaps[s];
			if (map.empty()) {
				continue;
			}
			for (std::uint32_t v = section.baseVertex; v < section.baseVertex + static_cast<std::uint32_t>(section.vertices) && v < vertexCount; ++v) {
				for (int k = 0; k < 4; ++k) {
					const auto at = static_cast<std::size_t>(std::min<std::int32_t>(local[v * 4 + static_cast<std::size_t>(k)], static_cast<std::int32_t>(map.size()) - 1));
					m.boneIds[v * 4 + static_cast<std::size_t>(k)] = map[at];
				}
			}
		}
		return m;
	}

	Mesh ReadStaticMesh(const PakIndex& a_paks, std::string_view a_base)
	{
		// The buffers are found by their headers rather than by walking every field before them (the fields before them vary from model to model).
		const Package p(a_paks, a_base);
		Mesh          m;
		m.name = LeafName(a_base);
		const auto* ex = p.FindExport(m.name);
		if (!ex) {
			Fail("no model named " + m.name + " in its package");
		}
		const std::vector<char> d(p.data().begin() + static_cast<std::ptrdiff_t>(ex->offset), p.data().begin() + static_cast<std::ptrdiff_t>(ex->offset + ex->size));
		Reader                  r{ d };
		// LOD 0 position buffer: stride 12, N, element size 12, N - the first that fits
		std::size_t   at = std::string::npos;
		std::uint32_t n = 0;
		for (std::size_t o = 0; o + 16 < d.size(); ++o) {
			if (d[o] == 12 && d[o + 1] == 0 && r.Peek<std::uint32_t>(o) == 12 && r.Peek<std::uint32_t>(o + 8) == 12) {
				const auto count = r.Peek<std::uint32_t>(o + 4);
				if (count == r.Peek<std::uint32_t>(o + 12) && count >= 3 && count < 3000000 && o + 16 + 12ull * count <= d.size()) {
					at = o, n = count;
					break;
				}
			}
		}
		if (at == std::string::npos) {
			Fail("no position buffer found in " + m.name);
		}
		m.positions.resize(static_cast<std::size_t>(n) * 3);
		std::memcpy(m.positions.data(), d.data() + at + 16, m.positions.size() * 4);
		std::size_t o = at + 16 + 12ull * n;
		// tangents: (element size 8 or 16, N) within the next few dozen bytes
		std::size_t   tangents = std::string::npos;
		std::uint32_t tangentSize = 0;
		for (std::size_t q = o; q < std::min(o + 64, d.size() - 8); ++q) {
			const auto es = r.Peek<std::uint32_t>(q), count = r.Peek<std::uint32_t>(q + 4);
			if ((es == 8 || es == 16) && count == n && q + 8 + static_cast<std::size_t>(es) * n <= d.size()) {
				tangents = q, tangentSize = es;
				break;
			}
		}
		if (tangents == std::string::npos) {
			Fail("no tangent buffer after the positions in " + m.name);
		}
		m.normals.resize(static_cast<std::size_t>(n) * 3);
		for (std::uint32_t v = 0; v < n; ++v) {
			double      normal[3];
			const auto* base = d.data() + tangents + 8 + static_cast<std::size_t>(v) * tangentSize;
			for (int k = 0; k < 3; ++k) {
				if (tangentSize == 8) {
					std::int8_t b;
					std::memcpy(&b, base + 4 + k, 1);
					normal[k] = static_cast<double>(b) / 127.0;
				} else {
					std::int16_t b;
					std::memcpy(&b, base + 8 + k * 2, 2);
					normal[k] = static_cast<double>(b) / 32767.0;
				}
			}
			const double length = std::max(std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]), 1e-6);
			for (int k = 0; k < 3; ++k) {
				m.normals[v * 3 + static_cast<std::size_t>(k)] = static_cast<float>(normal[k] / length);
			}
		}
		o = tangents + 8 + static_cast<std::size_t>(tangentSize) * n;
		// texture coordinates: (element size 4 = half floats or 8 = floats, N x sets)
		const auto uvSize = r.Peek<std::uint32_t>(o), uvCount = r.Peek<std::uint32_t>(o + 4);
		if ((uvSize != 4 && uvSize != 8) || uvCount % n || uvCount / n < 1 || uvCount / n > 8) {
			Fail("unexpected texture-coordinate header in " + m.name);
		}
		const auto sets = uvCount / n;
		m.uvs.resize(static_cast<std::size_t>(n) * 2);
		for (std::uint32_t v = 0; v < n; ++v) {
			const std::size_t q = o + 8 + (static_cast<std::size_t>(v) * sets) * uvSize;
			if (uvSize == 4) {
				m.uvs[v * 2] = HalfToFloat(r.Peek<std::uint16_t>(q));
				m.uvs[v * 2 + 1] = HalfToFloat(r.Peek<std::uint16_t>(q + 2));
			} else {
				m.uvs[v * 2] = r.Peek<float>(q);
				m.uvs[v * 2 + 1] = r.Peek<float>(q + 4);
			}
		}
		o += 8 + static_cast<std::size_t>(uvSize) * uvCount;
		// the colour buffer: 2 flag bytes, stride, count, and when it has colours their array (element size 4, count)
		{
			const auto stride = r.Peek<std::uint32_t>(o + 2), count = r.Peek<std::uint32_t>(o + 6);
			if ((stride == 0 || stride == 4) && (count == 0 || count == n)) {
				o += 10;
				if (count && r.Peek<std::uint32_t>(o) == 4 && r.Peek<std::uint32_t>(o + 4) == n) {
					o += 8 + 4ull * n;
				}
			}
		}
		// index buffer: bool 32-bit, then bytes (element size 1, count), within a few header bytes
		bool found = false;
		for (std::size_t q = o; q < std::min(o + 4096, d.size() - 12) && !found; ++q) {
			const auto wide = r.Peek<std::uint32_t>(q), es = r.Peek<std::uint32_t>(q + 4), count = r.Peek<std::uint32_t>(q + 8);
			if ((wide == 0 || wide == 1) && es == 1 && count && count % (wide ? 12u : 6u) == 0 && q + 12 + count <= d.size()) {
				const std::size_t         total = count / (wide ? 4u : 2u);
				std::vector<std::int64_t> indices(total);
				std::int64_t              largest = 0;
				for (std::size_t i = 0; i < total; ++i) {
					indices[i] = wide ? r.Peek<std::uint32_t>(q + 12 + i * 4) : r.Peek<std::uint16_t>(q + 12 + i * 2);
					largest = std::max(largest, indices[i]);
				}
				if (largest < static_cast<std::int64_t>(n) && static_cast<double>(largest) >= static_cast<double>(n) * 0.5) {
					m.indices = std::move(indices);
					found = true;
				}
			}
		}
		if (!found) {
			Fail("no index buffer found in " + m.name);
		}
		for (const auto& im : p.imports()) {
			if (im.className == "MaterialInstanceConstant" || im.className == "Material") {
				m.materials.emplace_back(im.name, im.name);
			}
		}
		if (m.materials.empty()) {
			m.materials.emplace_back("none", "none");
		}
		m.bones.push_back({ "root", -1, { 0, 0, 0, 1 }, { 0, 0, 0 } });
		m.boneIds.assign(static_cast<std::size_t>(n) * 4, 0);
		m.weights.assign(static_cast<std::size_t>(n) * 4, 0.0);
		for (std::uint32_t v = 0; v < n; ++v) {
			m.weights[v * 4] = 1.0;
		}
		m.sections.push_back({ 0, 0, static_cast<std::uint32_t>(m.indices.size() / 3), 0, static_cast<std::int32_t>(n) });
		return m;
	}

	Mesh ReadAnyMesh(const PakIndex& a_paks, std::string_view a_base)
	{
		try {
			return ReadSkeletalMesh(a_paks, a_base);
		} catch (const std::exception&) {
			return ReadStaticMesh(a_paks, a_base);
		}
	}

	// ------------------------------------------------------------------ the cache files

	void BakePieces(const PakIndex& a_paks, const std::vector<Piece>& a_pieces, const std::filesystem::path& a_out, int a_cap, std::string& a_log)
	{
		std::filesystem::create_directories(a_out);
#pragma pack(push, 1)
		struct Vertex
		{
			float        position[3];
			float        normal[3];
			float        uv[2];
			std::uint8_t bones[4];
			std::uint8_t weights[4];
		};
		struct SectionRecord
		{
			std::int32_t first, count;
			char         colour[64], mask[64];
			float        tints[9];
			char         emblem[64];
			float        emblemValues[11];
			char         normal[64], surface[64], hair[64], beard[64];
			float        skin[4];
		};
		struct BoneRecord
		{
			char         name[64];
			std::int32_t parent;
			float        rotation[4];
			float        translation[3];
		};
#pragma pack(pop)
		std::vector<Mesh::Bone>            bones;
		std::vector<Vertex>                vertices;
		std::vector<std::uint32_t>         indices;
		std::vector<SectionRecord>         sections;
		std::map<std::string, std::string> written;  // texture path -> file name
		const auto                         texture = [&](const std::string& a_path, int a_side, const char* a_kind) -> const std::string& {
			auto it = written.find(a_path);
			if (it == written.end()) {
				it = written.emplace(a_path, WriteDds(a_paks, a_path, a_out, std::min(a_side, a_cap))).first;
				if (a_kind) {
					a_log += std::string("  ") + a_kind + " " + it->second + "\n";
				}
			}
			return it->second;
		};
		// the base body: which colour map each of its materials uses, and MORDHAU's painted-on hair and beard
		static const char* const kHeadColour = "UMA/Separated/MaleCaucasian01/MaleHead01_d";
		static const char* const kHairLayer = "UMA/Separated/MaleCaucasian01/T_MaleCaucasian01Hair01_Albedo";
		static const char* const kBeardLayer = "UMA/Separated/MaleCaucasian01/T_MaleCaucasian01Beard04_Albedo";
		// which bones carry each body part an item can hide
		static const std::map<std::string, std::vector<std::string>> kHideBones{
			{ "Chest", { "LowerBack", "Spine", "Spine1", "LeftShoulder", "RightShoulder", "LeftTrapezius", "RightTrapezius", "LowerBackBelly", "LeftInnerBreast", "RightInnerBreast",
						   "LeftOuterBreast", "RightOuterBreast" } },
			{ "LeftArm", { "LeftArm", "LeftForeArm", "LeftForeArmTwist" } },
			{ "RightArm", { "RightArm", "RightForeArm", "RightForeArmTwist" } },
			{ "LeftHand", { "LeftHand" } },
			{ "RightHand", { "RightHand" } },
			{ "LeftLeg", { "LeftUpLeg", "LeftLeg", "LeftGluteus", "Hips" } },
			{ "RightLeg", { "RightUpLeg", "RightLeg", "RightGluteus", "Hips" } },
			{ "LeftFoot", { "LeftFoot", "LeftToeBase" } },
			{ "RightFoot", { "RightFoot", "RightToeBase" } },
		};
		const auto text = [](char (&a_field)[64], const std::string& a_value) {
			std::memset(a_field, 0, 64);
			std::memcpy(a_field, a_value.data(), std::min<std::size_t>(a_value.size(), 64));
		};
		std::uint32_t baseVertex = 0;
		for (const auto& piece : a_pieces) {
			Mesh m;
			try {
				m = ReadAnyMesh(a_paks, piece.model);
			} catch (const std::exception& ex) {
				if (bones.empty()) {
					throw;
				}
				a_log += "  LEFT OUT " + LeafName(piece.model) + ": its model is stored in a way the reader does not handle yet (" + ex.what() + ")\n";
				continue;
			}
			if (bones.empty()) {
				bones = m.bones;
			}
			// a piece built on a skeleton with a different bone list: its bones are matched to the first piece's by name
			// (a bone that one lacks falls back to its nearest ancestor that it has)
			std::vector<std::int32_t> remap;
			bool                      same = m.bones.size() == bones.size();
			for (std::size_t i = 0; same && i < bones.size(); ++i) {
				same = m.bones[i].name == bones[i].name;
			}
			if (!piece.weapon && !same) {
				for (const auto& bone : m.bones) {
					const Mesh::Bone* walk = &bone;
					const auto        indexOf = [&](const std::string& a_name) {
						for (std::size_t i = 0; i < bones.size(); ++i) {
							if (bones[i].name == a_name) {
								return static_cast<std::int32_t>(i);
							}
						}
						return -1;
					};
					std::int32_t found = indexOf(walk->name);
					while (found < 0 && walk->parent >= 0 && static_cast<std::size_t>(walk->parent) < m.bones.size()) {
						walk = &m.bones[static_cast<std::size_t>(walk->parent)];
						found = indexOf(walk->name);
					}
					remap.push_back(std::max(found, 0));
				}
				a_log += "  " + m.name + ": bones matched by name (" + std::to_string(m.bones.size()) + " -> " + std::to_string(bones.size()) + ")\n";
			}
			const auto count = static_cast<std::uint32_t>(m.vertexCount());
			for (std::uint32_t v = 0; v < count; ++v) {
				Vertex out{};
				std::memcpy(out.position, &m.positions[v * 3], 12);
				std::memcpy(out.normal, &m.normals[v * 3], 12);
				std::memcpy(out.uv, &m.uvs[v * 2], 8);
				if (piece.weapon) {
					out.bones[0] = static_cast<std::uint8_t>(bones.size());
					out.weights[0] = 255;
				} else {
					// weights as bytes summing to exactly 255 (the remainder goes to the first)
					long long w8[4];
					long long sum = 0;
					for (int k = 0; k < 4; ++k) {
						w8[k] = std::llrint(m.weights[v * 4 + static_cast<std::size_t>(k)] * 255.0);
						sum += w8[k];
					}
					w8[0] += 255 - sum;
					for (int k = 0; k < 4; ++k) {
						auto id = m.boneIds[v * 4 + static_cast<std::size_t>(k)];
						if (!remap.empty()) {
							id = remap[static_cast<std::size_t>(std::clamp<std::int32_t>(id, 0, static_cast<std::int32_t>(remap.size()) - 1))];
						}
						out.bones[k] = static_cast<std::uint8_t>(id);
						out.weights[k] = static_cast<std::uint8_t>(std::clamp<long long>(w8[k], 0, 255));
					}
				}
				vertices.push_back(out);
			}
			// body parts an item covers: a triangle goes when all its corners hang mainly on bones of a hidden part
			std::vector<char> hiddenVertex;
			if (!piece.hide.empty()) {
				std::set<std::int32_t> hiddenBones;
				for (const auto& part : piece.hide) {
					const auto stems = kHideBones.find(part);
					if (stems == kHideBones.end()) {
						continue;
					}
					for (const auto& stem : stems->second) {
						const bool hand = stem.size() >= 4 && stem.compare(stem.size() - 4, 4, "Hand") == 0;
						for (std::size_t i = 0; i < m.bones.size(); ++i) {
							const auto& name = m.bones[i].name;
							// the bone itself, its "Adjust" twin, and fingers for hands
							if (name == stem || name == stem + "Adjust" || (hand && name.rfind(stem + "Finger", 0) == 0)) {
								hiddenBones.insert(static_cast<std::int32_t>(i));
							}
						}
					}
				}
				if (!hiddenBones.empty()) {
					hiddenVertex.resize(count);
					for (std::uint32_t v = 0; v < count; ++v) {
						int strongest = 0;
						for (int k = 1; k < 4; ++k) {
							if (m.weights[v * 4 + static_cast<std::size_t>(k)] > m.weights[v * 4 + static_cast<std::size_t>(strongest)]) {
								strongest = k;
							}
						}
						hiddenVertex[v] = hiddenBones.contains(m.boneIds[v * 4 + static_cast<std::size_t>(strongest)]) ? 1 : 0;
					}
				}
			}
			for (const auto& s : m.sections) {
				const std::string material = s.material >= 0 && static_cast<std::size_t>(s.material) < m.materials.size() ? m.materials[static_cast<std::size_t>(s.material)].first : std::string();
				std::string       colourPath = piece.colourMap;
				if (piece.perMaterial) {
					const auto it = piece.byMaterial.find(material);
					colourPath = it != piece.byMaterial.end() ? it->second : piece.colourMap;
				}
				float       skin[4]{};
				std::string hairFile, beardFile;
				if (piece.body) {
					const auto hides = [&](const char* a_part) { return std::find(piece.hide.begin(), piece.hide.end(), a_part) != piece.hide.end(); };
					if (material == "INST_Torso") {
						skin[0] = 1.0f;
					} else if (material.rfind("INST_Head", 0) == 0) {
						skin[0] = 2.0f, skin[1] = hides("Hair") ? 0.0f : 1.0f, skin[2] = hides("Beard") ? 0.0f : 1.0f;
						hairFile = texture(kHairLayer, 1024, nullptr);
						beardFile = texture(kBeardLayer, 1024, nullptr);
					} else if (material == "INST_Eye") {
						skin[0] = 3.0f;
						colourPath = kHeadColour;  // (not sampled: eyes are drawn from their own texture coordinates)
					}
				}
				if (colourPath.empty()) {
					a_log += "  " + m.name + ": section with material " + material + " left out (no colour map chosen for it yet)\n";
					continue;
				}
				SectionRecord record{};
				text(record.colour, texture(colourPath, 4096, "texture"));
				if (!piece.patternMask.empty()) {
					// (a paint mask that cannot be read - the heater shield's - is left out: the part keeps its plain colours)
					try {
						text(record.mask, texture(piece.patternMask, 4096, "mask"));
					} catch (const std::exception& e) {
						a_log += "  " + m.name + ": paint mask " + piece.patternMask + " could not be read (" + e.what() + "); left out\n";
					}
				}
				std::memcpy(record.tints, piece.tints, sizeof(record.tints));
				if (!piece.emblem.empty()) {
					text(record.emblem, texture(piece.emblem, 4096, "emblem"));
					std::memcpy(record.emblemValues, piece.emblemValues, sizeof(record.emblemValues));
				}
				text(record.hair, hairFile);
				text(record.beard, beardFile);
				std::memcpy(record.skin, skin, sizeof(skin));
				if (!piece.normalMap.empty()) {
					try {
						text(record.normal, texture(piece.normalMap, 2048, "normal map"));
					} catch (const std::exception& ex) {
						a_log += "  normal map " + piece.normalMap + " left out: " + ex.what() + "\n";
					}
				}
				if (!piece.surfaceMap.empty()) {
					try {
						text(record.surface, texture(piece.surfaceMap, 2048, "surface map"));
					} catch (const std::exception& ex) {
						a_log += "  surface map " + piece.surfaceMap + " left out: " + ex.what() + "\n";
					}
				}
				record.first = static_cast<std::int32_t>(indices.size());
				const std::size_t first = s.baseIndex / 3;
				std::size_t       taken = 0;
				for (std::size_t t = first; t < first + s.triangles && t * 3 + 2 < m.indices.size(); ++t) {
					if (!hiddenVertex.empty() && hiddenVertex[static_cast<std::size_t>(m.indices[t * 3])] && hiddenVertex[static_cast<std::size_t>(m.indices[t * 3 + 1])] &&
						hiddenVertex[static_cast<std::size_t>(m.indices[t * 3 + 2])]) {
						continue;
					}
					++taken;
					for (int k = 0; k < 3; ++k) {
						indices.push_back(static_cast<std::uint32_t>(m.indices[t * 3 + static_cast<std::size_t>(k)]) + baseVertex);
					}
				}
				record.count = static_cast<std::int32_t>(taken * 3);
				sections.push_back(record);
			}
			baseVertex += count;
		}
		std::ofstream out(a_out / "character.dmchar", std::ios::binary);
		if (!out) {
			Fail("cannot write " + (a_out / "character.dmchar").string());
		}
		const std::int32_t header[5]{ 6, static_cast<std::int32_t>(bones.size()), static_cast<std::int32_t>(vertices.size()), static_cast<std::int32_t>(indices.size()),
			static_cast<std::int32_t>(sections.size()) };
		out.write("DMCH", 4);
		out.write(reinterpret_cast<const char*>(header), sizeof(header));
		for (const auto& bone : bones) {
			BoneRecord record{};
			std::memcpy(record.name, bone.name.data(), std::min<std::size_t>(bone.name.size(), 64));
			record.parent = bone.parent;
			std::memcpy(record.rotation, bone.rotation, 16);
			std::memcpy(record.translation, bone.translation, 12);
			out.write(reinterpret_cast<const char*>(&record), sizeof(record));
		}
		out.write(reinterpret_cast<const char*>(sections.data()), static_cast<std::streamsize>(sections.size() * sizeof(SectionRecord)));
		out.write(reinterpret_cast<const char*>(vertices.data()), static_cast<std::streamsize>(vertices.size() * sizeof(Vertex)));
		out.write(reinterpret_cast<const char*>(indices.data()), static_cast<std::streamsize>(indices.size() * 4));
		a_log += "character.dmchar: " + std::to_string(bones.size()) + " bones, " + std::to_string(vertices.size()) + " vertices, " + std::to_string(indices.size() / 3) + " triangles, " +
				 std::to_string(sections.size()) + " sections\n";
	}
}
