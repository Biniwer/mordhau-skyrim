"""Dev tool: bake the outfits Skyrim's humanoids are dressed in (sheets/looks.json) into cache/enemies/<n>/.

Each outfit is put together from MORDHAU's own wearables, read from the player's install, with the same pipeline as the
player's character (models, colour / pattern / normal / surface maps, dyes); nothing is authored. Textures are capped
at 1024 for the crowd. Also writes cache/enemies/looks.txt ("<n> <class> <id>" per line, read by the plugin) and
cache/live_config.lua (which mercenary is the player; read by the MORDHAU-side script).

  bake_looks.py            bake every row
  bake_looks.py <id> ...   only these rows (numbers stay as in the sheet)
"""
import json, os, shutil, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import profile as pf, character_spec as cs, bake_character as bc

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.join(here, '..', 'cache')
sheet = json.load(open(os.path.join(here, '..', 'sheets', 'looks.json'), encoding='utf-8'))
player = json.load(open(os.path.join(here, '..', 'sheets', 'standin_looks.json'), encoding='utf-8'))['player']
only = set(sys.argv[1:])
bc.CAP = 1024


def pick(options, wanted, what, row_id):
    """The wearable definition named `wanted` from this slot's own list; the list's first entry when "" or not there."""
    if not options:
        return None
    if wanted:
        bp = 'BP_' + wanted
        if bp in options:
            return bp
        print(f'  {row_id}: {what} "{wanted}" is not offered here; using {options[0]}')
    return options[0]


top = pf.top_lists()
lines = []
for n, row in enumerate(sheet['rows'], 1):
    lines.append(f'{n} {row["class"].replace(" ", "_")} {row["id"]}')
    if only and row['id'] not in only:
        continue
    if row['class'].startswith('creature:'):
        # one of MORDHAU's Demon Horde creatures: its own single model, no wearables
        out = os.path.join(root, 'enemies', str(n))
        shutil.rmtree(out, ignore_errors=True)
        print(f'--- look {n}: {row["id"]} ({row["class"]}): {row["model"].split("/")[-1]}')
        s = {'name': row['id'], 'no_body': True, 'pieces': [{'model': row['model'], 'colour_map': row['colour_map'], 'pattern_mask': None,
                                                              'dye': [], 'hide': [], 'normal_map': row['normal_map']}]}
        try:
            bc.bake_char(out, [], s)
            json.dump(s, open(os.path.join(out, 'spec.json'), 'w', encoding='utf-8'), indent=1)
        except Exception as ex:
            shutil.rmtree(out, ignore_errors=True)
            print(f'    FAILED: {type(ex).__name__}: {ex}')
        continue
    colours = [int(row['colour_1']), int(row['colour_2'])]
    head = pick(top['Head'], row['head'], 'head', row['id'])
    upper = pick(top['UpperChest'], row['upper_chest'], 'upper chest', row['id'])
    legs = pick(top['Legs'], row['legs'], 'legs', row['id'])
    arms = pick(pf.item_list(upper, 'ArmsWearables'), row['arms'], 'arms', row['id'])
    items = {
        'Head': head, 'UpperChest': upper, 'Legs': legs, 'Arms': arms,
        'Coif': pick(pf.item_list(head, 'CoifWearables'), row['coif'], 'coif', row['id']),
        'LowerChest': pick(pf.item_list(upper, 'LowerChestWearables'), row['lower_chest'], 'lower chest', row['id']),
        'Shoulders': pick(pf.item_list(upper, 'ShouldersWearables'), row['shoulders'], 'shoulders', row['id']),
        'Hands': pick(pf.item_list(arms, 'HandsWearables') if arms else [], row['hands'], 'hands', row['id']),
        'Feet': pick(pf.item_list(legs, 'FeetWearables'), row['feet'], 'feet', row['id']),
    }
    missing = [k for k, v in items.items() if not v]
    if missing:
        print(f'--- look {n} {row["id"]}: NOT baked, no wearable for {missing}')
        continue
    out = os.path.join(root, 'enemies', str(n))
    shutil.rmtree(out, ignore_errors=True)
    print(f'--- look {n}: {row["id"]} ({row["class"]}): ' + ', '.join(v[3:] for v in items.values()))
    try:
        s = cs.spec(row['id'], {slot: (bp, colours, 0) for slot, bp in items.items()})
        bc.bake_char(out, [], s)
        json.dump(s, open(os.path.join(out, 'spec.json'), 'w', encoding='utf-8'), indent=1)
    except Exception as ex:
        shutil.rmtree(out, ignore_errors=True)
        print(f'    FAILED: {type(ex).__name__}: {ex}')

os.makedirs(os.path.join(root, 'enemies'), exist_ok=True)
open(os.path.join(root, 'enemies', 'looks.txt'), 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')
names = [p[0].lower() for p in pf.read_profiles()]
cfg = ['-- written by tools/bake_looks.py: which of the player\'s own mercenaries MORDHAU spawns; enemies = one entry per',
       '-- baked outfit (the MORDHAU-side script only counts them)', 'return {',
       f'    profile_index = {names.index(player.lower())}, profile_name = "{player}",', '    enemies = {']
cfg += [f'        {{ name = "{row["id"]}", weapon = "" }},' for row in sheet['rows']]
cfg += ['    },', '}', '']
open(os.path.join(root, 'live_config.lua'), 'w', encoding='utf-8', newline='\n').write('\n'.join(cfg))
print('wrote looks.txt and live_config.lua')
