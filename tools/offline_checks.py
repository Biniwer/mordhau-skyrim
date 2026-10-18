"""Checks that need neither game: run before every build and at the start of tools/safety_tests.sh (a few seconds).
The sheets parse and their ids are unique; the generated Sheets.h matches the sheets; every sheet value the plugin asks
for by name exists (BuildValue gives 0 for an unknown name, without a word - a typo would go unnoticed); both sides of the
link agree on its sizes; the shipped code makes no network calls; the download holds no scripts and no private words;
no shipped text still names a removed part. usage: offline_checks.py     -> one line per check; exit 1 if any failed"""
import json, os, re, subprocess, sys

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
failed = []


def check(name, problems):
    print(('ok    ' if not problems else 'FAIL  ') + name + ('' if not problems else ': ' + '; '.join(problems[:12]) + (' ...' if len(problems) > 12 else '')))
    if problems:
        failed.append(name)


def read(rel):
    return open(os.path.join(root, rel), encoding='utf-8', errors='replace').read()


def files(base, exts, skip=('extern',)):
    for d, dirs, fs in os.walk(os.path.join(root, base)):
        dirs[:] = [x for x in dirs if x not in skip]
        for f in fs:
            if f.endswith(exts):
                yield os.path.join(d, f)


STRINGS = r'"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\''


def no_comments(text, lua=False):
    # (strings are matched first and kept, so a "//" or "--" inside one is not taken for a comment)
    comment = r'--\[\[.*?\]\]|--[^\n]*' if lua else r'/\*.*?\*/|//[^\n]*'
    return re.sub(STRINGS + '|' + comment, lambda m: m.group(0) if m.group(0)[0] in '"\'' else '', text, flags=re.S)


# --- the sheets parse, and no sheet has the same id twice
sheets, problems = {}, []
for f in sorted(os.listdir(os.path.join(root, 'sheets'))):
    if f.endswith('.json'):
        try:
            sheets[f[:-5]] = json.loads(read('sheets/' + f))
        except ValueError as e:
            problems.append(f'{f}: {e}')
            continue
        rows = sheets[f[:-5]].get('rows') if isinstance(sheets[f[:-5]], dict) else None
        ids = [r['id'] for r in rows or [] if isinstance(r, dict) and 'id' in r]
        problems += [f'{f}: id "{i}" twice' for i in sorted({i for i in ids if ids.count(i) > 1})]
check('sheets parse, ids unique', problems)

# --- the generated header is the sheets' current state
gen = os.path.join(root, 'plugin', 'src', 'gen', 'Sheets.h')
before = open(gen, encoding='utf-8').read() if os.path.exists(gen) else ''
subprocess.run([sys.executable, '-I', os.path.join(root, 'tools', 'gen_code.py')], capture_output=True)
after = open(gen, encoding='utf-8').read() if os.path.exists(gen) else ''
check('Sheets.h matches the sheets', [] if before == after else ['it was out of date (now regenerated: build again)'])

# --- every value the plugin asks for by name is in its sheet
cpp = ''.join(no_comments(open(p, encoding='utf-8', errors='replace').read()) for p in files('plugin/src', ('.cpp', '.h')) if os.sep + 'gen' + os.sep not in p)
ids_of = lambda sheet: {r['id'] for r in sheets.get(sheet, {}).get('rows', []) if isinstance(r, dict) and 'id' in r}
menu = set(re.findall(r'"id"\s*:\s*"([A-Za-z0-9_]+)"', read('sheets/settings_menu.json')))
problems = []
for fn, known, where in (('BuildValue', ids_of('builds'), 'builds.json'), ('CameraValue', ids_of('camera'), 'camera.json'),
                         ('Settings::Value', menu | ids_of('builds'), 'settings_menu.json')):
    for name in sorted(set(re.findall(r'\b' + re.escape(fn) + r'\("([A-Za-z0-9_]+)"', cpp)) - known):
        problems.append(f'{fn}("{name}") is not in {where}')
check('named sheet values exist', problems)

