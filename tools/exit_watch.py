"""Dev: who tells MORDHAU to quit? Attaches to the running MORDHAU as a debugger and puts a hardware write-watch on its
exit flag (GIsRequestingExit, found in MORDHAU's own debug symbols). When something writes the flag, the writing code's
address and the return addresses on that thread's stack are written, with their names, to cache/exit_watch.log.
MORDHAU keeps running at full speed (a hardware watch costs nothing) and is NOT closed when this script ends.
Local, offline match only (as the whole mashup is). usage: exit_watch.py   (run it after MORDHAU is in its match)"""
import ctypes, os, struct, sys, time
from ctypes import wintypes as w

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOG = os.path.join(ROOT, 'cache', 'exit_watch.log')
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pdb_symbols as sym   # (loads MORDHAU's .pdb at sym.BASE)

FLAG_RVA = sym.find('GIsRequestingExit')[0][1]
k32 = ctypes.WinDLL('kernel32', use_last_error=True)
psapi = ctypes.WinDLL('psapi', use_last_error=True)
dbg = sym.dbg

def say(text):
    line = time.strftime('%H:%M:%S ') + text
    print(line, flush=True)
    with open(LOG, 'a', encoding='utf-8') as f:
        f.write(line + '\n')

def pid_of(name):
    import subprocess
    out = subprocess.run(['tasklist', '/FI', f'IMAGENAME eq {name}', '/FO', 'CSV', '/NH'], capture_output=True, text=True).stdout
    for row in out.splitlines():
        parts = row.split('","')
        if len(parts) > 1 and parts[0].strip('"').lower() == name.lower():
            return int(parts[1])
    return 0

class SYMBOL_INFOW(ctypes.Structure):
    _fields_ = sym.SYMBOL_INFOW._fields_
dbg.SymFromAddrW.argtypes = [w.HANDLE, ctypes.c_uint64, ctypes.POINTER(ctypes.c_uint64), ctypes.POINTER(sym.SYMBOL_INFOW)]

def name_of(rva):
    info = sym.SYMBOL_INFOW(); info.SizeOfStruct = ctypes.sizeof(sym.SYMBOL_INFOW) - 1024 * 2; info.MaxNameLen = 1023
    disp = ctypes.c_uint64()
    if dbg.SymFromAddrW(sym.h, sym.BASE + rva, ctypes.byref(disp), ctypes.byref(info)):
        return f'{info.Name}+0x{disp.value:X}'
    return '?'

pid = pid_of('Mordhau-Win64-Shipping.exe')
if not pid:
    say('MORDHAU is not running'); sys.exit(1)
k32.OpenProcess.restype = w.HANDLE
proc = k32.OpenProcess(0x1F0FFF, False, pid)
mods = (w.HMODULE * 4)(); need = w.DWORD()
psapi.EnumProcessModules.argtypes = [w.HANDLE, ctypes.c_void_p, w.DWORD, ctypes.POINTER(w.DWORD)]
assert psapi.EnumProcessModules(proc, mods, ctypes.sizeof(mods), ctypes.byref(need)), ctypes.get_last_error()
base = mods[0]
class MODULEINFO(ctypes.Structure):
    _fields_ = [('lpBaseOfDll', ctypes.c_void_p), ('SizeOfImage', w.DWORD), ('EntryPoint', ctypes.c_void_p)]
mi = MODULEINFO()
psapi.GetModuleInformation.argtypes = [w.HANDLE, w.HMODULE, ctypes.POINTER(MODULEINFO), w.DWORD]
psapi.GetModuleInformation(proc, mods[0], ctypes.byref(mi), ctypes.sizeof(mi))
size = mi.SizeOfImage
flag = base + FLAG_RVA

# an aligned CONTEXT (x64: 1232 bytes, 16-byte aligned)
k32.VirtualAlloc.restype = ctypes.c_void_p
k32.VirtualAlloc.argtypes = [ctypes.c_void_p, ctypes.c_size_t, w.DWORD, w.DWORD]
ctx = k32.VirtualAlloc(None, 4096, 0x3000, 0x04)
CONTEXT_AMD64, DEBUG_REGS, CONTROL, INTEGER = 0x00100000, 0x10, 0x1, 0x2
k32.GetThreadContext.argtypes = [w.HANDLE, ctypes.c_void_p]
k32.SetThreadContext.argtypes = [w.HANDLE, ctypes.c_void_p]
k32.OpenThread.restype = w.HANDLE
k32.ReadProcessMemory.argtypes = [w.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]

