"""Dev: the stalled-script watchdog on the D: test copy (start a session first with tools/d_test.sh).
  stall_test.py spike <seconds>   MORDHAU's whole process is frozen for that long, then let go: must survive
  stall_test.py silent            the script is silenced while the game runs on: must be ended at the limit"""
import ctypes, os, subprocess, sys, time

CACHE = r'D:\MashupTest\cache-fresh'
k32, nt = ctypes.windll.kernel32, ctypes.windll.ntdll
out = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq Mordhau-Win64-Shipping.exe', '/FO', 'CSV', '/NH'], capture_output=True, text=True).stdout
pid = int(out.split(',')[1].strip('"'))
handle = k32.OpenProcess(0x00100000 | 0x0800 | 0x1000, False, pid)   # SYNCHRONIZE | SUSPEND_RESUME | QUERY_LIMITED_INFORMATION
assert handle, 'could not open the process'
alive = lambda: k32.WaitForSingleObject(handle, 0) != 0
if sys.argv[1] == 'spike':
    seconds = float(sys.argv[2])
    nt.NtSuspendProcess(handle)
    time.sleep(seconds)
    nt.NtResumeProcess(handle)
    time.sleep(8)
    print(f'frozen for {seconds:.0f} s, then let go: process {pid} {"still running" if alive() else "WAS ENDED"}')
else:
    tmp = os.path.join(CACHE, 'live_cmd.tmp')
    open(tmp, 'w').write('PS_stopped = true return "silenced"\n')
    t0 = time.perf_counter()
    os.replace(tmp, os.path.join(CACHE, 'live_cmd.lua'))
    while alive() and time.perf_counter() - t0 < 60:
        time.sleep(0.05)
    print(f'script silenced, game running on: process {pid} ' + (f'ended after {time.perf_counter() - t0:.1f} s' if not alive() else 'STILL RUNNING after 60 s'))
