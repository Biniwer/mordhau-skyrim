"""Dev: sets Skyrim's and MORDHAU's idea of how high a fought character stands side by side (measure=1 and
cache/measure.txt make both sides log an "hm" line twice a second, each with MORDHAU's game time).
head = the fought character's head against the player's head (cm, + = higher). What matters is the DIFFERENCE between
the two games: where MORDHAU has the head against where Skyrim shows it. 0 = a swing meets the body where it is seen.
usage: heights_compare.py <plugin log> <UE4SS.log> [label]"""
import re, statistics, sys

FIELD = re.compile(r'(\w+)=(-?[\d.]+)')


def lines(path):
    out = []
    for line in open(path, encoding='utf-8', errors='replace'):
        k = line.find(' hm gt=')
        if k < 0:
            k = line.find('] hm gt=')
        if 'hm gt=' in line:
            d = {a: float(b) for a, b in FIELD.findall(line[line.index('hm gt='):])}
            out.append(d)
    return out


sky, mor = lines(sys.argv[1]), lines(sys.argv[2])
label = sys.argv[3] if len(sys.argv) > 3 else ''
TOL = float(sys.argv[4]) if len(sys.argv) > 4 else 0.3   # how far apart in MORDHAU's game time two lines may be to count as a pair (s)
pairs = []
for s in sky:
    near = [m for m in mor if m['id'] == s['id'] and abs(m["gt"] - s["gt"]) < TOL]
    if near:
        m = min(near, key=lambda x: abs(x['gt'] - s['gt']))
        pairs.append((s, m))
print(f'{label}: {len(sky)} Skyrim lines, {len(mor)} MORDHAU lines, {len(pairs)} pairs')
if not pairs:
    sys.exit()


def show(name, values):
    v = sorted(values)
    print('  %-58s median %+6.1f   middle half %+6.1f .. %+6.1f   extremes %+6.1f .. %+6.1f' % (name, statistics.median(v), v[len(v) // 4], v[len(v) * 3 // 4], v[0], v[-1]))


show("Skyrim: enemy's head against the player's head", [s['head'] for s, m in pairs])
show("MORDHAU: stand-in's head against the player's head", [m['head'] for s, m in pairs])
show('MORDHAU minus Skyrim, HEAD (0 = judged where seen)', [m['head'] - s['head'] for s, m in pairs])
show('MORDHAU minus Skyrim, HIPS', [m['hips'] - s['hips'] for s, m in pairs])
show('MORDHAU minus Skyrim, FEET', [m['feet'] - s['feet'] for s, m in pairs])
show("player's head above his feet: Skyrim", [s['phead'] for s, m in pairs])
show("player's head above his feet: MORDHAU", [m['phead'] for s, m in pairs])
show("enemy's head above its feet: Skyrim", [s['ownhead'] for s, m in pairs])
show("stand-in's head above its feet: MORDHAU", [m['ownhead'] for s, m in pairs])
free = [(s, m) for s, m in pairs if m.get('free')]
held = [(s, m) for s, m in pairs if not m.get('free')]
for name, part in (('free (MORDHAU walks it)', free), ('held (Skyrim walks it)', held)):
    if part:
        show(f'head, MORDHAU minus Skyrim, {name}, {len(part)} pairs', [m['head'] - s['head'] for s, m in part])
off = [abs(m['head'] - s['head']) for s, m in pairs]
print('  head judged within 5 cm of where it is seen: %.0f%% of the time; within 10 cm: %.0f%%; more than 20 cm off: %.0f%%' % (
    100 * sum(1 for x in off if x <= 5) / len(off), 100 * sum(1 for x in off if x <= 10) / len(off), 100 * sum(1 for x in off if x > 20) / len(off)))
print('  scales: enemy %.2f, player %.2f' % (pairs[0][0]['scale'], pairs[0][0]['pscale']))
