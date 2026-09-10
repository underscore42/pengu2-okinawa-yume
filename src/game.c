/* game.c - Pengu: shared state, cell model, level setup
 *
 * Fields are authored data in maps.h, not geometry.  init_level() copies the
 * current level's field into grid[][] and picks up the penguin start from
 * the '@' marker.  tools/checkmaps.py validates every field for
 * reachability and slide quality before it gets here.
 */
#include "game.h"
#include "maps.h"

/* ---- grid ---- */
u8 grid[CELLS_H][CELLS_W];

/* ---- global state ---- */
u8  state, frame, paused;
u8  skip;
u16 score;
u8  pad_cur, pad_prev, pad_press;
u8  rand_seed;
u8  level, lives, sudden;
u16 level_frames;
u16 hatch_tmr;
u8  state_tmr, wall_cool, wall_shake;
u16 high_scores[5];
u8  konami_on;
u8  since_bonus, bonus_round, inter_done;
u16 bonus_tmr;
u8  whacks, holes_total;
u8  bonus_secs, bonus_sixty;
u8  or_on, or_cx, or_cy, or_tmr, or_gap, or_frame;
u8  palm_n;
u8  palm_cx[MAX_PALMS], palm_cy[MAX_PALMS];
u16 palm_mask[MAX_PALMS];
u16 spawn_tmr;
u8  spawns_left;
u8  lift_tmr, sway_i;

/* ---- penguin ---- */
u8 p_cx, p_cy, p_px, p_py;
u8 p_dir, p_sub, p_moving, p_face, p_anim, p_alive, p_invuln;

/* ---- sliding block ---- */
u8 sl_on, sl_cx, sl_cy, sl_px, sl_py, sl_dir, sl_sub, sl_kind;

/* ---- crushing block ---- */
u8 cr_on, cr_cx, cr_cy, cr_tmr;

/* ---- Sno-Bees ---- */
u8 bee_on[MAX_BEES];
u8 bee_cx[MAX_BEES], bee_cy[MAX_BEES];
u8 bee_px[MAX_BEES], bee_py[MAX_BEES];
u8 bee_dir[MAX_BEES], bee_sub[MAX_BEES], bee_tick[MAX_BEES];
u8 bee_anim[MAX_BEES], bee_stun[MAX_BEES];

/* ---- forward declarations ---- */
u8   cheap_rand(u8 max);
u8   adiff(u8 a, u8 b);
u8   opposite(u8 d);
u8   cell_at(u8 cx, u8 cy);
u8   in_bounds(u8 cx, u8 cy);
u8   walkable(u8 cx, u8 cy);
void step_cell(u8 d, u8 cx, u8 cy, u8 *ncx, u8 *ncy);
u8   eggs_left(void);
u8   holes_left(void);
u8   pines_left(void);
u8   blocks_left(void);
u8   level_start_bees(void);
void init_bonus(void);
u8   bonus_due(void);
u8   inter_due(void);
void init_level(void);
void new_game(void);

/* ---- RNG ----
 * (seed * 5 + 1) mod 256, full 256 cycle.  Reduction is a subtract loop
 * because cc900's signed-mod codegen is unreliable, and 5*x is (x<<2)+x
 * because there is no signed multiply.
 */
/* Eight canopy silhouettes. Authored, never random: the dead ground is
 * knowledge the player learns across attempts, so it must be identical every
 * run. Bit order is row-major from the top-left; bit 4 is the trunk's own
 * cell and is always set. */
static const u16 canopy_shape[8] = {
    0x0BA,   /* .#. ### .#.  - 5 cells */
    0x1FE,   /* .## ### ##.  - 7 */
    0x1DB,   /* ##. ### .##  - 7 */
    0x0BE,   /* .#. ### ##.  - 6 */
    0x1BA,   /* .## ### .#.  - 6 */
    0x03A,   /* ... ### .#.  - 4 */
    0x0B8,   /* .#. ### ...  - 4 */
    0x1B8    /* ##. ### ...  - 5 */
};

u8 cheap_rand(u8 max) {
    u8 r;
    rand_seed = ((rand_seed << 2) + rand_seed + 1) & 0xFF;
    if (max == 0) return 0;
    r = rand_seed;
    while (r >= max) r = r - max;
    return r;
}

