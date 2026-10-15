"""Read a cooked MORDHAU skeletal mesh (UE 4.25, MORDHAU's build) far enough to draw it: bones, LOD 0 positions,
triangles, skin weights. Read-only, from the player's own install.

load(base) -> dict(name, bones [(name, parent)], pose [(quat, pos, scale)], positions (N,3), indices (T,3),
bone_ids (N,4) as mesh bone indices, weights (N,4) summing to 1, sections [...], materials [...]).
Run as a script to print a summary.
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import uasset


class R:
    """Cursor over the export data."""

    def __init__(self, pkg, o):
        self.p, self.e, self.o = pkg, pkg.e, o

    def i32(self):
        v, = struct.unpack_from('<i', self.e, self.o); self.o += 4; return v

    def u32(self):
        v, = struct.unpack_from('<I', self.e, self.o); self.o += 4; return v

    def i16(self):
        v, = struct.unpack_from('<h', self.e, self.o); self.o += 2; return v

    def u16(self):
        v, = struct.unpack_from('<H', self.e, self.o); self.o += 2; return v

    def u8(self):
        v = self.e[self.o]; self.o += 1; return v

    def f(self, n=1):
        v = struct.unpack_from(f'<{n}f', self.e, self.o); self.o += 4*n; return v if n > 1 else v[0]

    def name(self):
        v, self.o = self.p.name(self.o); return v

    def skip(self, n):
        self.o += n

    def strip(self):
        g, c = self.e[self.o], self.e[self.o+1]; self.o += 2; return g, c

    def bulk(self):
        """Bulk array: element size, count, raw bytes."""
        size, count = struct.unpack_from('<ii', self.e, self.o); self.o += 8
        data = self.e[self.o:self.o + size*count]; self.o += size*count
        return size, count, data


def load(base):
    p = uasset.Package(base)
    name = base.split('/')[-1]
    # (a package can hold other objects of the same name, e.g. a clothing asset: take the mesh itself)
    same = [x for x in p.exports if x['name'] == name]
    ex = next((x for x in same if p.obj(x['class']) == 'SkeletalMesh' or x['class'] == 'SkeletalMesh'), same[0])
    _, o = p.props(ex['offset'])
    r = R(p, o)
    r.skip(4)                      # no object guid
    r.strip()
    bounds = r.f(7)
    materials = []
    for _ in range(r.i32()):
        mat = p.obj(r.i32()); slot = r.name()
        r.skip(4)                  # cooked: no imported slot name
        r.skip(24)                 # UV channel info
        materials.append((mat, slot))
    # reference skeleton
    nb = r.i32()
    bones = [(r.name(), r.i32()) for _ in range(nb)]
    assert r.i32() == nb
    pose = []
    for _ in range(nb):
        v = r.f(10); pose.append((v[0:4], v[4:7], v[7:10]))
    for _ in range(r.i32()):
        r.name(); r.i32()

    assert r.i32() == 1, 'not cooked'
    nlod = r.i32()
    # ---- LOD 0 ----
    r.strip()
    cooked_out, inlined = r.i32(), r.i32()
    assert not cooked_out and inlined, (cooked_out, inlined)
    r.skip(2 * r.i32())            # required bones
    sections = []
    for _ in range(r.i32()):
        r.strip()
        mat = r.i16(); base_index = r.u32(); ntri = r.u32()
        r.skip(4)                  # recompute tangent
        r.skip(1)                  # one byte the stock 4.25 layout lacks (vertex mask channel in later engines)
        r.skip(4)                  # cast shadow
        base_vertex = r.u32()
        r.skip(64 * r.i32())       # cloth mapping
        bone_map = [r.u16() for _ in range(r.i32())]
        nverts = r.i32(); max_inf = r.i32()
        r.skip(2 + 20)             # cloth asset index, clothing data
        r.skip(4 * r.i32()); r.skip(8 * r.i32())  # duplicated vertices buffers
        r.skip(4)                  # disabled
        sections.append({'material': mat, 'base_index': base_index, 'triangles': ntri, 'base_vertex': base_vertex,
                         'vertices': nverts, 'bone_map': bone_map, 'max_influences': max_inf})
    r.skip(2 * r.i32())            # active bones
    r.u32()                        # buffers size
    r.strip()
    r.u8()
    isize, icount, idata = r.bulk()
    indices = np.frombuffer(idata, dtype='<u2' if isize == 2 else '<u4').astype(np.int64).reshape(-1, 3)
    r.u32(); nv = r.u32()
    psize, pcount, pdata = r.bulk()
    assert psize == 12 and pcount == nv
    positions = np.frombuffer(pdata, dtype='<f4').reshape(-1, 3).astype(np.float64)
    r.strip()
    ntex = r.i32(); assert r.i32() == nv
    r.skip(8)                      # precision flags
    tsize, tcount, tdata = r.bulk()  # tangent basis: per vertex TangentX then TangentZ (the normal), 4 signed bytes each
    assert tsize == 8 and tcount == nv, (tsize, tcount)
    normals = np.frombuffer(tdata, dtype=np.int8).reshape(nv, 8)[:, 4:7].astype(np.float64) / 127.0
    usize, ucount, udata = r.bulk()
    all_uvs = np.frombuffer(udata, dtype='<f2' if usize == 4 else '<f4').reshape(nv, ntex, 2).astype(np.float64) if ucount else None
    uvs = all_uvs[:, 0, :] if ucount else None
    # skin weights: per vertex max_inf bone bytes then max_inf weight bytes
    r.strip()
    variable = r.i32(); max_inf = r.i32(); r.i32(); assert r.i32() == nv
    assert not variable, 'variable bone influences not handled'
    r.skip(4)                      # 16-bit bone index flag (false)
    wsize, wcount, wdata = r.bulk()
    assert wsize == 1 and wcount == nv * max_inf * 2, (wsize, wcount, nv, max_inf)
    raw = np.frombuffer(wdata, dtype=np.uint8).reshape(nv, 2, max_inf)
    local = raw[:, 0, :4].astype(np.int64)
    weights = raw[:, 1, :4].astype(np.float64)
    weights /= np.maximum(weights.sum(axis=1, keepdims=True), 1.0)
    bone_ids = np.zeros_like(local)
    for s in sections:
        bm = np.array(s['bone_map'], dtype=np.int64)
        sl = slice(s['base_vertex'], s['base_vertex'] + s['vertices'])
        bone_ids[sl] = bm[np.minimum(local[sl], len(bm) - 1)]
    return {'name': name, 'bounds': bounds, 'materials': materials, 'bones': bones, 'pose': pose, 'lods': nlod,
            'positions': positions, 'normals': normals, 'indices': indices, 'uvs': uvs, 'all_uvs': all_uvs, 'bone_ids': bone_ids, 'weights': weights, 'sections': sections}


if __name__ == '__main__':
    for base in sys.argv[1:]:
        try:
            m = load(base)
        except Exception as ex:  # report and carry on: this is a survey tool
            print(f'{base.split("/")[-1]}: FAILED {type(ex).__name__} {ex}')
            continue
        lo, hi = m['positions'].min(axis=0), m['positions'].max(axis=0)
        print(f"{m['name']}: {len(m['bones'])} bones, {len(m['positions'])} verts, {len(m['indices'])} tris, {len(m['sections'])} sections, "
              f"{m['lods']} LODs, box x {lo[0]:.0f}..{hi[0]:.0f} y {lo[1]:.0f}..{hi[1]:.0f} z {lo[2]:.0f}..{hi[2]:.0f}, "
              f"weights sum min {m['weights'].sum(axis=1).min():.2f}, bone ids max {m['bone_ids'].max()}")
