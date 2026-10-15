"""Dev (code cleanup, 2026-10-12): takes named top-level blocks out of one long function and makes each a function of
its own, placed right before the long one, with a call left in its place. Nothing in a block is changed.
Every local of the long function that the block names is handed over BY REFERENCE (const where it was declared
const), so reads and writes go to the same variable as before. A block is refused if it could leave the long function
(return at its own level), or if a name it uses is declared in a later-than-top-level scope of the long function.
usage: extract_blocks.py <file> <long function name> list
       extract_blocks.py <file> <long function name> apply <spec file>     (lines: <first words of the block's line, or ~words in its first five lines> => <NewName> | <comment>)"""
import re, sys

NL = chr(10)
path, func = sys.argv[1], sys.argv[2]
src = open(path, encoding='utf-8').read()
L = src.split(NL)
start = next(i for i, l in enumerate(L) if re.match(r'^\t\t(static )?[\w:<>\*& ]+\b' + re.escape(func) + r'\(', l) and L[i + 1] == '\t\t{')
end = next(j for j in range(start + 2, len(L)) if L[j] == '\t\t}')
T = '\t\t\t'
signature = L[start].strip()
params = re.findall(r'(\w+)\s*(?:,|\))', signature[signature.index('('):])


def blocks():
    out, i = [], start + 2
    while i < end:
        line = L[i]
        if line.startswith(T) and not line.startswith(T + '\t') and line.rstrip().endswith('{') and not line.strip().startswith('//'):
            j = next(k for k in range(i + 1, end + 1) if L[k].startswith(T + '}'))
            while L[j].rstrip().endswith('{'):
                j = next(k for k in range(j + 1, end + 1) if L[k].startswith(T + '}'))
            out.append((i, j))
            i = j + 1
        else:
            i += 1
    return out


# locals of the long function declared at its own level: name -> (line, is const, type text)
decl = re.compile(r'^\t\t\t(?!\t)(?P<stat>static )?(?P<const>const |constexpr )?(?P<type>auto|bool|int|float|double|unsigned|char|std::[\w:<>, ]+?|RE::[\w:<>]+|Clock::\w+|[A-Z][\w:<>]*)(?P<ptr>[\*&\s]+)(?P<rest>[^;(]*?[=;{(].*)$')
locals_ = {}
for i in range(start + 2, end):
    m = decl.match(L[i])
    if not m:
        continue
    # (several names in one declaration: float forward = 0.0f, right = 0.0f;)
    rest = re.sub(r'\{[^{}]*\}|\([^()]*\)', '', m.group('rest'))
    for piece in rest.split(','):
        name = re.match(r'\s*[\*&]*\s*(\w+)', piece)
        if name and not name.group(1)[0].isdigit():
            pointer = '*' in m.group('ptr')
            # (const T* x: the const is the pointed-to thing's, not the variable's)
            locals_.setdefault(name.group(1), (i, bool(m.group('const')) and not pointer, bool(m.group('stat')),
                                               ('const ' if m.group('const') and pointer else '') + m.group('type').strip() + ('*' if pointer else '')))

if sys.argv[3] == 'list':
    print(f'{func}: lines {start + 1}-{end + 1}; parameters {params}; {len(locals_)} top-level locals')
    for i, j in blocks():
        body = NL.join(L[i:j + 1])
        used = sorted(n for n in list(locals_) + params if locals_.get(n, (0,))[0] < i and re.search(r'(?<![\w.>:])' + re.escape(n) + r'\b', body))
        leaves = bool(re.search(r'^\t{4,}return\b', body, re.M)) and not re.search(r'\[[^\]]*\]\s*\(', body)
        print(f'{j - i + 1:5d} @{i + 1:5d} {"RETURNS " if re.search(chr(10) + T + chr(9) + "+return" , body) else ""}{L[i].strip()[:70]}  <- {used}')
    sys.exit()

# (what an 'auto' local of the long function really is, where a block is handed it)
KNOWN = {'link': 'Link::State', 'renderer': 'RE::BSGraphics::Renderer*', 'camera': 'RE::NiCamera*', 'player': 'RE::PlayerCharacter*', 'root': 'RE::NiAVObject*',
         'ui': 'RE::UI*', 'device': 'ID3D11Device*', 'context': 'ID3D11DeviceContext*', 'playerCamera': 'RE::PlayerCamera*', 'cell': 'RE::TESObjectCELL*'}
