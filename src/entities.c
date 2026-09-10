/* entities.c - Pengu: the penguin, the block in flight, the crush
 *
 * THE ONE STRUCTURAL TRICK
 *   Static blocks live on scroll plane 1 as 2x2 tiles.  Only ever one block
 *   is in motion, so a push does:
 *       erase the source cell to field  ->  hand the block to 4 sprites  ->
 *       animate 4px/frame  ->  on stop, stamp it back into the tilemap and
 *       free the sprites.
 *   Smooth slide, no per-frame tilemap churn, no ShiftScroll abuse.
 *
 * The shatter is sprites too, for a different reason: the shards need
 * transparent gaps, and index 0 is only truly transparent on the sprite
 * plane.
 */
#include "entities.h"
#include "screen.h"
#include "tiles.h"
#include "sound.h"
#include "snobee.h"
#include "orca.h"

/* ---- forward declarations ---- */
void update_player(void);
void update_slide(void);
void update_crush(void);
void draw_player(void);
void hide_player(void);
void draw_slide(void);
void draw_crush(void);
static void try_move(u8 d);
static void try_push(u8 d, u8 tcx, u8 tcy);
static void try_crush(void);
static u8   facing_of(u8 d);
static u8   jammed(u8 d, u8 tcx, u8 tcy);

/* ---- direction -> facing (which baked penguin set to use) ---- */
static u8 facing_of(u8 d) {
    if (d == DIR_UP)    return FACE_UP;
    if (d == DIR_LEFT)  return FACE_LEFT;
    if (d == DIR_RIGHT) return FACE_RIGHT;
    return FACE_DOWN;
}

/* A block at (tcx,tcy) pushed in direction d is jammed when the cell
 * beyond it is not empty.  Out of bounds reads as ICE, so the enclosing
 * wall jams it too. */
static u8 jammed(u8 d, u8 tcx, u8 tcy) {
    u8 bcx, bcy, c;

    step_cell(d, tcx, tcy, &bcx, &bcy);
    c = cell_at(bcx, bcy);
    if (c == CELL_EMPTY) return 0;
    /* A hole is somewhere a block can GO, so it does not jam anything.  This
     * one line is what lets you shove a block into a hole in the bonus round
     * instead of crushing it against the edge of one. */
    if (c == CELL_HOLE) return 0;
    return 1;
}

/* ---- push ----
 * Canonical Pengo: the block slides away and the penguin stays put.  You
 * walk into the vacated cell on the next input if you want it.
 */
static void try_push(u8 d, u8 tcx, u8 tcy) {
    u8 kind;

    if (sl_on) return;              /* one block in flight at a time */
    if (cr_on) return;
    if (jammed(d, tcx, tcy)) return;   /* A crushes it instead */

    kind = grid[tcy][tcx];
    grid[tcy][tcx] = CELL_EMPTY;
    draw_cell(tcx, tcy);            /* repaint the source cell as field */
    /* An egg block shoved into a wall still shatters on landing, so pushing
     * eggs around is a legitimate (if risky) way to clear them. */

    sl_on   = 1;
    sl_kind = kind;
    sl_cx   = tcx;
    sl_cy   = tcy;
    sl_px   = tcx << 4;
    sl_py   = PF_PY0 + (tcy << 4);
    sl_dir  = d;
    sl_sub  = 0;
    PlaySound(SND_PUSH);
}

/* ---- crush ----
 * A, facing a jammed ice block.  Diamonds never break.
 */
static void try_crush(void) {
    u8 tcx, tcy, d, kind;

    if (sl_on) return;
    if (cr_on) return;
    if (p_moving) return;

    /* Which way are we crushing?  Held stick first, so "shove toward it and
     * hit A" works, then current facing.
     *
     * NOT p_dir: that only updates when a move SUCCEEDS, so walking into a
     * block leaves it pointing wherever you last actually travelled.  Turn
     * to face a jammed block with no neutral frame in between and A would
     * crush whatever was behind your shoulder instead. */
    d = DIR_NONE;
    if (pad_cur & J_UP)         d = DIR_UP;
    else if (pad_cur & J_DOWN)  d = DIR_DOWN;
    else if (pad_cur & J_LEFT)  d = DIR_LEFT;
    else if (pad_cur & J_RIGHT) d = DIR_RIGHT;

    if (d == DIR_NONE) {
        if (p_face == FACE_UP)         d = DIR_UP;
        else if (p_face == FACE_LEFT)  d = DIR_LEFT;
        else if (p_face == FACE_RIGHT) d = DIR_RIGHT;
        else                           d = DIR_DOWN;
    } else {
        p_face = facing_of(d);
    }

    step_cell(d, p_cx, p_cy, &tcx, &tcy);

    /* Facing the enclosing wall: punch it.  Any Sno-Bee against that wall
     * is stunned and can then be walked over.  Third kill method, and the
     * reason the wall is more than scenery. */
    if (!in_bounds(tcx, tcy)) {
        if (wall_cool == 0) {
            wall_stun(d);
            wall_cool = WALL_COOL;
        }
        return;
    }

    kind = grid[tcy][tcx];
    if (kind == CELL_GEM) return;              /* coconuts never break */
    if (kind == CELL_PALM) return;             /* nor do palm trunks */
    if (kind == CELL_EMPTY) return;
    if (kind == CELL_PINE) return;
    if (!jammed(d, tcx, tcy)) return;          /* not jammed - push it */

    /* Clear the cell immediately and paint field under it, then play the
     * shatter as sprites on top.  That way the shard gaps show ice, not
     * black. */
    grid[tcy][tcx] = CELL_EMPTY;
    draw_cell(tcx, tcy);

    cr_on  = 1;
    cr_cx  = tcx;
    cr_cy  = tcy;
    cr_tmr = CRUSH_TIME;
    if (kind == CELL_EGG) score = score + 500;  /* killed it before it hatched */
    else                  score = score + 10;
    PlaySound(SND_CRUSH);
}

