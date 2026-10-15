import struct, sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
EXE = r'E:\steam object\steamapps\common\Mordhau\Mordhau\Binaries\Win64\Mordhau-Win64-Shipping.exe'
d = open(EXE, 'rb').read()
pe = struct.unpack_from('<I', d, 0x3C)[0]
nsec = struct.unpack_from('<H', d, pe + 6)[0]
opt = struct.unpack_from('<H', d, pe + 20)[0]
secs = [struct.unpack_from('<8sIIII', d, pe + 24 + opt + 40 * i) for i in range(nsec)]
def off(rva):
    for n, vs, va, rs, ro in secs:
        if va <= rva < va + max(vs, rs): return ro + rva - va
rva = int(sys.argv[1], 16); n = int(sys.argv[2])
for i in Cs(CS_ARCH_X86, CS_MODE_64).disasm(d[off(rva):off(rva) + n], 0x140000000 + rva):
    print(f'{i.address - 0x140000000:08X}  {i.mnemonic} {i.op_str}')
