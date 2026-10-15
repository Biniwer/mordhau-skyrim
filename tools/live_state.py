"""Dev: print what MORDHAU is streaming right now (the state block of cache/live_pose.bin), optionally sending input.
usage: live_state.py [seconds] [forward right sprint] [Action ...]   e.g. live_state.py 3 1 0 0 RightStrike"""
import struct, os, sys, time
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'cache')
secs = float(sys.argv[1]) if len(sys.argv) > 1 else 1.0
fwd, right, sprint = (sys.argv[2:5] + ['0', '0', '0'])[:3] if len(sys.argv) > 2 else ('0', '0', '0')
actions = sys.argv[5:]
def state():
    for _ in range(20):
        try:
            L = open(os.path.join(root, 'live_pose.bin'), 'rb').read()
            m, seq, t, n = struct.unpack_from('<4sIfi', L, 0)
            o = 16 + n * 28
            if len(L) >= o + 224 + 4 and struct.unpack_from('<I', L, len(L) - 4)[0] == seq:
                v = struct.unpack_from('<6f2i5f48s', L, o)
                return seq, v
        except (OSError, struct.error):
            pass
        time.sleep(0.002)
base = int(time.time() * 10) % 100000
t0 = time.time(); k = 0; last = None; first_seq = None
while time.time() - t0 < secs:
    k += 1
    acts = ' '.join(f'{base + i + 1}:{a}' for i, a in enumerate(actions)) if time.time() - t0 > 0.3 else ''
    # (the shared input block, in place: sequence, length, the line, the sequence again)
    line = f'{base + k} {fwd} {right} {sprint} 0 0 0 0 {acts}\n'.encode()
    import struct
    block = struct.pack('<II', base + k, len(line)) + line + struct.pack('<I', base + k)
    path = os.path.join(root, 'live_input.mem')
    try:
        if not os.path.exists(path) or os.path.getsize(path) < 8192:
            open(path, 'ab').write(b'\0' * (8192 - (os.path.getsize(path) if os.path.exists(path) else 0)))
        with open(path, 'r+b') as f:
            f.write(block)
    except OSError: pass
    s = state()
    if s:
        seq, v = s
        first_seq = first_seq or seq
        key = (v[6], v[7], v[13].split(b'\0')[0])
        if key != last or k % 40 == 0:
            last = key
            print(f'{time.time()-t0:5.2f}s seq+{seq-first_seq:4d} stage {v[6]:2d} kind {v[7]} t {v[8]:5.2f} stamina {v[9]:3.0f} health {v[10]:3.0f} speed right {v[11]:5.0f} fwd {v[12]:5.0f} '
                  f'blade ({v[0]:.0f},{v[1]:.0f},{v[2]:.0f})-({v[3]:.0f},{v[4]:.0f},{v[5]:.0f}) {key[2].decode()}')
    time.sleep(0.016)
