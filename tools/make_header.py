import sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter, ImageEnhance
src, out = sys.argv[1], sys.argv[2]
x0, y0, cw = int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
W, H = 1300, 372
im = Image.open(src).convert('RGB')
# small HUD marks painted over with their blurred surroundings
for (ax, ay, bx, by) in [tuple(int(v) for v in a.split(',')) for a in sys.argv[6:]]:
    pad = 26
    region = im.crop((ax - pad, ay - pad, bx + pad, by + pad))
    soft = region.filter(ImageFilter.MedianFilter(21)).filter(ImageFilter.GaussianBlur(6))
    mask = Image.new('L', region.size, 0)
    ImageDraw.Draw(mask).rectangle((pad - 4, pad - 4, region.size[0] - pad + 4, region.size[1] - pad + 4), fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(5))
    region.paste(soft, (0, 0), mask)
    im.paste(region, (ax - pad, ay - pad))
ch = round(cw * H / W)
im = im.crop((x0, y0, x0 + cw, y0 + ch)).resize((W, H), Image.LANCZOS)
im = ImageEnhance.Contrast(im).enhance(1.10)
im = ImageEnhance.Color(im).enhance(1.08)
fade = Image.new('L', (W, H), 0)
d = ImageDraw.Draw(fade)
for x in range(W):
    a = max(0.0, 1.0 - x / 820.0)
    d.line([(x, 0), (x, H)], fill=int(235 * a ** 1.25))
im = Image.composite(Image.new('RGB', (W, H), (8, 10, 14)), im, fade)
d = ImageDraw.Draw(im)
big = ImageFont.truetype(r'C:\Windows\Fonts\georgiab.ttf', 56)
small = ImageFont.truetype(r'C:\Windows\Fonts\georgia.ttf', 24)
tag = ImageFont.truetype(r'C:\Windows\Fonts\georgiai.ttf', 18)
x = 46
d.text((x + 2, 118 + 2), 'MORDHAU x SKYRIM', font=big, fill=(0, 0, 0))
d.text((x, 118), 'MORDHAU x SKYRIM', font=big, fill=(236, 228, 208))
d.line([(x, 192), (x + 500, 192)], fill=(178, 44, 36), width=3)
d.text((x, 206), "MORDHAU's melee combat inside Skyrim", font=small, fill=(225, 225, 225))
d.text((x, 244), 'experimental  -  needs both games  -  offline only', font=tag, fill=(175, 175, 175))
im.save(out)
print(im.size)
