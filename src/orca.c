/* orca.c - Pengu: the Konami whack-a-orca bonus round
 *
 * An orca surfaces through a hole in the ice. Shove a block into the hole
 * while its head is up and you bonk it.
 *
 * The decision that makes it a game rather than a reflex test: EITHER WAY
 * THE BLOCK PLUGS THE HOLE. Miss, and you have permanently destroyed one of
 * your own targets. So every shove is a bet - spend a block on a guess now,
 * or hold and wait for a head to show with the clock running.
 *
 * Round ends when the holes run out, the blocks run out, or the clock does.
 * Whack every hole and you take the perfect bonus.
 *
 * Costs 8 tiles (two 16x16 head poses) and no new sprite slots - the
 * Sno-Bees are off, so it borrows theirs.
 */
#include "orca.h"
#include "screen.h"
#include "tiles.h"
#include "sound.h"

/* ---- forward declarations ---- */
void orca_reset(void);
void update_orca(void);
void draw_orca(void);
void hide_orca(void);
u8   resolve_hole(u8 cx, u8 cy);
static void surface(void);
static u8   up_window(void);

void hide_orca(void) {
    UnsetSprite(SPR_ORCA);
    UnsetSprite(SPR_ORCA + 1);
    UnsetSprite(SPR_ORCA + 2);
    UnsetSprite(SPR_ORCA + 3);
}

void orca_reset(void) {
    or_on    = 0;
    or_gap   = ORCA_GAP;
    or_tmr   = 0;
    or_frame = 0;
    hide_orca();
}

/* The window shrinks as you land whacks, so the round gets harder because
 * you are getting better at it, not because a timer said so. */
static u8 up_window(void) {
    u8 w, shrink, start;

    /* Each bonus round starts tighter than the last, and tightens further
     * with every whack inside the round.  No multiply: shifts and adds. */
    start = ORCA_UP_MAX;
    shrink = (bonus_round << 3);            /* 8 frames per round played */
    if (shrink > (ORCA_UP_MAX - ORCA_UP_MIN)) start = ORCA_UP_MIN;
    else                                     start = ORCA_UP_MAX - shrink;

    shrink = (whacks << 3) + (whacks << 1);  /* 10 frames per whack */
    if (shrink >= (start - ORCA_UP_MIN)) return ORCA_UP_MIN;
    w = start - shrink;
    if (w < ORCA_UP_MIN) w = ORCA_UP_MIN;
    return w;
}

/* Pick a random still-open hole and come up through it. */
static void surface(void) {
    u8 x, y, n, pick;

    n = holes_left();
    if (n == 0) return;

    pick = cheap_rand(n);
    for (y = 0; y < CELLS_H; y++) {
        for (x = 0; x < CELLS_W; x++) {
            if (grid[y][x] != CELL_HOLE) continue;
            if (pick == 0) {
                or_cx    = x;
                or_cy    = y;
                or_on    = 1;
                or_tmr   = up_window();
                or_frame = 0;
                PlaySound(SND_HATCH);
                return;
            }
            pick--;
        }
    }
}

void update_orca(void) {
    if (or_on) {
        if (or_tmr > 0) or_tmr = or_tmr - 1;
        /* breach pose for the first third, then settle */
        or_frame = 0;
        if (or_tmr > (up_window() - (up_window() >> 2))) or_frame = 1;
        if (or_tmr == 0) {
            or_on  = 0;
            or_gap = ORCA_GAP;
            hide_orca();
        }
        return;
    }

    if (or_gap > 0) or_gap = or_gap - 1;
    if (or_gap == 0) surface();
}

/* Called by the block in flight the moment it drops into a hole.
 * Returns 1 if that was a hit. */
u8 resolve_hole(u8 cx, u8 cy) {
    u8 hit;

    hit = 0;
    if (or_on && or_cx == cx && or_cy == cy) {
        hit    = 1;
        whacks = whacks + 1;
        score  = score + WHACK_SCORE;
        or_on  = 0;
        or_gap = ORCA_GAP;
        hide_orca();
        PlaySound(SND_SQUASH);
    } else {
        PlaySound(SND_LAND);
    }

    /* Hit or miss, the block fills the hole. That is the whole bet. */
    grid[cy][cx] = CELL_EMPTY;
    draw_cell(cx, cy);
    return hit;
}

void draw_orca(void) {
    u8 base, px, py;

    if (!or_on) return;

    base = T_ORCA;
    if (or_frame) base = T_ORCA + 4;

    px = or_cx << 4;
    py = PF_PY0 + (or_cy << 4);

    SetSprite(SPR_ORCA,     base,     0, px,     py,     SP_PENG);
    SetSprite(SPR_ORCA + 1, base + 1, 0, px + 8, py,     SP_PENG);
    SetSprite(SPR_ORCA + 2, base + 2, 0, px,     py + 8, SP_PENG);
    SetSprite(SPR_ORCA + 3, base + 3, 0, px + 8, py + 8, SP_PENG);
}
