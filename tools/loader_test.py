"""Dev: the mod's own loader against a MORDHAU that the mod did NOT start (D: test copy). For each case the loader is
put into the folder, the game is started, and it is timed how long the process lives and what is left behind.
usage: loader_test.py"""
import os, shutil, subprocess, time

BIN = r'D:\MashupTest\Mordhau\Mordhau\Binaries\Win64'
EXE = os.path.join(BIN, 'Mordhau-Win64-Shipping.exe')
LOADER = r'E:\MordhauSkyrim\build\loader\dwmapi.dll'
NOTE = os.path.join(BIN, 'MordhauSkyrim-closed-MORDHAU.txt')

def running():
    return 'Mordhau-Win64' in subprocess.run(['tasklist'], capture_output=True, text=True).stdout

def clean():
    for name in ('dwmapi.dll', 'dwmapi.dll.old', 'MordhauSkyrim-closed-MORDHAU.txt'):
        try:
            os.remove(os.path.join(BIN, name))
        except OSError:
            pass

cases = [('no -insecure, no key', [], None),
         ('-insecure, but no key', ['-insecure'], None),
         ('key, but no -insecure', [], '0123456789abcdef0123456789abcdef')]
for name, extra, key in cases:
    clean()
    shutil.copy(LOADER, os.path.join(BIN, 'dwmapi.dll'))
    log = os.path.join(BIN, 'UE4SS.log')
    before = os.path.getmtime(log) if os.path.exists(log) else 0
    env = dict(os.environ, SteamAppId='629760', SteamGameId='629760')
    env.pop('MORDHAU_SKYRIM_SESSION', None)
    if key:
        env['MORDHAU_SKYRIM_SESSION'] = key
    t0 = time.time()
    p = subprocess.Popen([EXE, '-windowed', '-ResX=1280', '-ResY=720'] + extra, cwd=BIN, env=env)
    try:
        code = p.wait(timeout=60)
        lived = time.time() - t0
    except subprocess.TimeoutExpired:
        code, lived = 'STILL RUNNING after 60 s', 60
        p.kill()
    time.sleep(1)
    note = open(NOTE).read().splitlines()[1] if os.path.exists(NOTE) else '(no note)'
    print(f'{name:24s} process lived {lived:5.2f} s (exit {code}); loadable loader left: {os.path.exists(os.path.join(BIN, "dwmapi.dll"))}; '
          f'renamed copy: {os.path.exists(os.path.join(BIN, "dwmapi.dll.old"))}; script host started: {os.path.exists(log) and os.path.getmtime(log) > before}; {note}')
clean()
print('folder clean again:', not any(os.path.exists(os.path.join(BIN, n)) for n in ('dwmapi.dll', 'dwmapi.dll.old')), '| MORDHAU running:', running())
