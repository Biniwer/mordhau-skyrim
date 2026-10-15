"""Makes the pictures of MORDHAU's voice and emote menus as Skyrim shows them (sheets/menus.json -> cache/ui/<menu><page>.rgba:
int32 width, int32 height, then RGBA bytes)."""
import json, os, struct, sys
from PIL import Image, ImageDraw, ImageFont

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
cache = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, 'cache')
rows = json.load(open(os.path.join(root, 'sheets', 'menus.json'), encoding='utf-8'))['rows']
os.makedirs(os.path.join(cache, 'ui'), exist_ok=True)

def font(size, bold=False):
    for name in (['georgiab.ttf', 'timesbd.ttf'] if bold else ['georgia.ttf', 'times.ttf']):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()

titles = {'voice': 'VOICE', 'emote': 'EMOTES'}
keys = {'voice': 'C', 'emote': 'X'}
for menu in ('voice', 'emote'):
    pages = max(r['page'] for r in rows if r['menu'] == menu)
    for page in range(1, pages + 1):
        mine = sorted((r for r in rows if r['menu'] == menu and r['page'] == page), key=lambda r: r['key'])
        W, line, top = 420, 44, 70
        H = top + line * 9 + 56
        im = Image.new('RGBA', (W, H), (0, 0, 0, 0))
        d = ImageDraw.Draw(im)
        d.rectangle([0, 0, W - 1, H - 1], fill=(12, 10, 8, 200), outline=(170, 140, 80, 230), width=2)
        d.text((24, 18), titles[menu], font=font(30, True), fill=(225, 195, 120, 255))
        d.text((W - 24, 26), f'{page} / {pages}', font=font(20), fill=(190, 180, 160, 255), anchor='ra')
        d.line([20, 60, W - 20, 60], fill=(170, 140, 80, 230), width=1)
        for r in mine:
            y = top + (r['key'] - 1) * line
            d.text((30, y), str(r['key']), font=font(28, True), fill=(225, 195, 120, 255))
            d.text((76, y), r['label'], font=font(28), fill=(235, 230, 220, 255))
        foot = (f'{keys[menu]}  next page' if pages > 1 else f'{keys[menu]}  close')
        d.text((24, H - 40), foot, font=font(20), fill=(190, 180, 160, 255))
        with open(os.path.join(cache, 'ui', f'{menu}{page}.rgba'), 'wb') as f:
            f.write(struct.pack('<ii', W, H) + im.tobytes())
        im.save(os.path.join(cache, 'ui', f'{menu}{page}.png'))
print('menus written to', os.path.join(cache, 'ui'))
