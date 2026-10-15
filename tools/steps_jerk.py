"""Dev: how even a walked character's movement is in Skyrim, from %TEMP%/mordhauskyrim_steps.csv (measure=1).
The number that matters: by how much its speed changes from one Skyrim frame to the next, against its speed.
usage: steps_jerk.py [file]"""
import math, os, statistics, sys

path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.environ['TEMP'], 'mordhauskyrim_steps.csv')
rows = [[float(x) for x in l.split(',')] for l in open(path) if l.count(',') >= 10]
went = [(b[0] - a[0], math.hypot(b[6] - a[6], b[7] - a[7]), math.hypot(b[2] - a[2], b[3] - a[3])) for a, b in zip(rows, rows[1:])]
shown = [w[1] / w[0] for w in went if w[0] > 0.004]
told = [w[2] / w[0] for w in went if w[0] > 0.004]


def jerk(v):
    pairs = [(a, b) for a, b in zip(v, v[1:]) if (a + b) / 2 > 60 and (a + b) / 2 < 600]
    rel = sorted(abs(b - a) / ((a + b) / 2) for a, b in pairs)
    return len(pairs), 100 * rel[len(rel) // 2], 100 * rel[len(rel) * 9 // 10], statistics.mean((a + b) / 2 for a, b in pairs)


n, med, p90, speed = jerk(told)
print('%d frames, %.0f s' % (len(rows), rows[-1][0] - rows[0][0]))
print('as REPORTED by MORDHAU: %d moving frames at %.0f units/s; speed changes from frame to frame by %.0f%% (median), %.0f%% (worst tenth)' % (n, speed, med, p90))
n, med, p90, speed = jerk(shown)
print('as SHOWN in Skyrim:     %d moving frames at %.0f units/s; speed changes from frame to frame by %.0f%% (median), %.0f%% (worst tenth)' % (n, speed, med, p90))
back = [(b[6] - a[6], b[7] - a[7]) for a, b in zip(rows, rows[1:])]
pairs = [(p, q) for p, q in zip(back, back[1:]) if math.hypot(*p) > 0.3 and math.hypot(*q) > 0.3]
print('shown character stepping back against its own last step: %.0f%% of %d' % (100 * sum(1 for p, q in pairs if p[0] * q[0] + p[1] * q[1] < 0) / max(len(pairs), 1), len(pairs)))
