"""Decode a cooked MORDHAU UAnimSequence (UE4 per-track compression) to per-bone keys. Read-only.

decode(base) -> dict(num_frames, length, tracks=[{bone_index, trans:[(x,y,z)], rot:[(x,y,z,w)]}])
Formats per UE4 AnimationCompression: 1 Float96NoW, 2 Fixed48NoW, 3 IntervalFixed32NoW, 4 Fixed32NoW,
5 Float32NoW, 6 Identity. Header int32: format<<28 | flags<<24 | numKeys. flags bit0..2 = X,Y,Z present,
bit3 = key-time table follows.
"""
import struct, math, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import uasset

def _w(x, y, z):
    return math.sqrt(max(0.0, 1.0 - x*x - y*y - z*z))

def _rot(b, o, num_frames):
    hdr, = struct.unpack_from('<i', b, o); o += 4
    fmt, flags, n = (hdr >> 28) & 0xF, (hdr >> 24) & 0xF, hdr & 0xFFFFFF
    keys = []
    if fmt == 1:    # Float96NoW
        comp = flags & 7 or 7
        for _ in range(n):
            v = [0.0, 0.0, 0.0]
            for c in range(3):
                if comp & (1 << c): v[c], = struct.unpack_from('<f', b, o); o += 4
            keys.append((v[0], v[1], v[2], _w(*v)))
    elif fmt == 2:  # Fixed48NoW
        comp = flags & 7 or 7
        for _ in range(n):
            v = [0.0, 0.0, 0.0]
            for c in range(3):
                if comp & (1 << c):
                    u, = struct.unpack_from('<H', b, o); o += 2
                    v[c] = (u - 32767) / 32767.0
            keys.append((v[0], v[1], v[2], _w(*v)))
    elif fmt == 3:  # IntervalFixed32NoW
        mins, rng = [0.0]*3, [0.0]*3
        for c in range(3):
            if flags & (1 << c): mins[c], rng[c] = struct.unpack_from('<ff', b, o); o += 8
        for _ in range(n):
            u, = struct.unpack_from('<I', b, o); o += 4
            x = ((u >> 21) - 1023) / 1023.0 * rng[0] + mins[0]
            y = (((u >> 10) & 0x7FF) - 1023) / 1023.0 * rng[1] + mins[1]
            z = ((u & 0x3FF) - 511) / 511.0 * rng[2] + mins[2]
            keys.append((x, y, z, _w(x, y, z)))
    elif fmt in (4, 5):  # Fixed32NoW / Float32NoW (packed 11/11/10)
        for _ in range(n):
            u, = struct.unpack_from('<I', b, o); o += 4
            x = ((u >> 21) - 1023) / 1023.0; y = (((u >> 10) & 0x7FF) - 1023) / 1023.0; z = ((u & 0x3FF) - 511) / 511.0
            keys.append((x, y, z, _w(x, y, z)))
    elif fmt == 6:
        keys = [(0.0, 0.0, 0.0, 1.0)]
    else:
        raise ValueError(f'rot format {fmt}')
    return fmt, flags, keys, _times(b, o, flags, n, num_frames)

def _vec(b, o, num_frames):
    hdr, = struct.unpack_from('<i', b, o); o += 4
    fmt, flags, n = (hdr >> 28) & 0xF, (hdr >> 24) & 0xF, hdr & 0xFFFFFF
    keys = []
    if fmt == 1:    # Float96NoW: floats for flagged components
        comp = flags & 7 or 7
        for _ in range(n):
            v = [0.0, 0.0, 0.0]
            for c in range(3):
                if comp & (1 << c): v[c], = struct.unpack_from('<f', b, o); o += 4
            keys.append(tuple(v))
    elif fmt == 2:  # Fixed48NoW: uint16 per flagged component, +-128 range
        comp = flags & 7 or 7
        for _ in range(n):
            v = [0.0, 0.0, 0.0]
            for c in range(3):
                if comp & (1 << c):
                    u, = struct.unpack_from('<H', b, o); o += 2
                    v[c] = (u - 32767) / 32767.0 * 128.0
            keys.append(tuple(v))
    elif fmt == 3:  # IntervalFixed32NoW: 10/11/11 (x low)
        mins, rng = [0.0]*3, [0.0]*3
        for c in range(3):
            if flags & (1 << c): mins[c], rng[c] = struct.unpack_from('<ff', b, o); o += 8
        for _ in range(n):
            u, = struct.unpack_from('<I', b, o); o += 4
            z = ((u >> 21) - 1023) / 1023.0 * rng[2] + mins[2]
            y = (((u >> 10) & 0x7FF) - 1023) / 1023.0 * rng[1] + mins[1]
            x = ((u & 0x3FF) - 511) / 511.0 * rng[0] + mins[0]
            keys.append((x, y, z))
    elif fmt == 6:
        keys = [(0.0, 0.0, 0.0)]
    else:
        raise ValueError(f'vec format {fmt}')
    return fmt, flags, keys, _times(b, o, flags, n, num_frames)