static void try_move(u8 d) {
    u8 tcx, tcy, c;

    p_face = facing_of(d);
    step_cell(d, p_cx, p_cy, &tcx, &tcy);

    if (!in_bounds(tcx, tcy)) return;      /* face the wall, stay put */

    c = grid[tcy][tcx];
    if (c == CELL_EMPTY || c == CELL_PINE) {
        /* pineapples are walkable - you collect them by stepping on */
        p_dir    = d;
        p_moving = 1;
        p_sub    = 0;
        return;
    }
    if (c == CELL_HOLE) return;   /* stop at the edge; a hole is not a block */
    if (c == CELL_PALM) return;   /* a trunk is scenery you cannot shift */
    try_push(d, tcx, tcy);
}

void update_player(void) {
    u8 ncx, ncy;

    if (p_invuln > 0)   p_invuln = p_invuln - 1;
    if (wall_cool > 0)  wall_cool = wall_cool - 1;
    if (wall_shake > 0) wall_shake = wall_shake - 1;
    if (!p_alive) return;

    if (!p_moving) {
        if (pad_press & J_A) {
            try_crush();
            return;
        }
        /* Held, not pressed: walking should repeat while a direction is
         * held down. */
        if (pad_cur & J_UP)         try_move(DIR_UP);
        else if (pad_cur & J_DOWN)  try_move(DIR_DOWN);
        else if (pad_cur & J_LEFT)  try_move(DIR_LEFT);
        else if (pad_cur & J_RIGHT) try_move(DIR_RIGHT);
        else                        p_dir = DIR_NONE;
        return;
    }

    /* mid-cell: advance WALK_STEP px */
    if (p_dir == DIR_UP)         p_py = p_py - WALK_STEP;
    else if (p_dir == DIR_DOWN)  p_py = p_py + WALK_STEP;
    else if (p_dir == DIR_LEFT)  p_px = p_px - WALK_STEP;
    else                         p_px = p_px + WALK_STEP;

    p_sub  = p_sub + WALK_STEP;
    p_anim = p_anim + 1;

    if (p_sub >= 16) {
        step_cell(p_dir, p_cx, p_cy, &ncx, &ncy);
        p_cx     = ncx;
        p_cy     = ncy;
        if (grid[ncy][ncx] == CELL_PINE) {
            /* Points AND clarity: the canopy blows clear for a couple of
             * seconds, exposing everything hiding under the leaves. */
            grid[ncy][ncx] = CELL_EMPTY;
            draw_cell(ncx, ncy);
            score    = score + PINE_SCORE;
            lift_tmr = LIFT_TIME;
            PlaySound(SND_STOMP);
        }
        p_px     = ncx << 4;
        p_py     = PF_PY0 + (ncy << 4);
        p_sub    = 0;
        p_moving = 0;
    }
}

/* ---- the block in flight ----
 * Keeps going cell after cell until the next one is occupied.  4px/frame,
 * so 4 frames per cell - noticeably faster than the penguin walks, which
 * is what sells the shove.
 */
