"""Dev tool: pose MORDHAU skeletal meshes with a MORDHAU clip and draw a picture (trial step 1; local only).

Everything is read from the player's own install; the picture is for judging the decode, it is not shipped.
Usage: render_pose.py <out.png> <clip under Mordhau/Content/Mordhau/ or 'rest'> <fraction 0..1> <yaw degrees> <mesh>[:r,g,b] ...
"""
import math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from PIL import Image
import animdecode, skelmesh


def quat_matrix(q):
    x, y, z, w = q
    return np.array([[1 - 2*(y*y + z*z), 2*(x*y - w*z), 2*(x*z + w*y)],
                     [2*(x*y + w*z), 1 - 2*(x*x + z*z), 2*(y*z - w*x)],
                     [2*(x*z - w*y), 2*(y*z + w*x), 1 - 2*(x*x + y*y)]])


def local_matrix(q, t):
    m = np.eye(4); m[:3, :3] = quat_matrix(q); m[:3, 3] = t
    return m


def component(parents, locals_):
    out = [None] * len(parents)
    for i, par in enumerate(parents):
        out[i] = locals_[i] if par < 0 else out[par] @ locals_[i]
    return out


def nlerp(a, b, t):
    a, b = np.array(a), np.array(b)
    if a @ b < 0: b = -b
    q = a + (b - a) * t
    return q / np.linalg.norm(q)


def key(keys, f, frames, quat):
    if len(keys) == 1: return np.array(keys[0])
    x = f * (len(keys) - 1) / max(frames - 1, 1); i = min(int(x), len(keys) - 2); u = x - i
    return nlerp(keys[i], keys[i+1], u) if quat else np.array(keys[i]) + (np.array(keys[i+1]) - np.array(keys[i])) * u


def skin_matrices(mesh, clip, fraction):
    bones, pose = mesh['bones'], mesh['pose']
    parents = [b[1] for b in bones]
    ref_local = [local_matrix(q, t) for q, t, s in pose]
    ref = component(parents, ref_local)
    if clip == 'rest':
        return [np.eye(4)] * len(bones)
    a = animdecode.decode(clip); frames = a['num_frames']; f = fraction * (frames - 1)
    rot = {t['bone_index']: t['rot'][2] for t in a['tracks'] if t['rot'] and t['bone_index'] < len(bones)}
    tra = {t['bone_index']: t['trans'][2] for t in a['tracks'] if t['trans'] and t['bone_index'] < len(bones)}
    cur_local = []
    for i, (q, t, s) in enumerate(pose):
        cq = key(rot[i], f, frames, True) if i in rot else q
        ct = key(tra[i], f, frames, False) if i in tra else t
        cur_local.append(local_matrix(cq, ct))
    cur = component(parents, cur_local)
    return [cur[i] @ np.linalg.inv(ref[i]) for i in range(len(bones))]


def skinned(mesh, mats):
    m = np.stack(mats)                                    # (B,4,4)
    p = np.concatenate([mesh['positions'], np.ones((len(mesh['positions']), 1))], axis=1)  # (N,4)
    out = np.zeros((len(p), 3))
    for k in range(4):
        mk = m[mesh['bone_ids'][:, k]]                    # (N,4,4)
        out += mesh['weights'][:, k:k+1] * np.einsum('nij,nj->ni', mk, p)[:, :3]
    return out


def draw(parts, yaw_deg, out, size=(720, 960), scale=4.3, centre_z=100.0):
    w, h = size
    img = np.full((h, w, 3), 236, dtype=np.float64)
    zbuf = np.full((h, w), -1e9)
    yaw = math.radians(yaw_deg); c, s = math.cos(yaw), math.sin(yaw)
    light = np.array([0.35, 0.75, 0.55]); light /= np.linalg.norm(light)
    for verts, tris, colour in parts:
        # view: camera in front of the character (which faces +Y), looking back along -Y, Z up
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
            n /= ln
            shade = 0.35 + 0.65 * abs(n @ light)          # two-sided: cloth and hair cards face both ways
            xs = np.arange(minx, maxx + 1) + 0.5; ys = np.arange(miny, maxy + 1) + 0.5
            gx, gy = np.meshgrid(xs, ys)
            l0 = ((y1 - y2) * (gx - x2) + (x2 - x1) * (gy - y2)) / den
            l1 = ((y2 - y0) * (gx - x2) + (x0 - x2) * (gy - y2)) / den
            l2 = 1 - l0 - l1
            inside = (l0 >= 0) & (l1 >= 0) & (l2 >= 0)
            if not inside.any(): continue
            depth = l0 * vy[a] + l1 * vy[b] + l2 * vy[d]
            sub = zbuf[miny:maxy+1, minx:maxx+1]
            hit = inside & (depth > sub)
            sub[hit] = depth[hit]
            img[miny:maxy+1, minx:maxx+1][hit] = np.array(colour) * shade
    Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)).save(out)


if __name__ == '__main__':
    out, clip, fraction, yaw = sys.argv[1], sys.argv[2], float(sys.argv[3]), float(sys.argv[4])
    parts = []
    for spec in sys.argv[5:]:
        base, _, col = spec.partition(':')
        colour = tuple(int(c) for c in col.split(',')) if col else (190, 190, 190)
        mesh = skelmesh.load(base)
        parts.append((skinned(mesh, skin_matrices(mesh, clip, fraction)), mesh['indices'], colour))
    draw(parts, yaw, out)
    print('wrote', out)
