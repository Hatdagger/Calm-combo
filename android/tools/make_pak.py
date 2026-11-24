"""Pack the Neo Geo CD art for the Android version.

    python3 android/tools/make_pak.py OUT.pak

Every PNG under neogeo-cd/src/assets/gfx goes in as raw pixels (R, G, B, A),
named by its path without .png ("board/tiles", "lake/sky_night").  Magenta
(#FF00FF), the Neo Geo's see-through colour, becomes alpha 0, so the same art
files drive both versions.  Format: see android/core/gfx.c.
"""
import os, struct, sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
GFX = os.path.join(HERE, '..', '..', 'neogeo-cd', 'src', 'assets', 'gfx')


def main(out):
    items = []
    for root, _, files in os.walk(GFX):
        for f in sorted(files):
            if f.endswith('.png'):
                full = os.path.join(root, f)
                items.append((os.path.relpath(full, GFX)[:-4].replace(os.sep, '/'), full))
    items.sort()
    data = bytearray(b'CCPK' + struct.pack('<I', len(items)))
    for name, full in items:
        im = Image.open(full).convert('RGBA')
        w, h = im.size
        px = bytearray(im.tobytes())
        for i in range(0, len(px), 4):
            if px[i] == 255 and px[i + 1] == 0 and px[i + 2] == 255:
                px[i:i + 4] = b'\0\0\0\0'
            elif px[i + 3] < 128:
                px[i:i + 4] = b'\0\0\0\0'
            else:
                px[i + 3] = 255
        nb = name.encode()
        data += bytes([len(nb)]) + nb
        while len(data) % 4:
            data.append(0)
        data += struct.pack('<HH', w, h) + px
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, 'wb') as fh:
        fh.write(data)
    print(f'{out}: {len(items)} pictures, {len(data) // 1024} KB')


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'calm.pak')
