"""Read a cooked MORDHAU Texture2D (UE 4.25) from the player's own install. Read-only.

info(base) -> dict(width, height, format, mips [(w, h, bytes)]) with block-compressed data exactly as stored
(what a GPU takes directly). rgba(base, max_size) -> numpy (h, w, 4) uint8, decoding BC1/BC3/BC5 for offline pictures.
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import uasset


def _blob(path_suffix):
    return uasset.blob(path_suffix)


def info(base):
    p = uasset.Package(base)
    name = base.split('/')[-1]
    ex = next(x for x in p.exports if x['name'] == name)
    _, o = p.props(ex['offset'])
    e = p.e
    o += 4                                  # no object guid
    o += 2 + 2                              # strip flags: UTexture, UTexture2D
    cooked, = struct.unpack_from('<i', e, o); o += 4
    assert cooked == 1
    fmt_name, o = p.name(o)                 # pixel format name, then the platform data
    o += 8                                  # skip offset (int64)
    w, h, packed = struct.unpack_from('<iii', e, o); o += 12
    fmt, o = uasset.fstr(e, o)
    first_mip, nmips = struct.unpack_from('<ii', e, o); o += 8
    bulk = None
    mips = []
    for _ in range(nmips):
        o += 4                              # cooked
        flags, count, disk = struct.unpack_from('<Iii', e, o); o += 12
        offset, = struct.unpack_from('<q', e, o); o += 8
        if flags & 0x100:                   # payload in the separate .ubulk file
            if bulk is None:
                bulk = _blob(base + '.ubulk')
            data = bulk[offset:offset + disk]
        elif flags & 0x40 or not (flags & 0x1):  # inline
            data = e[o:o + disk]; o += disk
        else:                               # at the end of the file: offset counts from the start of .uasset
            start = offset - p.header_size
            data = e[start:start + disk]
        mw, mh, md = struct.unpack_from('<iii', e, o); o += 12
        mips.append((mw, mh, bytes(data)))
    return {'name': name, 'width': w, 'height': h, 'format': fmt, 'mips': mips}


def _bc1_block_colours(c0, c1, alpha_mode):
    def rgb(c):
        r = ((c >> 11) & 31) * 255 // 31; g = ((c >> 5) & 63) * 255 // 63; b = (c & 31) * 255 // 31
        return np.stack([r, g, b], axis=-1).astype(np.int32)
    a, b = rgb(c0), rgb(c1)
    four = (c0 > c1)[..., None] | alpha_mode
    c2 = np.where(four, (2*a + b) // 3, (a + b) // 2)
    c3 = np.where(four, (a + 2*b) // 3, 0)
    return np.stack([a, b, c2, c3], axis=-2)          # (..., 4 colours, 3)


def _decode_bc1(data, w, h, offset=0, stride=8, bc3=False):
    bw, bh = max(w // 4, 1), max(h // 4, 1)
    raw = np.frombuffer(data, dtype=np.uint8)[:bw*bh*stride].reshape(bh, bw, stride)
    blk = raw[..., offset:offset+8]
    c0 = blk[..., 0].astype(np.int32) | (blk[..., 1].astype(np.int32) << 8)
    c1 = blk[..., 2].astype(np.int32) | (blk[..., 3].astype(np.int32) << 8)
    pal = _bc1_block_colours(c0, c1, bc3)              # (bh, bw, 4, 3)
    bits = blk[..., 4].astype(np.uint32) | (blk[..., 5].astype(np.uint32) << 8) | (blk[..., 6].astype(np.uint32) << 16) | (blk[..., 7].astype(np.uint32) << 24)
    out = np.zeros((bh*4, bw*4, 3), dtype=np.uint8)
    for py in range(4):
        for px in range(4):
            idx = (bits >> (2 * (py*4 + px))) & 3
            out[py::4, px::4] = np.take_along_axis(pal, idx[..., None, None].astype(np.int64).repeat(3, axis=-1), axis=-2)[..., 0, :]
    return out


def rgba(base, max_size=1024):
    t = info(base)
    mw, mh, data = next((m for m in t['mips'] if max(m[0], m[1]) <= max_size and m[2]), t['mips'][-1])
    fmt = t['format']
    if fmt == 'PF_DXT1':
        rgb = _decode_bc1(data, mw, mh)
    elif fmt == 'PF_DXT5':
        rgb = _decode_bc1(data, mw, mh, offset=8, stride=16, bc3=True)
    elif fmt == 'PF_B8G8R8A8':
        px = np.frombuffer(data, dtype=np.uint8)[:mw*mh*4].reshape(mh, mw, 4)
        return np.ascontiguousarray(px[..., [2, 1, 0, 3]]), t
    else:
        raise ValueError(f'{t["name"]}: format {fmt} not decoded offline')
    out = np.concatenate([rgb[:mh, :mw], np.full((mh, mw, 1), 255, dtype=np.uint8)], axis=-1)
    return out, t


if __name__ == '__main__':
    for base in sys.argv[1:]:
        t = info(base)
        print(f"{t['name']}: {t['width']}x{t['height']} {t['format']}, {len(t['mips'])} mips, sizes {[(m[0], m[1], len(m[2])) for m in t['mips'][:4]]}")