u8 adiff(u8 a, u8 b) {
    if (a > b) return a - b;
    return b - a;
}

u8 opposite(u8 d) {
    if (d == DIR_UP)    return DIR_DOWN;
    if (d == DIR_DOWN)  return DIR_UP;
    if (d == DIR_LEFT)  return DIR_RIGHT;
    if (d == DIR_RIGHT) return DIR_LEFT;
    return DIR_NONE;
}

/* ---- cell access ----
 * Out of bounds reads as ICE, so the enclosing wall needs no cells of its
 * own: blocks stop against it and you can crush against it, exactly like a
 * real neighbour.  Costs 4 edge tiles instead of a 34-cell ring.
 */
u8 in_bounds(u8 cx, u8 cy) {
    if (cx >= CELLS_W) return 0;
    if (cy >= CELLS_H) return 0;
    return 1;
}

u8 cell_at(u8 cx, u8 cy) {
    if (!in_bounds(cx, cy)) return CELL_ICE;
    return grid[cy][cx];
}

u8 walkable(u8 cx, u8 cy) {
    if (!in_bounds(cx, cy)) return 0;
    if (grid[cy][cx] != CELL_EMPTY) return 0;
    return 1;
}

/* Unsigned coords, so a step off the top or left wraps to 255 and
 * in_bounds() rejects it.  Intentional - do not "fix" it with signed
 * arithmetic, cc900 will punish you.
 */
void step_cell(u8 d, u8 cx, u8 cy, u8 *ncx, u8 *ncy) {
    *ncx = cx;
    *ncy = cy;
    if (d == DIR_UP)         *ncy = cy - 1;
    else if (d == DIR_DOWN)  *ncy = cy + 1;
    else if (d == DIR_LEFT)  *ncx = cx - 1;
    else if (d == DIR_RIGHT) *ncx = cx + 1;
}

u8 eggs_left(void) {
    u8 x, y, n;
    n = 0;
    for (y = 0; y < CELLS_H; y++)
        for (x = 0; x < CELLS_W; x++)
            if (grid[y][x] == CELL_EGG) n++;
    return n;
}

/* maps.h is private to this module, so the table is reached through here
 * rather than leaking the header into main.c. */
u8 level_start_bees(void) {
    u8 m;
    m = level;
    while (m >= NUM_LEVELS) m = m - NUM_LEVELS;
    return lvl_start_bees[m];
}

void init_level(void) {
    u8 x, y, m;
    char c;

    m = level;
    while (m >= NUM_LEVELS) m = m - NUM_LEVELS;   /* levels wrap, arcade style */

    p_cx = 0;
    p_cy = CELLS_H - 1;
    palm_n = 0;

    for (y = 0; y < CELLS_H; y++) {
        for (x = 0; x < CELLS_W; x++) {
            c = ice_maps[m][y][x];
            if (c == '#')      grid[y][x] = CELL_ICE;
            else if (c == 'e') grid[y][x] = CELL_EGG;
            else if (c == '*') grid[y][x] = CELL_GEM;
            else if (c == 'P') grid[y][x] = CELL_PINE;
            else if (c == 'T') {
                grid[y][x] = CELL_PALM;
                if (palm_n < MAX_PALMS) {
                    palm_cx[palm_n]   = x;
                    palm_cy[palm_n]   = y;
                    palm_mask[palm_n] = canopy_shape[palm_n & 7];
                    palm_n = palm_n + 1;
                }
            }
            else {
                grid[y][x] = CELL_EMPTY;
                if (c == '@') {
                    p_cx = x;
                    p_cy = y;
                }
            }
        }
    }

    p_px     = p_cx << 4;
    p_py     = PF_PY0 + (p_cy << 4);
    p_dir    = DIR_NONE;
    p_face   = FACE_DOWN;
    p_sub    = 0;
    p_moving = 0;
    p_anim   = 0;
    p_alive  = 1;
    p_invuln = INVULN_TIME;

    sl_on        = 0;
    cr_on        = 0;
    paused       = 0;
    sudden       = 0;
    level_frames = 0;
    hatch_tmr    = HATCH_TIME;
    spawn_tmr    = SPAWN_TIME;
    spawns_left  = 6 + m;   /* how many crabs the sea sends this round */
    lift_tmr     = 0;
    sway_i       = 0;
    wall_cool    = 0;
    wall_shake   = 0;
}


