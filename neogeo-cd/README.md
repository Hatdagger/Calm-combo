# Calm Combo: Neo Geo CD port

Built with [NeoCore](https://github.com/David-Vandensteen/neocore), a Neo Geo CD kit using GCC 2.95 and DATlib (vendored in `neocore/`, MIT license).

## Status: milestone 1 (text-layer build)
The full game runs on the Neo Geo's text (fix) layer. Each 16×16 tile is a 2×2 block of font characters in its tile colour, so the rules can be played and tested before the sprite art exists.

- Title menu: Score Attack and Versus.
- Both boards are on screen at the same 320×224 positions as the web prototype. 2P can drop in with Start.
- Grab, slide, live preview, a valid drop or a free snap-back, cascades, LEVER bonus, extra moves.
- Versus: identical fields, stones sent and cancelled, the 7 s move timer, stones that land after 3 s, WIN/LOSE.
- The backdrop colour follows the scene (lake, shore, rain) and drifts from day to night over 10 minutes. C skips to the next scene.
- CD audio hooks are in place but switched off (`HAVE_CDDA 0`) until the songs exist as tracks.

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
2. Sprite tiles: real 16×16 pastel tiles with shapes, a smooth slide, and pop and fall animation.
3. Scenes as layered sprite art (lake, beach, window), with a fade at each scene change.
4. Audio: render the three lo-fi songs to CD tracks 2–4, and add sound effects on the Z80/ADPCM chip.
5. Test on real hardware: a burned CD-R or an SD loader.
