"""Makes MordhauWeapons.esp - MORDHAU's weapons as Skyrim items (sheets/mordhau_items.json) - from the player's own
Skyrim.esm: each item is the weapon record of the first Skyrim weapon in its row (kind, model, sounds, keywords), with
the row's name, damage, weight and value. A light plugin (ids 0x800 + row number). Items made by the plugin at run time
did not survive a save and reload; records in a plugin file do.
usage: make_items_esp.py [<Skyrim folder>]   -> writes Data/MordhauWeapons.esp and enables it in Plugins.txt"""
import json, os, struct, sys, zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKYRIM = sys.argv[1] if len(sys.argv) > 1 else r'E:\steam object\steamapps\common\Skyrim Special Edition'
ESM = os.path.join(SKYRIM, 'Data', 'Skyrim.esm')
OUT = os.path.join(SKYRIM, 'Data', 'MordhauWeapons.esp')
EDIDS = {'Steel Greatsword': 'SteelGreatsword', 'Iron Greatsword': 'IronGreatsword', 'Steel Battleaxe': 'SteelBattleaxe', 'Iron Battleaxe': 'IronBattleaxe',
         'Steel Warhammer': 'SteelWarhammer', 'Iron Warhammer': 'IronWarhammer', 'Steel Sword': 'SteelSword', 'Iron Sword': 'IronSword',
         'Steel War Axe': 'SteelWarAxe', 'Iron War Axe': 'IronWarAxe', 'Steel Mace': 'SteelMace', 'Iron Mace': 'IronMace', 'Steel Dagger': 'SteelDagger', 'Iron Dagger': 'IronDagger'}

def weapon_records(path):
    """EDID -> list of (subrecord type, bytes) for every WEAP record of the master."""
    found = {}
    with open(path, 'rb') as f:
        data = f.read()
    o = 0
    rtype, size = data[0:4], struct.unpack_from('<I', data, 4)[0]
    assert rtype == b'TES4'
    o = 24 + size
    while o < len(data):
        assert data[o:o + 4] == b'GRUP', data[o:o + 4]
        gsize, label = struct.unpack_from('<I4s', data, o + 4)
        if label == b'WEAP':
            p, end = o + 24, o + gsize
            while p < end:
                rt, rsize, flags = struct.unpack_from('<4sII', data, p)
                body = data[p + 24:p + 24 + rsize]
                if rt == b'WEAP':
                    if flags & 0x00040000:
                        body = zlib.decompress(body[4:])
                    subs, q, big = [], 0, None
                    while q < len(body):
                        st, ss = struct.unpack_from('<4sH', body, q)
                        if st == b'XXXX':
                            big = struct.unpack_from('<I', body, q + 6)[0]; q += 6 + ss; continue
                        n = big if big is not None else ss
                        big = None
                        subs.append((st, body[q + 6:q + 6 + n])); q += 6 + n
                    edid = next((d for t, d in subs if t == b'EDID'), b'').rstrip(b'\0').decode('latin1')
                    found[edid] = subs
                p += 24 + rsize
            break
        o += gsize
    return found

def sub(t, d):
    assert len(d) < 65536
    return t + struct.pack('<H', len(d)) + d

def record(rtype, formid, subs, flags=0):
    body = b''.join(sub(t, d) for t, d in subs)
    return rtype + struct.pack('<IIIIHH', len(body), flags, formid, 0, 44, 0) + body

