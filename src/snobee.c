/* snobee.c - Pengu: the Sno-Bees
 *
 * The chase is Blue Print's pick_bomb_dir, unchanged in substance: close the
 * larger axis gap first, fall back to the other axis, then any legal turn
 * that isn't a straight reversal, then reverse as a last resort.  It works
 * on cell coordinates only, so it transferred from 8px cells to 16px cells
 * without touching the logic.
 *
 * Three ways to kill a Sno-Bee, all canonical:
 *   1. slide a block into it            (kill_bees_in_cell, from update_slide)
 *   2. crush the egg before it hatches  (try_crush, in entities.c)
 *   3. stun it against a wall, then walk over it   (wall_stun + contact)
 *
 * Stunned bees are drawn with the ICE sprite palette - pale blue, reads as
 * frozen, and costs zero tiles and zero palette slots.
 */
#include "snobee.h"
#include "screen.h"
#include "tiles.h"
#include "sound.h"

/* ---- forward declarations ---- */
void  bees_reset(void);
u8    spawn_bee(u8 cx, u8 cy);
void  hatch_one(void);
void  update_bees(void);
void  draw_bees(void);
u8    kill_bees_in_cell(u8 cx, u8 cy);
u8    bees_alive(void);
void  wall_stun(u8 d);
static u8 pick_bee_dir(u8 i);

void bees_reset(void) {
    u8 i;
    for (i = 0; i < MAX_BEES; i++) {
        bee_on[i]   = 0;
        bee_stun[i] = 0;
        UnsetSprite(SPR_BEE + (i << 2));
        UnsetSprite(SPR_BEE + (i << 2) + 1);
        UnsetSprite(SPR_BEE + (i << 2) + 2);
        UnsetSprite(SPR_BEE + (i << 2) + 3);
    }
}

u8 spawn_bee(u8 cx, u8 cy) {
    u8 i;
    for (i = 0; i < MAX_BEES; i++) {
        if (bee_on[i] == 0) {
            bee_on[i]   = 1;
            bee_cx[i]   = cx;
            bee_cy[i]   = cy;
            bee_px[i]   = cx << 4;
            bee_py[i]   = PF_PY0 + (cy << 4);
            bee_dir[i]  = DIR_NONE;
            bee_sub[i]  = 0;
            bee_tick[i] = 0;
            bee_anim[i] = 0;
            bee_stun[i] = 0;
            return 1;
        }
    }
    return 0;   /* board is full - the egg waits its turn */
}

/* Pick a random egg cell and hatch it.  The cell empties, so crushing an
 * egg block first is a real strategy rather than a waste of a block. */
/* Pengu 2: crabs crawl out of the surf rather than hatching from blocks.
 * The pre-emptive kill is gone with the eggs - pressure is continuous now
 * instead of lumpy. Spawn cells are border cells only, and never one the
 * penguin is standing on.
 */
void spawn_from_surf(void) {
    u8 x, y, n, pick;

    if (spawns_left == 0) return;

    n = 0;
    for (y = 0; y < CELLS_H; y++)
        for (x = 0; x < CELLS_W; x++) {
            if (x != 0 && y != 0 && x != CELLS_W - 1 && y != CELLS_H - 1) continue;
            if (grid[y][x] != CELL_EMPTY) continue;
            if (x == p_cx && y == p_cy) continue;
            n++;
        }
    if (n == 0) return;

    pick = cheap_rand(n);
    for (y = 0; y < CELLS_H; y++) {
        for (x = 0; x < CELLS_W; x++) {
            if (x != 0 && y != 0 && x != CELLS_W - 1 && y != CELLS_H - 1) continue;
            if (grid[y][x] != CELL_EMPTY) continue;
            if (x == p_cx && y == p_cy) continue;
            if (pick == 0) {
                if (spawn_bee(x, y)) {
                    spawns_left = spawns_left - 1;
                    PlaySound(SND_HATCH);
                }
                return;
            }
            pick--;
        }
    }
}

