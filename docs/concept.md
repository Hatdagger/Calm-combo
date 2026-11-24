# Calm Combo — slide-match puzzle (demo-disc entry)

*Working title. Formerly "Line Shift."*

A relaxing color-matching puzzle built on HuniePop's board mechanic, without the dating or character layer. **First port target: Neo Geo CD.**

## Vibe
Calm and relaxing. Peaceful scenery and chill music, "a whole vibe."
- **Scenes** rotate every 5 minutes with a slow fade. One full day cycle takes 10 minutes, so the sun arcs across the sky, sets, and the moon rises:
  1. **Mountain lake:** parallax ridges and water reflections, the moon rising, fireflies at night.
  2. **Beach under an umbrella:** the striped canopy edge overhead, a swaying palm, waves rolling in and out on the shore (9-second cycle), sun or moon glitter on the sea.
  3. **Rain through a window:** a wooden window frame, rain falling outside, droplets sliding down the glass, city lights coming on at night, a steaming mug and a plant on the sill, warm lamp glow.
- Players can skip to the next scene at any time.
- **Music:** one lo-fi song per scene, each 32 bars long (intro, A, A with melody, B, B with melody, A, A, outro). Each song has its own key, tempo and repeating melody: lakeside at 72 BPM, shoreline at 80, window rain at 64.
- **Ambience matches the scene:** soft wind at the lake, wave swells timed to the on-screen shoreline, heavy rain at the window.
- Sound effects are soft chimes that rise in pitch along a chain. Each player's sounds are panned to their side of the stereo field.
- Tile colors are soft pastels, and each color also has its own shape (circle, diamond, square, triangle, star, hexagon).

## Core mechanic
- 8×7 board per player, 6 colors.
- Grab any tile and drag it along its row or column. The tile is re-inserted at the drop spot, and the tiles in between shift over by one.
- **The kept oversight:** a drop is valid if it creates a 3+ match anywhere. The dragged tile doesn't need to be part of the match.
- **LEVER bonus:** when the dragged tile is *not* in the first match, you get +50 points and a "LEVER!" callout. This teaches players the trick.
- An invalid drop snaps back and is free.
- Live preview: the line glows green when the drop is valid, and the tiles that would clear are highlighted.

## Screen layout
- The screen is always split into two boards, 1P on the left and 2P on the right, at the Neo Geo's native 320×224 resolution with 16-pixel tiles.
- 2P can drop in at any time by pressing Start (or tapping their side).

## Modes
**Score Attack (survival, relaxed):** each player plays their own board side by side. There is no timer.
- Start with 15 moves, and each move costs 1.
- Extra moves for keeping the chain going: +1 for each cascade step after the first, +1 for a 4-match, +2 for a 5+-match.
- Game over at 0 moves. Shared high score.

**No Versus (decided Nov 2026):** a head-to-head mode with stone attacks and a 7-second move timer was tried and removed. It added pressure the game is meant to take away, and playtesting showed side-by-side Score Attack was the favourite way to play together.

**Stones** (Puzzle mode and lessons): they never match, but they can be slid like any tile, and they break when a match clears next to them.

## Controls
- Neo Geo stick + buttons: A grab/drop, B cancel, C next scene, D music toggle, Start join/retry, Select menu.
- Android (portrait, one board): drag a tile and let go to drop it; MUSIC and >> buttons at the top; controllers use the Neo Geo layout (A grab/drop, B cancel, X next scene, Y music, Start, Select menu).

## Tuning knobs (prototype values)
Start moves 15 · chain +1 · 4-match +1 · 5-match +2 · stone cap 20 on a board.

## Neo Geo CD port notes
- A good fit for the hardware. The scenery is made of layered parallax backgrounds built from sprites, which is classic Neo Geo work. Animated pieces (palm fronds, waves, rain, droplets) are cheap sprite animation. The songs can be **CD audio tracks** played straight off the disc, the system's biggest strength. Sound effects and ambience loops run on the Z80 / ADPCM chip.
- Load each scene's graphics at a scene change, hidden by the fade. The music keeps streaming from the CD.
- Toolchain to confirm: ngdevkit (C on the 68000), tested in MAME, then a burned CD-R for real hardware.
- The board logic is small integer work and ports directly. The float animations become fixed-point. The seeded random generator is integer-only already.

## Name candidates considered
Calm Combo (chosen for now), Drift, Lull, Tidepool, Slow Tide, Stillwater, Unwind, Nudge, Lever, Shift Lagoon, Drift Garden, Line Shift.

## Later (not committed)
- More scenes (city rooftop at night, snowy cabin, forest stream), each with its own song.
- Hint ghost after idle. (The tutorial and Puzzle mode are now in the Neo Geo CD version.)
- Initials entry on the high-score table, saved to the console's internal save memory (now in the Neo Geo CD version).
