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

# id: (file, tiles per strip) -- same ids as ART_LIST in test/frontend.c
FILES = {1: ('logo.png', 2), 2: ('board/tiles.png', 7), 3: ('board/cursor.png', 2), 4: ('board/panel.png', 8),
         5: ('common/sun.png', 2), 6: ('common/moon.png', 2), 7: ('lake/sky.png', 10),
         8: ('lake/ridge_mid.png', 4), 9: ('lake/ridge_near.png', 3), 10: ('lake/water.png', 4),
         11: ('lake/firefly.png', 1), 12: ('beach/sky_sea.png', 12), 13: ('beach/glitter.png', 1),
         14: ('beach/shore.png', 3), 15: ('beach/palm.png', 12), 16: ('beach/umbrella.png', 3),
         17: ('window/city.png', 8), 18: ('window/rain.png', 4), 19: ('window/droplet.png', 2),
         20: ('window/frame.png', 14), 21: ('window/steam.png', 2)}
ART = {k: load(f) for k, (f, h) in FILES.items()}
HEIGHT = {k: h for k, (f, h) in FILES.items()}
FONT = load('font/font.png')
HI_PALS = (34, 35)                          # bright tile palettes (32 + 2 in the harness)

def ng(w):
    r = ((w >> 7) & 0x1e) | ((w >> 14) & 1)
    g = ((w >> 3) & 0x1e) | ((w >> 13) & 1)
    b = ((w << 1) & 0x1e) | ((w >> 12) & 1)
    return (r << 3 | r >> 2, g << 3 | g >> 2, b << 3 | b >> 2)

KEY_U = [0, 80, 400, 480, 560, 940, 1024]
KEY_C = [(220, 180, 200), (256, 256, 256), (256, 256, 256), (256, 176, 150), (88, 100, 176), (88, 100, 176), (220, 180, 200)]

def night_of(u):
    if u < 470: return 0
    if u < 560: return (u - 470) * 256 // 90
    if u < 940: return 256
    if u < 1010: return (1010 - u) * 256 // 70
    return 0

def tables(u, level):
    """Same maths as make_tables() in src/scene.c."""
    k = 0
    while k < len(KEY_U) - 2 and u >= KEY_U[k + 1]: k += 1
    m = [KEY_C[k][c] + (KEY_C[k + 1][c] - KEY_C[k][c]) * (u - KEY_U[k]) // (KEY_U[k + 1] - KEY_U[k]) for c in range(3)]
    t = [[min(31, (c * m[i] * level) >> 13) for c in range(32)] for i in range(3)]
    fd = [(c * level) >> 5 for c in range(32)]
    return t, fd

def tint_tile(tile, mode, tabs, fd, n):
    out = tile.copy(); px = out.load()
    e8 = lambda v: (v << 3) | (v >> 2)
    for y in range(out.size[1]):
        for x in range(out.size[0]):
            r, g, b, a = px[x, y]
            if not a: continue
            r, g, b = r >> 3, g >> 3, b >> 3
            if mode == 1:
                c = (fd[r], fd[g], fd[b])
            elif mode == 2 and (r, g, b) == (11, 12, 14):
                c = ((tabs[0][r] * (256 - n) + fd[31] * n) >> 8, (tabs[1][g] * (256 - n) + fd[26] * n) >> 8,
                     (tabs[2][b] * (256 - n) + fd[17] * n) >> 8)
            else:
                c = (tabs[0][r], tabs[1][g], tabs[2][b])
            px[x, y] = (e8(c[0]), e8(c[1]), e8(c[2]), 255)
    return out

def bright(tile):
    out = tile.copy(); px = out.load()
    for y in range(out.size[1]):
        for x in range(out.size[0]):
            r, g, b, a = px[x, y]
            if a:
                px[x, y] = (r + (255 - r) * 3 // 8, g + (255 - g) * 3 // 8, b + (255 - b) * 3 // 8, 255)
    return out

def render(path):
    pals, picts, sprs, fix, pmodes = {}, [], [], [], []
    tint = (0, 32)
    bg = (0, 0, 0)
    for line in open(path):
        p = line.rstrip('\n').split(' ')
        if p[0] == 'bg': bg = ng(int(p[1], 16))
        elif p[0] == 'pal': pals[int(p[1])] = [int(v, 16) for v in p[2:]]
        elif p[0] == 'pict': picts.append(tuple(int(v) for v in p[1:]))
        elif p[0] == 'spr':
            h = int(p[4]); words = [int(v, 16) for v in p[5:5 + h * 2]]
            sprs.append((int(p[1]), int(p[2]), int(p[3]), list(zip(words[0::2], words[1::2]))))
        elif p[0] == 'tint': tint = (int(p[1]), int(p[2]))
        elif p[0] == 'pmode': pmodes.append(tuple(int(v) for v in p[1:]))
        elif p[0] == 'fix': fix.append(line[4:].rstrip('\n'))
    im = Image.new('RGBA', (320, 224), bg + (255,))
    lines = [0] * 224
    tabs, fd = tables(*tint)
    n = night_of(tint[0])
    cache = {}
    def mode_of(pal):
        for first, count, mode in pmodes:
            if first <= pal < first + count: return mode
        return None
    items = [(s, 'spr', (x, y, tl)) for s, x, y, tl in sprs] + [(p[1], 'pict', p) for p in picts]
    for _, kind, d in sorted(items, key=lambda i: i[0]):
        if kind == 'pict':
            pid, base, x, y, w, pal = d
            art = ART[pid]
            im.alpha_composite(art, (x, y)) if 0 <= x and x + art.size[0] <= 320 and 0 <= y else im.paste(art, (x, y), art)
            for yy in range(max(0, y), min(224, y + art.size[1])): lines[yy] += w
            continue
        x, y, tl = d
        for row_i, (t, a) in enumerate(tl):
            pid, idx = t >> 10, t & 0x3ff
            if pid not in ART: continue
            h = HEIGHT[pid]
            col, row = idx // h, idx % h
            key = (t, a >> 8)
            tile = cache.get(key)
            if tile is None:
                tile = ART[pid].crop((col * 16, row * 16, col * 16 + 16, row * 16 + 16))
                mode = mode_of(a >> 8)
                if pid == 2 and (a >> 8) in HI_PALS: tile = bright(tile)
                elif mode is not None: tile = tint_tile(tile, mode, tabs, fd, n)
                cache[key] = tile
            ty = y + row_i * 16
            im.paste(tile, (x, ty), tile)
            for yy in range(max(0, ty), min(224, ty + 16)): lines[yy] += 1
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