void hatch_one(void) {
    u8 x, y, n, pick;

    n = eggs_left();
    if (n == 0) return;

    pick = cheap_rand(n);
    for (y = 0; y < CELLS_H; y++) {
        for (x = 0; x < CELLS_W; x++) {
            if (grid[y][x] != CELL_EGG) continue;
            if (pick == 0) {
                grid[y][x] = CELL_EMPTY;
                draw_cell(x, y);
                if (!spawn_bee(x, y)) grid[y][x] = CELL_EGG;  /* put it back */
                else PlaySound(SND_HATCH);
                return;
            }
            pick--;
        }
    }
}

/* Greedy grid chase - Blue Print's, verbatim in behaviour. */
static u8 pick_bee_dir(u8 i) {
    u8 ph, pv, first, second, ncx, ncy, d, back, k;

    ph = DIR_NONE;
    pv = DIR_NONE;
    if (bee_cx[i] < p_cx)      ph = DIR_RIGHT;
    else if (bee_cx[i] > p_cx) ph = DIR_LEFT;
    if (bee_cy[i] < p_cy)      pv = DIR_DOWN;
    else if (bee_cy[i] > p_cy) pv = DIR_UP;

    if (adiff(bee_cx[i], p_cx) >= adiff(bee_cy[i], p_cy)) {
        first  = ph;
        second = pv;
    } else {
        first  = pv;
        second = ph;
    }
    back = opposite(bee_dir[i]);

    if (first != DIR_NONE) {
        step_cell(first, bee_cx[i], bee_cy[i], &ncx, &ncy);
        if (walkable(ncx, ncy)) return first;
    }
    if (second != DIR_NONE) {
        step_cell(second, bee_cx[i], bee_cy[i], &ncx, &ncy);
        if (walkable(ncx, ncy)) return second;
    }
    /* wander: any legal direction that is not a straight reversal */
    d = 1 + cheap_rand(4);
    for (k = 0; k < 4; k++) {
        if (d != back) {
            step_cell(d, bee_cx[i], bee_cy[i], &ncx, &ncy);
            if (walkable(ncx, ncy)) return d;
        }
        d++;
        if (d > 4) d = 1;
    }
    return back;
}

void update_bees(void) {
    u8 i, ncx, ncy, gate;

    gate = BEE_TICK;
    if (sudden) gate = BEE_TICK_SD;

    for (i = 0; i < MAX_BEES; i++) {
        if (bee_on[i] == 0) continue;

        /* stunned: frozen in place, and lethal to nothing */
        if (bee_stun[i] > 0) {
            bee_stun[i] = bee_stun[i] - 1;
            if (p_alive && !p_invuln && p_cx == bee_cx[i] && p_cy == bee_cy[i]) {
                bee_on[i] = 0;
                UnsetSprite(SPR_BEE + (i << 2));
                UnsetSprite(SPR_BEE + (i << 2) + 1);
                UnsetSprite(SPR_BEE + (i << 2) + 2);
                UnsetSprite(SPR_BEE + (i << 2) + 3);
                score = score + 100;
                PlaySound(SND_STOMP);
            }
            continue;
        }

        bee_tick[i] = bee_tick[i] + 1;
        if (bee_tick[i] >= gate) {
            bee_tick[i] = 0;
            if (bee_sub[i] == 0) bee_dir[i] = pick_bee_dir(i);
            if (bee_dir[i] != DIR_NONE) {
                if (bee_dir[i] == DIR_UP)         bee_py[i] = bee_py[i] - BEE_STEP;
                else if (bee_dir[i] == DIR_DOWN)  bee_py[i] = bee_py[i] + BEE_STEP;
                else if (bee_dir[i] == DIR_LEFT)  bee_px[i] = bee_px[i] - BEE_STEP;
                else                              bee_px[i] = bee_px[i] + BEE_STEP;
                bee_sub[i] = bee_sub[i] + BEE_STEP;
                bee_anim[i] = bee_anim[i] + 1;
                if (bee_sub[i] >= 16) {
                    bee_sub[i] = 0;
                    step_cell(bee_dir[i], bee_cx[i], bee_cy[i], &ncx, &ncy);
                    bee_cx[i] = ncx;
                    bee_cy[i] = ncy;
                    bee_px[i] = ncx << 4;
                    bee_py[i] = PF_PY0 + (ncy << 4);
                }
            }
        }

        /* contact kills you, unless it is stunned (handled above) */
        if (p_alive && !p_invuln && p_cx == bee_cx[i] && p_cy == bee_cy[i])
            p_alive = 0;
    }
}

