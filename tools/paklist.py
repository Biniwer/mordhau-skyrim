"""List files in UE4 pak v11 archives (read-only). Usage: paklist.py <PaksDir> <out.tsv>"""
import struct, sys, glob, os

def fstr(b, o):
    n, = struct.unpack_from('<i', b, o); o += 4
    if n >= 0:
        return b[o:o+n-1].decode('utf-8', 'replace'), o+n
    n = -n
    return b[o:o+2*n-2].decode('utf-16le'), o+2*n

def read_pak(p):
    with open(p, 'rb') as f:
        f.seek(-400, 2); t = f.read()
        i = t.rfind(bytes.fromhex('e1126f5a'))
        ver, off, size = struct.unpack_from('<IQQ', t, i+4)
        methods = [t[k:k+32].split(b'\0')[0].decode() for k in range(i+44, len(t)-31, 32)]
        f.seek(off); b = f.read(size)
        mount, o = fstr(b, 0)
        n, seed, hasph = struct.unpack_from('<iQi', b, o); o += 16
        if hasph: o += 36
        hasfd, = struct.unpack_from('<i', b, o); o += 4
        fdoff, fdsize = struct.unpack_from('<qq', b, o); o += 36
        encsize, = struct.unpack_from('<i', b, o); o += 4
        enc = b[o:o+encsize]
        f.seek(fdoff); d = f.read(fdsize)
    out = []; o = 0
    nd, = struct.unpack_from('<i', d, o); o += 4
    for _ in range(nd):
        dn, o = fstr(d, o)
        nf, = struct.unpack_from('<i', d, o); o += 4
        for _ in range(nf):
            fn, o = fstr(d, o)
            eo, = struct.unpack_from('<i', d, o); o += 4
            if eo < 0: continue
            fl, = struct.unpack_from('<I', enc, eo); q = eo+4
            cbs = (fl & 0x3f) << 11
            if (fl & 0x3f) == 0x3f: cbs, = struct.unpack_from('<I', enc, q); q += 4
            offset, = struct.unpack_from('<I' if fl >> 31 else '<Q', enc, q); q += 4 if fl >> 31 else 8
            usize, = struct.unpack_from('<I' if (fl >> 30) & 1 else '<Q', enc, q)
            out.append((mount + dn + fn if dn != '/' else mount + fn, usize, (fl >> 23) & 0x3f, (fl >> 22) & 1, offset))
    return methods, out

if __name__ == '__main__':
    rows = []
    for p in sorted(glob.glob(os.path.join(sys.argv[1], '*.pak'))):
        m, es = read_pak(p)
        print(os.path.basename(p), len(es), file=sys.stderr) if 0 else print('#', os.path.basename(p), len(es), 'files', 'methods', [x for x in m if x], 'encrypted', sum(e[3] for e in es))
        rows += [(os.path.basename(p),) + e for e in es]
    with (sys.stdout if sys.argv[2] == '-' else open(sys.argv[2], 'w', encoding='utf-8')) as f:
        for r in rows: f.write('\t'.join(map(str, r)) + '\n')
