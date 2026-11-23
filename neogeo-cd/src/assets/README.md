# Calm Combo assets

Every file here is a placeholder sample that already follows the Neo Geo rules. Paint over a file, keep its name and pixel size, and upload it to the same folder. The full spec is in the **Calm Combo — Neo Geo CD Asset Brief** doc.

The game uses every image here now (milestone 3), except `board/panel.png`, which is switched off to leave sprites for the scenery. At night the scenes show their `_night.png` copies (same picture, night colours; starter versions are included, paint over them). Pictures without one get an automatic night tint, and in `window/city.png` lit windows painted in #586070 glow at night. See *Night art* in `neogeo-cd/README.md`.

- Check your art: `python3 neogeo-cd/tools/check_assets.py` (this also runs on GitHub with every push).
- Rebuild the samples: `python3 neogeo-cd/tools/make_samples.py` (this overwrites your art, so only use it to start over).
- Preview the scenes: `python3 neogeo-cd/tools/preview.py OUTDIR`.

Audio (milestone 4): `sfx/` holds the 16 sound effects and `cdda/` the three songs. The effect WAVs in the repo are placeholders; the songs are generated during the build unless you upload your own. `sounds/` is built from them by `neogeo-cd/tools/make_audio.py`, so don't edit it by hand. See `neogeo-cd/README.md` for the formats.

Credits: `gfx/font/font.png` is recoloured from NeoCore's `samples/custom_fix/font0.png` and `sounds/ngfx_driver.z80` is the NGFX SoundBuilder driver from NeoCore's `samples/soundFX_poly` (MIT license, David Vandensteen).