u8 pines_left(void) {
    u8 x, y, n;
    n = 0;
    for (y = 0; y < CELLS_H; y++)
        for (x = 0; x < CELLS_W; x++)
            if (grid[y][x] == CELL_PINE) n++;
    return n;
}

u8 holes_left(void) {
    u8 x, y, n;
    n = 0;
    for (y = 0; y < CELLS_H; y++)
        for (x = 0; x < CELLS_W; x++)
            if (grid[y][x] == CELL_HOLE) n++;
    return n;
}

u8 blocks_left(void) {
    u8 x, y, n;
    n = 0;
    for (y = 0; y < CELLS_H; y++)
        for (x = 0; x < CELLS_W; x++)
            if (grid[y][x] == CELL_ICE) n++;
    return n;
}

/* Bonus round: its own field, no eggs, no Sno-Bees, no diamonds. */
void init_bonus(void) {
    u8 x, y;
    char c;

    p_cx = 0;
    p_cy = CELLS_H - 1;

    for (y = 0; y < CELLS_H; y++) {
        for (x = 0; x < CELLS_W; x++) {
            c = bonus_map[y][x];
            if (c == '#')      grid[y][x] = CELL_ICE;
            else if (c == 'o') grid[y][x] = CELL_HOLE;
            else {
                grid[y][x] = CELL_EMPTY;
                if (c == '@') {
                    p_cx = x;
                    p_cy = y;
                }
            }
        }
    }

    p_px     = p_cx << 4;
    p_py     = PF_PY0 + (p_cy << 4);
    p_dir    = DIR_NONE;
    p_face   = FACE_UP;
    p_sub    = 0;
    p_moving = 0;
    p_anim   = 0;
    p_alive  = 1;
    p_invuln = 0;

    sl_on       = 0;
    cr_on       = 0;
    paused      = 0;
    sudden      = 0;
    wall_cool   = 0;
    wall_shake  = 0;

    bonus_tmr   = BONUS_TIME;
    bonus_secs  = 30;
    bonus_sixty = 60;
    whacks      = 0;
    holes_total = holes_left();
    or_on       = 0;
    or_gap      = ORCA_GAP;
    or_tmr      = 0;
    or_frame    = 0;
}

/* Called once per field cleared.  Returns 1 when the next stage should be
 * the bonus round.  Lives here rather than in main.c so it can be tested -
 * see tools/test_stages.c.
 *
 * Sequence: field 1,2,3,4, BONUS, 1,2,3,4, BONUS ... i.e. every 5th stage.
 * Konami makes it every stage.
 */
u8 bonus_due(void) {
    if (konami_on) {
        since_bonus = 0;
        return 1;
    }
    since_bonus = since_bonus + 1;
    if (since_bonus >= BONUS_EVERY) {
        since_bonus = 0;
        return 1;
    }
    return 0;
}

/* Intermissions land after fields 2, 6 and 14, then stop.
 *
 * All three are 2 mod 4. Bonus rounds are every 4th field, so this guarantees
 * a plain field between an intermission and an orca round - avoiding not just
 * a collision but a back-to-back reward chunk. The first schedule tried was
 * 3, 7, 13, which collided with nothing yet still put every intermission
 * directly next to a bonus. tools/test_stages.c asserts the gap.
 *
 * Spacing doubles - 2, then 4, then 8 - so the last one arrives after you
 * have stopped expecting them. Three scenes in a run, then gone.
 */
static const u8 inter_at[NUM_INTER] = { 2, 6, 14, 22 };  /* 4th = crab rave */

u8 inter_due(void) {
    if (inter_done >= NUM_INTER) return 0;
    if (level == inter_at[inter_done]) {
        inter_done = inter_done + 1;
        return 1;
    }
    return 0;
}

void new_game(void) {
    score = 0;
    frame = 0;
    level = 0;
    lives = START_LIVES;
    since_bonus = 0;
    bonus_round = 0;
    inter_done  = 0;
    konami_on = 0;
    rand_seed = 42;
    init_level();
}
