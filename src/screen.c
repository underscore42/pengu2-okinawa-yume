/* screen.c - Pengu: palettes, playfield, HUD, front end
 *
 * PLANE / PALETTE ARCHITECTURE (decided up front, on purpose):
 *
 *   The ice field does NOT come from index 0.  SetPalette col0 is forced to
 *   0 and scroll-plane index 0 is opaque, so index 0 paints black on both
 *   planes.  Blue Print never exposed this because its field is black.
 *   Here every playfield pixel is painted in indices 1-3, and an empty cell
 *   is an explicit T_FIELD tile rather than a gap.  Nothing relies on
 *   scroll-plane transparency, so there is no ghost-transparency to fight.
 *
 *   Plane 1 carries the whole playfield, the edge rules and the HUD.
 *   Plane 2 is unused.
 *   Sprites carry the penguin, the block in flight, the shatter, the
 *   Sno-Bees and any text over the field, because index 0 IS transparent
 *   there.
 *
 * The four levels are one tile set with four palette themes.  Retinting is
 * free; new tiles are not.  Level 3 is the desert island purely by palette -
 * same ice tiles, sand colours.
 *
 * Font order is always: palettes -> SysSetSystemFont -> install_tiles ->
 * wipe -> draw.  Never wipe before the font call or the Japanese charset
 * bleeds through.
 */
#include "screen.h"
#include "tiles.h"
#include "kana.h"

/* ---- forward declarations ---- */
void setup_palettes(void);
void apply_theme(u8 lv);
void pulse_egg_palette(void);
void wipe_screen(void);
void draw_cell(u8 cx, u8 cy);
void draw_playfield(void);
u16  frame_tile(u8 tx, u8 ty);
static void put_frame(u8 tx, u8 ty);
static void put_frame_ring(u8 cx, u8 cy);
void init_hud(void);
void draw_hud(void);
void draw_title(void);
void anim_title(void);
void draw_scores(void);
void clear_all_sprites(void);
void sprite_kana(u8 slot, u8 x, u8 y, const u8 *codes);
void clear_sprite_kana(void);
void show_pause(u8 on);
void show_banner(const u8 *codes);
static u8 kana_len(const u8 *codes);

/* ---- level themes ----
 * Each row: field r,g,b | ice face | ice shadow | ice highlight | bee body
 * All four levels share one tile set.  Level 3 reads as sand, level 4 as
 * volcanic rock, purely from these numbers.
 */
static const u8 theme_field[NUM_LEVELS][3] = {
    { 14, 12,  7 },   /* 1 beach       */
    { 13, 11,  6 },   /* 2 late sun    */
    { 11,  9,  5 },   /* 3 dusk sand   */
    {  9,  7,  4 }    /* 4 night beach */
};
static const u8 theme_face[NUM_LEVELS][3] = {
    { 13, 10,  5 }, { 12,  9,  5 }, { 11,  8,  4 }, {  9,  7,  4 }
};
static const u8 theme_shad[NUM_LEVELS][3] = {
    {  8,  5,  2 }, {  7,  4,  2 }, {  6,  3,  1 }, {  4,  2,  1 }
};
static const u8 theme_high[NUM_LEVELS][3] = {
    { 15, 14, 10 }, { 14, 13,  9 }, { 13, 11,  8 }, { 11, 10,  7 }
};
/* Arcade recolours the Sno-Bees every round; this is that cycle, trimmed. */
static const u8 theme_bee[NUM_LEVELS][3] = {
    { 14,  3,  2 },   /* red crab    */
    { 15,  6,  2 },   /* orange      */
    { 13,  2,  6 },   /* purple      */
    { 15,  9,  3 }    /* sand crab   */
};

static u8 cur_theme;

static u8 kana_len(const u8 *codes) {
    u8 n;
    n = 0;
    while (*codes) { n++; codes++; }
    return n;
}

