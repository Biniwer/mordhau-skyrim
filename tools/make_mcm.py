"""Makes the settings menu's MCM Helper files from sheets/settings_menu.json: MCM/Config/MordhauWeapons/config.json (the
pages) and settings.ini beside it (the defaults). MCM Helper finds them by the plugin's name (MordhauWeapons.esp, whose
quest MordhauSkyrimMCMQuest runs papyrus/MordhauSkyrimMCM.psc) and keeps the player's choices in
MCM/Settings/MordhauWeapons.ini. Each setting is "<prefix><Name>:<Section>" (f float, i int, b on/off), the section
being the page's name.
usage: make_mcm.py [<out folder>]   (default build/mcm; the files land in <out>/MCM/Config/MordhauWeapons)"""
import json, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'build', 'mcm'), 'MCM', 'Config', 'MordhauWeapons')
sheet = json.load(open(os.path.join(ROOT, 'sheets', 'settings_menu.json'), encoding='utf-8'))

def key(row, section):
    name = ''.join(w.capitalize() for w in row['id'].split('_'))
    if row['type'] == 'toggle':
        return f'b{name}:{section}', 'ModSettingBool'
    if row['type'] == 'choice':
        return f'i{name}:{section}', 'ModSettingInt'
    whole = all(float(row[k]) == int(row[k]) for k in ('min', 'max', 'step', 'default'))
    return (f'i{name}:{section}', 'ModSettingInt') if whole else (f'f{name}:{section}', 'ModSettingFloat')

def unknown(row):
    return any(isinstance(row.get(k), str) for k in ('min', 'max', 'choices', 'default'))

pages, ini = [], []
for page in sheet['pages']:
    section = ''.join(w.capitalize() for w in page['page'].split() if w.isalnum())
    content, values = [{'text': page['page'], 'type': 'header'}], []
    for row in page['rows']:
        if unknown(row):        # (how it can be done is not known yet: shown, not settable)
            content.append({'text': row['label'], 'type': 'text', 'help': row['help'], 'valueOptions': {'value': 'not yet'}})
            continue
        k, source = key(row, section)
        item = {'id': k, 'text': row['label'], 'help': row['help'], 'valueOptions': {'sourceType': source}}
        if row['type'] == 'toggle':
            item['type'] = 'toggle'
        elif row['type'] == 'choice':
            item['type'] = 'enum'
            item['valueOptions']['options'] = row['choices']
        else:
            item['type'] = 'slider'
            item['valueOptions'].update({'min': row['min'], 'max': row['max'], 'step': row['step']})
            if source == 'ModSettingFloat':
                item['valueOptions']['formatString'] = '{2}'
        content.append(item)
        values.append(f'{k.split(":")[0]} = {row["default"]}')
    pages.append({'pageDisplayName': page['page'], 'cursorFillMode': 'topToBottom', 'content': content})
    if values:
        ini += [f'[{section}]'] + values + ['']

os.makedirs(OUT, exist_ok=True)
config = {'modName': 'MordhauWeapons', 'displayName': 'MORDHAU x Skyrim', 'minMcmVersion': 9, 'pages': pages}
open(os.path.join(OUT, 'config.json'), 'w', encoding='utf-8', newline='\n').write(json.dumps(config, indent=2) + '\n')
open(os.path.join(OUT, 'settings.ini'), 'w', encoding='utf-8', newline='\r\n').write('\n'.join(ini))
print(f'{OUT}: {len(pages)} pages, {sum(1 for l in ini if "=" in l)} settings')
