"""Dev tool: draw a MORDHAU character from wearable definitions, with the textures those definitions name.

Each wearable definition (Mordhau/Blueprints/Wearables/...) gives the model (Mesh) and its AlbedoMap; the base body and
head use the colour maps their own materials name. Everything is read from the player's own install; nothing is painted
here. Local picture only (trial step 1b). Usage: render_outfit.py <out.png> <clip|'rest'> <fraction> <yaw> <wearable>...
"""
import math, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from PIL import Image
import skelmesh, texture, uasset
from render_pose import skin_matrices, skinned

BODY = 'UMA/UMA/Master/UMA_Master'
# base body: which colour map each of its materials uses (read from the materials' texture lists)
BODY_ALBEDO = {'INST_Torso': 'UMA/Textures/MaleBody01_d', 'INST_HeadMaleCaucasian01': 'UMA/Separated/MaleCaucasian01/MaleHead01_d'}


def find_texture(name):
    row = next((x for x in uasset.rows() if x[1].endswith('/' + name + '.uasset')), None)
    return row[1][:-7].split('Content/')[-1] if row else None


def wearable(bp):
    """(mesh path, albedo texture path) from a wearable definition, following its parents for unset values."""
    mesh = albedo = None
    while bp and not (mesh and albedo):
        p = uasset.Package(bp)
        name = bp.split('/')[-1]
        props, _ = p.props(next(e for e in p.exports if e['name'] == 'Default__' + name + '_C')['offset'])
        if not mesh and 'Mesh' in props:
            mesh = p.names[struct.unpack_from('<i', props['Mesh'][1])[0]].replace('/Game/', '').split('.')[0]
        if not albedo and 'AlbedoMap' in props:
            obj = p.obj(struct.unpack('<i', props['AlbedoMap'][1])[0])
            albedo = next((path for n, path in p.imports_of('Texture2D') if n == obj), None)
            albedo = albedo.replace('/Game/', '') if albedo else None
        cls = next((e for e in p.exports if e['name'] == name + '_C'), None)
        parent = p.obj(cls['super']) if cls else None
        row = next((x for x in uasset.rows() if parent and x[1].endswith('/' + parent[:-2] + '.uasset')), None)
        bp = row[1][:-7].split('Content/')[-1] if row else None
    return mesh, albedo


def draw(parts, yaw_deg, out, size=(720, 960), scale=4.3, centre_z=100.0):
    w, h = size
    img = np.full((h, w, 3), 236, dtype=np.float64)
    zbuf = np.full((h, w), -1e9)
    yaw = math.radians(yaw_deg); c, s = math.cos(yaw), math.sin(yaw)
    light = np.array([0.35, 0.75, 0.55]); light /= np.linalg.norm(light)
    for verts, tris, uvs, tex in parts:
        th, tw = tex.shape[:2]
        vx = verts[:, 0] * c - verts[:, 1] * s
        vy = verts[:, 0] * s + verts[:, 1] * c
        sx = w / 2 + vx * scale
        sy = h / 2 - (verts[:, 2] - centre_z) * scale
        view = np.stack([vx, vy, verts[:, 2]], axis=1)
        for a, b, d in tris:
            x0, y0, x1, y1, x2, y2 = sx[a], sy[a], sx[b], sy[b], sx[d], sy[d]
            minx, maxx = int(max(min(x0, x1, x2), 0)), int(min(max(x0, x1, x2) + 1, w - 1))
            miny, maxy = int(max(min(y0, y1, y2), 0)), int(min(max(y0, y1, y2) + 1, h - 1))
            if minx > maxx or miny > maxy: continue
            den = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
            if abs(den) < 1e-9: continue
            n = np.cross(view[b] - view[a], view[d] - view[a]); ln = np.linalg.norm(n)
            if ln < 1e-9: continue
            shade = 0.45 + 0.55 * abs((n / ln) @ light)
            gx, gy = np.meshgrid(np.arange(minx, maxx + 1) + 0.5, np.arange(miny, maxy + 1) + 0.5)
            l0 = ((y1 - y2) * (gx - x2) + (x2 - x1) * (gy - y2)) / den
            l1 = ((y2 - y0) * (gx - x2) + (x0 - x2) * (gy - y2)) / den
            l2 = 1 - l0 - l1
            inside = (l0 >= 0) & (l1 >= 0) & (l2 >= 0)
            if not inside.any(): continue
            depth = l0 * vy[a] + l1 * vy[b] + l2 * vy[d]
            sub = zbuf[miny:maxy+1, minx:maxx+1]
            hit = inside & (depth > sub)
            if not hit.any(): continue
            u = l0 * uvs[a, 0] + l1 * uvs[b, 0] + l2 * uvs[d, 0]
            v = l0 * uvs[a, 1] + l1 * uvs[b, 1] + l2 * uvs[d, 1]
            tx = np.mod(np.floor(u * tw), tw).astype(np.int64); ty = np.mod(np.floor(v * th), th).astype(np.int64)
            texel = tex[ty, tx, :3].astype(np.float64)
            sub[hit] = depth[hit]
            img[miny:maxy+1, minx:maxx+1][hit] = texel[hit] * shade
    Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)).save(out)


def pieces(mesh_path, albedo_by_material, default_albedo, clip, fraction, cache):
    mesh = skelmesh.load(mesh_path)
    verts = skinned(mesh, skin_matrices(mesh, clip, fraction))
    out = []
    for s in mesh['sections']:
        mat = mesh['materials'][s['material']][0] if s['material'] < len(mesh['materials']) else None
        tex_path = albedo_by_material.get(mat, default_albedo) if albedo_by_material is not None else default_albedo
        if not tex_path:
            print(f'  {mesh["name"]}: section with material {mat} skipped (no colour map chosen for it yet)')
            continue
        if tex_path not in cache:
            cache[tex_path], t = texture.rgba(tex_path, 1024)
            print(f'  texture {t["name"]}: {t["width"]}x{t["height"]} {t["format"]}')
        first = s['base_index'] // 3
        out.append((verts, mesh['indices'][first:first + s['triangles']], mesh['uvs'], cache[tex_path]))
    return out


if __name__ == '__main__':
    out, clip, fraction, yaw = sys.argv[1], sys.argv[2], float(sys.argv[3]), float(sys.argv[4])
    cache, parts = {}, []
    parts += pieces(BODY, BODY_ALBEDO, None, clip, fraction, cache)
    for bp in sys.argv[5:]:
        mesh_path, albedo = wearable(bp)
        print(f'{bp.split("/")[-1]}: model {mesh_path}, colour map {albedo}')
        parts += pieces(mesh_path, None, albedo, clip, fraction, cache)
    draw(parts, yaw, out)
    print('wrote', out)