def _times(b, o, flags, n, num_frames):
    if not (flags & 8) or n <= 1:
        return None
    o = (o + 3) & ~3
    if num_frames > 0xFF:
        return list(struct.unpack_from(f'<{n}H', b, o))
    return list(b[o:o+n])

def decode(base):
    p = uasset.Package(base)
    props, o = p.props(); e = p.e
    num_frames = struct.unpack('<i', props['NumFrames'][1])[0]
    length = struct.unpack('<f', props['SequenceLength'][1])[0]
    o += 4 + 16 + 2
    bser, rawsize, ntrk = struct.unpack_from('<iii', e, o); o += 12
    assert bser == 1
    bone_idx = struct.unpack_from(f'<{ntrk}i', e, o); o += 4*ntrk
    ncurves, = struct.unpack_from('<i', e, o); o += 4 + 8*ncurves
    nbytes, bulk = struct.unpack_from('<ii', e, o); o += 8
    assert bulk == 0
    stream = e[o:o+nbytes]; o += nbytes
    codec, o = uasset.fstr(e, o); curve_codec, o = uasset.fstr(e, o)
    ncb, = struct.unpack_from('<i', e, o); o += 4 + ncb
    tail = e[o:]
    # FUECompressedAnimData: numFrames, 4 format bytes, then view sizes
    cframes, = struct.unpack_from('<i', tail, 0)
    keyfmt, tfmt, rfmt, sfmt = tail[4:8]
    n_stream, n_off, n_scale, strip = struct.unpack_from('<iiii', tail, 8)
    assert n_off == 2*ntrk, (n_off, ntrk, tail[:40].hex())
    offs = struct.unpack_from(f'<{n_off}i', stream, 0)
    data = stream[4*n_off + 4*n_scale:]
    assert len(data) == n_stream, (len(data), n_stream)
    tracks = []
    for t in range(ntrk):
        to, ro = offs[2*t], offs[2*t+1]
        tr = _vec(data, to, cframes) if to != -1 else None
        rt = _rot(data, ro, cframes) if ro != -1 else None
        tracks.append({'bone_index': bone_idx[t], 'trans': tr, 'rot': rt})
    return {'num_frames': cframes, 'length': length, 'codec': codec, 'key_format': keyfmt, 'tracks': tracks,
            'tail_left': len(tail) - 24}

if __name__ == '__main__':
    a = decode(sys.argv[1])
    print('frames', a['num_frames'], 'length', a['length'], 'codec', a['codec'], 'keyfmt', a['key_format'], 'tail_left', a['tail_left'])
    bad = 0; fm = {}
    for t in a['tracks']:
        if t['rot']:
            f, fl, keys, times = t['rot']; fm[('rot', f)] = fm.get(('rot', f), 0) + 1
            for q in keys:
                n2 = sum(c*c for c in q)
                if abs(n2 - 1) > 1e-3 or any(abs(c) > 1.0001 for c in q): bad += 1
        if t['trans']:
            f, fl, keys, times = t['trans']; fm[('trans', f)] = fm.get(('trans', f), 0) + 1
            if any(abs(c) > 500 for k in keys for c in k): bad += 1
    print('formats', fm, 'bad keys', bad)
    for i in (1, 2, 40, 41, 42):
        t = a['tracks'][i]; f, fl, keys, times = t['rot']
        print('track', i, 'bone', t['bone_index'], 'rot fmt', f, 'flags', fl, 'keys', len(keys), 'first', [round(c, 3) for c in keys[0]], 'last', [round(c, 3) for c in keys[-1]], 'times', (times[:6] if times else None))
