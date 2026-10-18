"""Turns release/description.md into Nexus's formatting (release/description.bbcode.txt), for pasting into the mod
page's description editor in its code view. usage: make_bbcode.py"""
import re

raw = open('release/description.md', encoding='utf-8').read().splitlines()
# (the Markdown is wrapped at 120 columns: a list item's or a paragraph's next lines are joined to it first - each of
# them had become a list or a line of its own, 2026-10-18)
src = []
for line in raw:
    starts_block = not line.strip() or line.startswith(('#', '- ', '|')) or re.match(r'\d+\. ', line)
    if src and src[-1].strip() and not starts_block and not src[-1].startswith(('#', '|')):
        src[-1] = src[-1].rstrip() + ' ' + line.strip()
    else:
        src.append(line)
out, mode = [], None   # mode: None | 'ul' | 'ol' | 'table'

def close():
    global mode
    if mode in ('ul', 'ol', 'table'):
        out.append('[/list]')
    mode = None

def inline(t):
    t = re.sub(r'\*\*(.+?)\*\*', r'[b]\1[/b]', t)
    t = re.sub(r'`(.+?)`', r'[font=Courier New]\1[/font]', t)
    return t

for line in src:
    if line.startswith('# '):
        continue                                  # (the page has its own title)
    if line.startswith('## '):
        close(); out.append(''); out.append('[size=5][b]' + line[3:] + '[/b][/size]'); continue
    if line.startswith('- '):
        if mode != 'ul':
            close(); out.append('[list]'); mode = 'ul'
        out.append('[*]' + inline(line[2:])); continue
    m = re.match(r'\d+\. (.*)', line)
    if m:
        if mode != 'ol':
            close(); out.append('[list=1]'); mode = 'ol'
        out.append('[*]' + inline(m.group(1))); continue
    if line.startswith('|'):
        cells = [c.strip() for c in line.strip('|').split('|')]
        if set(cells[0]) <= set('-: ') or cells[0] in ('Input', 'Skyrim weapon'):
            continue
        if mode != 'table':
            close(); out.append('[list]'); mode = 'table'
        out.append('[*][b]' + cells[0] + '[/b]: ' + inline(cells[1])); continue
    if not line.strip():
        close(); out.append(''); continue
    close(); out.append(inline(line))
close()
text = re.sub(r'\n{3,}', '\n\n', '\n'.join(out)).strip()
text = text.replace('Address Library for SKSE Plugins (Nexus Mods, Skyrim Special Edition mod 32444)',
                    '[url=https://www.nexusmods.com/skyrimspecialedition/mods/32444]Address Library for SKSE Plugins[/url]')
# the warning at the very top of the page: large, bold, red (the user, 2026-10-11)
WARNING = ("[size=5][color=#ff2a2a][b]NEVER USE THIS IN ONLINE MORDHAU PLAY.[/b][/color][/size]\n"
           "[size=4][color=#ff2a2a][b]Before you play MORDHAU online, make sure no trace of this mod is active: there must be no file named "
           "dwmapi.dll (or dwmapi.dll.old) in MORDHAU's Mordhau\\Binaries\\Win64 folder. If there is one, delete it first.[/b][/color][/size]\n\n")
text = WARNING + text
# the Honest Mods strip at the very bottom (the user put it on the page, 2026-10-18): kept as it is, in its own file,
# so that replacing the description does not take it off the page (release/slop_meter.bbcode.txt, not in the source zip)
STRIP = 'release/slop_meter.bbcode.txt'
try:
    text += '\n\n' + open(STRIP, encoding='utf-8').read().strip()
except FileNotFoundError:
    print('NOTE: no', STRIP, '- the description has no Honest Mods strip')
open('release/description.bbcode.txt', 'w', encoding='utf-8', newline='\r\n').write(text)
print(len(text), 'characters')
