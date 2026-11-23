"""Turn the PC harness snapshots (test/frontend.c) into screenshots.

    python3 test/render_frames.py SHOTDIR [--sheet sheet.png]

Each SHOTDIR/NAME.txt becomes SHOTDIR/NAME.png at 2x, drawn with the real
art from src/assets/gfx.  It also checks the Neo Geo limit of 96 sprites on
one scanline and exits with code 1 if a snapshot goes over it.
"""
import glob, os, sys
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
GFX = os.path.join(HERE, '..', 'src', 'assets', 'gfx')
MAG = (255, 0, 255)

def load(rel):
    im = Image.open(os.path.join(GFX, rel)).convert('RGBA')
    px = im.load()
    for y in range(im.size[1]):
        for x in range(im.size[0]):
            if px[x, y][:3] == MAG:
                px[x, y] = (0, 0, 0, 0)
    return im

ART = {1: load('logo.png'), 2: load('board/tiles.png'), 3: load('board/cursor.png'), 4: load('board/panel.png')}
HEIGHT = {1: 2, 2: 7, 3: 2, 4: 8}           # tiles per strip, as in frontend.c
FONT = load('font/font.png')
HI_PALS = (34, 35)                          # bright tile palettes (32 + 2 in the harness)

def ng(w):
    r = ((w >> 7) & 0x1e) | ((w >> 14) & 1)
    g = ((w >> 3) & 0x1e) | ((w >> 13) & 1)
    b = ((w << 1) & 0x1e) | ((w >> 12) & 1)
    return (r << 3 | r >> 2, g << 3 | g >> 2, b << 3 | b >> 2)

def bright(tile):
    out = tile.copy(); px = out.load()
    for y in range(out.size[1]):
        for x in range(out.size[0]):
            r, g, b, a = px[x, y]
            if a:
                px[x, y] = (r + (255 - r) * 3 // 8, g + (255 - g) * 3 // 8, b + (255 - b) * 3 // 8, 255)
    return out

def render(path):
    pals, picts, sprs, fix = {}, [], [], []
    bg = (0, 0, 0)
    for line in open(path):
        p = line.rstrip('\n').split(' ')
        if p[0] == 'bg': bg = ng(int(p[1], 16))
        elif p[0] == 'pal': pals[int(p[1])] = [int(v, 16) for v in p[2:]]
        elif p[0] == 'pict': picts.append(tuple(int(v) for v in p[1:]))
        elif p[0] == 'spr': sprs.append((int(p[1]), int(p[2]), int(p[3]), int(p[4], 16), int(p[5], 16)))
        elif p[0] == 'fix': fix.append(line[4:].rstrip('\n'))
    im = Image.new('RGBA', (320, 224), bg + (255,))
    lines = [0] * 224
    items = [(s, 'spr', (x, y, t, a)) for s, x, y, t, a in sprs] + [(p[1], 'pict', p) for p in picts]
    for _, kind, d in sorted(items, key=lambda i: i[0]):
        if kind == 'pict':
            pid, base, x, y, w, pal = d
            art = ART[pid]
            im.alpha_composite(art, (x, y)) if 0 <= x and x + art.size[0] <= 320 and 0 <= y else im.paste(art, (x, y), art)
            for yy in range(max(0, y), min(224, y + art.size[1])): lines[yy] += w
            continue
        x, y, t, a = d
        pid, idx = t >> 10, t & 0x3ff
        h = HEIGHT[pid]
        col, row = idx // h, idx % h
        tile = ART[pid].crop((col * 16, row * 16, col * 16 + 16, row * 16 + 16))
        if pid == 2 and (a >> 8) in HI_PALS: tile = bright(tile)
        im.paste(tile, (x, y), tile)
        for yy in range(max(0, y), min(224, y + 16)): lines[yy] += 1
    for row, text in enumerate(fix):
        chars, palstr = text.split('|')
        for col, ch in enumerate(chars):
            code = ord(ch)
            if code == 32: continue
            pal = pals.get(int(palstr[col], 16))
            glyph = FONT.crop((code % 16 * 8, code // 16 * 8, code % 16 * 8 + 8, code // 16 * 8 + 8))
            if pal:
                gp = glyph.load(); tint = {248: ng(pal[1]), 168: ng(pal[2]), 40: ng(pal[3])}
                for yy in range(8):
                    for xx in range(8):
                        r, g, b, al = gp[xx, yy]
                        if al: gp[xx, yy] = tint.get(r, (r, g, b)) + (255,)
            im.paste(glyph, (col * 8, row * 8), glyph)
    return im.resize((640, 448), Image.NEAREST), max(lines)

def main():
    d = sys.argv[1]
    sheet = sys.argv[sys.argv.index('--sheet') + 1] if '--sheet' in sys.argv else None
    shots, bad = [], 0
    for path in sorted(glob.glob(os.path.join(d, '*.txt'))):
        im, peak = render(path)
        name = os.path.basename(path)[:-4]
        im.save(os.path.join(d, name + '.png'))
        flag = '  OVER THE LIMIT' if peak > 96 else ''
        bad += peak > 96
        print('%-22s peak %2d sprites on one line%s' % (name, peak, flag))
        shots.append((name, im))
    if sheet and shots:
        cols = 2
        rows = (len(shots) + cols - 1) // cols
        out = Image.new('RGB', (cols * 660, rows * 478), (24, 20, 32))
        dr = ImageDraw.Draw(out)
        for k, (name, im) in enumerate(shots):
            x, y = (k % cols) * 660 + 10, (k // cols) * 478 + 24
            out.paste(im.convert('RGB'), (x, y))
            dr.text((x, y - 16), name, fill=(240, 236, 255))
        out.save(sheet)
        print('wrote', sheet)
    sys.exit(1 if bad else 0)

if __name__ == '__main__':
    main()
