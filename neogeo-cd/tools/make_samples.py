"""Generate the sample assets for Calm Combo (Neo Geo CD).

Placeholder art that follows every hardware rule in the asset brief, so each
file can be painted over and swapped in. Run: python3 tools/make_samples.py
"""
import math, os, sys, random
from PIL import Image, ImageDraw

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'assets')
MAG = (255, 0, 255, 255)

def snap(c):
    return tuple(min(248, int(round(v / 8.0)) * 8) for v in c[:3])

def mix(a, b, f):
    return tuple(int(a[i] + (b[i] - a[i]) * f) for i in range(3))

def finish(im, tile=16):
    """Snap to 5-bit colour, hard transparency, <=15 colours per tile."""
    im = im.convert('RGBA')
    w, h = im.size
    px = im.load()
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            px[x, y] = MAG if a < 128 or (r, g, b) == (255, 0, 255) else snap((r, g, b)) + (255,)
    for ty in range(0, h, tile):
        for tx in range(0, w, tile):
            box = (tx, ty, tx + tile, ty + tile)
            t = im.crop(box)
            cols = {c for c in t.getdata() if c != MAG}
            if len(cols) <= 15:
                continue
            mask = [c == MAG for c in t.getdata()]
            q = t.convert('RGB').quantize(colors=15 if not any(mask) else 16).convert('RGB')
            out = Image.new('RGBA', t.size)
            out.putdata([MAG if m else snap(c) + (255,) for m, c in zip(mask, q.getdata())])
            # second pass if quantize kept magenta as a colour slot
            cols = {c for c in out.getdata() if c != MAG}
            if len(cols) > 15:
                q = t.convert('RGB').quantize(colors=14).convert('RGB')
                out.putdata([MAG if m else snap(c) + (255,) for m, c in zip(mask, q.getdata())])
            im.paste(out, box)
    return im

