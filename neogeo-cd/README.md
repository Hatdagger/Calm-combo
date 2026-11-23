# Calm Combo: Neo Geo CD port

Built with [NeoCore](https://github.com/David-Vandensteen/neocore), a Neo Geo CD kit using GCC 2.95 and DATlib (vendored in `neocore/`, MIT license).

## Status: milestone 2 (sprite board)
The board is drawn with sprites now, using the art in `src/assets/gfx/board/`. The rules are the same as milestone 1.

- Every tile is its own 16×16 sprite (tiles.png). When you slide a line, the tiles glide into place. A held tile lifts 2 px and brightens. Tiles a valid drop would clear pulse bright.
- When tiles clear they play the 3 pop frames (flash, shrink, sparkle). Then the survivors fall with gravity and new tiles drop in from above. At the start of a game the whole board drops in column by column.
- The cursor (cursor.png) pulses and glides between cells. 1P and 2P have their own colour.
- A soft panel (panel.png) sits behind each board. Set `USE_PANEL 0` in `main.c` to drop it if the scenery in milestone 3 needs those sprites.
- The HUD uses the custom font (font.png, fix bank 3). The game recolours it by swapping its three greys.
- Stones landing and shuffles flash the tiles that change.
- The backdrop is still a flat colour per scene with the day/night drift. CD audio is still off (`HAVE_CDDA 0`).

Sprite budget: each board line shows 8 tiles + 9 panel columns + the cursor, about 36 sprites per line for two players (the hardware limit is 96).

### Controls
| Button | Action |
|---|---|
| Stick | Move the cursor. While holding a tile, slide it along its row or column |
| A | Grab / drop |
| B | Cancel |
| C | Next scene |
| D | Music on/off |
| Start | Join / retry |
| Select | Back to the menu |

## Getting a disc image
**Easiest:** every push builds automatically on GitHub (Actions → *Neo Geo CD build*). Download the `CalmCombo-NeoGeoCD` artifact. It contains an ISO/CUE for burning or an SD loader, and a CHD for MAME.

**On your own Windows PC** (keep the path short, e.g. `C:\cc`, because GCC 2.95 has a 100-character path limit):
```
git clone https://github.com/Hatdagger/calm-combo C:\cc
cd C:\cc\neogeo-cd\src
mak lib
mak sprite
mak
mak run:mame
```
The first run downloads the compiler, MAME and Raine. `mak lib` (building the NeoCore library) is only needed once. MAME needs a Neo Geo CDZ BIOS (`neocdz.zip`), which you supply yourself.

## Testing without the console
`test/frontend.c` runs the real `main.c` on a PC against stand-in hardware. A script plays the game (`test/tour.txt`: title, Score Attack, 2P drop-in, Versus) and snapshots the sprites and text, and `test/render_frames.py` draws those snapshots with the real art. It also fails if a frame would overflow DATlib's sprite list or a scanline goes over 96 sprites.

```
sh test/build_frontend.sh
build-pc/frontend test/tour.txt shots
python3 test/render_frames.py shots --sheet shots/00_sheet.png
```
GitHub runs this on every push and uploads the pictures as the `CalmCombo-screenshots` artifact. It checks the game logic and layout, not the real hardware, so a boot test is still needed.

## Board rules
`src/board.c` / `board.h` hold the rules in plain C89 with no floats. `test/` checks them against `web/index.html`: the same seeded games give identical boards, refills, scores and move counts. This check also runs on every push.

```
gcc -std=c89 -pedantic -Wall -O2 -o sim src/board.c test/sim.c
./sim 42 60 > c.txt && node test/parity.js ../web/index.html 42 60 > j.txt && cmp c.txt j.txt
```

## Toolchain notes (Nov 2026)
- ngdevkit targets cartridges (AES/MVS) only, so it can't build a Neo Geo CD disc.
- NeoCore is built for Neo Geo CD. It makes ISO/CUE and CHD files, supports CD audio tracks (track 1 is the program), and its scripts are Windows PowerShell.

## Next milestones
3. Scenes as layered sprite art (lake, beach, window), with a palette day/night cycle and a fade at each scene change.
4. Audio: render the three lo-fi songs to CD tracks 2–4, and add sound effects on the Z80/ADPCM chip.
5. Test on real hardware: a burned CD-R or an SD loader.
