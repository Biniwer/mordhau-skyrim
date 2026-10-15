"""Dev tool: read MORDHAU's movement blends for a weapon and pack their clips (local cache, never shipped).

Reads, from the player's own install: the weapon definition's UpperBlendSpace and LowerAnimation, the shared leg blend
BS_LowerBodyLocomotion_Front, and the jump / fall / land clips. Writes every clip as a full-skeleton .dmfull and a
locomotion.txt the plugin loads:  layer;clip;direction_deg;axis_value;travel_speed_cm_s;length_s
(layer lower: axis 0 = walk, 1 = run; layer upper: axis = the blend's speed value; layer still/jump/fall/land: one clip).
travel speed is measured from the clip: the planted foot's speed against the hips.
Usage: bake_locomotion.py <out dir> <weapon definition name, e.g. BP_OneHandedSword>
"""
import math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import animdecode, skelmesh, uasset
from bake_character import bake_clip, BODY
from render_pose import key, local_matrix, component

rows = uasset.rows()

def find(name, kind=''):
    r = next((x for x in rows if x[1].endswith('/' + name + '.uasset') and kind in x[1]), None)
    return r[1][:-7].split('Content/')[-1] if r else None

def travel_speed(clip):
    """cm/s the clip is authored for: the lower of the two feet moves backwards against the hips at that speed."""
    m = skelmesh.load(BODY); pose = m['pose']; parents = [b[1] for b in m['bones']]; names = [b[0].lower() for b in m['bones']]
    a = animdecode.decode(clip); frames = a['num_frames']; dt = a['length'] / max(frames - 1, 1)
    rot = {t['bone_index']: t['rot'][2] for t in a['tracks'] if t['rot'] and t['bone_index'] < len(pose)}
    tra = {t['bone_index']: t['trans'][2] for t in a['tracks'] if t['trans'] and t['bone_index'] < len(pose)}
    feet = [names.index('leftfoot'), names.index('rightfoot')]; hips = names.index('hips')
    pos = []
    for f in range(frames):
        loc = [local_matrix(key(rot[i], f, frames, True) if i in rot else q, key(tra[i], f, frames, False) if i in tra else t) for i, (q, t, s) in enumerate(pose)]
        comp = component(parents, loc)
        pos.append([comp[j][:3, 3] - comp[hips][:3, 3] for j in feet])
    pos = np.array(pos)                      # (frames, 2 feet, 3)
    speeds = []
    for f in range(frames - 1):
        low = int(np.argmin(pos[f, :, 2]))   # the planted foot is the lower one
        v = (pos[f + 1, low, :2] - pos[f, low, :2]) / dt
        speeds.append(float(np.linalg.norm(v)))
    return float(np.median(speeds)), a['length']

def main(out, weapon):
    os.makedirs(out, exist_ok=True)
    p = uasset.Package(find(weapon))
    d = p.defaults('Default__' + weapon + '_C')
    lines, done = [], {}
    def add(layer, clip_name, direction, axis):
        path = find(clip_name, '/Animations/')
        if not path:
            print(f'  {clip_name}: clip not found'); return
        if clip_name not in done:
            bake_clip(os.path.join(out, clip_name + '.dmfull'), path.replace('Mordhau/', '', 1) if path.startswith('Mordhau/Animations') else path)
            done[clip_name] = travel_speed(path.replace('Mordhau/', '', 1) if path.startswith('Mordhau/Animations') else path)
        speed, length = done[clip_name]
        lines.append(f'{layer};{clip_name};{direction:g};{axis:g};{speed:.1f};{length:.4f}')
    upper = uasset.Package(find(d['UpperBlendSpace']))
    for s in upper.defaults(d['UpperBlendSpace'])['SampleData']:
        add('upper', s['Animation'], s['SampleValue'][0], s['SampleValue'][1])
    lower_name = 'BS_LowerBodyLocomotion_Front'
    lower = uasset.Package(find(lower_name))
    for s in lower.defaults(lower_name)['SampleData']:
        add('lower', s['Animation'], s['SampleValue'][0], s['SampleValue'][1])
    add('still', d['LowerAnimation'], 0, 0)
    for layer, key_ in (('jump', 'JumpAnimation'), ('fall', 'FallingAnimation'), ('land', 'LandAnimation')):
        if isinstance(d.get(key_), str): add(layer, d[key_], 0, 0)
    open(os.path.join(out, 'locomotion.txt'), 'w', encoding='ascii', newline='\n').write('\n'.join(lines) + '\n')
    print(f'{len(lines)} blend samples, {len(done)} clips')

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
