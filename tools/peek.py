"""Print summary of one cooked UE4 asset inside the paks. Usage: peek.py <PaksDir> <tsv> <name-substr>"""
import struct, sys, os, re
paks, tsv, want = sys.argv[1:4]
rows = [l.rstrip('\n').split('\t') for l in open(0 if tsv == '-' else tsv, encoding='utf-8')]
def blob(row):
    pak, path, usize, method, enc, off = row[0], row[1], int(row[2]), int(row[3]), int(row[4]), int(row[5])
    with open(os.path.join(paks, pak), 'rb') as f:
        f.seek(off)
        h = f.read(8+8+8+4+20)  # in-file entry header: offset, size, usize, method, hash
        csize, us, m = struct.unpack_from('<QQI', h, 8)
        assert m == 0, 'compressed entry'
        f.seek(off + 8+8+8+4+20 + 1 + 4)  # + encrypted flag + block size
        return f.read(us)
for r in rows:
    if want in r[1] and r[1].endswith('.uasset'):
        a = blob(r)
        e = blob(next(x for x in rows if x[1] == r[1][:-6] + 'uexp'))
        tag, legacy = struct.unpack_from('<Ii', a, 0)
        print(r[1], 'uasset', len(a), 'uexp', len(e), 'tag', hex(tag), 'legacyVer', legacy)
        print('ue4ver/licensee', struct.unpack_from('<iii', a, 8))
        strs = sorted(set(s.decode() for s in re.findall(rb'[A-Za-z_/][A-Za-z0-9_/\.\-]{3,}', a)))
        print(' '.join(strs))
        print('uexp head', e[:96].hex())
        estr = sorted(set(s.decode() for s in re.findall(rb'[A-Za-z_][A-Za-z0-9_]{4,}', e)))
        print('uexp strings', estr[:40])
        break