# (locals that are arrays: the whole parameter as it has to be written)
WHOLE = {'factor': 'const FLOAT (&factor)[4]'}
SMALL = ('bool', 'int', 'float', 'double', 'unsigned', 'UINT', 'std::size_t', 'std::uint32_t')
# ---- a loop's body as a function of its own: usage  extract_blocks.py <file> <function> loop "<first words of the for line>" <NewName> "<element type>" "<comment>"
# The body moves out unchanged except that a 'continue' of THIS loop becomes 'return'. Refused: a 'break' of this loop, a return outside a lambda.
if sys.argv[3] == 'loop':
    key, name, elem, comment = sys.argv[4], sys.argv[5], sys.argv[6], sys.argv[7]
    found = [(i, j) for i, j in blocks() if L[i].strip().startswith(key)]
    assert len(found) == 1, (key, len(found))
    i, j = found[0]
    head = re.match(r'^			for \((const )?auto(&?) (\w+) : (.+)\) \{$', L[i])
    assert head, L[i]
    var = head.group(3)
    body_lines = L[i + 1:j]
    out = []
    for k, line in enumerate(body_lines):
        code = line.split('//')[0]
        word = re.search(r'\b(continue|break|return)\b', code)
        if word:
            indent = len(line) - len(line.lstrip(chr(9)))
            owner = None
            for up in range(k - 1, -1, -1):
                ul = body_lines[up]
                ui = len(ul) - len(ul.lstrip(chr(9)))
                if ul.strip() and ui < indent:
                    indent = ui
                    text = ul.split('//')[0].strip()
                    if re.search(r'\[[^\]]*\]\s*(\([^)]*\))?\s*(mutable\s*)?(->\s*[\w:<>\*& ]+)?\s*\{\s*$', text):
                        owner = 'lambda'
                        break
                    if re.match(r'(for|while|do)\b', text) or (word.group(1) == 'break' and re.match(r'switch\b', text)):
                        owner = 'inner'
                        break
            one_line_lambda = re.search(r'\[[^\]]*\]\s*\([^)]*\)\s*(->\s*[\w:<>\*& ]+)?\s*\{[^{}]*$', code[:word.start()])
            if owner is None and not one_line_lambda:
                assert word.group(1) == 'continue', f'line {i + 2 + k}: a {word.group(1)} of this loop cannot move out: ' + line.strip()
                line = line[:word.start()] + 'return' + line[word.end():]
        out.append(line[1:] if line.startswith(chr(9)) else line)
    body = NL.join(body_lines)
    names = [n for n in params + sorted(locals_, key=lambda n: locals_[n][0]) if (n in params or locals_[n][0] < i) and re.search(r'(?<![\w.>:])' + re.escape(n) + r'\b', body)]
    args = []
    for n in names:
        if n in params:
            kind = re.search(r'((?:const )?[\w:<>]+\s*[\*&]*)\s*' + re.escape(n) + r'\s*[,)]', signature).group(1).strip()
            args.append((n, kind + ' ' + n))
        else:
            kind = locals_[n][3]
            if kind.startswith('auto'):
                kind = KNOWN.get(n, kind)
            assigned = re.search(r'(?<![\w.>:])' + re.escape(n) + r'\s*(=[^=]|\+=|-=|\*=|\+\+|--)', body) or re.search(r'&' + re.escape(n) + r'\b', body)
            if n in WHOLE:
                args.append((n, WHOLE[n]))
            elif kind.endswith('*') and not assigned:
                args.append((n, kind + ' ' + n))
            elif locals_[n][1] and kind in SMALL:
                args.append((n, 'const ' + kind + ' ' + n))
            else:
                args.append((n, ('const ' if locals_[n][1] else '') + kind + '& ' + n))
    first = (head.group(1) or '') + elem + (head.group(2) or '') + ' ' + var
    new_func = ['		// ' + comment, f'		void {name}(' + ', '.join([first] + [a for _, a in args]) + ')', '		{'] + out + ['		}', '']
    L[i + 1:j] = [f'{T}	{name}(' + ', '.join([var] + [n for n, _ in args]) + ');']
    L[start:start] = new_func
    open(path, 'w', encoding='utf-8', newline=NL).write(NL.join(L))
    print(f'{name}: {len(out)} lines, takes {[var] + [n for n, _ in args]}')
    sys.exit()