/* ---- palettes ---- */
void setup_palettes(void) {
    SetPalette(SCR_1_PLANE, PAL_KANA_B, 0,
               RGB(2, 5, 12), RGB(15, 15, 15), RGB(8, 11, 14));
    SetPalette(SCR_1_PLANE, PAL_KANA_W, 0,
               RGB(15, 15, 15), RGB(2, 5, 12), RGB(8, 11, 14));
    SetPalette(SCR_1_PLANE, PAL_EDGE, 0,
               RGB(2, 5, 12), RGB(15, 15, 15), RGB(8, 11, 14));
    /* A hole is T_FIELD (solid index 1) on dark water - zero extra tiles. */
    SetPalette(SCR_1_PLANE, PAL_HOLE, 0,
               RGB(0, 2, 6), RGB(0, 2, 6), RGB(0, 2, 6));
    SetPalette(SCR_1_PLANE, PAL_TRUNK, 0,
               RGB(7, 4, 2), RGB(3, 2, 1), RGB(10, 7, 4));
    SetPalette(SCR_1_PLANE, PAL_PINE, 0,
               RGB(15, 13, 3), RGB(11, 8, 1), RGB(6, 13, 4));
    /* Canopy lives on plane 2. Variant B, deep shade: chosen for VALUE
     * contrast, not hue - a washed-out LCD in sunlight destroys hue
     * separation but keeps light-versus-dark. */
    SetPalette(SCR_2_PLANE, PAL_FROND, 0,
               RGB(2, 7, 4), RGB(0, 4, 2), RGB(4, 10, 5));

    /* Sprite plane: slots 0-3 only.  Slot 4+ wraps and clobbers. */
    SetPalette(SPRITE_PLANE, SP_PENG, 0,
               RGB(1, 1, 3), RGB(15, 15, 15), RGB(15, 9, 1));
    SetPalette(SPRITE_PLANE, SP_ICE, 0,
               RGB(11, 14, 15), RGB(3, 6, 11), RGB(15, 15, 15));
    SetPalette(SPRITE_PLANE, SP_BEE, 0,
               RGB(4, 14, 4), RGB(3, 2, 1), RGB(15, 15, 15));
    SetPalette(SPRITE_PLANE, SP_TEXT, 0,
               RGB(15, 15, 15), RGB(2, 5, 12), RGB(8, 11, 14));

    apply_theme(0);
}

/* One call retints the whole level.  This is why four levels cost four map
 * strings and nothing else. */
void apply_theme(u8 lv) {
    u8 t;

    t = lv;
    while (t >= NUM_LEVELS) t = t - NUM_LEVELS;
    cur_theme = t;

    SetBackgroundColour(RGB(theme_field[t][0], theme_field[t][1],
                            theme_field[t][2]));
    SetWindowColor(RGB(theme_field[t][0], theme_field[t][1],
                       theme_field[t][2]));

    SetPalette(SCR_1_PLANE, PAL_FIELD, 0,
               RGB(theme_field[t][0], theme_field[t][1], theme_field[t][2]),
               RGB(theme_field[t][0], theme_field[t][1], theme_field[t][2]),
               RGB(theme_field[t][0], theme_field[t][1], theme_field[t][2]));

    SetPalette(SCR_1_PLANE, PAL_ICE, 0,
               RGB(theme_face[t][0], theme_face[t][1], theme_face[t][2]),
               RGB(theme_shad[t][0], theme_shad[t][1], theme_shad[t][2]),
               RGB(theme_high[t][0], theme_high[t][1], theme_high[t][2]));

    /* Coconuts, not diamonds. This inherited Pengu 1's ice-blue gem palette
     * and rendered as blue rocks on a beach. */
    SetPalette(SCR_1_PLANE, PAL_GEM, 0,
               RGB(theme_field[t][0], theme_field[t][1], theme_field[t][2]),
               RGB(7, 4, 1), RGB(12, 9, 5));

    SetPalette(SPRITE_PLANE, SP_BEE, 0,
               RGB(theme_bee[t][0], theme_bee[t][1], theme_bee[t][2]),
               RGB(3, 2, 1), RGB(15, 15, 15));

    SetPalette(SPRITE_PLANE, SP_ICE, 0,
               RGB(theme_face[t][0], theme_face[t][1], theme_face[t][2]),
               RGB(theme_shad[t][0], theme_shad[t][1], theme_shad[t][2]),
               RGB(theme_high[t][0], theme_high[t][1], theme_high[t][2]));
}

/* Egg blocks are the ICE tiles on an animated palette - zero extra tiles,
 * and the pulse reads as something alive inside. */
void pulse_egg_palette(void) {
    u8 t, k;

    t = cur_theme;
    /* pulse the block face between its normal ice colour and a warm glow -
     * reads as something alive inside, rather than a mud-coloured brick */
    k = 9;
    if (frame & 16) k = 14;

    SetPalette(SCR_1_PLANE, PAL_EGG, 0,
               RGB(15, k, k - 3),
               RGB(theme_shad[t][0], theme_shad[t][1], theme_shad[t][2]),
               RGB(15, 15, 13));
}