void update_slide(void) {
    u8 ncx, ncy;

    if (!sl_on) return;

    if (sl_dir == DIR_UP)         sl_py = sl_py - SLIDE_STEP;
    else if (sl_dir == DIR_DOWN)  sl_py = sl_py + SLIDE_STEP;
    else if (sl_dir == DIR_LEFT)  sl_px = sl_px - SLIDE_STEP;
    else                          sl_px = sl_px + SLIDE_STEP;

    sl_sub = sl_sub + SLIDE_STEP;
    if (sl_sub < 16) return;

    /* arrived in the next cell */
    step_cell(sl_dir, sl_cx, sl_cy, &ncx, &ncy);
    sl_cx  = ncx;
    sl_cy  = ncy;
    sl_px  = ncx << 4;
    sl_py  = PF_PY0 + (ncy << 4);
    sl_sub = 0;

    /* Squash check runs on every cell the block enters, not just where it
     * stops, so a long shove can take out a whole queue of Sno-Bees. */
    kill_bees_in_cell(sl_cx, sl_cy);

    /* Bonus round: a hole swallows the block, hit or miss. */
    if (grid[sl_cy][sl_cx] == CELL_HOLE) {
        resolve_hole(sl_cx, sl_cy);
        UnsetSprite(SPR_SLIDE);
        UnsetSprite(SPR_SLIDE + 1);
        UnsetSprite(SPR_SLIDE + 2);
        UnsetSprite(SPR_SLIDE + 3);
        sl_on = 0;
        return;
    }

    if (jammed(sl_dir, sl_cx, sl_cy)) {
        /* land it: back into the tilemap, sprites released */
        grid[sl_cy][sl_cx] = sl_kind;
        draw_cell(sl_cx, sl_cy);
        UnsetSprite(SPR_SLIDE);
        UnsetSprite(SPR_SLIDE + 1);
        UnsetSprite(SPR_SLIDE + 2);
        UnsetSprite(SPR_SLIDE + 3);
        sl_on = 0;
        PlaySound(SND_LAND);
    }
}

void update_crush(void) {
    if (!cr_on) return;
    if (cr_tmr > 0) cr_tmr = cr_tmr - 1;
    if (cr_tmr == 0) {
        UnsetSprite(SPR_CRUSH);
        UnsetSprite(SPR_CRUSH + 1);
        UnsetSprite(SPR_CRUSH + 2);
        UnsetSprite(SPR_CRUSH + 3);
        cr_on = 0;
    }
}

/* ---- drawing ---- */
/* The blink in run_dead() ends on a DRAW frame (state_tmr hits 0 and 0 & 4
 * is 0), and nothing cleared it afterwards - so the penguin sat on screen
 * underneath the GAME OVER banner. */
void hide_player(void) {
    UnsetSprite(SPR_PENG);
    UnsetSprite(SPR_PENG + 1);
    UnsetSprite(SPR_PENG + 2);
    UnsetSprite(SPR_PENG + 3);
}

void draw_player(void) {
    u8 base, f;

    /* No blink here.  It used to flicker through the invulnerability
     * window, but init_level() grants that window on every level start, not
     * just a respawn - so the penguin spent the first second of every round
     * half invisible and looked like he only appeared once you moved.
     * The immunity stays (silent insurance); the flicker does not. */

    /* two-frame walk cycle while moving, standing frame at rest */
    f = 0;
    if (p_moving && (p_anim & 4)) f = 1;

    /* table order: D0 D1 U0 U1 R0 R1 L0 L1, 4 tiles each */
    base = T_PENG + (p_face << 3) + (f << 2);

    SetSprite(SPR_PENG,     base,     0, p_px,     p_py,     SP_PENG);
    SetSprite(SPR_PENG + 1, base + 1, 0, p_px + 8, p_py,     SP_PENG);
    SetSprite(SPR_PENG + 2, base + 2, 0, p_px,     p_py + 8, SP_PENG);
    SetSprite(SPR_PENG + 3, base + 3, 0, p_px + 8, p_py + 8, SP_PENG);
}

void draw_slide(void) {
    u8 base;

    if (!sl_on) return;

    base = T_ICE;
    if (sl_kind == CELL_GEM) base = T_GEM;

    SetSprite(SPR_SLIDE,     base,     0, sl_px,     sl_py,     SP_ICE);
    SetSprite(SPR_SLIDE + 1, base + 1, 0, sl_px + 8, sl_py,     SP_ICE);
    SetSprite(SPR_SLIDE + 2, base + 2, 0, sl_px,     sl_py + 8, SP_ICE);
    SetSprite(SPR_SLIDE + 3, base + 3, 0, sl_px + 8, sl_py + 8, SP_ICE);
}

void draw_crush(void) {
    u8 base, px, py;

    if (!cr_on) return;

    base = T_SHAT;
    if (cr_tmr < (CRUSH_TIME >> 1)) base = T_SHAT + 4;

    px = cr_cx << 4;
    py = PF_PY0 + (cr_cy << 4);

    SetSprite(SPR_CRUSH,     base,     0, px,     py,     SP_ICE);
    SetSprite(SPR_CRUSH + 1, base + 1, 0, px + 8, py,     SP_ICE);
    SetSprite(SPR_CRUSH + 2, base + 2, 0, px,     py + 8, SP_ICE);
    SetSprite(SPR_CRUSH + 3, base + 3, 0, px + 8, py + 8, SP_ICE);
}
