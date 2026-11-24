# Calm Combo

A chill slide-match puzzle game and an entry in the *New Games Ideas* demo disc. It's built on HuniePop's board mechanic, with no dating layer. The game is just calm scenery, lo-fi music and a deceptively deep match-3 board.

## The hook

Grab **any** tile and slide it along its row or column. The whole line shifts with it. A move counts if it creates a match **anywhere** on the board, even when the tile you dragged isn't part of that match. That means any tile can be used as a lever to push a line into a clear, and you choose where it lands. Lever moves earn a bonus.

## Modes

- **Score Attack:** relaxed survival with no timer. You start with 15 moves, and chain combos earn more. A second player can drop in at any time on the other half of the screen.
- **Puzzle (Neo Geo CD):** 12 fixed boards. Break every stone within a set number of moves.
- **How to play (Neo Geo CD):** four short lessons: slide, lever, chains, stones.

There's no Versus mode on purpose. The game is about unwinding, and two people playing side by side on their own boards keeps it that way.

## Vibe

Three scenes rotate every 5 minutes with a full day/night cycle: a mountain lake with a rising moon, a beach under an umbrella with a swaying palm and rolling waves, and rain on a window. Each scene has its own lo-fi song and matching ambience.

## Play it

- **Neo Geo CD:** GitHub builds the disc image on every push. See [`neogeo-cd/README.md`](neogeo-cd/README.md).
- **Android:** a portrait, one-board version with every mode. Download `CalmCombo.apk` from the [**Calm Combo for Android (latest)**](https://github.com/Hatdagger/calm-combo/releases/tag/android-latest) release on your phone or tablet and open it to install. See [`android/README.md`](android/README.md).

## Platforms

Done: **Neo Geo CD** and **Android**. Planned: Windows, Linux, Original Xbox, Neo Geo AES/MVS. (The browser prototype was retired in Nov 2026; the Neo Geo CD's `board.c` is now the reference for the rules.) See [`docs/concept.md`](docs/concept.md) for the full design, tuning values and port notes.

## Repo layout

```
docs/       design doc
neogeo-cd/  Neo Geo CD port (NeoCore) and the shared rules, see neogeo-cd/README.md
android/    Android port (portrait, one board), see android/README.md
```
