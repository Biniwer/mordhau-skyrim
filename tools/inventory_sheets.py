"""Dev: overview sheets of the inventory pictures taken by inventory_walk_all.sh (shots/inv_all/NN.png): the 3D
preview and the item card of each row, 7 x 4 to a sheet. usage: inventory_sheets.py"""
import glob
from PIL import Image
fs = sorted(glob.glob('shots/inv_all/[0-9][0-9].png'))
cells = [Image.open(f).crop((1250, 150, 2450, 1100)).resize((300, 238)) for f in fs]
for k in range(0, len(cells), 28):
    part = cells[k:k + 28]
    sheet = Image.new('RGB', (7 * 300, 4 * 238))
    for j, c in enumerate(part):
        sheet.paste(c, ((j % 7) * 300, (j // 7) * 238))
    sheet.save('shots/inv_all/sheet%d.png' % (k // 28))
print(len(fs))
