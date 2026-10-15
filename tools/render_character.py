"""Dev tool: draw one of the player's own MORDHAU characters from its spec (tools/character_spec.py), dyed and with
covered body parts hidden. Everything comes from the player's install; local picture only.

Two things here are this mod's reading, not MORDHAU's own shader or rules:
- dye: colour map x lerp(1, colour, mask channel); mask red = colour 1, blue = colour 2, green = metal tint;
- hiding: a body triangle is dropped when all its vertices are mainly weighted to bones of a hidden part.
Usage: render_character.py <out.png> <spec.json> <clip|'rest'> <fraction> <yaw>
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import skelmesh, texture
from render_pose import skin_matrices, skinned
from render_outfit import BODY, BODY_ALBEDO, draw

HIDE_BONES = {
    'Chest': ['LowerBack', 'Spine', 'Spine1', 'LeftShoulder', 'RightShoulder', 'LeftTrapezius', 'RightTrapezius', 'LowerBackBelly',
              'LeftInnerBreast', 'RightInnerBreast', 'LeftOuterBreast', 'RightOuterBreast'],
    'LeftArm': ['LeftArm', 'LeftForeArm', 'LeftForeArmTwist'], 'RightArm': ['RightArm', 'RightForeArm', 'RightForeArmTwist'],
    'LeftHand': ['LeftHand'], 'RightHand': ['RightHand'],
    'LeftLeg': ['LeftUpLeg', 'LeftLeg', 'LeftGluteus', 'Hips'], 'RightLeg': ['RightUpLeg', 'RightLeg', 'RightGluteus', 'Hips'],
    'LeftFoot': ['LeftFoot', 'LeftToeBase'], 'RightFoot': ['RightFoot', 'RightToeBase'],
}


def hidden_bone_ids(mesh, hide):
    names = [b[0] for b in mesh['bones']]
    out = set()
    for part in hide:
        for stem in HIDE_BONES.get(part, []):
            for i, n in enumerate(names):
                # the bone itself, its "Adjust" twin, and fingers for hands
                if n == stem or n == stem + 'Adjust' or (stem.endswith('Hand') and n.startswith(stem + 'Finger')):
                    out.add(i)
    return out


def visible_triangles(mesh, tris, hidden):
    if not hidden:
        return tris
    dominant = mesh['bone_ids'][np.arange(len(mesh['weights'])), mesh['weights'].argmax(axis=1)]
    hid = np.isin(dominant, list(hidden))
    return tris[~hid[tris].all(axis=1)]


def gamma(c):
    return np.power(np.clip(np.array(c, dtype=np.float64), 0, 1), 1 / 2.2)


def dyed(piece, size=1024):
    base, _ = texture.rgba(piece['colour_map'], size)
    out = base[..., :3].astype(np.float64)
    if piece['pattern_mask']:
        mask, _ = texture.rgba(piece['pattern_mask'], size)
        h, w = out.shape[:2]
        ys = (np.arange(h) * mask.shape[0] // h)[:, None]; xs = (np.arange(w) * mask.shape[1] // w)[None, :]
        m = mask[ys, xs, :3].astype(np.float64) / 255.0
        tints = [d for d in piece['dye'] if d['used']] + ([piece['metal_tint']] if piece.get('metal_tint') else [])
        for d in tints:
            if d['linear_rgb'] and d['channel'] is not None:
                k = m[..., d['channel']:d['channel'] + 1]
                out = out * (1 - k + k * gamma(d['linear_rgb']))
    return np.clip(out, 0, 255).astype(np.uint8)


def build(spec, clip, fraction):
    hide = sorted({h[5:] for p in spec['pieces'] for h in p['hide']})
    parts, cache = [], {}
    body = skelmesh.load(BODY)
    verts = skinned(body, skin_matrices(body, clip, fraction))
    hidden = hidden_bone_ids(body, hide)
    for s in body['sections']:
        mat = body['materials'][s['material']][0]
        tp = BODY_ALBEDO.get(mat)
        if not tp:
            continue
        first = s['base_index'] // 3
        tris = visible_triangles(body, body['indices'][first:first + s['triangles']], hidden)
        if tp not in cache:
            cache[tp] = texture.rgba(tp, 1024)[0]
        parts.append((verts, tris, body['uvs'], cache[tp]))
    for p in spec['pieces']:
        if not p['model'] or not p['colour_map']:
            continue
        mesh = skelmesh.load(p['model'])
        v = skinned(mesh, skin_matrices(mesh, clip, fraction))
        parts.append((v, mesh['indices'], mesh['uvs'], dyed(p)))
    return parts, hide


if __name__ == '__main__':
    out, spec_path, clip, fraction, yaw = sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4]), float(sys.argv[5])
    spec = json.load(open(spec_path, encoding='utf-8'))
    parts, hide = build(spec, clip, fraction)
    draw(parts, yaw, out)
    print('wrote', out, '| hidden body parts:', ', '.join(hide))
