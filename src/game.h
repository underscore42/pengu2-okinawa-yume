/* game.h - Pengu: geometry, cell model, shared state
 * Studio So Not Kansai / underscore42
 *
 * Single screen, no scrolling.  160x152 splits as:
 *   tile row 0        HUD band (8px).  Deliberately dark: scroll-plane
 *                     index 0 is opaque and col0 is forced to 0, so text
 *                     glyphs carry a black cell.  Reads as an arcade bezel.
 *   tile rows 1-18    playfield, 18 rows = 9 cell rows x 16px = 144px
 *                     8 + 144 = 152 exactly.
 *
 * Cell (cx,cy) -> tile (cx<<1, PF_Y0 + (cy<<1))
 *              -> pixel (cx<<4, PF_PY0 + (cy<<4))
 */
#ifndef GAME_H
#define GAME_H

#include "ngpc.h"
#include "library.h"

/* ---- states ---- */
#define STATE_TITLE  0
#define STATE_GAME   1
#define STATE_DEAD   2
#define STATE_CLEAR  3
#define STATE_OVER   4
#define STATE_SCORES 5
#define STATE_BONUS  6   /* whack-a-orca round */
#define STATE_INTER  7   /* three-little-pigs intermission */

/* ---- geometry ---- */
#define CELLS_W  10
#define CELLS_H   9
#define PF_Y0     1
#define PF_PY0    8
#define HUD_Y     0

/* ---- cell contents ---- */
#define CELL_EMPTY 0
#define CELL_ICE   1
#define CELL_GEM   2
#define CELL_EGG   3    /* ice block with a Sno-Bee egg in it.  Behaves like
                         * ice for pushing and blocking; crush it and the egg
                         * dies before it hatches. */
#define CELL_HOLE  4    /* bonus round only: a hole through the ice.  Not
                         * walkable, but a sliding block DROPS INTO it -
                         * see jammed(), which reports a hole as passable. */
#define CELL_PALM  5    /* palm trunk. Neither pushable nor crushable - a
                         * piece of wall dropped in the middle of the field.
                         * The canopy above it is PURELY VISUAL: blocks and
                         * crabs pass under fronds freely. Only the trunk is
                         * solid. */
#define CELL_PINE  6    /* pineapple pickup. Walkable - you collect it by
                         * stepping on it. */

/* ---- directions ---- */
#define DIR_NONE  0
#define DIR_UP    1
#define DIR_DOWN  2
#define DIR_LEFT  3
#define DIR_RIGHT 4

/* ---- facings, index into the penguin tile table ---- */
#define FACE_DOWN  0
#define FACE_UP    1
#define FACE_RIGHT 2
#define FACE_LEFT  3

/* ---- scroll plane 1 palettes ---- */
#define PAL_KANA_B 0
#define PAL_KANA_W 1
#define PAL_ICE    2
#define PAL_GEM    3
#define PAL_FIELD  4
#define PAL_EDGE   5
#define PAL_EGG    6
#define PAL_HOLE   7
#define PAL_TRUNK  8
#define PAL_FROND  9    /* scroll plane 2 */
#define PAL_PINE  10    /* T_FIELD on dark water - a hole costs zero tiles */    /* same ICE tiles, animated palette - an egg block
                         * costs zero tiles, it just pulses */

/* ---- sprite palettes: slots 0-3 ONLY.  4-7 wrap and clobber. ---- */
#define SP_PENG 0
#define SP_ICE  1
#define SP_BEE  2
#define SP_TEXT 3

/* ---- sprite slots ---- */
#define SPR_PENG   0    /* 4 */
#define SPR_SLIDE  4    /* 4 */
#define SPR_CRUSH  8    /* 4 */
#define SPR_BEE   12    /* MAX_BEES * 4 = 24 */
#define SPR_ORCA  12    /* 4: shares the Sno-Bee slots.  The bees are off in
                         * the bonus round, so nothing overlaps. */
#define SPR_TEXT  40    /* up to 8 */

/* ---- speeds and timings ---- */
#define WALK_STEP   2   /* px/frame: 8 frames per cell */
#define SLIDE_STEP  4   /* px/frame: 4 frames per cell - faster than you
                         * walk, which is what sells the shove */
