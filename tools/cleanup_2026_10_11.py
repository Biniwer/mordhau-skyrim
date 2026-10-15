"""Dev, one-off (play-test note, 2026-10-11): thin out E:/MordhauSkyrim.
- backups/*.zip: all but the eight named ones (the Saves-... and mordhau-config folders are left alone)
- recordings: the raw captures and the video-only / audio-only intermediates (.raw, .mkv, .m4a); .mp4 and .txt stay
- shots/live and shots/now: the two big folders of test screenshots
- the nameless ".mp4" in the project root
usage: cleanup_2026_10_11.py list | delete"""
import os, shutil, sys

root = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
assert os.path.normcase(root) == os.path.normcase(r'E:\MordhauSkyrim'), root
keep = {'before-cleanup', 'before-fresh-arms', 'eye-view-built', 'nexus-0.1.6', 'offline-stop-tested', 'height-fixes-rolled-back', 'late-night-fixes', 'combat-done'}
files, folders = [], []
backups = os.path.join(root, 'backups')
have = {n[:-4] for n in os.listdir(backups) if n.endswith('.zip')}
missing = keep - have
assert not missing, f'a backup to keep is not there: {missing}'
for n in sorted(os.listdir(backups)):
    if n.endswith('.zip') and n[:-4] not in keep:
        files.append(os.path.join(backups, n))
rec = os.path.join(root, 'recordings')
for n in sorted(os.listdir(rec)):
    if os.path.isfile(os.path.join(rec, n)) and n.lower().endswith(('.raw', '.mkv', '.m4a')):
        files.append(os.path.join(rec, n))
nameless = os.path.join(root, '.mp4')
if os.path.isfile(nameless):
    files.append(nameless)
for n in ('live', 'now'):
    d = os.path.join(root, 'shots', n)
    if os.path.isdir(d):
        folders.append(d)


def size(path):
    if os.path.isfile(path):
        return os.path.getsize(path)
    return sum(os.path.getsize(os.path.join(a, f)) for a, _, fs in os.walk(path) for f in fs)


total = sum(size(x) for x in files + folders)
groups = {}
for x in files + folders:
    groups.setdefault(os.path.relpath(os.path.dirname(x) if os.path.isfile(x) else x, root), []).append(x)
for g, items in groups.items():
    print(f'{g}: {len(items)} item(s), {sum(size(x) for x in items) / 2**30:.1f} GB')
print(f'in all {len(files)} files and {len(folders)} folders, {total / 2**30:.1f} GB; backups kept: {sorted(keep)}')
if len(sys.argv) > 1 and sys.argv[1] == 'delete':
    failed = []
    for x in files:
        try:
            os.remove(x)
        except OSError as e:
            failed.append((x, str(e)))
    for x in folders:
        shutil.rmtree(x, ignore_errors=False)
    print('deleted' if not failed else f'deleted, but {len(failed)} could not be removed: {failed[:5]}')
