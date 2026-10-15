"""Dev tool: convert one MORDHAU clip into Skyrim model space for the plugin (local cache, never shipped).

For every row of sheets/bone_map.json writes, per frame, the rotation that takes the bone from MORDHAU's rest
pose to its animated pose, expressed in Skyrim's model space (MORDHAU mesh space mirrored across X).
Output (little endian): 'DMC1', float length, int32 frames, int32 bones, then per bone:
char[64] skyrim node, char[64] skyrim direction node, float[3] MORDHAU rest direction (mirrored), frames x float[4] quat (x,y,z,w).
Usage: bake_clip.py <clip path under Mordhau/Content/Mordhau/, without extension> <out file>
"""
import json, math, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import animdecode, skeleton as sk

def nlerp(a, b, t):
    if sum(x*y for x, y in zip(a, b)) < 0: b = tuple(-x for x in b)
    q = tuple(x + (y - x)*t for x, y in zip(a, b)); n = math.sqrt(sum(c*c for c in q)) or 1.0
    return tuple(c/n for c in q)

def key_at(keys, frame, frames):
    if len(keys) == 1: return keys[0]
    f = frame * (len(keys) - 1) / max(frames - 1, 1)
    i = min(int(f), len(keys) - 2)
    return nlerp(keys[i], keys[i+1], f - i)

def main(clip, out):
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
    rows = json.load(open(os.path.join(root, 'sheets', 'bone_map.json'), encoding='utf-8'))['rows']
    names, parents, rrot, rpos = sk.load()
    low = [n.lower() for n in names]
    ref_crot, ref_cpos = sk.component(parents, rrot, rpos)
    a = animdecode.decode(clip)
    frames = a['num_frames']
    track = {t['bone_index']: t['rot'][2] for t in a['tracks'] if t['rot'] and t['bone_index'] < len(names)}
    per_frame = []
    for f in range(frames):
        local = [key_at(track[i], f, frames) if i in track else rrot[i] for i in range(len(names))]
        crot, _ = sk.component(parents, local, rpos)
        per_frame.append(crot)
    blob = struct.pack('<4sfii', b'DMC1', a['length'], frames, len(rows))
    for r in rows:
        i = low.index(r['mordhau_bone'].lower())
        d = (0.0, 0.0, 0.0)
        if r['dir_mordhau_bone']:
            j = low.index(r['dir_mordhau_bone'].lower())
            v = tuple(ref_cpos[j][k] - ref_cpos[i][k] for k in range(3)); n = math.sqrt(sum(c*c for c in v))
            d = (-v[0]/n, v[1]/n, v[2]/n)  # mirror X
        blob += struct.pack('<64s64s3f', r['skyrim_node'].encode(), r['dir_skyrim_node'].encode(), *d)
        for f in range(frames):
            x, y, z, w = sk.qmul(per_frame[f][i], sk.qinv(ref_crot[i]))
            blob += struct.pack('<4f', x, -y, -z, w)  # mirror X
    sys.stdout.buffer.write(blob) if out == '-' else open(out, 'wb').write(blob)
    print(f'{clip}: {frames} frames, {a["length"]:.3f}s, {len(rows)} bones, {len(blob)} bytes', file=sys.stderr)

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
