"""Writes the section "Which MORDHAU weapon each Skyrim weapon is" of release/description.md from the sheets
(weapon_map.json, mordhau_items.json), so the page cannot say something else than the mod does.
2026-10-12: the table had been typed by hand for 0.1.7 and was not changed with the Scimitar, the Poleaxe, the Falx
and the sword rows - the author found the page out of date. usage: make_weapon_table.py"""
import json, os

root = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
NL = chr(10)
rows = json.load(open(os.path.join(root, 'sheets', 'weapon_map.json'), encoding='utf-8'))['rows']
items = json.load(open(os.path.join(root, 'sheets', 'mordhau_items.json'), encoding='utf-8'))['rows']
# MORDHAU's weapon definitions -> the names players know (the items' own names where there is one)
names = {r['mordhau_weapon'].split('BP_')[-1]: r['name'] for r in items}
names.update({'Greatsword': 'Greatsword', 'Scimitar': 'Scimitar', 'Poleaxe': 'Poleaxe', 'Knife': 'Knife'})
families = [('one_hand_sword', 'One-handed swords'), ('one_hand_dagger', 'Daggers'), ('one_hand_axe', 'War axes'), ('one_hand_mace', 'Maces'), ('two_hand_sword', 'Greatswords'),
            ('two_hand_axe', 'Battleaxes'), ('two_hand_hammer', 'Warhammers')]
words = {'Scimitar': 'scimitars (of any material)', 'Hatchet': 'hatchets', 'Woodcutter': "woodcutter's axes", 'Blades': 'Blades swords', 'Katana': 'katanas'}
out = ['## Which MORDHAU weapon each Skyrim weapon is', '',
       'A Skyrim weapon keeps its own look and fights as a MORDHAU weapon, chosen by its kind and by words in its name. The first line that fits wins; "Other" is every weapon of that kind not named before it.', '']
for family, title in families:
    mine = [r for r in rows if r['family'] == family]
    assert mine, family
    out += [f'**{title}**', '', '| Skyrim weapon | Fights as |', '| --- | --- |']
    for r in mine:
        leaf = r['mordhau_weapon'].split('BP_')[-1]
        assert leaf in names, leaf
        what = ', '.join(words.get(w, w) for w in r['name_contains'].split('|')) if r['name_contains'] else 'Other'
        out.append(f'| {what[0].upper() + what[1:]} | {names[leaf]} |')
    out.append('')
out += ['**MORDHAU weapons as Skyrim items.** These are added to Skyrim as their own items with MORDHAU\'s models, and turn up in loot, in shops and on enemies: ' +
        ', '.join(r['name'] for r in items) + '.', '']
p = os.path.join(root, 'release', 'description.md')
s = open(p, encoding='utf-8').read()
i = s.index('## Which MORDHAU weapon each Skyrim weapon is')
j = s.index("## What it does to MORDHAU's folder")
open(p, 'w', encoding='utf-8', newline=NL).write(s[:i] + NL.join(out) + NL + s[j:])
print(NL.join(out))
