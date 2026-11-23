# Calm Combo: Neo Geo CD port

Built with [NeoCore](https://github.com/David-Vandensteen/neocore), a Neo Geo CD kit using GCC 2.95 and DATlib (vendored in `neocore/`, MIT license).

## Status: milestone 4 (sound)
Every scene now has its song, and the board has its sound effects.

- **Music:** the three songs are CD audio tracks 2–4 (lake, beach, rain window), each with its ambience mixed in: wind at the lake, a wave swell every 9 seconds at the shore (in step with the shoreline animation), heavy rain at the window. The track changes with the scene. **D** pauses and resumes it; if the scene changed while the music was off, the new scene's song starts instead.
- **Sound effects:** 16 effects on the sound chip: grab, no match, the chain chimes (cascade steps 1–9, each one note higher), LEVER!, extra moves, stones landing, the last seconds of the Versus move timer, and the menu. When two people play, 1P sounds come from the left speaker and 2P from the right; one player alone is centred.
- **Placeholders:** the songs and effects are rendered by `tools/make_audio.py` from the web prototype's own song data (same chords, tempos, form and melodies), so the disc sounds like the prototype. To use your own, see *Your own audio* below.
- Code: `src/sound.c` (effect queue, CD track control) and the hooks in `src/main.c`.

### Your own audio
- **Songs:** put a 44.1 kHz 16-bit stereo WAV named `track02_lakeside.wav`, `track03_shoreline.wav` or `track04_rain.wav` in `src/assets/cdda/` (on GitHub: *Add file → Upload files*). Make it at least 5 minutes, the length of a scene. Any track you don't supply is generated during the build.
- **Effects:** replace a file in `src/assets/sfx/` with a mono 16-bit WAV of the same name, under 2 seconds. 18,500 Hz is ideal; other rates are converted. The build encodes them for the sound chip.
- Everything is checked and built on every push. To build the sound files on your own PC: `python tools/make_audio.py` (needs Python with numpy and scipy).

How the effects work: the sound chip (YM2610) plays 4-bit ADPCM-A samples. `tools/make_audio.py` encodes `sfx/*.wav` into `src/assets/sounds/calm.pcm` and writes the sample table into `calm.z80`, the Z80 sound driver from NGFX SoundBuilder (taken from NeoCore's soundFX_poly sample, MIT). Each effect has three driver commands (left, centre, right), and the game sends one command per frame. The effects use 96 KB of the 512 KB budget.

## Milestone 3 (scenes)
The three scenes now play behind the boards, built from the art in `src/assets/gfx/` (lake, beach, window, common).

- **Mountain lake:** sky, two mountain ridges that drift at different speeds and wrap around, shimmering water, and fireflies at night.
- **Beach:** sky and sea, a wave rolling in and out every 9 seconds, a swaying palm, the flapping umbrella edge, and glitter on the sea under the sun or moon.
- **Rain window:** the city (its lit windows switch on at night), falling rain, droplets sliding down the glass, the window frame and steam off the mug.
- **Day and night:** a 10-minute day done with palettes only: dawn, day, sunset, night. The sun and moon cross the sky on an arc. Light sources (sun, moon, fireflies, glitter) only fade, so they stay bright at night. The game starts mid-morning.
- **Scene changes:** every 5 minutes, or when you press C, the scene fades to black and the next one fades in. Every frame of every scene is loaded once at boot, so nothing loads during play.
- The tile panels are off (`USE_PANEL 0`) to leave sprites for the scenery. The tiles have their own dark plates.

Sprite budget: the busiest screen (Versus over the rain window at night) shows 80 sprites on one line (the limit is 96). Code: `src/scene.c`.

### Night art
Every scene picture can have a hand-painted night copy next to it, for example `lake/sky.png` → `lake/sky_night.png`. As night falls, the picture fades from its day colours into the night copy's colours, so at full night you see exactly what was painted. Dawn and sunset still use the automatic tint.

- A night copy is the **same picture in different colours**: same size, the same pixels transparent, and each day colour turns into exactly one night colour. (Two areas that share a colour by day share one by night. If you want them different at night, give them slightly different day colours.)
- It costs no sprite memory. `tools/make_night.py` turns each night copy into a small colour table (`src/night_maps.h`), and CI re-runs it on every push, so just upload the PNG.
- Starter night copies are in the repo: moonlit blue outside, lit windows in the city, warm lamplight on the window frame and mug. Paint over them as you like. A picture without a night copy keeps the automatic tint, and the city's **#586070** windows still glow on their own there.
- `python3 tools/make_night.py --samples` makes a starter copy for any picture that has none (it never overwrites one).
- The sun, moon, fireflies and glitter are lights and never get a night copy.

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
| D | Music on/off (pauses the CD track) |
| Start | Join / retry |
| Select | Back to the menu |

## Getting a disc image
**Easiest:** every push builds automatically on GitHub (Actions → *Neo Geo CD build*). Download the `CalmCombo-BINCUE` artifact for a raw BIN/CUE (one BIN per track) for burning, SD loaders or emulators, or `CalmCombo-NeoGeoCD` for NeoCore's ISO/WAV/CUE and a CHD for MAME. You need to be signed in to GitHub to download artifacts, and they are kept for 90 days.

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

Before `mak`, run `python ..\tools\make_audio.py` once to make the CD tracks (they are not stored in the repo because they are about 60 MB each). For `mak dist:iso`, also copy `assets\cdda\*.wav` into `..\build\CalmCombo\assets\cdda\`, because NeoCore reads them from there.

## Testing without the console
`test/frontend.c` runs the real `main.c` (and `sound.c`) on a PC against stand-in hardware. Every sound command and CD track change is logged to `sounds.txt`, and the tour checks that the expected effects played. Scripts play the game (`test/tour.txt`: title, Score Attack, 2P drop-in, Versus; `test/scenes.txt`: every scene by day and night, a scene change and the busiest Versus screen) and snapshot the sprites and text, and `test/render_frames.py` draws those snapshots with the real art. It also fails if a frame would overflow DATlib's sprite list or a scanline goes over 96 sprites.

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
5. Test on real hardware: a burned CD-R or an SD loader.
6. Later: survival pacing, hint ghost, tutorial, Puzzle mode, CPU Versus, high-score initials.
