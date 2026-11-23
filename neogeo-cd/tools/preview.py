"""Compose preview screens from the current assets (frame 1 of each strip).
Run: python3 tools/preview.py OUTDIR  -> lake.png, beach.png, window.png, board.png (2x scale)"""
import os, sys, random
from PIL import Image
A = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'assets', 'gfx')
OUT = sys.argv[1] if len(sys.argv) > 1 else '.'

def load(rel, fw=None, fh=None, frame=0):
    im = Image.open(os.path.join(A, rel)).convert('RGBA')
    px = im.load()
    for y in range(im.size[1]):
        for x in range(im.size[0]):
            if px[x, y][:3] == (255, 0, 255): px[x, y] = (0, 0, 0, 0)
    if fw: im = im.crop((frame * fw, 0, frame * fw + fw, fh or im.size[1]))
    return im

def boards(scr):
    tiles = load('board/tiles.png'); rnd = random.Random(3)
    panel = load('board/panel.png'); cur = load('board/cursor.png')
    for i, ox in enumerate((24, 168)):
        scr.alpha_composite(panel, (ox - 8, 64))
        for r in range(7):
            for c in range(8):
                k = rnd.randrange(6) if rnd.random() > .05 else 6
                scr.alpha_composite(tiles.crop((0, k * 16, 16, k * 16 + 16)), (ox + c * 16, 72 + r * 16))
        scr.alpha_composite(cur.crop((0, i * 16, 16, i * 16 + 16)), (ox + 3 * 16, 72 + 3 * 16))
    return scr

def save(scr, name):
    scr.resize((640, 448), Image.NEAREST).save(os.path.join(OUT, name)); print('wrote', name)

s = Image.new('RGBA', (320, 224), (0, 0, 0, 255))
s.alpha_composite(load('lake/sky.png'), (0, 0)); s.alpha_composite(load('common/sun.png'), (210, 40))
s.alpha_composite(load('lake/ridge_mid.png').crop((0, 0, 320, 64)), (0, 88))
s.alpha_composite(load('lake/ridge_near.png').crop((40, 0, 360, 48)), (0, 104))
s.alpha_composite(load('lake/water.png', 320), (0, 160))
lake = s.copy(); save(boards(s), 'lake.png')
s = Image.new('RGBA', (320, 224), (0, 0, 0, 255))
s.alpha_composite(load('beach/sky_sea.png'), (0, 0)); s.alpha_composite(load('common/sun.png'), (120, 30))
s.alpha_composite(load('beach/glitter.png', 64), (104, 122))
s.alpha_composite(load('beach/shore.png', 320), (0, 176))
s.alpha_composite(load('beach/wave.png', 320), (0, 183))
s.alpha_composite(load('beach/palm.png', 80), (240, 32)); s.alpha_composite(load('beach/umbrella.png', 160), (0, 0))
save(boards(s), 'beach.png')
s = Image.new('RGBA', (320, 224), (92, 97, 112, 255))
s.alpha_composite(load('window/city.png'), (0, 64))
rain = load('window/rain.png', 64)
for y in range(0, 224, 64):
    for x in range(0, 320, 64): s.alpha_composite(rain, (x, y))
for x, y in ((60, 40), (200, 120), (120, 150), (270, 60)): s.alpha_composite(load('window/droplet.png', 16), (x, y))
s.alpha_composite(load('window/frame.png'), (0, 0)); s.alpha_composite(load('window/steam.png', 16), (20, 156))
save(boards(s), 'window.png')
# board sheet: every board asset at 4x on a dark plate
sheet = Image.new('RGBA', (400, 150), (14, 10, 21, 255))
x = 8
for rel in ('board/tiles.png', 'board/cursor.png', 'font/font.png', 'common/sun.png', 'common/moon.png'):
    im = load(rel); sheet.alpha_composite(im, (x, 8)); x += im.size[0] + 12
sheet.resize((800, 300), Image.NEAREST).save(os.path.join(OUT, 'board.png')); print('wrote board.png')
