"""Dev: the user's own install keeps the project's cache folder (see tools/install_own.sh)."""
p = r'E:\steam object\steamapps\common\Skyrim Special Edition\Data\SKSE\Plugins\MordhauSkyrim.ini'
s = open(p).read()
s = s.replace('CacheFolder=\n', 'CacheFolder=E:\\MordhauSkyrim\\cache\n')
open(p, 'w').write(s)
