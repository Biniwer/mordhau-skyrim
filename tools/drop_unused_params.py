"""Dev (code cleanup): parameters the compiler reports as never used (warning C4100) in functions made by
extract_blocks.py are taken out of the function, its prototype and its calls. usage: drop_unused_params.py <build log>"""
import re, sys, glob

NL, BS = chr(10), chr(92)
log = open(sys.argv[1], encoding='utf-8', errors='replace').read().replace(BS, '/')
seen = set(re.findall(r"src/([\w/]+\.(?:cpp|h))\((\d+),\d+\): warning C4100: '(\w+)'", log))
files = {f.replace(BS, '/'): open(f, encoding='utf-8').read().split(NL) for f in glob.glob('plugin/src/*.cpp') + glob.glob('plugin/src/*.h')}


def split_args(text):
    out, depth, cur = [], 0, ''
    for ch in text:
        if ch in '([{<':
            depth += 1
        elif ch in ')]}>':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur.strip())
            cur = ''
        else:
            cur += ch
    return out + [cur.strip()] if cur.strip() else out


drops = {}
for f, line, name in sorted(seen):
    L = files['plugin/src/' + f]
    k = int(line) - 1
    while not re.match(r'^\t\t(void|bool) \w+\(.*\)$', L[k]):
        k -= 1
    m = re.match(r'^(\t\t(?:void|bool) (\w+)\()(.*)\)$', L[k])
    params = split_args(m.group(3))
    idx = [i for i, p in enumerate(params) if re.search(r'[\s\*&(]' + name + r'(\)\[\d+\])?$', p)]
    assert len(idx) == 1, (f, line, name, params)
    drops.setdefault(m.group(2), set()).add((idx[0], name))
for func, which in drops.items():
    indexes = sorted(i for i, _ in which)
    done = 0
    for f, L in files.items():
        for k, l in enumerate(L):
            m = re.match(r'^(\t+(?:(?:void|bool) |if \()?' + func + r'\()(.*?)(\)|\);|\)\) \{)$', l)
            if not m:
                continue
            args = split_args(m.group(2))
            L[k] = m.group(1) + ', '.join(a for i, a in enumerate(args) if i not in indexes) + m.group(3)
            done += 1
    print(func, sorted(n for _, n in which), 'lines changed', done)
for f, L in files.items():
    open(f, 'w', encoding='utf-8', newline=NL).write(NL.join(L))
