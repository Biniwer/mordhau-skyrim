"""Dev: by how much the sound of a recording is late against its picture. Sudden changes in the picture (hits, parries)
are matched against sudden rises in loudness; prints the shift (s) that lines them up best, and the runners-up.
Positive = the sound comes that much AFTER the picture. usage: av_offset.py <video> [start s] [length s]"""
import subprocess, sys, os
import numpy as np

ff = os.path.expandvars(r'%LOCALAPPDATA%\universal-modder\ffmpeg\bin\ffmpeg.exe')
src = sys.argv[1]
start = sys.argv[2] if len(sys.argv) > 2 else '0'
length = sys.argv[3] if len(sys.argv) > 3 else '70'
v = subprocess.run([ff, '-loglevel', 'error', '-ss', start, '-t', length, '-i', src, '-vf', 'fps=60,scale=64:36,format=gray', '-f', 'rawvideo', '-'], capture_output=True).stdout
a = subprocess.run([ff, '-loglevel', 'error', '-ss', start, '-t', length, '-i', src, '-vn', '-ac', '1', '-ar', '6000', '-f', 's16le', '-'], capture_output=True).stdout
frames = np.frombuffer(v, np.uint8).reshape(-1, 36 * 64).astype(np.float32)
motion = np.abs(np.diff(frames, axis=0)).mean(axis=1)
sound = np.abs(np.frombuffer(a, np.int16).astype(np.float32))
n = min(len(motion), len(sound) // 100)
loud = sound[:n * 100].reshape(n, 100).mean(axis=1)


def onsets(x):
    d = np.diff(x, prepend=x[0])
    d[d < 0] = 0
    return (d - d.mean()) / (d.std() + 1e-9)


m, s = onsets(motion[:n]), onsets(loud)
best = []
for lag in range(-90, 91):      # frames; +lag = the sound is later
    if lag >= 0:
        c = float(np.dot(m[:n - lag], s[lag:])) / (n - lag)
    else:
        c = float(np.dot(m[-lag:], s[:n + lag])) / (n + lag)
    best.append((c, lag))
best.sort(reverse=True)
print('frames', n, '| best shifts (s, score):', [(round(l / 60, 3), round(c, 3)) for c, l in best[:5]])
