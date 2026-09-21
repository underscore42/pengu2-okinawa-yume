# Pengu 2: Okinawa Yume — オキナワのゆめ

Sequel to [Pengu](https://github.com/underscore42/pengu) for the **Neo Geo
Pocket Color**. Same penguin, wrong hemisphere.

Studio So Not Kansai / [underscore42](https://github.com/underscore42).

> `CartTitle "PENGU 2 YUME"` · `CartID 0x0056` · 160×152 · \*\*beta release\*\*

オキナワのゆめ — *Okinawa no yume*

Okinawa is also, emphatically, not Kansai.

\---

## Not a reskin

Four rule changes, not just different pixels.

**Palm trunks** are immovable and uncrushable — a piece of wall dropped in the
middle of the field. You cannot clear a route through one, and they are the
only interior anvil you can crush a block against.

**Canopy occlusion.** Fronds sit on scroll plane 2, put in *front* of the
sprites, so crabs can hide under them. Each palm carries its own 9-bit canopy
mask, so no two silhouettes match. The canopy sways — one register write a
frame — and the dead ground drifts with it, so cover you were relying on
slides out from under a crab.

The canopy occludes **visually only**. Blocks and crabs pass under fronds
freely; only the trunk is solid.

**Crabs come from the sea.** No eggs. The surf sends a fixed quota each round,
so the pressure is continuous rather than something you can pre-empt.

**Pineapples** pay points *and* blow the canopy clear for a couple of seconds,
exposing whatever is hiding under the leaves.

## The game

A 10×9 grid of 16px cells. Four fields, then they wrap.

Walk into a sand block and it slides until it hits something. The penguin
stays put; the block travels. A block with nothing behind it can't be broken,
only pushed — jam it against a block, a coconut, a trunk or the surf, then
press A.

|kill|how|score|
|-|-|-|
|squash|slide a block into a crab|400 / 1600 / 3200 for multiples|
|stomp|punch the surf to stun one, then walk over it|100|

Clear the sea's quota to take the round. At 60 seconds the round turns: crabs
move at your speed and the surf sends them twice as fast.

### Controls

|input|action|
|-|-|
|D-pad|walk; into a block, pushes it|
|A|break the jammed block you're facing. Facing the surf, punch it|
|OPTION|pause. On the title screen, the score table|

\---

## Building

Drop `src/` into a tree with the ameliandev `ngpc-project-template` layout —
`Makefile`, `toolchain.mk`, `lcf/` and `common/`, with `cc900`/`tulink` under
Wine.

`fontdump/` is a separate project with its own `src/`, so the same Makefile
builds it unmodified.

### Tools

```sh
python3 tools/mkart.py       # regenerate tiles from the ASCII art
python3 tools/checkmaps.py   # validate fields: reachability, slides, palms
python3 tools/checksound.py  # audit sound.c against the NGPC sound rules
python3 tools/levels.py      # render all four fields as PNGs
```

Host harnesses compile the *real* game modules against stubbed library calls:

```sh
gcc -std=gnu90 -D\_\_interrupt= -Isrc -o sim \\
    tools/sim.c src/game.c src/entities.c src/snobee.c src/orca.c \\
    src/scenes.c src/screen.c
```

`sim.c` asserts invariants over thousands of frames. `test\_crush.c`,
`test\_orca.c`, `test\_stages.c`, `test\_banks.c` and `test\_canopy.c` cover the
mechanics that timing-dependent random play never reaches.

\---

## Implementation notes

**Tiles 144–255. 112 used, zero spare.** Nothing else fits without banking.

Two sets are banked at runtime: the tropical set (trunk, frond, pineapple)
sits over the 12-tile title logo and is swapped back by
`install\_title\_tiles()` on returning to the front end; the intermission set
sits over the kana block. Both restore on the way out.

**`SwapPlanes()` is a toggle**, not a setter — `SCR\_PRIORITY ^= 0x80`. Calling
it once per level entry flips the canopy behind the field on alternate stages.
`set\_canopy\_front()` tracks the state and only flips when it changes.

**The canopy masks never extend below the trunk.** Fronds in the cell below a
trunk read as leaves growing out of the ground. `test\_canopy.c` asserts it.

**Katakana are traced from the BIOS charset**, not reconstructed — hand-drawn
8×8 Japanese letterforms come out plausible-looking and wrong, because the
strokes don't survive the resolution. See `ngpc-fontkit` for the ripper.

The hiragana (のゆめ) are hand-drawn, because **JIS X 0201 is katakana only**
and the BIOS has no hiragana at all. They are the one part of the font with no
reference behind it.

**Flash save is stubbed.** `library.c`'s `Flash()` hardcodes an offset of
`0x1E0000` and erases block 30, neither of which exists on a 256KB cart. Scores
live in RAM and reset on power-up. This affects every title in the catalogue
that calls `Flash()`.

\---

## Free ROM only

Homage terms, same as Pengu. No physical cartridges of this title are produced
or sold. *Pengo* is a trademark of SEGA; this project is not affiliated with,
endorsed by, or connected to SEGA in any way. All code and art are original.

## The Happy Meal Theory

🍔🍟🥤🧸 If it gave you more fun than a Happy Meal would have, maybe send a bit
where it might actually do some good:

[CurePSP](https://www.psp.org/ways-to-give) ·
[Victoria Hospice](https://victoriahospice.org/donate) ·
[Hospice Southland](https://www.hospicesouthland.org.nz/about-us/donation/)