/* ClearScreen() writes tile 0, which is a visible font glyph.  Always wipe
 * with an explicit space instead. */
void wipe_screen(void) {
    FillScreen(SCR_1_PLANE, ' ', PAL_KANA_B);
    FillScreen(SCR_2_PLANE, ' ', PAL_KANA_B);
}

void clear_all_sprites(void) {
    u8 i;
    for (i = 0; i < 64; i++) UnsetSprite(i);
}

/* ---- one cell = 2x2 tiles ---- */
/* T_FIELD is solid index 1, so it must go down under PAL_FIELD; the rule
 * tiles want PAL_EDGE.  One helper keeps that straight. */
static void put_frame(u8 tx, u8 ty) {
    u16 t;
    t = frame_tile(tx, ty);
    if (t == T_FIELD) PutTile(SCR_1_PLANE, PAL_FIELD, tx, ty, t);
    else              PutTile(SCR_1_PLANE, PAL_EDGE,  tx, ty, t);
}

/* A hole or block on the border ring has eaten part of the wall rule; only
 * the tiles the cell does NOT cover can be restored, so redraw the whole
 * ring frame for that cell's edges. */
static void put_frame_ring(u8 cx, u8 cy) {
    u8 tx, ty;
    tx = cx << 1;
    ty = PF_Y0 + (cy << 1);
    if (cx == 0) {
        put_frame(tx, ty);
        put_frame(tx, ty + 1);
    }
    if (cx == (CELLS_W - 1)) {
        put_frame(tx + 1, ty);
        put_frame(tx + 1, ty + 1);
    }
    if (cy == 0) {
        put_frame(tx, ty);
        put_frame(tx + 1, ty);
    }
    if (cy == (CELLS_H - 1)) {
        put_frame(tx, ty + 1);
        put_frame(tx + 1, ty + 1);
    }
}

void draw_cell(u8 cx, u8 cy) {
    u8 tx, ty, c, pal;
    u16 base;

    tx = cx << 1;
    ty = PF_Y0 + (cy << 1);
    c  = grid[cy][cx];

    if (c == CELL_EMPTY) {
        /* frame_tile() returns the wall rule on the border ring and plain
         * field everywhere else, so the frame is never an overlay */
        put_frame(tx,     ty);
        put_frame(tx + 1, ty);
        put_frame(tx,     ty + 1);
        put_frame(tx + 1, ty + 1);
        return;
    }

    if (c == CELL_HOLE) {
        PutTile(SCR_1_PLANE, PAL_HOLE, tx,     ty,     T_FIELD);
        PutTile(SCR_1_PLANE, PAL_HOLE, tx + 1, ty,     T_FIELD);
        PutTile(SCR_1_PLANE, PAL_HOLE, tx,     ty + 1, T_FIELD);
        PutTile(SCR_1_PLANE, PAL_HOLE, tx + 1, ty + 1, T_FIELD);
        if (cx == 0 || cy == 0 || cx == (CELLS_W - 1) || cy == (CELLS_H - 1))
            put_frame_ring(cx, cy);
        return;
    }
    if (c == CELL_PALM) {
        /* 8x16 trunk, centred: the outer thirds of the cell stay sand,
         * because scroll-plane index 0 is transparent to the background. */
        put_frame(tx, ty);
        put_frame(tx + 1, ty);
        put_frame(tx, ty + 1);
        put_frame(tx + 1, ty + 1);
        PutTile(SCR_1_PLANE, PAL_TRUNK, tx + 1, ty,     T_TRUNK);
        PutTile(SCR_1_PLANE, PAL_TRUNK, tx + 1, ty + 1, T_TRUNK + 1);
        return;
    }
    if (c == CELL_PINE) {
        put_frame(tx, ty);
        put_frame(tx + 1, ty);
        put_frame(tx, ty + 1);
        PutTile(SCR_1_PLANE, PAL_PINE, tx + 1, ty + 1, T_PINE);
        return;
    }
    if (c == CELL_EGG)      { base = T_ICE; pal = PAL_EGG; }
    else if (c == CELL_GEM) { base = T_GEM; pal = PAL_GEM; }
    else                    { base = T_ICE; pal = PAL_ICE; }

    PutTile(SCR_1_PLANE, pal, tx,     ty,     base);
    PutTile(SCR_1_PLANE, pal, tx + 1, ty,     base + 1);
    PutTile(SCR_1_PLANE, pal, tx,     ty + 1, base + 2);
    PutTile(SCR_1_PLANE, pal, tx + 1, ty + 1, base + 3);
}

