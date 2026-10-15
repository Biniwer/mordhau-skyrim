"""Dev: the height test on the D: test copy (start it first with tools/d_test.sh). The player stands still; twice a
second the stand-in's feet and head heights against the player's are read inside MORDHAU - a stretch with the body
shift on, then the same with it switched off live. usage: height_test.py [samples per stretch, default 60]"""
import subprocess, sys, time, re, statistics

N = int(sys.argv[1]) if len(sys.argv) > 1 else 60
SAMPLE = ('local out = "none" for _, b in ipairs(PS_get_bots()) do if b.free and b.obj:IsValid() then local c = b.obj '
          'local l = c:K2_GetActorLocation() local pw = PS_pc.Pawn local p = pw:K2_GetActorLocation() '
          'local hb = c.Mesh:GetSocketLocation(FName("Head")).Z local hp = pw.Mesh:GetSocketLocation(FName("Head")).Z '
          'local pb = c.Mesh:GetSocketLocation(FName("Pelvis")).Z local pp = pw.Mesh:GetSocketLocation(FName("Pelvis")).Z '
          'out = string.format("feet %.1f head %.1f pelvis %.1f", l.Z - p.Z, hb - hp, pb - pp) end end return out')
OFF = ('PS_body_height_on = PS_body_height PS_body_height = function(c, b) pcall(function() local h = c.BaseTranslationOffset '
       'if math.abs(h.Z + 97) > 0.1 then c.BaseTranslationOffset = { X = h.X, Y = h.Y, Z = -97 } end b.lift = nil end) end return "body shift OFF"')
ON = 'if PS_body_height_on then PS_body_height = PS_body_height_on end return "body shift ON"'

def lua(snippet):
    return subprocess.run(['bash', 'tools/d_lua.sh', snippet], capture_output=True, text=True).stdout.strip()

def stretch(name):
    rows = []
    while len(rows) < N:
        m = re.search(r'feet (-?[\d.]+) head (-?[\d.]+) pelvis (-?[\d.]+)', lua(SAMPLE))
        if m:
            rows.append(tuple(float(x) for x in m.groups()))
        time.sleep(0.25)
    feet, head, pelvis = zip(*rows)
    def line(label, v):
        return f'  {label:7s} lowest {min(v):6.1f}  highest {max(v):6.1f}  spread {max(v) - min(v):5.1f}  typical swing (std dev) {statistics.pstdev(v):5.1f} cm'
    print(f'{name}: {len(rows)} samples, against the player (cm)')
    print(line('feet', feet)); print(line('pelvis', pelvis)); print(line('head', head))
    return rows

print(lua(ON)); time.sleep(2)
stretch('FIX ON ')
print(lua(OFF)); time.sleep(2)
stretch('FIX OFF')
print(lua(ON))
