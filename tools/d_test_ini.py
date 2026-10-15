"""Dev: points the D: test copy's MordhauSkyrim.ini at the D: MORDHAU and its own cache (see tools/d_test.sh)."""
p = r'D:\MashupTest\Skyrim Special Edition\Data\SKSE\Plugins\MordhauSkyrim.ini'
s = open(p).read()
s = s.replace('MordhauPath=\n', 'MordhauPath=D:\\MashupTest\\Mordhau\n').replace('CacheFolder=\n', 'CacheFolder=D:\\MashupTest\\cache-fresh\n')
open(p, 'w').write(s)