void draw_playfield(void) {
    u8 x, y;
    for (y = 0; y < CELLS_H; y++)
        for (x = 0; x < CELLS_W; x++)
            draw_cell(x, y);
}

/* The containing wall used to be stamped over the outer ring AFTER the
 * blocks, which clipped every block touching the border - index 0 in the
 * edge tile is transparent, so it erased the block underneath.
 *
 * Now the frame is part of the empty-cell fill instead of an overlay: an
 * empty border tile draws the rule, an occupied one draws the block and the
 * block simply IS the wall there.  Nothing overwrites anything.
 */
u16 frame_tile(u8 tx, u8 ty) {
    u8 top, bot, lft, rgt;

    top = (ty == PF_Y0);
    bot = (ty == PF_Y0 + (CELLS_H << 1) - 1);
    lft = (tx == 0);
    rgt = (tx == (CELLS_W << 1) - 1);

    if (top && lft) return T_EDGE + 4;
    if (top && rgt) return T_EDGE + 5;
    if (bot && lft) return T_EDGE + 6;
    if (bot && rgt) return T_EDGE + 7;
    if (top) return T_EDGE;
    if (bot) return T_EDGE + 1;
    if (lft) return T_EDGE + 2;
    if (rgt) return T_EDGE + 3;
    return T_FIELD;
}

/* ---- the canopy ----
 * Plane 2, put in FRONT of plane 1 and the sprites by SwapPlanes(), which is
 * a single register bit flip (SCR_PRIORITY ^= 0x80). That is the only way to
 * draw over a sprite on this hardware - sprites always beat plane 1.
 *
 * The canopy occludes VISUALLY ONLY. Nothing collides with a frond.
 */
/* SwapPlanes() is a TOGGLE - SCR_PRIORITY ^= 0x80 - not a setter. Calling it
 * once per enter_level() flips the canopy behind the playfield on every
 * second stage. Track the state and only flip when it actually needs to
 * change. */
static u8 canopy_front;

void set_canopy_front(u8 on) {
    if (on == canopy_front) return;
    SwapPlanes();
    canopy_front = on;
}

void draw_canopy(void) {
    u8 i, dx, dy, cx, cy, tx, ty, bit;

    FillScreen(SCR_2_PLANE, ' ', PAL_FROND);

    for (i = 0; i < palm_n; i++) {
        for (dy = 0; dy < 3; dy++) {
            for (dx = 0; dx < 3; dx++) {
                bit = (dy * 3) + dx;
                if (!((palm_mask[i] >> (8 - bit)) & 1)) continue;
                cx = palm_cx[i] + dx - 1;
                cy = palm_cy[i] + dy - 1;
                if (cx >= CELLS_W) continue;      /* unsigned wrap = off-field */
                if (cy >= CELLS_H) continue;
                tx = cx << 1;
                ty = PF_Y0 + (cy << 1);
                PutTile(SCR_2_PLANE, PAL_FROND, tx,     ty,     T_FROND);
                PutTile(SCR_2_PLANE, PAL_FROND, tx + 1, ty,     T_FROND + 1);
                PutTile(SCR_2_PLANE, PAL_FROND, tx,     ty + 1, T_FROND + 2);
                PutTile(SCR_2_PLANE, PAL_FROND, tx + 1, ty + 1, T_FROND + 3);
            }
        }
    }
}

/* One register write a frame. The dead ground drifts with the leaves, so the
 * cover you were relying on slides out from under a crab. */
void sway_canopy(void) {
    static const u8 sway_tab[16] = {
        0, 1, 1, 2, 2, 2, 1, 1, 0, 255, 255, 254, 254, 254, 255, 255
    };
    u8 vt;

    sway_i = (sway_i + 1) & 127;
    vt = 0;
    /* pineapple: the whole canopy lifts clear, revealing everything under it */
    if (lift_tmr > 0) vt = 224;      /* -32px */
    ShiftScroll(SCR_2_PLANE, sway_tab[(sway_i >> 3) & 15], vt);
}

/* ---- HUD ----
 * One tile row.  Glyph cells carry index 0, which paints black, so the HUD
 * reads as a dark bezel strip.  That is the intent, not an accident.
 *   score .......... cols 0-5
 *   level marker ... cols 8-9
 *   lives .......... cols 17-19, one penguin head per spare life
 */
