"""Makes sheets/skyrim_combat_sounds.json: Skyrim's own weapon sounds by kind, read from the player's Skyrim.esm and the
official add-ons (their sound records, SNDR, by editor id: WPNImpact... = hits, WPNSwing... = swings, WPNBlock... /
WPNBash... = blocks and bashes, WPN...Draw / WPN...Sheathe = drawing and putting away). The settings menu's Skyrim
sound sliders turn each kind down (plugin SkyrimSounds.cpp). Only names and record numbers are kept, no sound.
usage: skyrim_combat_sounds.py [<Skyrim folder>]"""
import json, os, struct, sys, zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKYRIM = sys.argv[1] if len(sys.argv) > 1 else r'E:\steam object\steamapps\common\Skyrim Special Edition'
MASTERS = ['Skyrim.esm', 'Update.esm', 'Dawnguard.esm', 'HearthFires.esm', 'Dragonborn.esm']

def kind(edid):
    if not edid.startswith('WPN'):
        return None
    if edid.startswith('WPNImpact'):
        return 'hits'
    if edid.startswith('WPNSwing'):
        return 'swings'
    if edid.startswith(('WPNBlock', 'WPNBash')):
        return 'blocks'
    if 'Draw' in edid or 'Sheathe' in edid:
        return 'draw'
    return None

def sounds(path):
    data = open(path, 'rb').read()
    masters = 0
    hsize = struct.unpack_from('<I', data, 4)[0]
    q = 24
    while q < 24 + hsize:
        st, ss = struct.unpack_from('<4sH', data, q)
        masters += st == b'MAST'
        q += 6 + ss
    o = 24 + hsize
    while o < len(data):
        size, label = struct.unpack_from('<I4s', data, o + 4)
        if label == b'SNDR':
            p, end = o + 24, o + size
            while p < end:
                rsize, flags, fid = struct.unpack_from('<III', data, p + 4)
                body = data[p + 24:p + 24 + rsize]
                if flags & 0x40000:
                    body = zlib.decompress(body[4:])
                if body[:4] == b'EDID':
                    edid = body[6:6 + struct.unpack_from('<H', body, 4)[0]].rstrip(b'\0').decode('latin1')
                    if (fid >> 24) == masters:      # (this file's own records; overrides of a master's are found in the master)
                        yield fid & 0xFFFFFF, edid
                p += 24 + rsize
        o += size

rows = []
for m in MASTERS:
    path = os.path.join(SKYRIM, 'Data', m)
    if not os.path.exists(path):
        continue
    for local, edid in sounds(path):
        k = kind(edid)
        if k:
            rows.append({'file': m, 'id': f'{local:06X}', 'edid': edid, 'kind': k})
sheet = {'_about': __doc__.split('\n')[0] + ' Made by tools/skyrim_combat_sounds.py from the official files; kinds: hits, swings, blocks, draw.', 'rows': rows}
out = os.path.join(ROOT, 'sheets', 'skyrim_combat_sounds.json')
open(out, 'w', encoding='utf-8', newline='\n').write(json.dumps(sheet, indent=1) + '\n')
from collections import Counter
print(out, Counter(r['kind'] for r in rows), Counter(r['file'] for r in rows))
