"""Dev: the header of a Skyrim SE model file (.nif) and the start of each block, to compare a file this mod writes
with one of Skyrim's own. usage: nif_look.py <file.nif> [bytes of each block to show, default 96]"""
import struct, sys

d = open(sys.argv[1], 'rb').read()
show = int(sys.argv[2]) if len(sys.argv) > 2 else 96
o = d.index(b'\n') + 1
print(repr(d[:o]))
version, endian, user, blocks = struct.unpack_from('<IBII', d, o); o += 13
stream, = struct.unpack_from('<I', d, o); o += 4
print(f'version {version:#x}, endian {endian}, user {user}, blocks {blocks}, stream {stream}')
for label in ('author', 'process script', 'export script'):
    n = d[o]; print(f'  {label}: {d[o + 1:o + 1 + n]!r}'); o += 1 + n
kinds_n, = struct.unpack_from('<H', d, o); o += 2
kinds = []
for _ in range(kinds_n):
    n, = struct.unpack_from('<I', d, o); o += 4
    kinds.append(d[o:o + n].decode()); o += n
kind_of = struct.unpack_from('<%dH' % blocks, d, o); o += 2 * blocks
sizes = struct.unpack_from('<%dI' % blocks, d, o); o += 4 * blocks
strings_n, longest = struct.unpack_from('<II', d, o); o += 8
strings = []
for _ in range(strings_n):
    n, = struct.unpack_from('<I', d, o); o += 4
    strings.append(d[o:o + n].decode(errors='replace')); o += n
groups, = struct.unpack_from('<I', d, o); o += 4
print('kinds', kinds); print('strings', strings[:12], 'longest', longest, 'groups', groups)
print('data starts at', o, 'file is', len(d), 'blocks sum to', sum(sizes), '-> ends at', o + sum(sizes))
for i in range(blocks):
    print(f'[{i}] {kinds[kind_of[i]]} {sizes[i]} bytes: {d[o:o + min(show, sizes[i])].hex(" ", 4)}')
    o += sizes[i]