void init_hud(void) {
    u8 i;
    for (i = 0; i < 20; i++)
        PutTile(SCR_1_PLANE, PAL_KANA_B, i, HUD_Y, ' ');
    print_kana(SCR_1_PLANE, PAL_KANA_W, 8, HUD_Y, s_stage);
}

void draw_hud(void) {
    PrintDecimal(SCR_1_PLANE, PAL_KANA_W, 0, HUD_Y, score, 6);
    PrintDecimal(SCR_1_PLANE, PAL_KANA_W, 12, HUD_Y, (u16)(level + 1), 2);

    /* T_PENG is the top-left quadrant of a 16x16 sprite, so it would show
     * as a quarter of a head here.  Digits until there is an 8x8 icon. */
    print_kana(SCR_1_PLANE, PAL_KANA_W, 15, HUD_Y, s_life);
    PrintDecimal(SCR_1_PLANE, PAL_KANA_W, 19, HUD_Y, (u16)lives, 1);
    /* sudden death: flash the stage marker */
    if (sudden && (frame & 8))
        PutTile(SCR_1_PLANE, PAL_KANA_B, 12, HUD_Y, ' ');
}

/* Paint the whole playfield area as plain field - no wall frame, no cells.
 * The intermissions are a stage, not a level. */
void fill_field(void) {
    u8 x, y;
    for (y = PF_Y0; y < PF_Y0 + (CELLS_H << 1); y++)
        for (x = 0; x < (CELLS_W << 1); x++)
            PutTile(SCR_1_PLANE, PAL_FIELD, x, y, T_FIELD);
}

/* Clear the two tile rows the intermission house occupies. */
void wipe_house(void) {
    u8 x, y;
    for (y = PF_Y0 + 8; y < PF_Y0 + 11; y++)
        for (x = 4; x < 14; x++)
            PutTile(SCR_1_PLANE, PAL_FIELD, x, y, T_FIELD);
}

void init_bonus_hud(void) {
    u8 i;
    for (i = 0; i < 20; i++)
        PutTile(SCR_1_PLANE, PAL_KANA_B, i, HUD_Y, ' ');
    PrintString(SCR_1_PLANE, PAL_KANA_W, 8, HUD_Y, "ANA");
    PrintString(SCR_1_PLANE, PAL_KANA_W, 14, HUD_Y, "T");
}

void draw_bonus_hud(void) {
    PrintDecimal(SCR_1_PLANE, PAL_KANA_W, 0, HUD_Y, score, 6);
    PrintDecimal(SCR_1_PLANE, PAL_KANA_W, 12, HUD_Y, (u16)holes_left(), 1);
    /* bonus_secs is counted down by run_bonus(); cc900 has no usable
     * divide, so never compute this as bonus_tmr / 60. */
    PrintDecimal(SCR_1_PLANE, PAL_KANA_W, 16, HUD_Y, (u16)bonus_secs, 2);
    PrintDecimal(SCR_1_PLANE, PAL_KANA_W, 19, HUD_Y, (u16)blocks_left(), 1);
}

/* ---- overlay text on the sprite plane ----
 * Kana over the playfield must be sprites: on the scroll plane the glyph's
 * index 0 would paint a black box around every character.
 */
void sprite_kana(u8 slot, u8 x, u8 y, const u8 *codes) {
    u8 n;
    n = 0;
    while (*codes) {
        if (n >= 8) return;
        SetSprite(slot + n, *codes, 0, x, y, SP_TEXT);
        x = x + 8;
        codes++;
        n++;
    }
}

void clear_sprite_kana(void) {
    u8 i;
    for (i = 0; i < 8; i++) UnsetSprite(SPR_TEXT + i);
}

void show_pause(u8 on) {
    if (on) show_banner(s_pause);
    else    clear_sprite_kana();
}

/* Centred banner, sprite plane, so it floats over the field cleanly. */
void show_banner(const u8 *codes) {
    u8 w, x;
    w = kana_len(codes) << 3;
    x = 80 - (w >> 1);
    clear_sprite_kana();
    sprite_kana(SPR_TEXT, x, 72, codes);
}

/* ---- title ----
 * Animated: a penguin waddles across the ice being chased by a Sno-Bee,
 * both on the same sprite tables the game uses, so it costs no art.
 */
