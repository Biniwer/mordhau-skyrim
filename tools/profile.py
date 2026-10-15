"""Resolve a MORDHAU character profile (the player's own, from Game.ini) into wearable definitions. Read-only.

MORDHAU stores a profile as nine wearable entries (item number, colours, pattern). Top-level lists (head, upper chest,
legs) are in BP_MordhauSingleton; the other slots are lists on the parent item: head -> coif; upper chest -> lower chest,
arms, shoulders; arms -> hands; legs -> feet.
"""
import itertools, os, re, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import uasset

GAME_INI = os.path.expandvars(r'%LOCALAPPDATA%\Mordhau\Saved\Config\WindowsClient\Game.ini')
SLOTS = ['Head', 'Coif', 'Shoulders', 'UpperChest', 'LowerChest', 'Arms', 'Hands', 'Legs', 'Feet']
PARENT = {'Coif': ('Head', 'CoifWearables'), 'LowerChest': ('UpperChest', 'LowerChestWearables'), 'Arms': ('UpperChest', 'ArmsWearables'),
          'Shoulders': ('UpperChest', 'ShouldersWearables'), 'Hands': ('Arms', 'HandsWearables'), 'Feet': ('Legs', 'FeetWearables')}
_cache = {}

def _find(name):
    r = next((x for x in uasset.rows() if x[1].endswith('/' + name + '.uasset')), None)
    return r[1][:-7].split('Content/')[-1] if r else None

def _soft_list(p, raw):
    n, = struct.unpack_from('<i', raw, 0)
    return [p.names[struct.unpack_from('<i', raw, 4 + 12*i)[0]].split('.')[-1][:-2] for i in range(n)]

def item_list(bp, key):
    """A wearable definition's sub-slot list, from itself or the nearest parent that has it."""
    if (bp, key) in _cache: return _cache[(bp, key)]
    out, cur = [], bp
    while cur:
        path = _find(cur)
        if not path: break
        p = uasset.Package(path)
        props, _ = p.props(next(e for e in p.exports if e['name'] == 'Default__' + cur + '_C')['offset'])
        if key in props:
            out = _soft_list(p, props[key][1]); break
        cls = next((e for e in p.exports if e['name'] == cur + '_C'), None)
        parent = p.obj(cls['super']) if cls else None
        cur = parent[:-2] if parent and parent.endswith('_C') else None
    _cache[(bp, key)] = out
    return out

def top_lists():
    if 'top' not in _cache:
        s = uasset.Package('Mordhau/Blueprints/BP_MordhauSingleton')
        props, _ = s.props(next(e for e in s.exports if e['name'].startswith('Default__'))['offset'])
        _cache['top'] = {slot: _soft_list(s, props[slot + 'Wearables'][1]) for slot in ('Head', 'UpperChest', 'Legs')}
        _cache['equipment'] = _soft_list(s, props['Equipment'][1])
    return _cache['top']

def resolve(ids, order):
    """ids: nine item numbers in profile order; order: slot name per position. -> {slot: definition} or None if invalid."""
    by_slot = dict(zip(order, ids)); out = {}
    for slot in ('Head', 'UpperChest', 'Legs', 'Coif', 'LowerChest', 'Arms', 'Shoulders', 'Hands', 'Feet'):
        lst = top_lists()[slot] if slot not in PARENT else item_list(out[PARENT[slot][0]], PARENT[slot][1])
        i = by_slot[slot]
        if i >= len(lst): return None
        out[slot] = lst[i]
    return out

def read_profiles():
    """[(name, [(id, colors, pattern)] * 9, raw line)] from the player's Game.ini."""
    out = []
    for line in open(GAME_INI, encoding='utf-8', errors='replace'):
        if not line.startswith('CharacterProfiles='): continue
        name = re.search(r'Name=INVTEXT\("([^"]*)"\)', line)
        w = re.search(r'Wearables=\((.*?)\),Equipment=', line)
        entries = []
        for m in re.finditer(r'\((?:ID=(\d+))?(?:,?Colors=\(([\d,]*)\))?(?:,?Pattern=(\d+))?\)', w.group(1) if w else ''):
            entries.append((int(m.group(1) or 0), [int(c) for c in m.group(2).split(',')] if m.group(2) else [], int(m.group(3) or 0)))
        out.append((name.group(1) if name else '?', entries, line.strip()))
    return out

if __name__ == '__main__':
    profiles = read_profiles()
    print(len(profiles), 'profiles in Game.ini;', sum(1 for p in profiles if len(p[1]) == 9), 'with nine wearable entries')
    usable = [p for p in profiles if len(p[1]) == 9]
    valid = []
    for order in itertools.permutations(SLOTS):
        if all(resolve([e[0] for e in p[1]], order) for p in usable): valid.append(order)
        if len(valid) > 5: break
    print('slot orders valid for every profile:', len(valid))
    for o in valid[:5]: print('  ', o)
