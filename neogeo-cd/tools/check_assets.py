"""Check Calm Combo art and audio against the Neo Geo CD asset brief.

Run: python3 tools/check_assets.py        (exit code 1 if anything breaks a rule)
Errors stop the build; warnings are advice. Every problem names the file and
the 16x16 tile (column, row counted from 1) so it is easy to find.
"""
import os, sys, wave
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'assets')
MAG = (255, 0, 255)

# path: (width, height, frames)   frames are laid side by side
GFX = {
    'gfx/logo.png': ('logo', None, 1),
    'gfx/board/tiles.png': (64, 112, 4),
    'gfx/board/cursor.png': (64, 32, 4),
    'gfx/board/panel.png': (144, 128, 1),
    'gfx/font/font.png': (128, 128, 1),
    'gfx/common/sun.png': (32, 32, 1),
    'gfx/common/moon.png': (32, 32, 1),
    'gfx/lake/sky.png': (320, 160, 1),
    'gfx/lake/ridge_mid.png': (512, 64, 1),
    'gfx/lake/ridge_near.png': (512, 48, 1),
    'gfx/lake/water.png': (5120, 64, 16),
    'gfx/lake/firefly.png': (128, 16, 8),
    'gfx/beach/sky_sea.png': (320, 192, 1),
    'gfx/beach/glitter.png': (512, 16, 8),
    'gfx/beach/shore.png': (320, 48, 1),
    'gfx/beach/wave.png': (1280, 32, 4),
    'gfx/beach/palm.png': (1280, 192, 16),
    'gfx/beach/umbrella.png': (1280, 48, 8),
    'gfx/window/city.png': (320, 128, 1),
    'gfx/window/rain.png': (256, 64, 4),
    'gfx/window/droplet.png': (16, 32, 1),
    'gfx/window/frame.png': (320, 224, 1),
    'gfx/window/steam.png': (128, 32, 8),
}
SFX = ['grab', 'no_match'] + ['clear_%d' % i for i in range(1, 10)] + ['lever', 'moves', 'stones', 'tick', 'menu']
CDDA = ['track02_lakeside', 'track03_shoreline', 'track04_rain']

errors, warnings = [], []
total_tiles = 0

def err(f, m): errors.append('%s: %s' % (f, m))
def warn(f, m): warnings.append('%s: %s' % (f, m))

def pixels(path):
    im = Image.open(path)
    if im.mode not in ('RGBA', 'RGB', 'P', 'LA', 'L'):
        warn(path, 'unusual PNG mode %s, converted' % im.mode)
    return im.convert('RGBA')

