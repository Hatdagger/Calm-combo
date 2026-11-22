# Calm Combo

A chill slide-match puzzle game and an entry in the *New Games Ideas* demo disc. It's built on HuniePop's board mechanic, with no dating layer. The game is just calm scenery, lo-fi music and a deceptively deep match-3 board.

## The hook

Grab **any** tile and slide it along its row or column. The whole line shifts with it. A move counts if it creates a match **anywhere** on the board, even when the tile you dragged isn't part of that match. That means any tile can be used as a lever to push a line into a clear, and you choose where it lands. Lever moves earn a bonus.

## Modes

- **Score Attack:** relaxed survival. You start with 15 moves, and chain combos earn more. A second player can drop in at any time on the other half of the screen.
- **Versus:** both players start on an identical board. Combos send stone blocks to the other side, and there's a 7-second move timer so nobody can stall. Whoever runs out of moves first loses.

## Vibe

Three scenes rotate every 5 minutes with a full day/night cycle: a mountain lake with a rising moon, a beach under an umbrella with a swaying palm and rolling waves, and rain on a window. Each scene has its own lo-fi song and matching ambience.

## Play the prototype

Open [`web/index.html`](web/index.html) in a browser. It's a single file with no build step. The prototype runs at the Neo Geo's native 320×224 resolution, scaled up.

| | 1P | 2P |
|---|---|---|
| Move | WASD | Arrow keys |
| Grab / drop | F or Space | K or Enter |
| Cancel | G | L |
| Join / retry | 1 | 2 |

Other keys: **M** music on/off, **N** next scene, **Esc** menu.

Controllers: the first pad is 1P and the second is 2P. A grabs/drops, B cancels, X skips to the next scene, Y toggles music, Start joins, Select goes to the menu.

Touch and mouse: drag on your own side. Two people can share one tablet. Tap **>>** in the top-right corner to skip scenes.

## Platforms

Planned order: **Neo Geo CD first**, then Windows, Linux, Android, Original Xbox, Neo Geo AES/MVS and web. See [`docs/concept.md`](docs/concept.md) for the full design, tuning values and port notes.

## Repo layout

```
web/        browser prototype (reference implementation of the rules)
docs/       design doc
```

Port folders (e.g. `neogeo-cd/`) will be added as each port starts.
