"""Dev: turn a plugin offset from the log ("exception ... in this plugin at +11EE91") into function, file and line.
usage: plugin_addr.py 11EE91 [more offsets]   (reads build/RelWithDebInfo/DirectionalMelee.dll and its .pdb)"""
import ctypes, os, sys
from ctypes import wintypes as w

DLL = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'build', 'RelWithDebInfo', 'DirectionalMelee.dll')
BASE = 0x180000000
dbg = ctypes.WinDLL('dbghelp')
k32 = ctypes.WinDLL('kernel32')
k32.GetCurrentProcess.restype = w.HANDLE
proc = k32.GetCurrentProcess()

class SYMBOL_INFO(ctypes.Structure):
    _fields_ = [('SizeOfStruct', w.ULONG), ('TypeIndex', w.ULONG), ('Reserved', ctypes.c_ulonglong * 2), ('Index', w.ULONG), ('Size', w.ULONG),
                ('ModBase', ctypes.c_ulonglong), ('Flags', w.ULONG), ('Value', ctypes.c_ulonglong), ('Address', ctypes.c_ulonglong), ('Register', w.ULONG),
                ('Scope', w.ULONG), ('Tag', w.ULONG), ('NameLen', w.ULONG), ('MaxNameLen', w.ULONG), ('Name', ctypes.c_char * 1024)]

class IMAGEHLP_LINE64(ctypes.Structure):
    _fields_ = [('SizeOfStruct', w.DWORD), ('Key', ctypes.c_void_p), ('LineNumber', w.DWORD), ('FileName', ctypes.c_char_p), ('Address', ctypes.c_ulonglong)]

dbg.SymSetOptions(0x10 | 0x2)  # load lines, undecorate
dbg.SymInitialize.argtypes = [w.HANDLE, ctypes.c_char_p, w.BOOL]
dbg.SymLoadModuleEx.argtypes = [w.HANDLE, w.HANDLE, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_ulonglong, w.DWORD, ctypes.c_void_p, w.DWORD]
dbg.SymLoadModuleEx.restype = ctypes.c_ulonglong
dbg.SymFromAddr.argtypes = [w.HANDLE, ctypes.c_ulonglong, ctypes.POINTER(ctypes.c_ulonglong), ctypes.POINTER(SYMBOL_INFO)]
dbg.SymGetLineFromAddr64.argtypes = [w.HANDLE, ctypes.c_ulonglong, ctypes.POINTER(w.DWORD), ctypes.POINTER(IMAGEHLP_LINE64)]
assert dbg.SymInitialize(proc, os.path.dirname(DLL).encode(), False)
assert dbg.SymLoadModuleEx(proc, None, DLL.encode(), None, BASE, 0, None, 0), 'could not load ' + DLL
for arg in sys.argv[1:]:
    off = int(arg.lstrip('+'), 16)
    sym = SYMBOL_INFO(); sym.SizeOfStruct = ctypes.sizeof(SYMBOL_INFO) - 1024; sym.MaxNameLen = 1023
    disp = ctypes.c_ulonglong()
    name = sym.Name.decode(errors='replace') if dbg.SymFromAddr(proc, BASE + off, ctypes.byref(disp), ctypes.byref(sym)) else '?'
    line = IMAGEHLP_LINE64(); line.SizeOfStruct = ctypes.sizeof(IMAGEHLP_LINE64)
    d2 = w.DWORD()
    where = f'{os.path.basename(line.FileName.decode())}:{line.LineNumber}' if dbg.SymGetLineFromAddr64(proc, BASE + off, ctypes.byref(d2), ctypes.byref(line)) else '?'
    print(f'+{off:X}: {name} +0x{disp.value:X}  {where}')
