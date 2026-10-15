"""Makes release/README.txt - the readme document on the mod's Nexus page - from the package's own readme
(installer/modmanager/README.md) as plain text, with the warning lines on top. usage: make_readme_txt.py"""
import os, re

root = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
NL = chr(10)
src = open(os.path.join(root, 'installer', 'modmanager', 'README.md'), encoding='utf-8').read().split(NL)
out = ['NEVER USE THIS IN ONLINE MORDHAU PLAY.',
       'Before you play MORDHAU online, make sure there is no file named dwmapi.dll (or dwmapi.dll.old)',
       "in MORDHAU's Mordhau\\Binaries\\Win64 folder. If there is one, delete it first.", '']
for line in src:
    plain = re.sub(r'\*\*(.+?)\*\*', r'\1', line)
    plain = re.sub(r'(?<![A-Za-z0-9])\*(.+?)\*(?![A-Za-z0-9])', r'\1', plain)
    plain = plain.replace('`', '').replace('**', '')   # (also bold that runs over two lines)
    if plain.startswith('# '):
        title = plain[2:].upper().replace(' X ', ' X ')
        out += [title, '=' * len(title)]
    elif plain.startswith('## '):
        title = plain[3:].upper()
        out += [title, '-' * len(title)]
    else:
        out.append(plain)
open(os.path.join(root, 'release', 'README.txt'), 'w', encoding='utf-8', newline='\r\n').write(NL.join(out).rstrip() + NL)
print('release/README.txt', len(out), 'lines')
