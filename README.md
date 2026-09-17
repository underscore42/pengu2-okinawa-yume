# Pengu 2 (ペング okinawa yume)

A single-screen arcade homage for the **Neo Geo Pocket Color**, in the spirit of
Pengo (1982). Push ice blocks, crush the ones that can't move, clear the field
of Sno-Bees before they corner you.

Built under the **Studio So Not Kansai** label by [underscore42](https://github.com/underscore42).

> `CartTitle "PENGU       "` · `CartID 0x0054` · `System 0x1000` (colour) · 160×152

---

## The game

A 10×9 grid of 16px cells on a bright Antarctic ice field. Four fields, then
they wrap.

**Walk into a block and it slides** until it hits something — another block, a
diamond, or the wall. The penguin stays put; the block does the travelling.

**A block with nothing behind it can't be broken, only pushed.** Jam it against
something first, then press A. That constraint is the whole game: you're always
deciding whether to spend a block as a projectile or set it up as a wall.

### Three ways to kill a Sno-Bee

| Method | How | Score |
|---|---|---|
| Squash | Slide a block into it | 400 / 1600 / 3200 for 1 / 2 / 3 in one shove |
| Egg | Crush its block before it hatches | 500 |
| Stun | Punch the wall it's standing against, then walk over it | 100 |

Clear every Sno-Bee **and** every egg to take the round. Eggs keep hatching
while you play, so ignoring them is how you lose.

At 60 seconds the round turns: Sno-Bees move at your speed and eggs hatch twice
as fast. The stage marker flashes.

### Controls

| Input | Action |
|---|---|
| D-pad | Walk. Into a block, pushes it. |
| A | Break the jammed block you're facing. Facing the wall, punches it. |
| OPTION | Pause. On the title screen, shows the score table. |

Menus and HUD are in Japanese (katakana) — スタート, ステージ, ライフ, ポーズ,
ゲームオーバー, ハイスコア.

---

## Building

Needs the ameliandev `ngpc-project-template` layout: a `Makefile`,
`toolchain.mk`, `lcf/` and `common/`, with `cc900`/`tulink` under Wine.

```sh
# drop src/ into a template tree, then
make
```

`fontdump/` is a separate project with its own `src/`, so the same Makefile
builds it unmodified.

### Tools

```sh
python3 tools/mkart.py       # regenerate tiles from the ASCII art in the script
python3 tools/checkmaps.py   # validate fields: reachability, slide quality, sealed eggs
python3 tools/checksound.py  # audit sound.c against the NGPC sound rules
```

Two host harnesses compile the *real* game modules against stubbed library
calls, so logic can be tested without hardware:

```sh
gcc -std=gnu90 -D__interrupt= -Isrc -o sim \
    tools/sim.c src/game.c src/entities.c src/snobee.c src/screen.c
gcc -std=gnu90 -D__interrupt= -Isrc -o test_crush \
    tools/test_crush.c src/game.c src/entities.c src/snobee.c src/screen.c
```

`sim.c` drives thousands of frames of random input and asserts invariants
(coordinates in bounds, no runaway slide, chase actually closes, score never
goes backwards). `test_crush.c` is a regression suite for the crush rules.

`checkmaps.py` has already caught two unwinnable fields where an egg hatched
into a sealed pocket. Run it after editing `src/maps.h`.

---

## Implementation notes

**Tiles 144–241, 98 used, 18 spare.** Four levels, egg blocks, stunned
Sno-Bees and the desert-island theme all cost **zero** tiles — they're palette
work. One tile set, retinted.

**The field is not index 0.** Every playfield pixel is painted in indices 1–3
and an empty cell is an explicit tile. Consequence worth knowing: a tile
stamped over existing art *erases* it wherever the new tile is index 0, which
is why the wall frame is part of the empty-cell fill rather than an overlay.

**Blocks slide as sprites.** Static blocks live on scroll plane 1; only one
block is ever in motion, so a push hands it to four sprites, animates it at
4px/frame, and stamps it back into the tilemap when it lands. No per-frame
tilemap churn.

**The chase** is the greedy grid pursuit from Blue Print Neo — close the larger
axis gap first, fall back to the other axis, then any legal non-reversing turn.
It works purely on cell coordinates, so it moved from 8px to 16px cells
untouched.

**Sound: the table must be `const`.** Non-const initialised data needs a
`.data` copy ROM→RAM that doesn't happen on this cart runtime, so the array is
garbage when `InstallSounds()` reads it — silent, while graphics work perfectly.
`tools/checksound.py` enforces this and the other footguns (no `WaitVsync`
between the install calls, non-zero `ToneStep`, decay to volume 0 within
`Length`, 1-based `PlaySound`).

Deeper build notes, gotchas and the full architecture write-up: **[DEVNOTES.md](DEVNOTES.md)**.

---

## Legal

**Pengu is a homage, not a port.** No original code, art, audio or data is
used. Everything here was written from scratch for the NGPC.

*Pengo* is a trademark of SEGA. This project is not affiliated with, endorsed
by, or connected to SEGA in any way. The name, artwork and all assets are
original and deliberately distinct.

---

## The Happy Meal Theory

Every homage in this collection is free. If it gave you a good half hour,
consider passing that on rather than back:

- [CurePSP](https://www.psp.org/) — progressive supranuclear palsy research and support
- [Victoria Hospice Society](https://victoriahospice.org/) — Victoria, BC
- [Hospice Southland](https://hospicesouthland.co.nz/) — Invercargill, NZ

No obligation, no tracking, no follow-up. Play the game.
