"""Dev: how level is the ground MORDHAU gives a fight on each map? Run with the D: test copy in its duel scene
(tools/d_test.sh start "autotest=2;duel=1;god=1"). For each map named: MORDHAU is restarted into it (the plugin starts
it again by itself), and once a stand-in is free its feet height against the player's and its speed are read 40
times. A picture of MORDHAU's own window is saved to shots/maps/<map>.png.
usage: map_test.py <map> [<map> ...]"""
import subprocess, sys, time, re, os, statistics

CACHE = r'D:\MashupTest\cache-fresh'
MOD_SCRIPT = r'D:\MashupTest\Skyrim Special Edition\Data\SKSE\Plugins\MordhauSkyrim\mordhau\Mods\PoseStream\Scripts\main.lua'
LOG = os.path.expandvars(r'%USERPROFILE%\Documents\My Games\Skyrim Special Edition\SKSE\MordhauSkyrim.log')
UELOG = r'D:\MashupTest\Mordhau\Mordhau\Binaries\Win64\UE4SS.log'
SAMPLE = ('local out = "none" local pw = PS_pc and PS_pc.Pawn if pw and pw:IsValid() then local p = pw:K2_GetActorLocation() '
          'out = string.format("nobot player at (%.0f %.0f %.0f)", p.X, p.Y, p.Z) '
          'for _, b in ipairs(PS_get_bots()) do if b.free and b.obj:IsValid() then local c = b.obj local l = c:K2_GetActorLocation() '
          'local v = c:GetVelocity() out = string.format("feet %.1f speed %.0f dist %.0f mode %d", l.Z - p.Z, math.sqrt(v.X*v.X+v.Y*v.Y), '
          'math.sqrt((l.X-p.X)^2+(l.Y-p.Y)^2), c.CharacterMovement.MovementMode) end end end return out')

def lua(snippet, wait=6):
    try:
        return subprocess.run(['bash', 'tools/d_lua.sh', snippet], capture_output=True, text=True, timeout=wait).stdout.strip()
    except subprocess.TimeoutExpired:
        return ''

def count(path, text):
    try:
        return open(path, encoding='utf-8', errors='ignore').read().count(text)
    except OSError:
        return 0

os.makedirs('shots/maps', exist_ok=True)
subprocess.run(['cp', 'mordhau_side/PoseStream/Scripts/main.lua', MOD_SCRIPT.replace('\\', '/').replace('D:', '/d')])
for name in sys.argv[1:]:
    open(os.path.join(CACHE, 'map.txt'), 'w').write(name + '\n')
    lives = count(LOG, 'session: MORDHAU is live')
    subprocess.run(['taskkill', '/F', '/IM', 'Mordhau-Win64-Shipping.exe'], capture_output=True)
    started = time.time()
    while count(LOG, 'session: MORDHAU is live') == lives and time.time() - started < 150:
        time.sleep(3)
    state, rows = 'MORDHAU did not go live', []
    def focus():   # (Skyrim stands still while it is not the window in front)
        subprocess.run(['bash', 'tools/skyrim_keys.sh', 'wait 300'], capture_output=True)
    focus()
    if count(LOG, 'session: MORDHAU is live') > lives:
        state = 'live, but no free stand-in within 80 s'
        t0 = time.time()
        while time.time() - t0 < int(os.environ.get("T", "80")) and len(rows) < int(os.environ.get("N", "40")):
            answer = lua(SAMPLE)
            m = re.search(r'feet (-?[\d.]+) speed (\d+) dist (\d+) mode (\d+)', answer)
            if m:
                rows.append(tuple(float(x) for x in m.groups()))
            elif 'nobot' in answer:
                state = 'live, but no free stand-in within 80 s (' + answer[answer.find('player at'):] + ')'
            time.sleep(0.4)
            if int(time.time() - t0) % 10 == 0:
                focus()
    subprocess.run(f'cd _deps/universal-modder && python -m um win shot --exe Mordhau-Win64-Shipping.exe E:/MordhauSkyrim/shots/maps/{name}.png --scale 0.35',
                   shell=True, capture_output=True)
    last = [l for l in open(UELOG, encoding='utf-8', errors='ignore').read().splitlines() if 'PoseStream' in l][-1][36:150]
    if len(rows) >= 10:
        feet = [r[0] for r in rows]; speed = [r[1] for r in rows]
        print(f'{name:22s} feet vs player: {min(feet):6.1f} .. {max(feet):6.1f} cm (spread {max(feet) - min(feet):5.1f}, std dev {statistics.pstdev(feet):4.1f}); '
              f'moving in {sum(s > 20 for s in speed) * 100 // len(speed)}% of samples, top speed {max(speed):.0f}; mode {rows[-1][3]:.0f}; {len(rows)} samples', flush=True)
    else:
        print(f'{name:22s} {state} | last script line: {last}', flush=True)
