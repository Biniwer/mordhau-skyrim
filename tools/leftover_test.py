"""Dev: tries to reach a state in which the mod's script is alive in a MORDHAU that the mod did NOT start (D: test
copy) - the state from which an online server could be joined with it loaded. Nothing is ever connected to: each case
only starts the game, watches how long it lives and what the script host and the script did, and ends it.
Cases: the mod's files as a session leaves them; and the script host loaded by ANOTHER loader (UE4SS's own proxy, as
someone has who uses UE4SS for other MORDHAU mods), with no key, with -insecure, with a made-up key, with the last
session's real key (now stale).
usage: leftover_test.py"""
import os, shutil, subprocess, time

BIN = r'D:\MashupTest\Mordhau\Mordhau\Binaries\Win64'
EXE = os.path.join(BIN, 'Mordhau-Win64-Shipping.exe')
STOCK = r'E:\MordhauSkyrim\_deps\ue4ss\dwmapi.dll'
LOG = os.path.join(BIN, 'UE4SS.log')
SESSION = r'D:\MashupTest\cache-fresh\live_session.txt'
real = ''
if os.path.exists(SESSION):
    parts = open(SESSION).read().split()
    real = max(parts, key=len) if parts else ''
    age = (time.time() - os.path.getmtime(SESSION)) / 60


def clean():
    for name in ('dwmapi.dll', 'MordhauSkyrim-closed-MORDHAU.txt'):
        try:
            os.remove(os.path.join(BIN, name))
        except OSError:
            pass


cases = [("mod's files as left after a session (renamed loader only)", None, [], None, 35),
         ('other loader, normal start, no key', STOCK, [], None, 150),
         ('other loader, -insecure, no key', STOCK, ['-insecure'], None, 150),
         ('other loader, a made-up key', STOCK, [], 'f' * 32, 150),
         (f"other loader, the last session's real key ({age:.0f} min old)", STOCK, [], real, int(os.environ.get('LEFTOVER_KNOWN_SECONDS', '150')))]   # (the known, accepted case: tools/safety_tests.sh checks it for 30 s)
for name, loader, extra, key, limit in cases:
    clean()
    if loader:
        shutil.copy(loader, os.path.join(BIN, 'dwmapi.dll'))
    size = os.path.getsize(LOG) if os.path.exists(LOG) else 0
    stamp = os.path.getmtime(LOG) if os.path.exists(LOG) else 0
    env = dict(os.environ, SteamAppId='629760', SteamGameId='629760')
    env.pop('MORDHAU_SKYRIM_SESSION', None)
    if key:
        env['MORDHAU_SKYRIM_SESSION'] = key
    t0 = time.time()
    p = subprocess.Popen([EXE, '-windowed', '-ResX=1280', '-ResY=720'] + extra, cwd=BIN, env=env)
    try:
        code = p.wait(timeout=limit)
        lived = f'ended by itself after {time.time() - t0:.1f} s'
    except subprocess.TimeoutExpired:
        p.kill()
        lived = f'STILL RUNNING after {limit} s (ended by the test)'
    time.sleep(2)
    host = os.path.exists(LOG) and os.path.getmtime(LOG) > stamp
    lines = []
    if host:
        text = open(LOG, encoding='utf-8', errors='replace').read()
        text = text[size:] if len(text) > size else text
        lines = [l.split('[PoseStream] ', 1)[1][:150] for l in text.splitlines() if '[PoseStream]' in l]
    frames = sum(1 for l in lines if l.startswith(('alive:', 'the player', 'stand-in', 'linked', 'pose')))
    print(f'--- {name}: {lived}; script host loaded: {host}; lines from the mod\'s script: {len(lines)}, of them from a running session: {frames}')
    for l in lines[:3] + (['...'] if len(lines) > 6 else []) + lines[-3:] if len(lines) > 3 else lines:
        print('     ', l.encode('ascii', 'replace').decode())
clean()
print('test loader removed again:', not os.path.exists(os.path.join(BIN, 'dwmapi.dll')), '| MORDHAU running:',
      'Mordhau-Win64' in subprocess.run(['tasklist'], capture_output=True, text=True).stdout)
