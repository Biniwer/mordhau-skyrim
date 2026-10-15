"""Read a cooked MORDHAU static mesh (UE 4.25, MORDHAU's build) far enough to draw it: LOD 0 positions, normals,
texture coordinates, triangles. Read-only, from the player's own install.

The buffers are found by their headers rather than by walking every field before them (the layout before the
buffers differs between engine builds): the position buffer is "stride 12, N, element size 12, N", followed by the
tangent buffer (element size 8 or 16, N), the texture coordinates (element size 4 or 8 per set, N x sets), then the
index buffer (16- or 32-bit, stored as bytes). Everything found is checked (indices below N, sizes inside the file).

load(base) -> the same dict tools/skelmesh.py gives (one bone "root", every vertex fully on it), so the character
baker can bake it. Run as a script to print a summary.
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import uasset


def load(base):
    pkg = uasset.Package(base)
    name = base.split('/')[-1]
    ex = next(e for e in pkg.exports if e['name'] == name)
    d = pkg.e[ex['offset']:ex['offset'] + ex['size']]
    # LOD 0 position buffer: the first that fits
    at = n = None
    for o in range(0, len(d) - 16):
        if d[o] == 12 and d[o + 1] == 0:
            a, cnt, b, c = struct.unpack_from('<IIII', d, o)
            if a == 12 and b == 12 and cnt == c and 3 <= cnt < 3000000 and o + 16 + 12 * cnt <= len(d):
                at, n = o, cnt
                break
    if at is None:
        raise ValueError('no position buffer found in ' + base)
    positions = np.frombuffer(d, '<f4', n * 3, at + 16).reshape(n, 3).astype(np.float64)
    o = at + 16 + 12 * n
    # tangents: (element size 8 or 16, N) within the next few dozen bytes
    tan = None
    for q in range(o, min(o + 64, len(d) - 8)):
        es, cnt = struct.unpack_from('<II', d, q)
        if es in (8, 16) and cnt == n and q + 8 + es * n <= len(d):
            tan = (q, es)
            break
    if tan is None:
        raise ValueError('no tangent buffer after the positions in ' + base)
    q, es = tan
    if es == 8:
        raw = np.frombuffer(d, 'i1', n * 8, q + 8).reshape(n, 8)
        normals = raw[:, 4:7].astype(np.float64) / 127.0
    else:
        raw = np.frombuffer(d, '<i2', n * 8, q + 8).reshape(n, 8)
        normals = raw[:, 4:7].astype(np.float64) / 32767.0
    length = np.maximum(np.linalg.norm(normals, axis=1, keepdims=True), 1e-6)
    normals = normals / length
    o = q + 8 + es * n
    # texture coordinates: (element size 4 = half floats or 8 = floats, N x sets)
    es, cnt = struct.unpack_from('<II', d, o)
    if es not in (4, 8) or cnt % n or not 1 <= cnt // n <= 8:
        raise ValueError(f'unexpected texture-coordinate header ({es}, {cnt}) in {base}')
    sets = cnt // n
    uv = np.frombuffer(d, '<f2' if es == 4 else '<f4', cnt * 2, o + 8).reshape(n, sets, 2).astype(np.float64)
    o = o + 8 + es * cnt
    # the colour buffer: 2 flag bytes, stride, count, and when it has colours their array (element size 4, count)
    cstride, ccount = struct.unpack_from('<II', d, o + 2)
    if cstride in (0, 4) and ccount in (0, n):
        o += 10
        if ccount:
            es, cnt = struct.unpack_from('<II', d, o)
            if es == 4 and cnt == n:
                o += 8 + 4 * n
    # index buffer: bool 32-bit, then bytes (element size 1, count), within a few header bytes
    indices = None
    for q in range(o, min(o + 4096, len(d) - 12)):
        wide, es, cnt = struct.unpack_from('<III', d, q)
        if wide in (0, 1) and es == 1 and cnt and cnt % (12 if wide else 6) == 0 and q + 12 + cnt <= len(d):
            idx = np.frombuffer(d, '<u4' if wide else '<u2', cnt // (4 if wide else 2), q + 12).astype(np.int64)
            if idx.max() < n and idx.max() >= n * 0.5:
                indices = idx.reshape(-1, 3)
                break
    if indices is None:
        raise ValueError('no index buffer found in ' + base)
    materials = [(i[2], i[2]) for i in pkg.import_info if i[0] in ('MaterialInstanceConstant', 'Material')]
    weights = np.zeros((n, 4)); weights[:, 0] = 1.0
    return {'name': name, 'bounds': None, 'materials': materials or [('none', 'none')], 'bones': [('root', -1)],
            'pose': [((0.0, 0.0, 0.0, 1.0), (0.0, 0.0, 0.0), (1.0, 1.0, 1.0))], 'lods': 1, 'positions': positions, 'normals': normals,
            'indices': indices, 'uvs': uv[:, 0, :], 'all_uvs': uv, 'bone_ids': np.zeros((n, 4), dtype=np.int64), 'weights': weights,
            'sections': [{'material': 0, 'base_index': 0, 'triangles': len(indices), 'base_vertex': 0, 'vertices': n, 'bone_map': [0], 'max_influences': 1}]}


def textures(material_base):
    """(colour map, normal map, surface map) paths named by a material, by their usual suffixes; None where not found."""
    names = [x[6:] for x in uasset.Package(material_base).names if x.startswith('/Game/') and '.' not in x]
    def pick(*ends):
        return next((x for x in names if x.lower().endswith(ends)), None)
    return (pick('_d', '_albedo', '_basecolor', '_diffuse', '_bc', '_color', '_diff'), pick('_n', '_normal', '_nrm'), pick('_rma', '_orm', '_mrao'))


if __name__ == '__main__':
    for base in sys.argv[1:]:
        m = load(base)
        p = m['positions']
        print(f"{m['name']}: {len(p)} vertices, {len(m['indices'])} triangles, size {np.ptp(p, axis=0).round(1)}, from {p.min(0).round(1)}, materials {m['materials']}")
