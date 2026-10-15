"""Dev: one gallery picture per MORDHAU weapon as it shows in Skyrim's inventory, from the row pictures taken by
inventory_walk_all.sh on 2026-10-12 (shots/inv_all/NN.png): release/gallery/weapons/NN-<name>.jpg.
usage: inventory_gallery.py"""
import os, shutil
from PIL import Image
rows = [(0, 'Arming Sword'), (1, 'Axe'), (2, 'Bardiche'), (3, 'Bastard Sword'), (4, 'Battle Axe'), (5, 'Billhook'), (6, 'Estoc'), (7, 'Eveningstar'), (8, 'Executioners Sword'),
        (9, 'Falchion'), (10, 'Falx'), (11, 'Flanged Mace'), (12, 'Halberd'), (13, 'Hand Axe'), (25, 'Longsword'), (26, 'Maul'), (27, 'Messer'), (28, 'Polehammer'), (35, 'Rapier'),
        (36, 'Rondel Dagger'), (37, 'Short Sword'), (38, 'Spear'), (44, 'War Axe'), (45, 'Warhammer'), (46, 'Zweihander')]
out = 'release/gallery/weapons'
shutil.rmtree(out, ignore_errors=True)
os.makedirs(out)
for k, (r, name) in enumerate(rows, 1):
    Image.open('shots/inv_all/%02d.png' % r).convert('RGB').crop((1250, 150, 2450, 1100)).save('%s/%02d-%s.jpg' % (out, k, name.replace(' ', '-')), quality=93)
print(len(os.listdir(out)), 'pictures,', sum(os.path.getsize(os.path.join(out, f)) for f in os.listdir(out)) // 1024, 'KB')
