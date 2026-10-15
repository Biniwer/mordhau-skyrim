"""Read MORDHAU's UMA_Master_Skeleton reference skeleton (names, parents, rest pose). Read-only."""
import struct, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import uasset

def qmul(a, b):  # Hamilton product, (x,y,z,w)
    ax, ay, az, aw = a; bx, by, bz, bw = b
    return (aw*bx + ax*bw + ay*bz - az*by, aw*by - ax*bz + ay*bw + az*bx, aw*bz + ax*by - ay*bx + az*bw, aw*bw - ax*bx - ay*by - az*bz)

def qrot(q, v):
    x, y, z, w = q; vx, vy, vz = v
    tx, ty, tz = 2*(y*vz - z*vy), 2*(z*vx - x*vz), 2*(x*vy - y*vx)
    return (vx + w*tx + y*tz - z*ty, vy + w*ty + z*tx - x*tz, vz + w*tz + x*ty - y*tx)

def qinv(q):
    return (-q[0], -q[1], -q[2], q[3])

def load():
    p = uasset.Package('UMA/UMA/Master/UMA_Master_Skeleton')
    ex = next(x for x in p.exports if x['name'] == 'UMA_Master_Skeleton')
    _, o = p.props(ex['offset']); e = p.e
    o += 4
    n, = struct.unpack_from('<i', e, o); o += 4
    names, parents = [], []
    for _ in range(n):
        nm, o = p.name(o); par, = struct.unpack_from('<i', e, o); o += 4
        names.append(nm); parents.append(par)
    m, = struct.unpack_from('<i', e, o); o += 4
    assert m == n, (m, n)
    rot, pos = [], []
    for _ in range(n):
        f = struct.unpack_from('<10f', e, o); o += 40
        rot.append(f[0:4]); pos.append(f[4:7])
    return names, parents, rot, pos

def component(parents, rot, pos):
    """Local -> component space. child = parent o local."""
    crot, cpos = [None]*len(parents), [None]*len(parents)
    for i, par in enumerate(parents):
        if par < 0:
            crot[i], cpos[i] = rot[i], pos[i]
        else:
            crot[i] = qmul(crot[par], rot[i])
            r = qrot(crot[par], pos[i])
            cpos[i] = tuple(cpos[par][k] + r[k] for k in range(3))
    return crot, cpos

if __name__ == '__main__':
    names, parents, rot, pos = load()
    crot, cpos = component(parents, rot, pos)
    print(len(names), 'bones')
    for b in ('Position', 'Hips', 'LowerBack', 'Spine', 'Spine1', 'Neck', 'Head', 'RightShoulder', 'RightArm', 'RightForeArm', 'RightHand', 'RightWeapon', 'LeftHand', 'LeftFoot', 'LeftToeBase', 'RightFoot'):
        i = names.index(b)
        print(f'{b:14s} idx {i:3d} parent {names[parents[i]] if parents[i] >= 0 else "-":14s} comp pos {tuple(round(c, 1) for c in cpos[i])}')
