# Calm Combo: Android

A portrait, one-board version of the Neo Geo CD game, made for phones and tablets.

## Install

On your phone or tablet, open the [**Calm Combo for Android (latest)**](https://github.com/Hatdagger/calm-combo/releases/tag/android-latest) release, download `CalmCombo.apk` and open it. Android will ask you to allow installs from your browser or file manager the first time. Every push to `main` rebuilds the APK there. A new build installs over the old one and keeps your scores, because every build is signed with the same key (`app/calm-hobby.keystore`). That key is a hobby key kept in the repo on purpose; don't reuse it for anything you publish on a store.

## What's in it

- **Modes:** Score Attack, Puzzle (the 12 boards), How to play (the 4 lessons) and High scores with initials. These are the Neo Geo CD's modes, built from the same rule and puzzle files (`neogeo-cd/src/board.c`, `puzzle.c`, `puzzles.h`), so a puzzle plays out exactly as on the disc.
- **Scenes:** the lake, beach and rain window with the 10-minute day and night, re-laid for a tall screen. The sky carries on up to the top, and the umbrella, palm and window frame are pulled in to stay on screen. The art is the Neo Geo CD's own PNGs.
- **Sound:** the 16 effects and the three scene songs (with their ambience), the same files as the disc.
- **Touch:** drag a tile along its row or column; the line glows green when letting go would clear something. Let go to drop it, or drag back to cancel. Tap **MUSIC** and **>>** at the top, and **MENU** / **RETRY** at the bottom.
- **Controllers** (and keyboards): the Neo Geo layout. D-pad or stick moves, A grabs and drops, B cancels, Start retries, Select goes to the menu, X skips the scene, Y toggles music. The cursor appears as soon as a controller is used.
- **Screen:** drawn at about 180 pixels wide and scaled up by a whole number, so the pixel art stays sharp on any screen. Taller phones get more room between the parts.
- **Saving:** high scores and solved puzzles are kept in the app's own storage.

## How it's built

```
core/        the game in portable C: calm.c (modes, board, input, screens),
             scenery.c (the three scenes), gfx.c (pictures and text)
app/         the Android app: Java for the window, touch, keys, sound and
             saving; cpp/jni.c connects it to core/
tools/       make_pak.py packs the art; prepare_assets.sh adds the art,
             sound effects and songs to the app
test/        pc.c runs the real game on a PC from a script (tour.txt) and
             saves screenshots
```

GitHub builds it on every push (`.github/workflows/android.yml`): first the PC play-through with screenshots (artifact `CalmCombo-android-screenshots`), then the APK (artifact `CalmCombo-android`, and the release page above).

To try the play-through yourself on Linux:

```
sh android/test/build_pc.sh build-pc
python3 android/tools/make_pak.py build-pc/calm.pak
mkdir -p shots && build-pc/calm-pc build-pc/calm.pak android/test/tour.txt shots
python3 android/test/shots.py shots
```

To build the APK by hand: run `sh android/tools/prepare_assets.sh` (needs Python with pillow, numpy and scipy, plus ffmpeg), then `gradle -p android assembleRelease` with the Android SDK and NDK installed.
