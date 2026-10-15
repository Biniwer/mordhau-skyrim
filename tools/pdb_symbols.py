"""Look up symbol addresses in MORDHAU's own debug symbols (the .pdb shipped next to its program). Read-only.
Usage: pdb_symbols.py <name or wildcard> ...   -> name, RVA (offset from the program's base), size
"""
import ctypes, os, sys
from ctypes import wintypes as w

EXE = 'E:/steam object/steamapps/common/Mordhau/Mordhau/Binaries/Win64/Mordhau-Win64-Shipping.exe'.replace('/', os.sep)
BASE = 0x140000000
dbg = ctypes.WinDLL('dbghelp.dll', use_last_error=True)
h = w.HANDLE(0x1234)
dbg.SymSetOptions.argtypes = [w.DWORD]
dbg.SymSetOptions(0x00000002 | 0x00000004)  # UNDNAME | DEFERRED_LOADS
dbg.SymInitializeW.argtypes = [w.HANDLE, w.LPCWSTR, w.BOOL]
assert dbg.SymInitializeW(h, os.path.dirname(EXE), False), ctypes.get_last_error()
dbg.SymLoadModuleExW.restype = ctypes.c_uint64
dbg.SymLoadModuleExW.argtypes = [w.HANDLE, w.HANDLE, w.LPCWSTR, w.LPCWSTR, ctypes.c_uint64, w.DWORD, ctypes.c_void_p, w.DWORD]
mod = dbg.SymLoadModuleExW(h, None, EXE, None, BASE, 0, None, 0)
assert mod, ctypes.get_last_error()


class SYMBOL_INFOW(ctypes.Structure):
    _fields_ = [('SizeOfStruct', w.ULONG), ('TypeIndex', w.ULONG), ('Reserved', ctypes.c_uint64 * 2), ('Index', w.ULONG), ('Size', w.ULONG),
                ('ModBase', ctypes.c_uint64), ('Flags', w.ULONG), ('Value', ctypes.c_uint64), ('Address', ctypes.c_uint64), ('Register', w.ULONG),
                ('Scope', w.ULONG), ('Tag', w.ULONG), ('NameLen', w.ULONG), ('MaxNameLen', w.ULONG), ('Name', ctypes.c_wchar * 1024)]


CB = ctypes.WINFUNCTYPE(w.BOOL, ctypes.POINTER(SYMBOL_INFOW), w.ULONG, ctypes.c_void_p)
dbg.SymEnumSymbolsW.argtypes = [w.HANDLE, ctypes.c_uint64, w.LPCWSTR, CB, ctypes.c_void_p]


def find(mask, limit=40):
    out = []

    def cb(info, size, ctx):
        out.append((info.contents.Name, info.contents.Address - BASE, info.contents.Size))
        return len(out) < limit

    dbg.SymEnumSymbolsW(h, mod, mask, CB(cb), None)
    return out


if __name__ == '__main__':
    for mask in sys.argv[1:]:
        hits = find(mask)
        print(f'{mask}: {len(hits)} match(es)')
        for name, rva, size in hits[:40]:
            print(f'   0x{rva:08X}  size {size:6d}  {name}')
