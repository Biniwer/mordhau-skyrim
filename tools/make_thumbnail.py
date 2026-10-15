"""The mod page's thumbnail: the header's picture and lettering in 16:9 (1280x720), from the same frame of the
Ebony Warrior recording. usage: make_thumbnail.py <frame.png> <out.png>"""
import sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter, ImageEnhance
src, out = sys.argv[1], sys.argv[2]
W, H = 1280, 720
im = Image.open(src).convert('RGB')
# the same small HUD marks as in the header, painted over with their blurred surroundings
for (ax, ay, bx, by) in [(1180, 855, 1320, 880), (1262, 692, 1300, 712), (1180, 555, 1320, 575)]:
    pad = 26
    region = im.crop((ax - pad, ay - pad, bx + pad, by + pad))
    soft = region.filter(ImageFilter.MedianFilter(21)).filter(ImageFilter.GaussianBlur(6))
    mask = Image.new('L', region.size, 0)
    ImageDraw.Draw(mask).rectangle((pad - 4, pad - 4, region.size[0] - pad + 4, region.size[1] - pad + 4), fill=255)
    region.paste(soft, (0, 0), mask.filter(ImageFilter.GaussianBlur(5)))
    im.paste(region, (ax - pad, ay - pad))
x0, y0, cw = 620, 262, 1800            # both fighters, the HUD bars at the top and bottom left out
im = im.crop((x0, y0, x0 + cw, y0 + round(cw * H / W))).resize((W, H), Image.LANCZOS)
im = ImageEnhance.Color(ImageEnhance.Contrast(im).enhance(1.10)).enhance(1.08)
# a dark band along the bottom for the lettering (a thumbnail is seen small: big letters, one line under them)
fade = Image.new('L', (W, H), 0)
d = ImageDraw.Draw(fade)
for y in range(H):
    a = max(0.0, (y - 380) / 340.0)
    d.line([(0, y), (W, y)], fill=int(235 * min(1.0, a) ** 1.2))
im = Image.composite(Image.new('RGB', (W, H), (8, 10, 14)), im, fade)
d = ImageDraw.Draw(im)
big = ImageFont.truetype(r'C:\Windows\Fonts\georgiab.ttf', 104)
small = ImageFont.truetype(r'C:\Windows\Fonts\georgia.ttf', 44)
title = 'MORDHAU x SKYRIM'
tw = d.textlength(title, font=big)
x = (W - tw) / 2
d.text((x + 3, 513), title, font=big, fill=(0, 0, 0))
d.text((x, 510), title, font=big, fill=(236, 228, 208))
d.line([(x, 628), (x + tw, 628)], fill=(178, 44, 36), width=5)
line = "MORDHAU's melee combat inside Skyrim"
d.text(((W - d.textlength(line, font=small)) / 2, 644), line, font=small, fill=(225, 225, 225))
im.save(out)
print(im.size)
