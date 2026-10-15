"""Dev: the mid-session failure on the D: test copy (start a session first with tools/d_test.sh). The script is told
that its match is no longer a stand-alone local one (PS_testNotAlone); timed from outside: how long MORDHAU's process
really lives after that, with a handle on the process itself. usage: stop_test.py"""
import ctypes, os, subprocess, time

CACHE = r'D:\MashupTest\cache-fresh'
k32 = ctypes.windll.kernel32
out = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq Mordhau-Win64-Shipping.exe', '/FO', 'CSV', '/NH'], capture_output=True, text=True).stdout
pid = int(out.split(',')[1].strip('"'))
handle = k32.OpenProcess(0x00100000 | 0x1000, False, pid)   # SYNCHRONIZE | QUERY_LIMITED_INFORMATION
assert handle, 'could not open the process'
pose = os.path.join(CACHE, 'live_pose.bin')
def sequence():
    with open(pose, 'rb') as f:
        return f.read(8)[4:]
tmp = os.path.join(CACHE, 'live_cmd.tmp')
switch = __import__('sys').argv[1] if len(__import__('sys').argv) > 1 else 'PS_testNotAlone'   # or PS_testUnknown
open(tmp, 'w').write(switch + ' = true return "told"\n')
t0 = time.perf_counter()
os.replace(tmp, os.path.join(CACHE, 'live_cmd.lua'))
# when does the script stop writing poses, and when is the process gone?
last, changed_at, gone_at = sequence(), t0, None
while time.perf_counter() - t0 < 30:
    if k32.WaitForSingleObject(handle, 0) == 0:
        gone_at = time.perf_counter()
        break
    now = sequence()
    if now != last:
        last, changed_at = now, time.perf_counter()
    time.sleep(0.002)
print(f'process {pid}: last pose written {1000 * (changed_at - t0):.0f} ms after the snippet was handed over;',
      f'process gone after {1000 * (gone_at - t0):.0f} ms' if gone_at else 'process STILL RUNNING after 30 s')
print('stop file left:', os.path.exists(os.path.join(CACHE, 'live_stop.txt')))
