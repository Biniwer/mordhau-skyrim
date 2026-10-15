"""Dev: are the files that keep the mod out of MORDHAU online play still what was released?
Compares the working copies with the copies inside a backup zip, byte for byte.
usage: isolation_check.py <backup name>     e.g. nexus-0.1.6"""
import hashlib, os, sys, zipfile

root = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
z = zipfile.ZipFile(os.path.join(root, 'backups', sys.argv[1] + '.zip'))
inside = {n.replace(chr(92), '/'): n for n in z.namelist()}
watch = ['plugin/src/Session.cpp', 'plugin/src/Session.h', 'loader/loader.cpp', 'loader/gen_exports.py', 'loader/exports.def', 'tools/build_loader.bat',
         'mordhau_side/PoseStream/Scripts/main.lua', 'sheets/live_link.json', 'build/loader/dwmapi.dll', 'plugin/src/main.cpp', 'plugin/src/Link.cpp', 'plugin/src/Link.h',
         'plugin/src/bake/Spec.cpp']
same = True
for name in watch:
    here = os.path.join(root, name)
    key = 'project/' + name
    if key not in inside:
        print(f'{name}: not in the backup (cannot compare)')
        continue
    if not os.path.exists(here):
        print(f'{name}: MISSING in the working copy')
        same = False
        continue
    a = hashlib.sha256(open(here, 'rb').read()).hexdigest()
    b = hashlib.sha256(z.read(inside[key])).hexdigest()
    print(f'{name}: {"identical" if a == b else "DIFFERENT"}')
    same = same and a == b
print('all identical' if same else 'NOT all identical')
