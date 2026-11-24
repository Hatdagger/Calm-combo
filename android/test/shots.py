"""Turn the PC harness's .ppm screenshots into PNGs (3x) plus one contact sheet.
    python3 android/test/shots.py SHOTS_DIR"""
import glob, os, sys
from PIL import Image

d = sys.argv[1]
ims = []
for f in sorted(glob.glob(os.path.join(d, '*.ppm'))):
    im = Image.open(f).convert('RGB')
    big = im.resize((im.size[0] * 3, im.size[1] * 3), Image.NEAREST)
    big.save(f[:-4] + '.png')
    os.remove(f)
    ims.append(im)
if ims:
    w = sum(i.size[0] + 4 for i in ims)
    h = max(i.size[1] for i in ims)
    sheet = Image.new('RGB', (w, h), (30, 30, 30))
    x = 0
    for i in ims:
        sheet.paste(i, (x, 0)); x += i.size[0] + 4
    sheet.resize((w * 2, h * 2), Image.NEAREST).save(os.path.join(d, '00_sheet.png'))
print(f'{len(ims)} screenshots')
