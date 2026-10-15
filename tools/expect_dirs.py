"""Dev check: where MORDHAU's clip says each upper-body bone points at 25/50/75 % (mirrored into Skyrim's space)."""
import json, math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import animdecode, skeleton as sk
from bake_clip import key_at
rows = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'sheets', 'bone_map.json'), encoding='utf-8'))['rows']
names, parents, rrot, rpos = sk.load(); low = [n.lower() for n in names]
a = animdecode.decode(sys.argv[1]); frames = a['num_frames']
rt = {t['bone_index']: t['rot'][2] for t in a['tracks'] if t['rot'] and t['bone_index'] < len(names)}
tt = {t['bone_index']: t['trans'][2] for t in a['tracks'] if t['trans'] and t['bone_index'] < len(names)}
def vkey(keys, f):
    if len(keys) == 1: return keys[0]
    x = f * (len(keys) - 1) / max(frames - 1, 1); i = min(int(x), len(keys) - 2); u = x - i
    return tuple(p + (q - p) * u for p, q in zip(keys[i], keys[i+1]))
for pct in (25, 50, 75):
    f = pct / 100 * (frames - 1)
    lrot = [key_at(rt[i], f, frames) if i in rt else rrot[i] for i in range(len(names))]
    lpos = [vkey(tt[i], f) if i in tt else rpos[i] for i in range(len(names))]
    crot, cpos = sk.component(parents, lrot, lpos)
    for r in rows:
        if r['layer'] != 'upper' or not r['dir_mordhau_bone']: continue
        i, j = low.index(r['mordhau_bone'].lower()), low.index(r['dir_mordhau_bone'].lower())
        v = [cpos[j][k] - cpos[i][k] for k in range(3)]; n = math.sqrt(sum(c*c for c in v))
        print(f"probe {pct}% {r['id']} dir ({-v[0]/n:+.2f},{v[1]/n:+.2f},{v[2]/n:+.2f})")