def save(im, rel, tile=16):
    path = os.path.join(ROOT, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    finish(im, tile).save(path)
    print('wrote', rel, im.size)

def canvas(w, h):
    return Image.new('RGBA', (w, h), (0, 0, 0, 0))

def vgrad(d, x0, y0, x1, y1, top, bot, band=4):
    for y in range(y0, y1, band):
        f = (y - y0) / max(1, (y1 - y0 - 1))
        d.rectangle([x0, y, x1 - 1, min(y1, y + band) - 1], fill=mix(top, bot, f))

def ridge(seed, amp, hz, period):
    fs = [(k, amp / (i + 1.3), seed * (i + 1) * 1.7) for i, k in enumerate([1, 2, 3, 5, 8])]
    return lambda x: hz - sum(a * (.5 + .5 * math.sin(k * x * math.pi * 2 / period + p)) for k, a, p in fs)

# ---------------------------------------------------------------- board
COLORS = [(255, 143, 135), (124, 196, 255), (127, 224, 168), (255, 224, 138), (198, 168, 255), (255, 182, 124)]
DARK = [(194, 88, 90), (74, 143, 204), (74, 168, 119), (201, 166, 78), (141, 111, 208), (204, 127, 69)]

def shape(d, k, cx, cy, s, fill, outline):
    if k == 0:
        d.ellipse([cx - s, cy - s, cx + s, cy + s], fill=fill, outline=outline)
        return
    if k == 2:
        d.rounded_rectangle([cx - s * .9, cy - s * .9, cx + s * .9, cy + s * .9], radius=max(1, s * .3), fill=fill, outline=outline)
        return
    n, rad, rot, inner = {1: (4, 1.15, -math.pi / 2, 0), 3: (3, 1.2, -math.pi / 2, 0),
                          4: (5, 1.25, -math.pi / 2, .5), 5: (6, 1.05, 0, 0)}[k]
    pts = []
    m = n * 2 if inner else n
    for i in range(m):
        a = rot + i * math.pi * 2 / m
        r = s * rad * (inner if inner and i % 2 else 1)
        pts.append((cx + math.cos(a) * r, cy + (s * .2 if k == 3 else 0) + math.sin(a) * r))
    d.polygon(pts, fill=fill, outline=outline)

def make_tiles():
    im = canvas(64, 112)
    d = ImageDraw.Draw(im)
    plate, plate_hi = (36, 28, 56), (80, 66, 110)
    for row in range(7):
        y = row * 16
        for f in range(4):
            x = f * 16
            cx, cy = x + 7.5, y + 7.5
            if row == 6:  # stone
                if f == 3:
                    for dx, dy in ((3, 3), (11, 4), (5, 11), (12, 12)):
                        d.point((x + dx, y + dy), fill=(133, 124, 149))
                    continue
                s = [6, 7, 5][f] if f < 3 else 0
                d.rounded_rectangle([cx - s, cy - s, cx + s, cy + s], radius=2,
                                    fill=(200, 192, 216) if f == 1 else (133, 124, 149), outline=(77, 70, 89))
                if f == 0:
                    d.line([(x + 5, y + 4), (x + 8, y + 8), (x + 7, y + 12)], fill=(77, 70, 89))
                    d.line([(x + 8, y + 8), (x + 12, y + 7)], fill=(77, 70, 89))
                continue
            col, dk = COLORS[row], DARK[row]
            if f == 0:
                d.rounded_rectangle([x + 1, y + 1, x + 14, y + 14], radius=3, fill=plate)
                shape(d, row, cx, cy + .5, 4.6, col, dk)
            elif f == 1:
                d.rounded_rectangle([x + 1, y + 1, x + 14, y + 14], radius=3, fill=plate_hi)
                shape(d, row, cx, cy, 5.4, (255, 255, 255), col)
            elif f == 2:
                d.rounded_rectangle([x + 3, y + 3, x + 12, y + 12], radius=2, fill=plate)
                shape(d, row, cx, cy, 3, col, None)
            else:
                for dx, dy in ((2, 3), (12, 2), (7, 7), (3, 12), (13, 12)):
                    d.point((x + dx, y + dy), fill=col)
                d.point((x + 7, y + 6), fill=(255, 255, 255))
    save(im, 'gfx/board/tiles.png')

def make_cursor():
    im = canvas(64, 32)
    px = im.load()
    for row, col in enumerate([(255, 231, 168), (168, 236, 255)]):
        for f in range(4):
            o = [0, 0, 1, 1][f]                      # pulse: brackets step in by 1 px
            c = col if f in (0, 3) else mix(col, (255, 255, 255), .5)
            for cx, cy, sx, sy in ((0, 0, 1, 1), (15, 0, -1, 1), (0, 15, 1, -1), (15, 15, -1, -1)):
                for k in range(5):
                    for (x, y, colour) in ((cx + sx * (o + k), cy + sy * o, c), (cx + sx * o, cy + sy * (o + k), c),
                                           (cx + sx * (o + k), cy + sy * (o + 1), (40, 32, 56)),
                                           (cx + sx * (o + 1), cy + sy * (o + k), (40, 32, 56))):
                        X, Y = f * 16 + x, row * 16 + y
                        if colour == (40, 32, 56) and (k == 0 or px[X, Y][3]):
                            continue
                        px[X, Y] = colour + (255,)
    save(im, 'gfx/board/cursor.png')

def make_panel():
    im = canvas(144, 128)
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([0, 0, 143, 127], radius=8, fill=(20, 16, 32), outline=(64, 52, 88))
    d.rounded_rectangle([3, 3, 140, 124], radius=6, outline=(36, 28, 56))
    save(im, 'gfx/board/panel.png')

def make_font():
    src = Image.open(sys.argv[1] if len(sys.argv) > 1 else '/tmp/font0.png').convert('RGB')
    out = canvas(128, 128)
    po, ps = out.load(), src.load()
    for y in range(128):
        for x in range(128):
            r, g, b = ps[x, y]
            if (r, g, b) == (255, 0, 255):
                continue
            lum = .3 * r + .59 * g + .11 * b
            po[x, y] = ((248, 248, 248) if lum > 170 else (168, 168, 168) if lum > 90 else (40, 40, 40)) + (255,)
    save(out, 'gfx/font/font.png', tile=8)

def make_logo():
    from PIL import ImageFont
    W, H = 160, 32
    small = canvas(W // 2, H // 2)
    d = ImageDraw.Draw(small)
    try:
        f = ImageFont.load_default(size=11)
    except TypeError:
        f = ImageFont.load_default()
    txt = 'CALM COMBO'
    bb = d.textbbox((0, 0), txt, font=f)
    x = (W // 2 - (bb[2] - bb[0])) // 2 - bb[0]
    y = (H // 2 - (bb[3] - bb[1])) // 2 - bb[1]
    d.text((x + 1, y + 1), txt, font=f, fill=(76, 52, 112))
    d.text((x, y), txt, font=f, fill=(255, 231, 168))
    im = small.resize((W, H), Image.NEAREST)
    px = im.load()
    for j in range(H):
        for i in range(W):
            r, g, b, a = px[i, j]
            px[i, j] = (0, 0, 0, 0) if a < 128 else ((255, 231, 168, 255) if r > 160 else (76, 52, 112, 255))
    save(im, 'gfx/logo.png')

# ---------------------------------------------------------------- common
def make_sun_moon():
    im = canvas(32, 32); d = ImageDraw.Draw(im)
    d.ellipse([2, 2, 29, 29], fill=(255, 227, 181))
    d.ellipse([6, 6, 25, 25], fill=(255, 240, 210))
    save(im, 'gfx/common/sun.png')
    im = canvas(32, 32); d = ImageDraw.Draw(im)
    d.ellipse([6, 6, 25, 25], fill=(238, 240, 255))
    d.rectangle([12, 12, 13, 13], fill=(201, 204, 230)); d.rectangle([18, 17, 19, 18], fill=(201, 204, 230))
    d.rectangle([14, 20, 15, 21], fill=(201, 204, 230))
    save(im, 'gfx/common/moon.png')

# ---------------------------------------------------------------- lake
def make_lake():
    im = canvas(320, 160); d = ImageDraw.Draw(im)
    vgrad(d, 0, 0, 320, 160, (62, 59, 112), (244, 179, 154), band=8)
    far = ridge(1.3, 30, 150, 320)
    for x in range(320):
        d.line([(x, int(far(x))), (x, 159)], fill=(106, 90, 146))
    save(im, 'gfx/lake/sky.png')
    for name, seed, amp, base, h, col in (('ridge_mid', 2.9, 20, 62, 64, (138, 111, 158)),
                                          ('ridge_near', 4.1, 11, 46, 48, (176, 138, 168))):
        im = canvas(512, h); d = ImageDraw.Draw(im)
        f = ridge(seed, amp * 1.6, base, 512)
        for x in range(512):
            d.line([(x, int(f(x))), (x, h - 1)], fill=col)
        save(im, 'gfx/lake/%s.png' % name)
    WF = 16                     # frames, shown 6 video frames each: a 1.6 s loop
    im = canvas(320 * WF, 64); d = ImageDraw.Draw(im)
    rnd = random.Random(7)
    for fr in range(WF):
        x0 = fr * 320
        ph = 2 * math.pi * fr / WF
        vgrad(d, x0, 0, x0 + 320, 64, (111, 106, 163), (12, 10, 28), band=8)
        refl = ridge(1.3, 30, 150, 320)
        for x in range(320):
            dep = int((150 - refl(x)) * .45)
            if dep > 0:
                d.line([(x0 + x, 0), (x0 + x, dep)], fill=(92, 80, 136))
        for k in range(14):
            # each glint drifts and stretches on its own slow loop, so the
            # last frame leads straight back into the first
            y = 6 + (k * 17) % 56
            xx = int(k * 53 + 5 * math.sin(ph + k * 1.1)) % 320
            ln = int(8 + (k % 3) * 4 + 3 * math.sin(ph * 2 + k))
            d.line([(x0 + xx, y), (x0 + min(319, xx + ln), y)], fill=(150, 146, 196))
    save(im, 'gfx/lake/water.png')
    glow = [0, 1, 2, 3, 3, 2, 1, 0]   # 8 frames: a slow pulse
    im = canvas(16 * len(glow), 16); d = ImageDraw.Draw(im)
    for fr, r in enumerate(glow):
        cx = fr * 16 + 8
        if r >= 3:
            d.ellipse([cx - 3, 5, cx + 2, 10], fill=(120, 112, 40))
        if r >= 2:
            d.ellipse([cx - 2, 6, cx + 1, 9], fill=(170, 160, 70))
        if r >= 1:
            d.rectangle([cx - 1, 7, cx, 8], fill=(220, 210, 110))
        d.point((cx, 8), fill=(255, 243, 166))
    save(im, 'gfx/lake/firefly.png')

# ---------------------------------------------------------------- beach
def make_beach():
    im = canvas(320, 192); d = ImageDraw.Draw(im)
    vgrad(d, 0, 0, 320, 118, (95, 168, 224), (205, 234, 240), band=8)
    vgrad(d, 0, 118, 320, 192, (72, 136, 180), (130, 200, 212), band=8)
    for k in range(12):
        y = 126 + k * 6
        xx = (k * 61) % 340 - 20
        d.line([(max(0, xx), y), (min(319, xx + 10 + k), y)], fill=(210, 236, 240))
    save(im, 'gfx/beach/sky_sea.png')
    GF = 8
    im = canvas(64 * GF, 16); d = ImageDraw.Draw(im)
    for fr in range(GF):
        x0 = fr * 64
        for k in range(4):
            w = int(10 + 6 * math.sin(2 * math.pi * fr / GF + k * 1.7))
            d.line([(x0 + 32 - w // 2, 2 + k * 4), (x0 + 32 + w // 2, 2 + k * 4)], fill=(255, 247, 218))
    save(im, 'gfx/beach/glitter.png')
    # the sand (still) and the wave (its own layer, slid up and down a pixel
    # at a time by the game, one swell every 9 s in step with the music)
    im = canvas(320, 48); d = ImageDraw.Draw(im)
    d.rectangle([0, 12, 319, 47], fill=(236, 213, 166))
    d.rectangle([0, 12, 319, 33], fill=(201, 174, 128))
    d.rectangle([0, 0, 319, 15], fill=(130, 200, 212))
    save(im, 'gfx/beach/shore.png')
    WV = 4                      # foam ripple frames, shown 8 video frames each
    im = canvas(320 * WV, 32); d = ImageDraw.Draw(im)
    for fr in range(WV):
        x0 = fr * 320
        for x in range(320):
            y = int(16 + math.sin(x * .05 + 2 * math.pi * fr / WV) * 2)
            d.line([(x0 + x, 0), (x0 + x, y)], fill=(130, 200, 212))
            d.line([(x0 + x, y), (x0 + x, y + 1)], fill=(248, 248, 248))
    save(im, 'gfx/beach/wave.png')
    PF = 16                     # one smooth sway, shown 8 video frames each
    im = canvas(80 * PF, 192); d = ImageDraw.Draw(im)
    for fr in range(PF):
        x0 = fr * 80
        ph = 2 * math.pi * fr / PF
        sway = math.sin(ph) * 3
        pts = []
        for i in range(13):
            s = i / 12
            pts.append((x0 + 76 - s * 22 - math.sin(s * 2.2) * 10 + sway * s * s, 192 - s * 178))
        for i in range(12):
            d.line([pts[i], pts[i + 1]], fill=(122, 90, 58), width=max(2, int(7 - i * .35)))
        cx, cy = pts[12]
        for k, a0 in enumerate([-2.9, -2.3, -1.7, -1.1, -.5, .1]):
            a = a0 + math.sin(ph + k) * .07
            x, y, ang = cx, cy, a
            for s in range(10):
                nx, ny = x + math.cos(ang) * 5, y + math.sin(ang) * 5
                if x0 <= nx < x0 + 80 and x0 <= x < x0 + 80:
                    d.line([(x, y), (nx, ny)], fill=(63, 143, 90), width=2)
                    L = 7 * (1 - s / 10)
                    for da in (1.9, -1.9):
                        ex = min(x0 + 79, max(x0, nx + math.cos(ang + da) * L))
                        d.line([(nx, ny), (ex, ny + math.sin(ang + da) * L + 2)], fill=(63, 143, 90))
                x, y = nx, ny
                ang += (1 if math.cos(a) > 0 else -1) * .09 + .04
        d.ellipse([cx - 5, cy, cx + 1, cy + 6], fill=(90, 63, 42))
    save(im, 'gfx/beach/palm.png')
    UF = 8
    im = canvas(160 * UF, 48); d = ImageDraw.Draw(im)
    for fr in range(UF):
        x0 = fr * 160
        flap = math.sin(2 * math.pi * fr / UF) * 1.5
        for i in range(9):
            xa, xb = -10 + i * 18, 8 + i * 18
            ya = 34 - xa * .16 + flap * (1 if i % 2 else -1) * .5
            yb = 34 - xb * .16
            col = (255, 143, 135) if i % 2 == 0 else (255, 243, 220)
            poly = [(xa, 0), (xb, 0), (xb, yb), ((xa + xb) / 2, (ya + yb) / 2 + 6 + flap), (xa, ya)]
            poly = [(x0 + max(0, min(159, px)), max(0, min(47, py))) for px, py in poly]
            d.polygon(poly, fill=col)
        d.rectangle([x0, 0, x0 + 139, 2], fill=(200, 110, 104))
    save(im, 'gfx/beach/umbrella.png')

# ---------------------------------------------------------------- window
def make_window():
    im = canvas(320, 128); d = ImageDraw.Draw(im)
    hills = ridge(5.7, 12, 168, 320)
    for x in range(320):
        d.line([(x, int(hills(x)) - 16 - 64), (x, 127)], fill=(84, 84, 104))
    rnd = random.Random(99)
    x = -4
    while x < 330:
        w = 12 + rnd.randrange(20); h = 18 + rnd.randrange(46)
        top = 106 - h
        d.rectangle([x, top, x + w - 1, 127], fill=(58, 62, 78))
        for yy in range(top + 3, 126, 5):
            for xx in range(x + 2, x + w - 2, 4):
                if rnd.random() < .32:
                    d.rectangle([xx, yy, xx + 1, yy + 1], fill=(88, 96, 112))  # becomes lit at night
        x += w + 1
    save(im, 'gfx/window/city.png')
    im = canvas(256, 64); d = ImageDraw.Draw(im)
    rnd = random.Random(5)
    drops = [(rnd.randrange(64), rnd.randrange(64)) for _ in range(14)]
    for fr in range(4):
        for (sx, sy) in drops:
            y = (sy + fr * 16) % 64
            for k in range(6):
                px, py = (sx - k // 3) % 64, (y - k) % 64
                d.point((fr * 64 + px, py), fill=(200, 215, 240))
    save(im, 'gfx/window/rain.png')
    im = canvas(16, 32); d = ImageDraw.Draw(im)   # one frame: the game slides it down
    y = 20
    d.line([(7, y - 12), (7, y - 2)], fill=(120, 132, 160))
    d.ellipse([5, y - 2, 9, y + 2], fill=(220, 228, 248))
    d.point((6, y - 1), fill=(255, 255, 255))
    save(im, 'gfx/window/droplet.png')
    im = canvas(320, 224); d = ImageDraw.Draw(im)
    wood, woodl = (106, 74, 54), (138, 100, 72)
    for box in ([0, 0, 8, 223], [311, 0, 319, 223], [0, 0, 319, 5], [157, 0, 162, 195], [0, 109, 319, 112], [0, 202, 319, 223]):
        d.rectangle(box, fill=wood)
    d.rectangle([0, 196, 319, 201], fill=woodl)
    d.rectangle([22, 186, 33, 196], fill=(232, 224, 214)); d.arc([31, 187, 38, 195], -80, 80, fill=(232, 224, 214), width=2)
    d.rectangle([284, 188, 297, 196], fill=(192, 112, 74))
    for k in range(5):
        cx = 291 + (k - 2) * 3
        d.ellipse([cx - 2, 176 - abs(k - 2) * 2, cx + 2, 188 - abs(k - 2) * 2], fill=(90, 163, 106))
    save(im, 'gfx/window/frame.png')
    SF = 8
    im = canvas(16 * SF, 32); d = ImageDraw.Draw(im)
    for fr in range(SF):
        for k in range(2):
            pts = [(fr * 16 + 5 + k * 5 + math.sin(2 * math.pi * fr / SF - s * .6 + k) * 2, 30 - s * 2.6) for s in range(11)]
            d.line(pts, fill=(216, 216, 224))
    save(im, 'gfx/window/steam.png')

if __name__ == '__main__':
    make_logo(); make_tiles(); make_cursor(); make_panel(); make_font()
    make_sun_moon(); make_lake(); make_beach(); make_window()