# --- both sides of the link agree on its sizes
link_h, link_cpp, lua = read('plugin/src/Link.h'), read('plugin/src/Link.cpp'), read('mordhau_side/PoseStream/Scripts/main.lua')
pairs = [('stand-in slots', r'kGhosts\s*=\s*(\d+)', link_h, r'MAX_GHOSTS\s*=\s*(\d+)', lua),
         ('pose memory bytes', r'kPoseBytes\s*=\s*(\d+)', link_cpp, r'POSE_BYTES,\s*INPUT_BYTES\s*=\s*(\d+)', lua),
         ('input memory bytes', r'kInputBytes\s*=\s*(\d+)', link_cpp, r'POSE_BYTES,\s*INPUT_BYTES\s*=\s*\d+,\s*(\d+)', lua)]
problems = []
for what, a, at, b, bt in pairs:
    x, y = re.search(a, at), re.search(b, bt)
    if not x or not y:
        problems.append(f'{what}: not found on {"the plugin" if not x else "the script"} side')
    elif x.group(1) != y.group(1):
        problems.append(f'{what}: plugin {x.group(1)}, script {y.group(1)}')
check('link sizes agree', problems)

# --- the shipped code makes no network calls (the page says so)
net = re.compile(r'\b(WinHttp\w*|Internet(Open|Connect)\w*|URLDownload\w*|WSAStartup|getaddrinfo|connect\s*\(|HttpRequest\w*|socket\s*\()|https?://')
problems = []
for base, exts, lua_side in (('plugin/src', ('.cpp', '.h'), False), ('loader', ('.cpp', '.h', '.asm'), False), ('mordhau_side/PoseStream', ('.lua',), True)):
    for p in files(base, exts):
        for m in net.finditer(no_comments(open(p, encoding='utf-8', errors='replace').read(), lua_side)):
            problems.append(f'{os.path.relpath(p, root)}: {m.group(0)}')
check('no network calls in the shipped code', problems)

# --- the download: no scripts (Nexus hid the mod once over renamed scripts, 2026-10-15), no private words
package = os.path.join(root, 'release', 'MordhauSkyrim-ModManager')
if os.path.isdir(package):
    wf = os.path.join(root, '_deps', 'private_words.txt')
    words = [w.strip() for w in open(wf, encoding='utf-8')] if os.path.exists(wf) else []
    private = re.compile('|'.join(re.escape(w) for w in [w for w in words if w] + ['paypal', 'gmail']).encode(), re.I)
    problems = []
    for d, _, fs in os.walk(package):
        for f in fs:
            rel = os.path.relpath(os.path.join(d, f), package)
            if f.lower().endswith(('.ps1', '.bat', '.cmd', '.vbs', '.js', '.py', '.sh', '.exe', '.reg')):
                problems.append(f'script or program: {rel}')
            if private.search(open(os.path.join(d, f), 'rb').read()):
                problems.append(f'private word in {rel}')   # (the word itself is not printed)
    for rel in ('release/description.md', 'release/README.txt', 'HOW-TO-PLAY.md'):
        if os.path.exists(os.path.join(root, rel)) and private.search(open(os.path.join(root, rel), 'rb').read()):
            problems.append(f'private word in {rel}')
    check('download: no scripts, no private words' + ('' if words else ' (no _deps/private_words.txt: general words only)'), problems)
else:
    print('skip  download (no release/MordhauSkyrim-ModManager yet)')

# --- nothing shipped still names a part that is gone
gone = re.compile(r'Play-Mashup|Install-Mashup|Uninstall-Mashup|Make-Mordhau-Online-Safe|\bthe launcher\b|bow\'s pose', re.I)
problems = []
for base, exts in (('plugin/src', ('.cpp', '.h')), ('mordhau_side/PoseStream', ('.lua',)), ('loader', ('.cpp', '.h')), ('installer', ('.md', '.ini', '.txt'))):
    for p in files(base, exts):
        for i, line in enumerate(open(p, encoding='utf-8', errors='replace'), 1):
            if gone.search(line):
                problems.append(f'{os.path.relpath(p, root)}:{i}')
for rel in ('release/description.md', 'HOW-TO-PLAY.md', 'sheets/live_link.json', 'sheets/hooks.json'):
    for i, line in enumerate(read(rel).split('\n'), 1):
        if gone.search(line):
            problems.append(f'{rel}:{i}')
check('no mentions of removed parts', problems)

print('all passed' if not failed else f'{len(failed)} failed')
sys.exit(1 if failed else 0)
