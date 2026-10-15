"""Preflight: lay every sheet's rows and columns over each other.

Lists every unfilled cell (null / empty string), every unverified row, and every reference between
sheets that does not resolve. Exit code 0 only when nothing is unfilled or unresolved.
Usage: python tools/preflight.py [sheets_dir] [--strict]   (--strict also fails on unverified rows)
"""
import json, os, sys

args = [a for a in sys.argv[1:] if not a.startswith('--')]
root = args[0] if args else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'sheets')
strict = '--strict' in sys.argv

sheets = {}
for fn in sorted(os.listdir(root)):
    if fn.endswith('.json'):
        with open(os.path.join(root, fn), encoding='utf-8') as f:
            sheets[fn[:-5]] = json.load(f)['rows']

ids = {name: {r['id'] for r in rows} for name, rows in sheets.items()}

# column -> sheet it must resolve in (value may be a single id or a list of ids)
REFS = {
    ('swings', 'chambers_with'): 'swings',
    ('moves', 'hook'): 'hooks',
    ('moves', 'ui'): 'ui',
    ('ui', 'hook'): 'hooks',
    ('movement', 'hook'): 'hooks',
    ('enemy_ai', 'hook'): 'hooks',
    ('enemy_ai', 'uses'): 'moves',
    ('attack_stats', 'used_by'): 'moves',
    ('twist_bones', 'owner'): 'bone_map',
    ('twist_bones', 'toward'): 'bone_map',
    ('twist_bones', 'relative_to'): 'bone_map',
}
# cells allowed to be null by design: (sheet, column) -> predicate(row)
OPTIONAL = {
    ('swings', 'angle_deg'): lambda r: r['kind'] == 'stab',
    ('swings', 'mordhau_clip_1p'): lambda r: r['kind'] == 'stab',
    ('moves', 'mordhau_clip'): lambda r: True,
    ('moves', 'ui'): lambda r: True,
    ('ui', 'anchor'): lambda r: r['id'] == 'vanilla_hud',
    ('ui', 'size_px'): lambda r: r['id'] == 'vanilla_hud',
    ('movement', 'mordhau_property'): lambda r: not r['read_ok'],
    ('clips', 'windup_end'): lambda r: r['move'] == 'Parry',
    ('clips', 'riposte_3p'): lambda r: r['move'] == 'Parry',
    ('clips', 'riposte_windup_end'): lambda r: r['move'] == 'Parry',
    ('clips', 'riposte_release_end'): lambda r: r['move'] == 'Parry',
    ('clips', 'release_end'): lambda r: r['move'] == 'Parry',
    ('parry_stats', 'mordhau_property'): lambda r: not r['read_ok'],
    ('bone_map', 'dir_skyrim_node'): lambda r: r['id'] in ('head', 'weapon'),
    ('bone_map', 'dir_mordhau_bone'): lambda r: r['id'] in ('head', 'weapon'),
}

unfilled, unresolved, unverified, dupes = [], [], [], []
for name, rows in sheets.items():
    seen = set()
    cols = {c for r in rows for c in r}
    for r in rows:
        if r['id'] in seen:
            dupes.append(f'{name}.{r["id"]}')
        seen.add(r['id'])
        for c in sorted(cols):
            v = r.get(c, None) if c in r else '<missing column>'
            if v is None or v == '' or v == '<missing column>':
                if v != '<missing column>' and OPTIONAL.get((name, c), lambda r: False)(r):
                    continue
                unfilled.append(f'{name}.{r["id"]}.{c}')
            target = REFS.get((name, c))
            if target and v not in (None, '', '<missing column>'):
                for one in (v if isinstance(v, list) else [v]):
                    if (name == 'enemy_ai' and c == 'uses' and one == 'swings') or (name == 'attack_stats' and one == 'attack'):
                        continue
                    if one not in ids[target]:
                        unresolved.append(f'{name}.{r["id"]}.{c} -> {target}.{one}')
        if r.get('verified') is not True:
            unverified.append(f'{name}.{r["id"]}')

# clips.json is keyed by clip set + move: every weapon family's set and every swing's move needs its row
clip_ids = ids.get('clips', set())
for fam in sheets.get('weapon_families', []):
    for sw in sheets.get('swings', []):
        key = f"{fam.get('clip_set')}.{sw.get('move')}"
        if key not in clip_ids:
            unresolved.append(f"weapon_families.{fam['id']} x swings.{sw['id']} -> clips.{key}")

total = sum(len(r) for r in sheets.values())
print(f'sheets: {len(sheets)}  rows: {total}')
for title, items in (('UNFILLED CELLS', unfilled), ('UNRESOLVED REFERENCES', unresolved),
                     ('DUPLICATE IDS', dupes), ('UNVERIFIED ROWS', unverified)):
    print(f'\n{title}: {len(items)}')
    for i in items:
        print('  ' + i)
bad = unfilled or unresolved or dupes or (strict and unverified)
print('\nPREFLIGHT ' + ('FAILED' if bad else 'CLEAN'))
sys.exit(1 if bad else 0)
