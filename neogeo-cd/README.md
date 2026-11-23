# Calm Combo: Neo Geo CD port

Built with [NeoCore](https://github.com/David-Vandensteen/neocore), a Neo Geo CD kit using GCC 2.95 and DATlib (vendored in `neocore/`, MIT license).

## Status: milestone 3 (scenes)
The three scenes now play behind the boards, built from the art in `src/assets/gfx/` (lake, beach, window, common).

- **Mountain lake:** sky, two mountain ridges that drift at different speeds and wrap around, shimmering water, and fireflies at night.
- **Beach:** sky and sea, a wave rolling in and out every 9 seconds, a swaying palm, the flapping umbrella edge, and glitter on the sea under the sun or moon.
- **Rain window:** the city (its lit windows switch on at night), falling rain, droplets sliding down the glass, the window frame and steam off the mug.
- **Day and night:** a 10-minute day done with palettes only: dawn, day, sunset, night. The sun and moon cross the sky on an arc. Light sources (sun, moon, fireflies, glitter) only fade, so they stay bright at night. The game starts mid-morning.
- **Scene changes:** every 5 minutes, or when you press C, the scene fades to black and the next one fades in. Every frame of every scene is loaded once at boot, so nothing loads during play.
- The tile panels are off (`USE_PANEL 0`) to leave sprites for the scenery. The tiles have their own dark plates.

Sprite budget: the busiest screen (Versus over the rain window at night) shows 80 sprites on one line (the limit is 96). Code: `src/scene.c`.

Not yet: `<name>_night.png` recoloured copies are not used yet. Night is the automatic tint for now. In `window/city.png`, paint lit windows in **#586070** and they glow warm at night.

### Milestone 2 (sprite board)
- Every tile is its own 16×16 sprite (tiles.png). When you slide a line, the tiles glide into place. A held tile lifts 2 px and brightens. Tiles a valid drop would clear pulse bright.
- When tiles clear they play the 3 pop frames, then the survivors fall and new tiles drop in from above. The cursor (cursor.png) pulses and glides between cells.
- The HUD uses the custom font (font.png, fix bank 3), recoloured per message.

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
`test/frontend.c` runs the real `main.c` on a PC against stand-in hardware. Scripts play the game (`test/tour.txt`: title, Score Attack, 2P drop-in, Versus; `test/scenes.txt`: every scene by day and night, a scene change and the busiest Versus screen) and snapshot the sprites and text, and `test/render_frames.py` draws those snapshots with the real art. It also fails if a frame would overflow DATlib's sprite list or a scanline goes over 96 sprites.

```
sh test/build_frontend.sh
build-pc/frontend test/tour.txt shots
build-pc/frontend test/scenes.txt shots
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
4. Audio: render the three lo-fi songs to CD tracks 2–4, and add sound effects on the Z80/ADPCM chip.
5. Test on real hardware: a burned CD-R or an SD loader.
6. Later: use `_night.png` copies, survival pacing, hint ghost, tutorial, Puzzle mode, CPU Versus, high-score initials.