#define CRUSH_TIME  16
#define BEE_STEP    2
#define BEE_TICK    2   /* move every 2nd frame: half the penguin's pace */
#define BEE_TICK_SD 1   /* sudden death: matches the penguin */
#define STUN_TIME   120
#define INVULN_TIME 60
#define WALL_COOL   24
#define HATCH_TIME  300
#define SUDDEN_AT   3600  /* 60 seconds */
#define MAX_BEES    6
#define START_LIVES 3
#define NUM_LEVELS  4

/* ---- bonus round: whack-a-orca ---- */
#define BONUS_TIME  1800  /* 30 seconds */
#define ORCA_UP_MAX 90    /* surfacing window at the start */
#define ORCA_UP_MIN 30    /* ...and once you are good at it */
#define ORCA_GAP    40    /* frames submerged between appearances */
#define WHACK_SCORE 500
#define PERFECT_BONUS 5000
#define BONUS_EVERY 4
#define NUM_INTER   4

/* ---- Pengu 2: tropical ---- */
#define MAX_PALMS   6
#define SPAWN_TIME  240   /* frames between crabs crawling out of the surf */
#define LIFT_TIME   150   /* pineapple: canopy blows clear for 2.5s */
#define PINE_SCORE  300     /* three scenes, then never again */     /* fields cleared between bonus rounds: 1,2,3,4,
                           * BONUS, 1,2,3,4, BONUS ... i.e. every 5th stage */

/* ---- grid ---- */
extern u8 grid[CELLS_H][CELLS_W];

/* ---- global state ---- */
extern u8  state, frame, paused;
extern u8  skip;    /* frames of input to swallow after a state change,
                     * so one press cannot bleed through a transition */
extern u16 score;
extern u8  pad_cur, pad_prev, pad_press;
extern u8  rand_seed;
extern u8  level, lives, sudden;
extern u16 level_frames;
extern u16 hatch_tmr;
extern u8  state_tmr, wall_cool, wall_shake;
extern u16 high_scores[5];
extern u8  konami_on;    /* Konami: bonus round after EVERY field instead of
                          * every fifth stage */
extern u8  inter_done;   /* how many intermissions have played */
extern u8  since_bonus;  /* fields cleared since the last bonus round */
extern u8  bonus_round;  /* how many bonus rounds have been played - the
                          * surfacing window starts tighter each time */
extern u16 bonus_tmr;
extern u8  whacks, holes_total;
extern u8  bonus_secs, bonus_sixty;
extern u8  or_on, or_cx, or_cy, or_tmr, or_gap, or_frame;

/* ---- palms: position plus a 9-bit canopy mask, so no two share a
 *      silhouette. Bit 4 (centre) is always set. ---- */
extern u8  palm_n;
extern u8  palm_cx[MAX_PALMS], palm_cy[MAX_PALMS];
extern u16 palm_mask[MAX_PALMS];
extern u16 spawn_tmr;
extern u8  spawns_left;   /* crabs the surf has left to send this round */
extern u8  lift_tmr, sway_i;

/* ---- penguin ---- */
extern u8 p_cx, p_cy, p_px, p_py;
extern u8 p_dir, p_sub, p_moving, p_face, p_anim, p_alive, p_invuln;

/* ---- the sliding block: exactly one at a time ---- */
extern u8 sl_on, sl_cx, sl_cy, sl_px, sl_py, sl_dir, sl_sub, sl_kind;

/* ---- the crushing block ---- */
extern u8 cr_on, cr_cx, cr_cy, cr_tmr;

/* ---- Sno-Bees ---- */
extern u8 bee_on[MAX_BEES];
extern u8 bee_cx[MAX_BEES], bee_cy[MAX_BEES];
extern u8 bee_px[MAX_BEES], bee_py[MAX_BEES];
extern u8 bee_dir[MAX_BEES], bee_sub[MAX_BEES], bee_tick[MAX_BEES];
extern u8 bee_anim[MAX_BEES], bee_stun[MAX_BEES];

/* ---- helpers ---- */
u8   cheap_rand(u8 max);
u8   adiff(u8 a, u8 b);
u8   opposite(u8 d);
u8   cell_at(u8 cx, u8 cy);
u8   in_bounds(u8 cx, u8 cy);
u8   walkable(u8 cx, u8 cy);
void step_cell(u8 d, u8 cx, u8 cy, u8 *ncx, u8 *ncy);
u8   eggs_left(void);
u8   level_start_bees(void);
void init_level(void);
void init_bonus(void);
u8   bonus_due(void);
u8   inter_due(void);
u8   holes_left(void);
u8   pines_left(void);
u8   blocks_left(void);
void new_game(void);

#endif
