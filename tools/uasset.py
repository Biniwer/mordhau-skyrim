"""Minimal reader for cooked, unversioned UE4 packages inside MORDHAU's paks (read-only)."""
import struct, os

PAKS = "E:/steam object/steamapps/common/Mordhau/Mordhau/Content/Paks"
_rows = None

def rows(tsv=None):
    global _rows
    if _rows is None:
        tsv = tsv or os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'mordhau_files.tsv')
        _rows = [l.rstrip('\n').split('\t') for l in open(tsv, encoding='utf-8')]
    return _rows

def blob(path_suffix):
    r = next(x for x in rows() if x[1].endswith(path_suffix))
    with open(os.path.join(PAKS, r[0]), 'rb') as f:
        f.seek(int(r[5])); h = f.read(48)
        usize, method = struct.unpack_from('<QI', h, 16)
        assert method == 0, 'compressed pak entry'
        f.seek(int(r[5]) + 48 + 1 + 4)
        return f.read(usize)

def fstr(b, o):
    n, = struct.unpack_from('<i', b, o); o += 4
    if n >= 0:
        return b[o:o+max(n-1, 0)].decode('utf-8', 'replace'), o + n
    return b[o:o-2*n-2].decode('utf-16le'), o - 2*n

class Package:
    def __init__(self, base):
        self.a = a = blob(base + '.uasset'); self.e = blob(base + '.uexp')
        tag, legacy, ue3, ue4, lic, ncustom = struct.unpack_from('<Iiiiii', a, 0)
        assert tag == 0x9E2A83C1 and ncustom == 0, (hex(tag), ncustom)
        o = 24
        self.header_size, = struct.unpack_from('<i', a, o); o += 4
        _, o = fstr(a, o)
        self.flags, ncount, noff = struct.unpack_from('<Iii', a, o); o += 12
        _gc, _go, ecount, eoff, icount, ioff = struct.unpack_from('<iiiiii', a, o)
        self.names = []; p = noff
        for _ in range(ncount):
            s, p = fstr(a, p); p += 4
            self.names.append(s)
        # export table (4.25 cooked entry = 104 bytes); serial offsets count from the start of .uasset
        self.exports = []
        for i in range(ecount):
            q = eoff + 104 * i
            cls, sup, tmpl, outer = struct.unpack_from('<iiii', a, q)
            ni, nn, flags = struct.unpack_from('<iiI', a, q + 16)
            ssize, soff = struct.unpack_from('<qq', a, q + 28)
            self.exports.append({'name': self.names[ni] + (f'_{nn-1}' if nn else ''), 'class': cls, 'super': sup, 'outer': outer,
                                 'size': ssize, 'offset': soff - self.header_size})
        self.imports = []
        self.import_info = []  # (class name, outer index, object name) per import
        for i in range(icount):
            q = ioff + 28 * i
            ci, = struct.unpack_from('<i', a, q + 8)
            outer, ni, nn = struct.unpack_from('<iii', a, q + 16)
            name = self.names[ni] + (f'_{nn-1}' if nn else '')   # (a name ending in _<number> is kept as base name + number)
            self.imports.append(name)
            self.import_info.append((self.names[ci], outer, name))

    def import_path(self, index):
        """Package path of import number `index` (0-based): the outermost object's name, e.g. /Game/UMA/.../T_Foo."""
        cls, outer, name = self.import_info[index]
        while outer < 0:
            cls, outer, name = self.import_info[-outer - 1]
        return name

    def imports_of(self, class_name):
        """[(object name, package path)] for imports of one class, e.g. 'Texture2D'."""
        return [(n, self.import_path(i)) for i, (c, o, n) in enumerate(self.import_info) if c == class_name]

    def obj(self, idx):
        """Name of an object reference (FPackageIndex): >0 export, <0 import, 0 none."""
        if idx > 0: return self.exports[idx - 1]['name']
        if idx < 0: return self.imports[-idx - 1]
        return None

    def value(self, t, raw, extra):
        """Decode a property value far enough for timing/stamina sheets."""
        if t == 'FloatProperty': return round(struct.unpack('<f', raw)[0], 5)
        if t == 'IntProperty': return struct.unpack('<i', raw)[0]
        if t == 'BoolProperty': return bool(extra)
        if t == 'ObjectProperty': return self.obj(struct.unpack('<i', raw)[0])
        if t in ('NameProperty',): return self.names[struct.unpack_from('<i', raw)[0]]
        if t in ('ByteProperty', 'EnumProperty') and len(raw) == 8: return self.names[struct.unpack_from('<i', raw)[0]]
        if t == 'StructProperty' and extra not in ('Vector', 'Rotator', 'Quat', 'Guid', 'LinearColor', 'Color', 'Vector2D', 'SoftObjectPath', 'GameplayTagContainer'):
            save = self.e; self.e = raw
            try:
                sub, _ = self.props(0)
                return {k: self.value(*v) for k, v in sub.items()}
            except Exception:
                return f'<{extra} {len(raw)}b>'
            finally:
                self.e = save
        if t == 'StructProperty' and extra == 'Vector': return [round(x, 4) for x in struct.unpack('<fff', raw)]
        if t == 'ArrayProperty' and extra == 'StructProperty' and len(raw) >= 4:
            # count, one inner tag (name, type, size, array index, struct name, guid, flag), then the elements
            count, = struct.unpack_from('<i', raw, 0)
            save = self.e; self.e = raw
            try:
                o = 4
                _, o = self.name(o); _, o = self.name(o); o += 8
                inner, o = self.name(o); o += 17
                out = []
                for _ in range(count):
                    if inner in ('Vector',):
                        out.append([round(x, 4) for x in struct.unpack_from('<fff', raw, o)]); o += 12
                        continue
                    sub, o = self.props(o)
                    out.append({k: self.value(*v) for k, v in sub.items()})
                return out
            except Exception as ex:
                return f'<array of {extra} {len(raw)}b: {type(ex).__name__}>'
            finally:
                self.e = save
        if t == 'ArrayProperty' and extra in ('ObjectProperty', 'IntProperty') and len(raw) >= 4:
            count, = struct.unpack_from('<i', raw, 0)
            if len(raw) == 4 + 4 * count:
                vals = struct.unpack_from(f'<{count}i', raw, 4)
                return [self.obj(v) for v in vals] if extra == 'ObjectProperty' else list(vals)
        if t == 'ArrayProperty' and extra == 'ByteProperty' and len(raw) >= 4:
            count, = struct.unpack_from('<i', raw, 0)
            if len(raw) == 4 + count:
                return list(raw[4:])
        return f'<{t} {len(raw)}b>'

    def defaults(self, export_name):
        ex = next(x for x in self.exports if x['name'] == export_name)
        props, _ = self.props(ex['offset'])
        return {k: self.value(*v) for k, v in props.items()}

    # ---- tagged properties (cooked 4.25 still writes tags) ----
    def name(self, o):
        i, num = struct.unpack_from('<ii', self.e, o)
        return self.names[i] + (f'_{num-1}' if num else ''), o + 8

    def props(self, o=0):
        """Parse tagged properties from the export data; returns ({name: (type, raw bytes, extra)}, end)."""
        e, out = self.e, {}
        while True:
            n, o = self.name(o)
            if n == 'None':
                return out, o
            t, o = self.name(o)
            size, idx = struct.unpack_from('<ii', e, o); o += 8
            extra = None
            if t == 'StructProperty':
                extra, o = self.name(o); o += 16
            elif t == 'BoolProperty':
                extra = e[o]; o += 1
            elif t in ('ByteProperty', 'EnumProperty', 'ArrayProperty', 'SetProperty'):
                extra, o = self.name(o)
            elif t == 'MapProperty':
                k, o = self.name(o); v, o = self.name(o); extra = (k, v)
            o += 1  # HasPropertyGuid
            out[n] = (t, e[o:o+size], extra); o += size