rows = json.load(open(os.path.join(ROOT, 'sheets', 'mordhau_items.json'), encoding='utf-8'))['rows']
masters = weapon_records(ESM)
records = []
for i, row in enumerate(rows):
    like = next((EDIDS[n] for n in row['skyrim_like'].split('|') if EDIDS.get(n) in masters), None)
    if not like:
        print('no Skyrim weapon record for', row['name'], '(', row['skyrim_like'], ')'); continue
    subs = []
    for t, d in masters[like]:
        if t == b'EDID': d = ('Mordhau' + ''.join(c for c in row['name'] if c.isalnum())).encode() + b'\0'
        elif t == b'FULL': d = row['name'].encode('latin1') + b'\0'          # (the master keeps names in its string tables; here they are plain text)
        elif t == b'DESC': d = b'\0'
        elif t == b'DATA': d = struct.pack('<IfH', int(row['value']), float(row['weight']), int(row['damage']))
        elif t in (b'VMAD', b'EITM', b'EAMT', b'CNAM'): continue            # (no scripts, enchantment or template carried over)
        subs.append((t, d))
    records.append(record(b'WEAP', 0x01000800 + i, subs))
def grup(label, recs):
    body = b''.join(recs)
    return b'GRUP' + struct.pack('<I4sIHHHH', 24 + len(body), label, 0, 0, 0, 0, 0) + body

def wstr(s):
    return struct.pack('<H', len(s)) + s.encode()

# The settings menu (sheets/settings_menu.json): a hidden quest that runs from the game's start with the script
# MordhauSkyrimMCM (papyrus/, extends MCM Helper's MCM_ConfigBase) and a player alias carrying SkyUI's
# SKI_PlayerLoadGameAlias, so the menu registers again after each load. Laid out as SkyUI_SE.esp's own menu quests
# (SKI_WidgetManagerInstance): the same VMAD, DNAM flags and alias fields.
MCM_QUEST = 0x01000800 + len(rows)
vmad = (struct.pack('<hhH', 5, 2, 1) + wstr('MordhauSkyrimMCM') + b'\0' + struct.pack('<H', 0)   # one script, no properties
        + b'\x02' + struct.pack('<H', 0) + wstr('')                                               # no fragments
        + struct.pack('<H', 1) + struct.pack('<HhI', 0, 0, MCM_QUEST)                             # alias 0 of this quest
        + struct.pack('<hhH', 5, 2, 1) + wstr('SKI_PlayerLoadGameAlias') + b'\0' + struct.pack('<H', 0))
quest = record(b'QUST', MCM_QUEST, [
    (b'EDID', b'MordhauSkyrimMCMQuest\0'), (b'VMAD', vmad), (b'FULL', b'MordhauSkyrimMCMQuest\0'),
    (b'DNAM', bytes.fromhex('110100ff0000000000000000')), (b'NEXT', b''), (b'ANAM', struct.pack('<I', 1)),
    (b'ALST', struct.pack('<I', 0)), (b'ALID', b'PlayerAlias\0'), (b'FNAM', struct.pack('<I', 0)),
    (b'ALFR', struct.pack('<I', 0x14)), (b'VTCK', struct.pack('<I', 0)), (b'ALED', b'')])

header = record(b'TES4', 0, [(b'HEDR', struct.pack('<fiI', 1.70, len(records) + 3, 0x800 + len(rows) + 1)), (b'CNAM', b'MordhauSkyrim\0'),
                             (b'MAST', b'Skyrim.esm\0'), (b'DATA', struct.pack('<Q', 0))], flags=0x200)   # 0x200 = light plugin
with open(OUT, 'wb') as f:
    f.write(header + grup(b'WEAP', records) + grup(b'QUST', [quest]))
print(f'{OUT}: {len(records)} weapons and the settings menu quest {MCM_QUEST:08X}, {os.path.getsize(OUT)} bytes')

plugins = os.path.join(os.environ['LOCALAPPDATA'], 'Skyrim Special Edition', 'Plugins.txt')
lines = open(plugins, encoding='utf-8', errors='replace').read().splitlines() if os.path.exists(plugins) else []
if not any(l.strip().lstrip('*').lower() == 'mordhauweapons.esp' for l in lines):
    lines.append('*MordhauWeapons.esp')
    os.makedirs(os.path.dirname(plugins), exist_ok=True)
    open(plugins, 'w', encoding='utf-8', newline='\r\n').write('\n'.join(lines) + '\n')
    print('enabled in', plugins)
else:
    print('already listed in', plugins)
