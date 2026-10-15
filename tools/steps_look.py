"""Dev: looks at the per-frame record of a walked character (measure=1 writes %TEMP%/mordhauskyrim_steps.csv:
time, id, told x, told y, aimed x, aimed y, here x, here y, player x, player y, yaw).
usage: steps_look.py"""
import os, statistics, math

rows = [[float(x) for x in l.split(',')] for l in open(os.path.join(os.environ['TEMP'], 'mordhauskyrim_steps.csv')) if l.count(',') == 10]
print(len(rows), 'frames,', round(rows[-1][0] - rows[0][0], 1), 's')
dts = [b[0] - a[0] for a, b in zip(rows, rows[1:])]
print('Skyrim frame time ms: median %.1f, 95%% %.1f, worst %.1f' % (1000 * statistics.median(dts), 1000 * sorted(dts)[int(len(dts) * 0.95)], 1000 * max(dts)))
# how the stand-in's told place arrives
changed = [abs(b[2] - a[2]) > 0.01 or abs(b[3] - a[3]) > 0.01 for a, b in zip(rows, rows[1:])]
moving = [math.hypot(b[2] - a[2], b[3] - a[3]) for a, b in zip(rows, rows[1:])]
print('frames in which the told place changed: %d of %d (%.0f%%)' % (sum(changed), len(changed), 100 * sum(changed) / len(changed)))
runs, n = [], 0
for c in changed:
    if c:
        runs.append(n)
        n = 0
    else:
        n += 1
print('frames with nothing new between two reports:', {k: runs.count(k) for k in sorted(set(runs))})
# the told place: per-report jump sizes and whether it goes back and forth
jumps = [m for m, c in zip(moving, changed) if c]
print('size of a change in the told place (units): median %.2f, 90%% %.2f, worst %.1f' % (statistics.median(jumps), sorted(jumps)[int(len(jumps) * 0.9)], max(jumps)))
# reversals: the told place stepping against its own previous step
steps = [(b[2] - a[2], b[3] - a[3]) for a, b, c in zip(rows, rows[1:], changed) if c]
back = sum(1 for p, q in zip(steps, steps[1:]) if p[0] * q[0] + p[1] * q[1] < 0 and math.hypot(*p) > 0.3 and math.hypot(*q) > 0.3)
print('told place stepping BACK against its last step: %d of %d reports (%.0f%%)' % (back, len(steps), 100.0 * back / max(len(steps), 1)))
# the same for where the character actually went
went = [(b[6] - a[6], b[7] - a[7]) for a, b in zip(rows, rows[1:])]
wback = sum(1 for p, q in zip(went, went[1:]) if p[0] * q[0] + p[1] * q[1] < 0 and math.hypot(*p) > 0.3 and math.hypot(*q) > 0.3)
print('character stepping BACK against its last step: %d of %d frames (%.0f%%)' % (wback, len(went), 100.0 * wback / len(went)))
# how far the character lags the told place, and the told place against the PLAYER (is it the player's own movement?)
lag = [math.hypot(r[2] - r[6], r[3] - r[7]) for r in rows]
print('distance character - told place (units): median %.1f, 90%% %.1f' % (statistics.median(lag), sorted(lag)[int(len(lag) * 0.9)]))
pm = [math.hypot(b[8] - a[8], b[9] - a[9]) for a, b in zip(rows, rows[1:])]
print('player moved in %d of %d frames' % (sum(1 for x in pm if x > 0.05), len(pm)))
# frame-to-frame change of the character's speed (the jerk one sees), against its speed
sp = [math.hypot(*w) / max(d, 1e-4) for w, d in zip(went, dts)]
jerk = [abs(b - a) for a, b in zip(sp, sp[1:])]
mv = [(j, s) for j, s in zip(jerk, sp[1:]) if s > 40]
print('while moving: speed median %.0f units/s; change of speed from one frame to the next: median %.0f, 90%% %.0f' % (statistics.median(s for _, s in mv), statistics.median(j for j, _ in mv), sorted(j for j, _ in mv)[int(len(mv) * 0.9)]))
yaw = [((b[10] - a[10] + 180) % 360) - 180 for a, b in zip(rows, rows[1:])]
print('heading change per frame (degrees): median %.2f, 90%% %.2f, worst %.1f' % (statistics.median(abs(y) for y in yaw), sorted(abs(y) for y in yaw)[int(len(yaw) * 0.9)], max(abs(y) for y in yaw)))
print('a stretch of 14 frames (ms since last frame | told step | went step, units):')
k = next((i for i in range(len(rows) - 15) if all(m > 0.2 or not c for m, c in zip(moving[i:i + 14], changed[i:i + 14])) and sum(changed[i:i + 14]) > 6), 100)
print('  ' + '  '.join('%2.0f|%4.1f|%4.1f' % (1000 * dts[i], moving[i], math.hypot(*went[i])) for i in range(k, k + 14)))