def check_image(rel, im, tile, frames, count=True):
    """Per-tile colour count, hard transparency, 5-bit colour. Returns unique tile count."""
    global total_tiles
    w, h = im.size
    data = im.load()
    soft = off = 0
    uniq = set()
    for ty in range(0, h, tile):
        for tx in range(0, w, tile):
            cols, key = set(), []
            for y in range(ty, min(h, ty + tile)):
                for x in range(tx, min(w, tx + tile)):
                    r, g, b, a = data[x, y]
                    if a == 0 or (r, g, b) == MAG:
                        key.append(None); continue
                    if a < 255: soft += 1
                    if r % 8 or g % 8 or b % 8: off += 1
                    c = (r >> 3, g >> 3, b >> 3)
                    cols.add(c); key.append(c)
            if len(cols) > 15:
                err(rel, 'tile at column %d, row %d has %d colours (max 15 + transparent)'
                    % (tx // tile + 1, ty // tile + 1, len(cols)))
            if cols:
                uniq.add(tuple(key))
    if soft:
        err(rel, '%d pixels are partly transparent; use fully opaque or fully transparent only' % soft)
    if off:
        warn(rel, '%d pixels use colour values that are not multiples of 8 (they will be rounded)' % off)
    total_tiles += len(uniq) if tile == 16 and count else 0
    return len(uniq)

def check_night(rel, day, night):
    if day.size != night.size:
        err(rel, 'night copy is %dx%d but the day file is %dx%d' % (night.size + day.size)); return
    m = {}
    a, b = day.load(), night.load()
    for y in range(day.size[1]):
        for x in range(day.size[0]):
            d, n = a[x, y], b[x, y]
            dt, nt = d[3] == 0 or d[:3] == MAG, n[3] == 0 or n[:3] == MAG
            if dt != nt:
                err(rel, 'pixel %d,%d is transparent in one version only; night must not move pixels' % (x, y)); return
            if dt: continue
            if m.setdefault(d[:3], n[:3]) != n[:3]:
                err(rel, 'day colour #%02X%02X%02X turns into two different night colours (e.g. at %d,%d)'
                    % (d[:3] + (x, y))); return

report = []
for rel, (W, H, frames) in GFX.items():
    path = os.path.join(ROOT, rel)
    if not os.path.exists(path):
        err(rel, 'missing (the sample file can be restored from git history)'); continue
    im = pixels(path)
    w, h = im.size
    if W == 'logo':
        if w % 16 or h % 16 or w > 288 or h > 64:
            err(rel, 'is %dx%d; must be multiples of 16, at most 288x64' % (w, h))
    elif (w, h) != (W, H):
        err(rel, 'is %dx%d; must be exactly %dx%d (%d frame%s)' % (w, h, W, H, frames, '' if frames == 1 else 's'))
        continue
    tile = 8 if rel.endswith('font.png') else 16
    n = check_image(rel, im, tile, frames)
    report.append((rel, n))
    night = path[:-4] + '_night.png'
    if os.path.exists(night):
        nim = pixels(night)
        check_image(rel[:-4] + '_night.png', nim, tile, frames, False)   # colours only: no sprite memory
        check_night(rel[:-4] + '_night.png', im, nim)

if os.path.isdir(os.path.join(ROOT, 'gfx')):
    for dp, _, fs in os.walk(os.path.join(ROOT, 'gfx')):
        for f in fs:
            rel = os.path.relpath(os.path.join(dp, f), ROOT).replace(os.sep, '/')
            if rel not in GFX and not (rel.endswith('_night.png') and rel[:-10] + '.png' in GFX):
                warn(rel, 'not in the asset list, so the game will ignore it')

def check_wav(rel, channels, rate, required):
    path = os.path.join(ROOT, rel)
    if not os.path.exists(path):
        if required: err(rel, 'missing')
        return 'missing'
    try:
        w = wave.open(path)
    except Exception as e:
        err(rel, 'not a readable WAV (%s)' % e); return 'bad'
    ch, sw, fr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
    if sw != 2: err(rel, 'must be 16-bit (is %d-bit)' % (sw * 8))
    if ch != channels: (err if channels == 2 else warn)(rel, 'should be %s (has %d channels)' % ('stereo' if channels == 2 else 'mono', ch))
    if rate and fr != rate: (err if channels == 2 else warn)(rel, 'should be %d Hz (is %d Hz)' % (rate, fr))
    return '%.1f s' % (n / float(fr))

sfx = {s: check_wav('sfx/%s.wav' % s, 1, 18500, False) for s in SFX}
for s, v in sfx.items():
    if v not in ('missing', 'bad') and float(v[:-2]) > 2.0: warn('sfx/%s.wav' % s, 'is %s; keep effects under 2 s' % v)
cd = {t: check_wav('cdda/%s.wav' % t, 2, 44100, False) for t in CDDA}

print('Calm Combo asset check')
print('----------------------')
for rel, n in report:
    print('  %-28s %4d unique tiles' % (rel, n))
mb = total_tiles * 128 / 1048576.0
print('  total: %d tiles = %.2f MB of sprite data, about %.1f s to load on a 1x drive' % (total_tiles, mb, total_tiles * 128 / 150000.0))
print('  sound effects present: %d of %d   CD tracks present: %d of %d'
      % (sum(v not in ('missing', 'bad') for v in sfx.values()), len(SFX), sum(v not in ('missing', 'bad') for v in cd.values()), len(CDDA)))
if total_tiles > 8000:
    warnings.append('total tiles %d is over the 8,000 budget (boot load gets long)' % total_tiles)
for w in warnings: print('WARNING ' + w)
for e in errors: print('ERROR   ' + e)
print('OK' if not errors else '%d error(s)' % len(errors))
sys.exit(1 if errors else 0)
