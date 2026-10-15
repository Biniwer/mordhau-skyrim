"""Takes the ENB setup out of Skyrim again (ENBSeries, Silent Horizons shader core, Dawnfire preset and addon, NAT.ENB
weather, KiLoader, ENB Extender, KreatE): every file listed in downloads/enb_installed_files.txt and
downloads/shader_installed_files.txt is MOVED to downloads/enb_removed/ (same layout; nothing is deleted), and the two
plugins leave Plugins.txt. Skyrim must not be running. usage: remove_enb.py [check]   (check = only report)"""
import os, shutil, struct, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKYRIM = r'E:\steam object\steamapps\common\Skyrim Special Edition'
KEEP = os.path.join(ROOT, 'downloads', 'enb_removed')
PLUGINS = ('nat-enb.esp', 'dawnfire enb nat3 addon.esp')
check = len(sys.argv) > 1 and sys.argv[1] == 'check'

def masters(path):
    d = open(path, 'rb').read(8192)
    size = struct.unpack_from('<I', d, 4)[0]
    body, q, out = d[24:24 + size], 0, []
    while q + 6 <= len(body):
        t, n = struct.unpack_from('<4sH', body, q)
        if t == b'MAST':
            out.append(body[q + 6:q + 6 + n].rstrip(b'\0').decode('latin1'))
        q += 6 + n
    return out

# does anything that stays need one of the plugins that go?
needing = []
for f in os.listdir(os.path.join(SKYRIM, 'Data')):
    if f.lower().endswith(('.esp', '.esm', '.esl')) and f.lower() not in PLUGINS:
        try:
            m = [x for x in masters(os.path.join(SKYRIM, 'Data', f)) if x.lower() in PLUGINS]
        except Exception:
            m = []
        if m:
            needing.append((f, m))
print('plugins that need the ENB\'s plugins as masters:', needing if needing else 'none')

files = []
for lst in ('enb_installed_files.txt', 'shader_installed_files.txt'):
    files += [l.strip() for l in open(os.path.join(ROOT, 'downloads', lst), encoding='utf-8') if l.strip()]
files = sorted(set(files))
present = [f for f in files if os.path.isfile(os.path.join(SKYRIM, f))]
print(f'{len(present)} of {len(files)} listed files are in Skyrim\'s folder')
if check:
    sys.exit(0)
moved = 0
for f in present:
    src, dst = os.path.join(SKYRIM, f), os.path.join(KEEP, f)
    if not os.path.isfile(src):
        continue   # (listed twice in different letter case: already moved)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    if os.path.exists(dst):
        os.remove(dst)
    shutil.move(src, dst)
    moved += 1
# folders the setup made that are empty now
for folder in ('enbseries', os.path.join('Data', 'KiLoader'), os.path.join('Data', 'KreatE'), 'enbcache'):
    p = os.path.join(SKYRIM, folder)
    if os.path.isdir(p):
        for base, dirs, names in os.walk(p, topdown=False):
            if not os.listdir(base):
                os.rmdir(base)
        if os.path.isdir(p):
            print('left in place (holds files the setup did not install):', folder, os.listdir(p)[:6])
path = os.path.join(os.environ['LOCALAPPDATA'], 'Skyrim Special Edition', 'Plugins.txt')
lines = open(path, encoding='utf-8', errors='replace').read().splitlines()
kept = [l for l in lines if l.strip().lstrip('*').lower() not in PLUGINS]
open(path, 'w', encoding='utf-8', newline='\r\n').write('\n'.join(kept) + '\n')
print(f'moved {moved} files to {KEEP}; Plugins.txt: {len(lines) - len(kept)} line(s) removed')
