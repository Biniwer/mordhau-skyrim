"""Writes the forwarding part of the mod's own loader from Windows' own dwmapi.dll: exports.asm (one jump per export
through g_real), exports.def (the export list, by name and by number), exports_table.cpp (names and numbers).
usage: gen_exports.py   (run in loader/)"""
import struct

def exports(path):
    d = open(path, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    nsec = struct.unpack_from('<H', d, pe + 6)[0]
    opt = pe + 24
    optsize = struct.unpack_from('<H', d, pe + 20)[0]
    dd = opt + (112 if struct.unpack_from('<H', d, opt)[0] == 0x20b else 96)
    erva = struct.unpack_from('<I', d, dd)[0]
    secs = [struct.unpack_from('<8sIIII', d, opt + optsize + i * 40) for i in range(nsec)]
    def off(rva):
        for _, vs, va, rs, ro in secs:
            if va <= rva < va + max(vs, rs):
                return rva - va + ro
    e = off(erva)
    base, nfun, nnames, afun, anames, aords = struct.unpack_from('<IIIIII', d, e + 16)
    names = {}
    for i in range(nnames):
        n = off(struct.unpack_from('<I', d, off(anames) + 4 * i)[0])
        names[struct.unpack_from('<H', d, off(aords) + 2 * i)[0]] = d[n:d.index(b'\0', n)].decode()
    return [(base + i, names.get(i)) for i in range(nfun) if struct.unpack_from('<I', d, off(afun) + 4 * i)[0]]

ex = exports(r'C:\Windows\System32\dwmapi.dll')
with open('exports.asm', 'w') as f:
    f.write('; made by gen_exports.py: every export of dwmapi.dll jumps to the real one\nEXTERN g_real:QWORD\n.code\n')
    for i, _ in enumerate(ex):
        f.write(f'f_{i} PROC\n\tjmp QWORD PTR [g_real + {i * 8}]\nf_{i} ENDP\n')
    f.write('END\n')
with open('exports.def', 'w') as f:
    f.write('LIBRARY dwmapi\nEXPORTS\n')
    for i, (ordinal, name) in enumerate(ex):
        f.write(f'\t{name}=f_{i} @{ordinal}\n' if name else f'\tf_{i} @{ordinal} NONAME\n')
with open('exports_table.cpp', 'w') as f:
    f.write('// made by gen_exports.py\n')
    f.write(f'extern "C" {{ void* g_real[{len(ex)}] = {{}}; extern const int g_count = {len(ex)};\n')
    f.write('extern const char* const g_names[] = { ' + ', '.join(f'"{n}"' if n else 'nullptr' for _, n in ex) + ' };\n')
    f.write('extern const unsigned short g_ordinals[] = { ' + ', '.join(str(o) for o, _ in ex) + ' }; }\n')
print(len(ex), 'exports')
