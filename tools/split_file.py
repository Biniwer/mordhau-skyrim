"""Dev (code cleanup): splits one big .cpp whose code sits in an anonymous namespace into several .cpp files by topic.
Nothing inside a function changes. What the anonymous namespace held at its own level is sorted into:
  - functions: their bodies go to the .cpp file the map names for them (or stay in the original file); a prototype
    takes their place in the internal header (none for a function with 'auto' in its signature - it stays usable only
    in its own file, below where it is defined);
  - everything else (types, variables, constants, prototypes, loose comments): moved to the internal header in its
    original order, variables marked 'inline' so that every file sees the one same variable.
The anonymous namespace becomes 'inline namespace internal', so every name is found exactly as before.
usage: split_file.py <file.cpp> <InternalHeader.h> <map file> [write]
map file lines:  NewFile.cpp: FunctionA FunctionB ...      (a function may be named Name@line when the name is not unique)"""
import os, re, sys

NL, T = chr(10), chr(9)
src_path, header_name, map_path = sys.argv[1], sys.argv[2], sys.argv[3]
write = len(sys.argv) > 4 and sys.argv[4] == 'write'
L = open(src_path, encoding='utf-8').read().split(NL)
base = os.path.dirname(src_path)

open_at = next(i for i, l in enumerate(L) if l == T + 'namespace' and L[i + 1] == T + '{')
ns_line = max(i for i in range(open_at) if L[i].startswith('namespace '))
assert L[ns_line + 1] == '{'


# ---- depth after every line (strings, raw strings and comments do not count)
def scan(lines):
    depth, out, in_block, raw_end = 0, [], False, None
    for line in lines:
        i, code = 0, ''
        while i < len(line):
            if raw_end:
                k = line.find(raw_end, i)
                if k < 0:
                    i = len(line)
                else:
                    i, raw_end = k + len(raw_end), None
                continue
            if in_block:
                k = line.find('*/', i)
                if k < 0:
                    i = len(line)
                else:
                    i, in_block = k + 2, False
                continue
            c = line[i]
            if line.startswith('//', i):
                break
            if line.startswith('/*', i):
                in_block, i = True, i + 2
                continue
            m = re.match(r'R"([^(]*)\(', line[i:])
            if m:
                raw_end, i = ')' + m.group(1) + '"', i + len(m.group(0))
                continue
            if c == '"':
                i += 1
                while i < len(line) and line[i] != '"':
                    i += 2 if line[i] == chr(92) else 1
                i += 1
                continue
            if c == "'":
                m = re.match(r"'(\\.|[^'\\])'", line[i:])
                if m:
                    i += len(m.group(0))
                    continue
            if c in '{([':
                depth += 1
            elif c in '})]':
                depth -= 1
            code += c
            i += 1
        out.append((depth, code.strip()))
    return out


close_at = None
info = scan(L[open_at + 2:])
for k, (depth, code) in enumerate(info):
    if depth < 0:
        close_at = open_at + 2 + k
        break
assert L[close_at].startswith(T + '}'), L[close_at]
body = L[open_at + 2:close_at]
info = info[:len(body)]

# ---- items
items, i = [], 0
while i < len(body):
    if not body[i].strip():
        i += 1
        continue
    start, code_seen = i, False
    while True:
        depth, code = info[i]
        if code:
            code_seen = True
        ended = depth == 0 and code_seen and (code.endswith(';') or code == '}' or code.endswith('};'))
        loose_comment = not code_seen and (i + 1 >= len(body) or not body[i + 1].strip())
        i += 1
        if ended or loose_comment or i >= len(body):
            break
    items.append((start, i))

FUNC = re.compile(r'^\t\t(?!\t)[^/#{}].*\)( const)?( noexcept)?$')


def classify(a, b):
    lines = body[a:b]
    code = [k for k in range(a, b) if info[k][1]]
    if not code:
        return 'comment', None
    first = body[code[0]]
    stripped = first.strip()
    if stripped.startswith('template'):
        return 'verbatim', None
    brace = next((k for k in code if body[k] == T + T + '{'), None)
    if brace is not None and brace != code[0] and body[code[-1]] == T + T + '}' and not re.match(r'(struct|class|union|enum)\b', stripped):
        joined = ' '.join(info[k][1] for k in code if k < brace)
        if re.search(r'\)( const)?( noexcept)?$', joined) and '=' not in joined.split('(')[0]:
            return 'func', re.search(r'([\w~]+)\s*\(', stripped.split('(')[0] + '(').group(1)
    if re.match(r'(struct|class|union|enum)\b', stripped):
        last = body[code[-1]].strip()
        return ('type' if last == '};' or (len(code) == 1 and last.endswith('};')) or (len(code) == 1 and last.endswith(';') and '{' not in last and '=' not in last) else 'ODD'), None
    if re.match(r'(using|typedef|namespace|extern|static_assert)\b', stripped) or stripped.startswith('#'):
        return 'verbatim', None
    only = info[code[0]][1]
    if len(code) == 1 and only.endswith(');') and '=' not in only.split('(')[0] and (re.search(r'\(\s*\)', only) or re.search(r'[\w>\*&]\s+[\*&]*a_\w+', only)):
        return 'proto', None
    return 'var', None