/* Called by the block in flight for every cell it enters, and for the cell
 * it lands in.  Multiple bees in one cell is possible, and the arcade pays
 * more for a multi-kill, so count them. */
u8 kill_bees_in_cell(u8 cx, u8 cy) {
    u8 i, n;

    n = 0;
    for (i = 0; i < MAX_BEES; i++) {
        if (bee_on[i] == 0) continue;
        if (bee_cx[i] != cx) continue;
        if (bee_cy[i] != cy) continue;
        bee_on[i] = 0;
        UnsetSprite(SPR_BEE + (i << 2));
        UnsetSprite(SPR_BEE + (i << 2) + 1);
        UnsetSprite(SPR_BEE + (i << 2) + 2);
        UnsetSprite(SPR_BEE + (i << 2) + 3);
        n++;
    }
    /* arcade scoring: doubling per extra bee in one shove */
    if (n == 1)      score = score + 400;
    else if (n == 2) score = score + 1600;
    else if (n >= 3) score = score + 3200;
    if (n) PlaySound(SND_SQUASH);
    return n;
}

u8 bees_alive(void) {
    u8 i, n;
    n = 0;
    for (i = 0; i < MAX_BEES; i++) if (bee_on[i]) n++;
    return n;
}

/* Punch the wall you are facing.  Any Sno-Bee sitting against that wall is
 * stunned; walk over it to finish the job. */
void wall_stun(u8 d) {
    u8 i, hit;

    hit = 0;
    for (i = 0; i < MAX_BEES; i++) {
        if (bee_on[i] == 0) continue;
        if (bee_stun[i]) continue;
        if (d == DIR_UP    && bee_cy[i] != 0)            continue;
        if (d == DIR_DOWN  && bee_cy[i] != CELLS_H - 1)  continue;
        if (d == DIR_LEFT  && bee_cx[i] != 0)            continue;
        if (d == DIR_RIGHT && bee_cx[i] != CELLS_W - 1)  continue;
        bee_stun[i] = STUN_TIME;
        bee_sub[i]  = 0;
        bee_px[i]   = bee_cx[i] << 4;
        bee_py[i]   = PF_PY0 + (bee_cy[i] << 4);
        hit++;
    }
    wall_shake = 8;
    if (hit) PlaySound(SND_STUN);
    else     PlaySound(SND_THUD);
}

void draw_bees(void) {
    u8 i, base, slot, pal;

    for (i = 0; i < MAX_BEES; i++) {
        slot = SPR_BEE + (i << 2);
        if (bee_on[i] == 0) continue;

        base = T_BEE;
        pal  = SP_BEE;

        if (bee_stun[i] > 0) {
            /* frozen: ice palette, and blink out in the last half second so
             * you know the window is closing */
            pal = SP_ICE;
            if (bee_stun[i] < 30 && (frame & 4)) {
                UnsetSprite(slot);
                UnsetSprite(slot + 1);
                UnsetSprite(slot + 2);
                UnsetSprite(slot + 3);
                continue;
            }
        } else if (bee_anim[i] & 4) {
            base = T_BEE + 4;
        }

        SetSprite(slot,     base,     0, bee_px[i],     bee_py[i],     pal);
        SetSprite(slot + 1, base + 1, 0, bee_px[i] + 8, bee_py[i],     pal);
        SetSprite(slot + 2, base + 2, 0, bee_px[i],     bee_py[i] + 8, pal);
        SetSprite(slot + 3, base + 3, 0, bee_px[i] + 8, bee_py[i] + 8, pal);
    }
}
