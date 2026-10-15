"""Dev: bake with the plugin's own baker (the test program) and compare every file with what the Python tools wrote.

  compare_bake.py                 everything: props, enemy weapons, all outfits, the player's mercenary
  compare_bake.py props|weapons|looks|player
  compare_bake.py looks 1 17 33   only these outfit numbers
"""
import filecmp, json, os, subprocess, sys

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.join(here, '..')
exe = os.path.join(root, 'build', 'RelWithDebInfo', 'mordhau_bake_test.exe')
paks = r'E:\steam object\steamapps\common\Mordhau\Mordhau\Content\Paks'
game_ini = os.path.expandvars(r'%LOCALAPPDATA%\Mordhau\Saved\Config\WindowsClient\Game.ini')
what = sys.argv[1] if len(sys.argv) > 1 else 'all'
only = set(sys.argv[2:])
totals = [0, 0]


def check(label, ref, args):
    out = os.path.join(root, 'cache', 'bake_test', label)
    res = subprocess.run([exe, paks] + [args[0], out] + args[1:], capture_output=True, text=True)
    if not os.path.isdir(ref):
        print(f'{label}: no Python bake to compare with ({res.stdout.strip()[:120]})')
        return
    names = sorted(f for f in os.listdir(ref) if f.endswith(('.dmchar', '.dds')))
    same = [n for n in names if os.path.exists(os.path.join(out, n)) and filecmp.cmp(os.path.join(ref, n), os.path.join(out, n), shallow=False)]
    extra = sorted(set(os.listdir(out)) - set(names)) if os.path.isdir(out) else []
    totals[0] += len(same); totals[1] += len(names)
    if len(same) == len(names) and not extra and res.returncode == 0:
        print(f'{label}: {len(same)}/{len(names)} identical')
        return
    print(f'{label}: {len(same)}/{len(names)} identical, differing or missing {sorted(set(names) - set(same))}' + (f', extra {extra}' if extra else ''))
    print('    ' + res.stdout.strip().replace('\n', ' | ')[:500])


def dash(v):
    return str(v) if v not in ('', None) else '-'


props = json.load(open(os.path.join(root, 'sheets', 'props.json'), encoding='utf-8'))['rows']
if what in ('all', 'props'):
    for r in props:
        if r.get('kind') != 'weapon':
            check('props/' + r['id'], os.path.join(root, 'cache', 'props', r['id']), ['prop', r['model'], r['colour_map'], dash(r.get('normal_map')), dash(r.get('surface_map'))])
if what in ('all', 'weapons'):
    for r in props:
        if r.get('kind') == 'weapon':
            check('props/' + r['id'], os.path.join(root, 'cache', 'props', r['id']), ['weapon', r['weapon_bp']])
if what in ('all', 'looks'):
    for n, r in enumerate(json.load(open(os.path.join(root, 'sheets', 'looks.json'), encoding='utf-8'))['rows'], 1):
        if only and str(n) not in only:
            continue
        ref = os.path.join(root, 'cache', 'enemies', str(n))
        if r['class'].startswith('creature:'):
            check(f'enemies/{n} {r["id"]}', ref, ['prop', r['model'], r['colour_map'], dash(r.get('normal_map'))])
        else:
            check(f'enemies/{n} {r["id"]}', ref, ['look', r['id']] + [dash(r[k]) for k in ('head', 'coif', 'upper_chest', 'lower_chest', 'shoulders', 'arms', 'hands', 'legs', 'feet')]
                  + [str(r['colour_1']), str(r['colour_2'])])
if what in ('all', 'player'):
    player = json.load(open(os.path.join(root, 'sheets', 'standin_looks.json'), encoding='utf-8'))['player']
    check('character', os.path.join(root, 'cache', 'character'), ['profile', game_ini, player])
print(f'TOTAL: {totals[0]}/{totals[1]} files identical')
