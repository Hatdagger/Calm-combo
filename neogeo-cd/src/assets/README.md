# Calm Combo assets

Every file here is a placeholder sample that already follows the Neo Geo rules. Paint over a file, keep its name and pixel size, and upload it to the same folder. The full spec is in the **Calm Combo — Neo Geo CD Asset Brief** doc.

The game uses `logo.png`, `board/*` and `font/font.png` now (milestone 2). The scene files come in with milestone 3.

- Check your art: `python3 neogeo-cd/tools/check_assets.py` (this also runs on GitHub with every push).
- Rebuild the samples: `python3 neogeo-cd/tools/make_samples.py` (this overwrites your art, so only use it to start over).
- Preview the scenes: `python3 neogeo-cd/tools/preview.py OUTDIR`.

Credits: `gfx/font/font.png` is recoloured from NeoCore's `samples/custom_fix/font0.png` (MIT license, David Vandensteen).
