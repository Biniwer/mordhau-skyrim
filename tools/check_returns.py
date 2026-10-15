"""Dev (code cleanup): for functions made by extract_blocks.py, every 'return' in them and the scope it sits in - a
return that is not inside a lambda used to leave the long function and now only leaves the new one.
usage: check_returns.py <file> <function name>..."""
import re, sys
L = open(sys.argv[1], encoding='utf-8').read().split(chr(10))
for func in sys.argv[2:]:
    start = next(i for i, l in enumerate(L) if re.match(r'^\t\tvoid ' + func + r'\(', l))
    end = next(j for j in range(start + 2, len(L)) if L[j] == '\t\t}')
    plain = []
    for k in range(start, end):
        code = L[k].split('//')[0]
        for m in re.finditer(r'\breturn\b', code):
            if re.search(r'\[[^\]]*\]\s*(\([^)]*\))?\s*(->\s*[\w:<>\*& ]+)?\s*\{[^{}]*$', code[:m.start()]):
                continue   # a lambda on this line
            indent = len(L[k]) - len(L[k].lstrip('\t'))
            scope = None
            for up in range(k - 1, start, -1):
                ui = len(L[up]) - len(L[up].lstrip('\t'))
                if L[up].strip() and ui < indent:
                    indent = ui
                    text = L[up].split('//')[0]
                    if re.search(r'\[[^\]]*\]\s*(\([^)]*\))?\s*(mutable\s*)?(->\s*[\w:<>\*& ]+)?\s*\{\s*$', text):
                        scope = text.strip()[:90]
                        break
            if scope is None:
                plain.append((k + 1, L[k].strip()[:80]))
    print(func, f'({end - start + 1} lines):', 'every return is inside a lambda' if not plain else f'{len(plain)} RETURN(S) AT FUNCTION LEVEL: {plain[:6]}')