spec = [l.split('=>') for l in open(sys.argv[4], encoding='utf-8').read().split(NL) if '=>' in l]
done = []
for key, target in spec:
    key = key.strip()
    name, _, comment = [x.strip() for x in target.partition('|')]
    if key.startswith('~'):   # (a block that opens with a bare brace: told by words in its first five lines)
        found = [(i, j) for i, j in blocks() if key[1:] in NL.join(L[i:i + 3])]
    else:
        found = [(i, j) for i, j in blocks() if L[i].strip().startswith(key)]
    assert len(found) == 1, (key, len(found))
    i, j = found[0]
    body_lines = L[i:j + 1]
    body = NL.join(body_lines)
    # refuse: a return that is not inside a lambda would leave the long function
    refused = None
    for line in body_lines:
        code = line.split('//')[0]
        if re.search(r'\breturn\b', code):
            # allowed only inside a lambda: look for an enclosing '[...](' opener above with deeper indent bookkeeping
            indent = len(line) - len(line.lstrip('\t'))
            k = body_lines.index(line)
            # (a lambda written on this one line: [](char c) { return ...; })
            inside = all(re.search(r'\[[^\]]*\]\s*\([^)]*\)\s*(->\s*[\w:<>\*& ]+)?\s*\{[^{}]*$', code[:m.start()]) for m in re.finditer(r'\breturn\b', code))
            for up in range(k - 1, -1, -1):
                if inside:
                    break
                ui = len(body_lines[up]) - len(body_lines[up].lstrip('\t'))
                if ui < indent and re.search(r'\[[^\]]*\]\s*(\([^)]*\))?\s*(->\s*[\w:<>\*& ]+)?\s*\{\s*$', body_lines[up].split('//')[0]):
                    inside = True
                    break
                if ui < indent:
                    indent = ui
            if not inside:
                refused = f'a return at line {i + 1 + k} would leave {func}'
                break
    if refused:
        print(f'SKIPPED {name}: {refused}')
        continue
    names = [n for n in params + sorted(locals_, key=lambda n: locals_[n][0]) if (n in params or locals_[n][0] < i) and re.search(r'(?<![\w.>:])' + re.escape(n) + r'\b', body)]
    later = [n for n in locals_ if locals_[n][0] > j and False]
    args = []
    for n in names:
        if n in params:
            kind = re.search(r'((?:const )?[\w:<>]+\s*[\*&]*)\s*' + re.escape(n) + r'\s*[,)]', signature).group(1).strip()
            args.append((n, kind + ' ' + n))
        else:
            kind = locals_[n][3]
            if kind.startswith('auto'):
                kind = KNOWN.get(n, kind)
            assigned = re.search(r'(?<![\w.>:])' + re.escape(n) + r'\s*(=[^=]|\+=|-=|\*=|\+\+|--)', body) or re.search(r'&' + re.escape(n) + r'\b', body)
            if n in WHOLE:
                args.append((n, WHOLE[n]))
            elif kind.endswith('*') and not assigned:
                args.append((n, kind + ' ' + n))            # (a pointer the block only uses: handed as it is)
            elif locals_[n][1] and kind in SMALL:
                args.append((n, 'const ' + kind + ' ' + n))  # (a small constant: handed as it is)
            else:
                args.append((n, ('const ' if locals_[n][1] else '') + kind + '& ' + n))
    head = ['\t\t// ' + comment] if comment else []
    head += [f'\t\tvoid {name}(' + ', '.join(a for _, a in args) + ')', '\t\t{']
    # the block keeps its own line and braces; one tab less
    moved = [l[1:] if l.startswith('\t') else l for l in body_lines]
    new_func = head + moved + ['\t\t}', '']
    L[i:j + 1] = [f'{T}{name}(' + ', '.join(n for n, _ in args) + ');']
    L[start:start] = new_func
    shift = len(new_func)
    start += shift
    end += shift - (j - i)
    locals_ = {n: (v[0] + shift - ((j - i) if v[0] > j else 0), v[1], v[2], v[3]) for n, v in locals_.items() if not (i <= v[0] <= j)}
    done.append((name, j - i + 1, [n for n, _ in args]))
open(path, 'w', encoding='utf-8', newline=NL).write(NL.join(L))
for name, n, args in done:
    print(f'{name}: {n} lines, takes {args}')
print(f'{func} is now {end - start + 1} lines')
