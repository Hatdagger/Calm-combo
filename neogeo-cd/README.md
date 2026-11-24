# Calm Combo: Neo Geo CD port

Built with [NeoCore](https://github.com/David-Vandensteen/neocore), a Neo Geo CD kit using GCC 2.95 and DATlib (vendored in `neocore/`, MIT license).

## Status: milestone 5 (tutorial, Puzzle mode, high scores)
Versus has been removed (Nov 2026) to keep the game relaxing; two players still play Score Attack side by side.

The title menu has four entries: **Score Attack**, **Puzzle**, **How to play** and **High scores**. Up/down picks one, A or Start begins. Whoever presses plays, so 2P starts a puzzle on the right-hand board.

- **How to play:** four short lessons on one board, with the instructions on the other half of the screen. 1: slide a line into a match. 2: the lever (the tile you move doesn't need to be in the match). 3: chains (make a match that drops tiles into a second one, 3 tries). 4: stones (break both stones in 3 moves). A wrong move just resets the lesson. After lesson 4 it goes back to the menu.
- **Puzzle mode:** 12 fixed boards. Break every stone within the move count shown (1 move for puzzle 1, then 2, then 3 from puzzle 6). The refills are fixed too, so every try plays out exactly the same way. Puzzles open one at a time. On the menu, left/right picks any open puzzle, and a **\*** marks the solved ones. **Start** during a puzzle starts it over. Bonus moves from combos don't count here: the move count is the move count.
- **High scores:** the top five Score Attack scores with 3-letter initials. Beat the 5th place and you enter your initials when the game ends: up/down changes the letter, A or right moves on, B or left goes back. *High scores* on the menu shows the table. BEST on the top line is the table's top score.
- **Saving:** the high scores and solved puzzles are kept in the Neo Geo CD's built-in backup memory (the BIOS "memory card"), through the BIOS CARD call. It's one 64-byte save under NeoCore's game number 0x7777, save 0, marked "CC01" with a checksum, so other data there is ignored. It is never formatted from here. If saving fails, the High scores screen shows `NOT SAVED: ERROR nn`. Saving hasn't been tried on real hardware yet.
- The boards are made by `test/puzzle_gen.c`, which plays every possible move sequence (with the real rules and refills). CI checks each board on every push: it must start with no match, be solvable within its moves, and (Puzzle mode) not be solvable in fewer. Puzzle 1 is solved by 17 of its 114 possible moves. Puzzle 12 is solved by 4 of about 416,000 three-move sequences.
- Code: `src/puzzle.c` (loading a board), `src/puzzles.h` (the boards, generated), `src/save.c`, and the flow in `src/main.c`.

## Milestone 4 (sound)
Every scene now has its song, and the board has its sound effects.

- **Music:** the three songs are CD audio tracks 2–4 (lake, beach, rain window), each with its ambience mixed in: wind at the lake, a wave swell every 9 seconds at the shore (in step with the shoreline animation), heavy rain at the window. The track changes with the scene. **D** pauses and resumes it; if the scene changed while the music was off, the new scene's song starts instead.
- **Sound effects:** 16 effects on the sound chip: grab, no match, the chain chimes (cascade steps 1–9, each one note higher), LEVER!, extra moves, stones landing, a soft tick (name entry, puzzle picker) and the menu. When two people play, 1P sounds come from the left speaker and 2P from the right; one player alone is centred.
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
- **Scene changes:** every 5 minutes, or when you press C, the scene fades to black and the next one fades in. Every frame of every scene is loaded once at boot, so nothing loads during play. The change is spread over several frames so the board never stutters: the fade only dims colours that were already worked out, and while the screen is black the new scene is laid down a few layers per frame and its colours one picture per frame (the night colour tables are worked out once at boot). The fade-in starts when all of it is ready, a few frames later.
- The tile panels are off (`USE_PANEL 0`) to leave sprites for the scenery. The tiles have their own dark plates.

Sprite budget: the busiest screen (two boards over the rain window at night) shows 80 sprites on one line (the limit is 96). Code: `src/scene.c`.

### Animation
Every loop now has enough in-between frames to move smoothly, and the last frame leads straight back into the first:

| Picture | Frames | Each frame shows | Loop |
|---|---|---|---|
| `lake/water.png` (5120×64) | 16 | 6 video frames | 1.6 s |
| `lake/firefly.png` (128×16) | 8 (glow up and down) | 5 | 0.7 s |
| `beach/wave.png` (1280×32) | 4 (foam ripple) | 8 | 0.5 s |
| `beach/palm.png` (1280×192) | 16 (one sway) | 8 | 2.1 s |
| `beach/umbrella.png` (1280×48) | 8 | 4 | 0.5 s |
| `beach/glitter.png` (512×16) | 8 | 4 | 0.5 s |
| `window/steam.png` (128×32) | 8 | 6 | 0.8 s |

- **The shore wave** is its own layer now: `shore.png` (320×48) is just the sand, and `wave.png` is the water and foam, which the game slides up and down one pixel at a time, one swell every 9 seconds, in step with the wave sound in the beach song. In `wave.png` the foam sits about 16 px from the top and everything below it is transparent.
- **Droplets** are one picture (16×32); the game slides them down the glass.

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
| Title menu | Up/down to pick, left/right for the puzzle number, A or Start to begin |
| Stick | Move the cursor. While holding a tile, slide it along its row or column |
| A | Grab / drop |
| B | Cancel |
| C | Next scene |
| D | Music on/off (pauses the CD track) |
| Start | Join / retry (in a puzzle: start it over) |
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
`test/frontend.c` runs the real `main.c` (and `sound.c`) on a PC against stand-in hardware. Every sound command and CD track change is logged to `sounds.txt`, and the tour checks that the expected effects played. Scripts play the game (`test/tour.txt`: title, Score Attack, 2P drop-in; `test/scenes.txt`: every scene by day and night, a scene change and the busiest two-board screen; `test/modes.txt`: all four lessons, puzzles 1 and 2 including a failed try, and a high score with initials entry, checked against a fake backup memory) and snapshot the sprites and text, and `test/render_frames.py` draws those snapshots with the real art. It also fails if a frame would overflow DATlib's sprite list or a scanline goes over 96 sprites.

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

To make new puzzle boards (a few minutes): `gcc -std=c89 -O2 -Isrc -o pgen src/board.c test/puzzle_gen.c && ./pgen > src/puzzles.h`. To check them: build with `-DCHECK` and `src/puzzle.c` added, as in the CI job.

## Next milestones
6. Test milestone 5 on real hardware, especially saving.
7. Later: survival pacing, hint ghost, more puzzles.