def arm(thread):
    ctypes.memset(ctx, 0, 1232)
    ctypes.cast(ctx + 0x30, ctypes.POINTER(w.DWORD))[0] = CONTEXT_AMD64 | DEBUG_REGS
    if not k32.GetThreadContext(thread, ctx):
        return False
    ctypes.cast(ctx + 0x48, ctypes.POINTER(ctypes.c_uint64))[0] = flag          # Dr0
    ctypes.cast(ctx + 0x68, ctypes.POINTER(ctypes.c_uint64))[0] = 0             # Dr6
    ctypes.cast(ctx + 0x70, ctypes.POINTER(ctypes.c_uint64))[0] = 0x00010001    # Dr7: Dr0 on, on write, 1 byte
    ctypes.cast(ctx + 0x30, ctypes.POINTER(w.DWORD))[0] = CONTEXT_AMD64 | DEBUG_REGS
    return bool(k32.SetThreadContext(thread, ctx))

class DEBUG_EVENT(ctypes.Structure):
    _fields_ = [('code', w.DWORD), ('pid', w.DWORD), ('tid', w.DWORD), ('pad', w.DWORD), ('u', ctypes.c_byte * 160)]
assert k32.DebugActiveProcess(pid), ctypes.get_last_error()
k32.DebugSetProcessKillOnExit(False)
say(f'watching MORDHAU (pid {pid}): its exit flag is at +0x{FLAG_RVA:X}')
event = DEBUG_EVENT()
threads, hits = 0, 0
while True:
    if not k32.WaitForDebugEvent(ctypes.byref(event), 1000):
        continue
    status = 0x80010001   # DBG_EXCEPTION_NOT_HANDLED
    raw = bytes(event.u)
    if event.code == 3:      # the process itself: its first thread
        t = k32.OpenThread(0x1FFFFF, False, event.tid)
        if t and arm(t): threads += 1
        if t: k32.CloseHandle(t)
    elif event.code == 2:    # a thread (the ones that exist already come as these too)
        t = k32.OpenThread(0x1FFFFF, False, event.tid)
        if t and arm(t): threads += 1
        if t: k32.CloseHandle(t)
    elif event.code == 1:    # an exception
        code = struct.unpack_from('<I', raw, 0)[0]
        if code == 0x80000003:      # the attach breakpoint (or the engine's own debug break): carry on
            status = 0x00010002
            if threads and not hits: say(f'attached; the watch is on {threads} threads')
        elif code == 0x80000004:    # single step = our watch
            status = 0x00010002
            hits += 1
            t = k32.OpenThread(0x1FFFFF, False, event.tid)
            ctypes.memset(ctx, 0, 1232)
            ctypes.cast(ctx + 0x30, ctypes.POINTER(w.DWORD))[0] = CONTEXT_AMD64 | CONTROL | INTEGER | DEBUG_REGS
            k32.GetThreadContext(t, ctx)
            rip = ctypes.cast(ctx + 0xF8, ctypes.POINTER(ctypes.c_uint64))[0]
            rsp = ctypes.cast(ctx + 0x98, ctypes.POINTER(ctypes.c_uint64))[0]
            value = ctypes.c_ubyte(); got = ctypes.c_size_t()
            k32.ReadProcessMemory(proc, flag, ctypes.byref(value), 1, ctypes.byref(got))
            say(f'*** the exit flag was written (now {value.value}) by thread {event.tid}, code at +0x{rip - base:X}  {name_of(rip - base) if base <= rip < base + size else "(outside MORDHAU: another module)"}')
            stack = ctypes.create_string_buffer(8192)
            k32.ReadProcessMemory(proc, rsp, stack, 8192, ctypes.byref(got))
            shown = 0
            for i in range(0, got.value - 7, 8):
                a = struct.unpack_from('<Q', stack.raw, i)[0]
                if base <= a < base + size and shown < 28:
                    say(f'      stack +0x{i:04X}: +0x{a - base:X}  {name_of(a - base)}')
                    shown += 1
            ctypes.cast(ctx + 0x68, ctypes.POINTER(ctypes.c_uint64))[0] = 0
            ctypes.cast(ctx + 0x30, ctypes.POINTER(w.DWORD))[0] = CONTEXT_AMD64 | DEBUG_REGS
            k32.SetThreadContext(t, ctx)
            k32.CloseHandle(t)
    elif event.code == 5:    # the process ended
        say(f'MORDHAU ended (exit code {struct.unpack_from("<I", raw, 0)[0]}); the flag was written {hits} time(s) while watched')
        k32.ContinueDebugEvent(event.pid, event.tid, 0x00010002)
        break
    k32.ContinueDebugEvent(event.pid, event.tid, status)