void draw_title(void) {
    u8 i, tx;
    u16 b;

    apply_theme(0);
    wipe_screen();
    FillScreen(SCR_1_PLANE, T_FIELD, PAL_FIELD);

    /* a row of ice blocks along the bottom for the chase to run past */
    for (i = 0; i < CELLS_W; i++) {
        if (i & 1) continue;
        PutTile(SCR_1_PLANE, PAL_ICE, i << 1,       16, T_ICE);
        PutTile(SCR_1_PLANE, PAL_ICE, (i << 1) + 1, 16, T_ICE + 1);
        PutTile(SCR_1_PLANE, PAL_ICE, i << 1,       17, T_ICE + 2);
        PutTile(SCR_1_PLANE, PAL_ICE, (i << 1) + 1, 17, T_ICE + 3);
    }

    /* ペング, three 16x16 glyphs */
    tx = 7;
    for (i = 0; i < 3; i++) {
        b = T_TITLE + (i << 2);
        PutTile(SCR_1_PLANE, PAL_KANA_B, tx,     4, b);
        PutTile(SCR_1_PLANE, PAL_KANA_B, tx + 1, 4, b + 1);
        PutTile(SCR_1_PLANE, PAL_KANA_B, tx,     5, b + 2);
        PutTile(SCR_1_PLANE, PAL_KANA_B, tx + 1, 5, b + 3);
        tx = tx + 2;
    }

    /* オキナワのゆめ - Okinawa no yume. Kana, not kanji: 縄 is 15 strokes
     * and does not survive 16x16, and a handheld of this vintage would have
     * shipped kana-only anyway. */
    print_kana(SCR_1_PLANE, PAL_KANA_B, 7, 8, s_okinawa);
    print_kana(SCR_1_PLANE, PAL_KANA_B, 8, 10, s_start);
    PrintString(SCR_1_PLANE, PAL_KANA_B, 0, 13, "STUDIO SO NOT KANSAI");
    if (konami_on) PrintString(SCR_1_PLANE, PAL_KANA_W, 3, 17, "ORCA EVERY STAGE");
    PrintString(SCR_1_PLANE, PAL_KANA_B, 4, 15, "HI");
    PrintDecimal(SCR_1_PLANE, PAL_KANA_W, 7, 15, high_scores[0], 6);
}

/* Called every frame while STATE_TITLE. */
void anim_title(void) {
    u8 x, bx, f, base;

    /* 0..159 sweep, 1px every 2 frames */
    x  = (u8)((frame >> 1) & 127);
    x  = x + (x >> 2);          /* stretch 0-127 to roughly 0-159 */
    bx = x - 40;
    if (x < 40) bx = 0;

    f = 0;
    if (frame & 8) f = 1;

    base = T_PENG + (FACE_RIGHT << 3) + (f << 2);
    SetSprite(SPR_PENG,     base,     0, x,     104, SP_PENG);
    SetSprite(SPR_PENG + 1, base + 1, 0, x + 8, 104, SP_PENG);
    SetSprite(SPR_PENG + 2, base + 2, 0, x,     112, SP_PENG);
    SetSprite(SPR_PENG + 3, base + 3, 0, x + 8, 112, SP_PENG);

    base = T_BEE + (f << 2);
    SetSprite(SPR_BEE,     base,     0, bx,     104, SP_BEE);
    SetSprite(SPR_BEE + 1, base + 1, 0, bx + 8, 104, SP_BEE);
    SetSprite(SPR_BEE + 2, base + 2, 0, bx,     112, SP_BEE);
    SetSprite(SPR_BEE + 3, base + 3, 0, bx + 8, 112, SP_BEE);

    /* blink START by swapping its palette - free, the glyphs are one tile
     * set with a body index and a shadow index */
    if (frame & 16) print_kana(SCR_1_PLANE, PAL_KANA_B, 8, 10, s_start);
    else            print_kana(SCR_1_PLANE, PAL_KANA_W, 8, 10, s_start);
}

void draw_scores(void) {
    u8 i;

    clear_all_sprites();
    apply_theme(0);
    wipe_screen();
    FillScreen(SCR_1_PLANE, T_FIELD, PAL_FIELD);

    print_kana(SCR_1_PLANE, PAL_KANA_B, 5, 3, s_hiscore);
    for (i = 0; i < 5; i++) {
        PrintDecimal(SCR_1_PLANE, PAL_KANA_W, 5, 6 + i, (u16)(i + 1), 1);
        PrintDecimal(SCR_1_PLANE, PAL_KANA_W, 8, 6 + i, high_scores[i], 6);
    }
}
