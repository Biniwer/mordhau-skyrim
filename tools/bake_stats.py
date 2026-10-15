"""Dev tool: read each weapon family's attack numbers from the player's MORDHAU (local cache, never shipped).

Follows the weapon definition's parent chain so values a weapon leaves unset come from its parent.
Output lines: <family id>.<strike|stab>.<MORDHAU property>=<value>, plus <family id>.BlockStaminaNegation.
Usage: bake_stats.py <out file>
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import uasset

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
fams = json.load(open(os.path.join(ROOT, 'sheets', 'weapon_families.json'), encoding='utf-8'))['rows']
stats = json.load(open(os.path.join(ROOT, 'sheets', 'attack_stats.json'), encoding='utf-8'))['rows']
props = [s['mordhau_property'] for s in stats if s['read_ok']]

def chain(bp_name):
    """Defaults of a weapon definition and its parents, parent first."""
    out = []
    while True:
        row = next((x for x in uasset.rows() if x[1].endswith('/' + bp_name + '.uasset')), None)
        if not row:
            break
        p = uasset.Package(row[1][:-7].split('Content/')[-1])
        cdo = next(x['name'] for x in p.exports if x['name'].startswith('Default__'))
        out.append((bp_name, p.defaults(cdo)))
        cls = next((x for x in p.exports if x['name'] == bp_name + '_C'), None)
        parent = p.obj(cls['super']) if cls else None
        if not parent or not parent.endswith('_C'):
            break
        bp_name = parent[:-2]
    return list(reversed(out))

lines = []
for f in fams:
    c = chain(f['mordhau_weapon'].split('/')[-1])
    merged = {'StrikeAttack': {}, 'StabAttack': {}}
    weapon_level = {}
    for name, d in c:
        for blk in merged:
            if isinstance(d.get(blk), dict):
                merged[blk].update({k: v for k, v in d[blk].items() if isinstance(v, (int, float)) and not isinstance(v, bool)})
        weapon_level.update({k: v for k, v in d.items() if isinstance(v, (int, float)) and not isinstance(v, bool)})
    for blk, key in (('StrikeAttack', 'strike'), ('StabAttack', 'stab')):
        for pr in props:
            if pr in merged[blk]:
                lines.append(f"{f['id']}.{key}.{pr}={merged[blk][pr]}")
    if 'BlockStaminaNegation' in weapon_level:
        lines.append(f"{f['id']}.BlockStaminaNegation={weapon_level['BlockStaminaNegation']}")
    print(f"{f['id']}: chain {' > '.join(n for n, _ in c)}", file=sys.stderr)
parry = json.load(open(os.path.join(ROOT, 'sheets', 'parry_stats.json'), encoding='utf-8'))['rows']
row = next(x for x in uasset.rows() if x[1].endswith('/Blueprints/Motions/BP_ParryMotion.uasset'))
pk = uasset.Package(row[1][:-7].split('Content/')[-1])
pd = pk.defaults(next(x['name'] for x in pk.exports if x['name'].startswith('Default__')))
for r in parry:
    if r['mordhau_property'] and isinstance(pd.get(r['mordhau_property']), (int, float)):
        lines.append(f"parry.{r['mordhau_property']}={pd[r['mordhau_property']]}")
move = json.load(open(os.path.join(ROOT, 'sheets', 'movement.json'), encoding='utf-8'))['rows']
row = next(x for x in uasset.rows() if x[1].endswith('/Blueprints/Characters/BP_MordhauCharacter.uasset'))
ck = uasset.Package(row[1][:-7].split('Content/')[-1])
sources = {'movement': ck.defaults('CharMoveComp'), 'character': ck.defaults('Default__BP_MordhauCharacter_C')}
for r in move:
    v = sources[r['source']].get(r['mordhau_property']) if r['mordhau_property'] else None
    if isinstance(v, (int, float)) and not isinstance(v, bool):
        lines.append(f"move.{r['mordhau_property']}={v}")
open(sys.argv[1], 'w', encoding='ascii', newline='\n').write('\n'.join(lines) + '\n')
print(f'{len(lines)} values', file=sys.stderr)
