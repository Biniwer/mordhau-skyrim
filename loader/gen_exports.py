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
# (each export jumps through its place in g_real. The place is empty until the export's first call, which asks
# ResolveReal in loader.cpp: the real dwmapi.dll is loaded then, not in DllMain. The argument registers - the four
# integer and the four floating-point ones - are kept across that lookup.)
RESOLVE = '''resolve PROC FRAME
\tpush rcx
\t.pushreg rcx
\tpush rdx
\t.pushreg rdx
\tpush r8
\t.pushreg r8
\tpush r9
\t.pushreg r9
\tsub rsp, 104
\t.allocstack 104
\t.endprolog
\tmovdqu XMMWORD PTR [rsp + 32], xmm0
\tmovdqu XMMWORD PTR [rsp + 48], xmm1
\tmovdqu XMMWORD PTR [rsp + 64], xmm2
\tmovdqu XMMWORD PTR [rsp + 80], xmm3
\tmov ecx, eax
\tcall ResolveReal
\tmovdqu xmm0, XMMWORD PTR [rsp + 32]
\tmovdqu xmm1, XMMWORD PTR [rsp + 48]
\tmovdqu xmm2, XMMWORD PTR [rsp + 64]
\tmovdqu xmm3, XMMWORD PTR [rsp + 80]
\tadd rsp, 104
\tpop r9
\tpop r8
\tpop rdx
\tpop rcx
\tjmp rax
resolve ENDP
'''
with open('exports.asm', 'w') as f:
    f.write('; made by gen_exports.py: every export of dwmapi.dll jumps to the real one, looked up on its first call\n'
            'EXTERN g_real:QWORD\nEXTERN ResolveReal:PROC\n.code\n' + RESOLVE)
    for i, _ in enumerate(ex):
        f.write(f'f_{i} PROC\n\tmov rax, QWORD PTR [g_real + {i * 8}]\n\ttest rax, rax\n\tjz first_{i}\n\tjmp rax\n'
                f'first_{i}:\n\tmov eax, {i}\n\tjmp resolve\nf_{i} ENDP\n')
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