kinds = [classify(a, b) for a, b in items]

# ---- the map
target, wanted = {}, {}
for line in open(map_path, encoding='utf-8').read().split(NL):
    if ':' in line and not line.startswith('#'):
        f, names = line.split(':', 1)
        for n in names.split():
            wanted[n] = f.strip()
names_seen = {}
for (a, b), (kind, name) in zip(items, kinds):
    if kind == 'func':
        names_seen.setdefault(name, []).append(a)
for (a, b), (kind, name) in zip(items, kinds):
    if kind == 'func':
        line_no = open_at + 3 + next(k for k in range(a, b) if info[k][1])
        key = f'{name}@{line_no}' if f'{name}@{line_no}' in wanted else name
        target[a] = wanted.get(key)
        if key in wanted:
            wanted[key] = None
missing = [n for n, f in wanted.items() if f is not None]

count = {}
for k, _ in kinds:
    count[k] = count.get(k, 0) + 1
print('items:', count)
print('ODD items (need a hand):', [(open_at + 3 + a, body[a].strip()[:70]) for (a, b), (k, _) in zip(items, kinds) if k == 'ODD'])
print('names in the map that are not functions here:', missing)
print('function names used more than once:', {n: [open_at + 3 + x for x in v] for n, v in names_seen.items() if len(v) > 1})
if '-v' in sys.argv:
    for (a, b), (k, n) in zip(items, kinds):
        if k in ('var', 'proto', 'verbatim'):
            first = next((body[x] for x in range(a, b) if info[x][1]), body[a])
            print(f'  {k:8s} {open_at + 3 + a:5d} {first.strip()[:110]}')
if not write:
    sys.exit()
assert not missing and not any(k == 'ODD' for k, _ in kinds)


# ---- writing
def strip_defaults(sig):
    """'void F(int a = 1, bool b = true)' -> 'void F(int a, bool b)' (the defaults stay on the prototype)"""
    o = sig.index('(')
    depth, out, skipping = 0, '', False
    for ch in sig[o:]:
        if ch in '([{<':
            depth += 1
        elif ch in ')]}>':
            depth -= 1
        if depth == 1 and ch == '=' and not skipping:
            skipping = True
            out = out.rstrip()
            continue
        if skipping and ((depth == 1 and ch == ',') or depth == 0):
            skipping = False
        if not skipping:
            out += ch
    return sig[:o] + out


header, parts = [], {}
for (a, b), (kind, name) in zip(items, kinds):
    lines = body[a:b]
    if kind == 'func':
        k = next(x for x in range(a, b) if info[x][1])
        brace = next(x for x in range(k, b) if body[x] == T + T + '{')
        sig = body[k] if brace == k + 1 else T + T + ' '.join(body[x].strip() for x in range(k, brace))   # (a signature over several lines: joined)
        plain = re.sub(r'^(\t\t)(static |inline )+', r'\1', sig)
        templated = bool(re.search(r'\bauto\b', sig))
        definition = body[a:k] + [strip_defaults(plain) if not templated else plain] + body[brace:b]
        parts.setdefault(target[a], []).append(definition)
        if not templated:
            header.append([plain + ';'])
    elif kind == 'var':
        k = next(x for x in range(a, b) if info[x][1])
        first = re.sub(r'^(\t\t)static ', r'\1', body[k])
        if not re.match(r'\t\t(inline|constexpr|extern)\b', first):
            first = T + T + 'inline ' + first[2:]
        header.append(body[a:k] + [first] + body[k + 1:b])
    else:
        header.append(lines)

top = L[:ns_line]                      # the includes
tail = L[close_at + 1:]                # the rest of the named namespace, and whatever follows it
ns_open = [L[ns_line], '{', T + 'inline namespace internal', T + '{']
guard = ['#pragma once', ''] + [l for l in top if l.strip()] + ['']
about = [f'// What the files of {os.path.basename(src_path)[:-4]} share among themselves: its types, its state and the prototypes of its functions.',
         f'// Not for use outside them - the public interface is {os.path.basename(src_path)[:-4]}.h.']
out_header = about + guard + ns_open + [l for item in header for l in item + ['']][:-1] + [T + '}', '}', '']
files = {os.path.join(base, header_name): out_header}
for f, defs in parts.items():
    text = [f'#include "{header_name}"', ''] + ns_open + [l for d in defs for l in d + ['']][:-1] + [T + '}']
    if f is None:
        files[src_path] = text + tail
    else:
        files[os.path.join(base, f)] = text + ['}', '']
if src_path not in files:
    files[src_path] = [f'#include "{header_name}"', ''] + ns_open + [T + '}'] + tail
for f, text in files.items():
    open(f, 'w', encoding='utf-8', newline=NL).write(NL.join(text))
    print(f'{f}: {len(text)} lines')
