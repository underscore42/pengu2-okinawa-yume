/* scenes.c - Pengu: the three-little-pigs intermissions
 *
 * Three scripted scenes, no input except A to skip. The penguin builds a
 * house, a leopard seal huffs it down, and the third one pays off the
 * diamond blocks - which until now were obstacles with no meaning attached.
 *
 *   scene 0   loose snow      -> blown flat instantly
 *   scene 1   ice blocks      -> blown apart, penguin legs it
 *   scene 2   diamond blocks  -> seal huffs, nothing happens, seal keels over
 *
 * ALL TIMINGS ARE THE CONSTANTS BELOW, in frames at 60Hz. They are guesses -
 * no test can tell you whether a joke lands, so expect to retune these by eye
 * in Mednafen. Everything else in this file is driven off them.
 *
 * Tiles are banked: install_inter_tiles() overwrites the kana block on entry,
 * install_tiles() restores the font on the way out. No kana are on screen
 * during a scene, so nothing is lost.
 */
#include "scenes.h"
#include "screen.h"
#include "tiles.h"
#include "sound.h"

/* ---- beat sheet, in frames ---- */
#define IN_WALK_END   54    /* penguin walks in from the left        */
#define IN_BUILD_END 126    /* house goes up, one piece at a time    */
#define IN_SEAL_END  168    /* seal slides in from the right         */
#define IN_DRAW_END  234    /* inhale - the wind-up IS the joke      */
#define IN_GUST_END  282    /* gust sweeps right to left             */
#define IN_BEAT_END  330    /* the result lands                      */
#define IN_HOLD_END  396    /* reaction, then out                    */

/* ---- crab rave (scene 3) ----
 * 120 BPM at 60Hz is exactly 30 frames a beat, which is why the bob, the
 * palette strobe and the kick all lock without drifting. Any tempo that
 * doesn't divide 3600 evenly would separate over a couple of minutes.
 *
 * No music: BGM needs NeoTracker, which is a catalogue-wide decision because
 * it takes over SFX too. A kick on the beat and a blip off it is well inside
 * the SFX driver's budget - one sound every 30 frames, not a per-note melody.
 */
#define BEAT       30    /* frames per beat at 120 BPM */
#define RAVE_CRABS 12
#define RAVE_END  600    /* 20 beats, then out */

#define HOUSE_X   3         /* cell column the house starts at */
#define HOUSE_Y   4         /* cell row the house sits on      */
#define SEAL_X  108         /* pixel x the seal settles at     */
#define SEAL_Y   62

/* ---- forward declarations ---- */
void inter_start(u8 scene);
u8   inter_run(void);
static void draw_house(u8 pieces);
static void draw_seal(u8 px, u8 py, u8 open);
static void hide_seal(void);
static void draw_gust(u8 px);
static void hide_gust(void);
static void draw_peng(u8 px, u8 py, u8 face, u8 f);
static void draw_rave_crabs(void);

static u8  in_scene;
static u16 in_t;
static u8  in_built;

void inter_start(u8 scene) {
    in_scene = scene;
    if (in_scene > 3) in_scene = 3;
    in_t     = 0;
    in_built = 0;

    clear_all_sprites();
    apply_theme(0);
    wipe_screen();
    fill_field();
    install_inter_tiles();
}

/* The house is three pieces wide: dome corners on top of the walls, with a
 * doorway. Material changes per scene; the silhouette does not. */
static void draw_house(u8 pieces) {
    u8 tx, ty, i;
    u16 wall;

    wall = T_SNOW;
    if (in_scene == 1) wall = T_ICE;
    if (in_scene == 2) wall = T_GEM;

    tx = HOUSE_X << 1;
    ty = PF_Y0 + (HOUSE_Y << 1);

    for (i = 0; i < pieces; i++) {
        if (i >= 6) break;
        if (in_scene == 0) {
            /* loose snow: a row of mounds, no structure at all */
            PutTile(SCR_1_PLANE, PAL_ICE, tx + i, ty + 1, T_SNOW);
        } else {
            /* walls, then the dome, then the doorway */
            if (i < 2) {
                PutTile(SCR_1_PLANE, PAL_ICE, tx + (i << 1),     ty + 1, wall);
                PutTile(SCR_1_PLANE, PAL_ICE, tx + (i << 1) + 1, ty + 1, wall + 1);
            } else if (i < 4) {
                PutTile(SCR_1_PLANE, PAL_ICE, tx + ((i - 2) << 1),
                        ty, T_IGLOO + (i - 2));
                PutTile(SCR_1_PLANE, PAL_ICE, tx + ((i - 2) << 1) + 1,
                        ty, T_IGLOO + (i - 2));
            } else {
                PutTile(SCR_1_PLANE, PAL_ICE, tx + 2, ty + 1, T_IGLOO + 2);
                PutTile(SCR_1_PLANE, PAL_ICE, tx + 3, ty + 1, T_IGLOO + 3);
            }
        }
    }
}

/* 32x24 = 12 sprites, borrowed from the Sno-Bee block. The open maw swaps
 * only the two right-hand tiles of the head row. */
static void draw_seal(u8 px, u8 py, u8 open) {
    u8 r, c, n;
    u16 t;

    n = 0;
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 4; c++) {
            t = T_SEAL + n;
            if (open && r == 0 && c >= 2) t = T_MAW + (c - 2);
            SetSprite(SPR_BEE + n, t, 0,
                      px + (c << 3), py + (r << 3), SP_BEE);
            n++;
        }
    }
}

static void hide_seal(void) {
    u8 i;
    for (i = 0; i < 12; i++) UnsetSprite(SPR_BEE + i);
}

static void draw_gust(u8 px) {
    u8 i;
    u16 t;
    for (i = 0; i < 4; i++) {
        t = T_GUST;
        if ((in_t + i) & 4) t = T_GUST + 1;
        SetSprite(SPR_TEXT + i, t, 0, px + (i << 3), SEAL_Y + 8, SP_TEXT);
    }
}

static void hide_gust(void) {
    u8 i;
    for (i = 0; i < 4; i++) UnsetSprite(SPR_TEXT + i);
}

static void draw_peng(u8 px, u8 py, u8 face, u8 f) {
    u8 base;
    base = T_PENG + (face << 3) + (f << 2);
    SetSprite(SPR_PENG,     base,     0, px,     py,     SP_PENG);
    SetSprite(SPR_PENG + 1, base + 1, 0, px + 8, py,     SP_PENG);
    SetSprite(SPR_PENG + 2, base + 2, 0, px,     py + 8, SP_PENG);
    SetSprite(SPR_PENG + 3, base + 3, 0, px + 8, py + 8, SP_PENG);
}

/* Twelve crabs in two staggered rows, bobbing a beat apart.
 * 12 x 4 = 48 sprites, plus the penguin's 4 = 52 of 64. */
static u8 rave_phase;

static void draw_rave_crabs(void) {
    u8 i, col, row, px, py, f, slot;
    u16 base;

    for (i = 0; i < RAVE_CRABS; i++) {
        col = i;
        row = 0;
        if (i >= 6) { col = i - 6; row = 1; }
        slot = 8 + (i << 2);

        px = 6 + (col * 24) + (row << 3);
        py = 48 + (row * 28);

        f = rave_phase;
        if ((i & 1) != 0) f = 1 - f;   /* alternate crabs on the off-beat */
        if (f) py = py - 3;

        base = T_BEE;
        if (f) base = T_BEE + 4;

        SetSprite(slot,     base,     0, px,     py,     SP_BEE);
        SetSprite(slot + 1, base + 1, 0, px + 8, py,     SP_BEE);
        SetSprite(slot + 2, base + 2, 0, px,     py + 8, SP_BEE);
        SetSprite(slot + 3, base + 3, 0, px + 8, py + 8, SP_BEE);
    }
}

/* Returns 1 when the scene is finished. */
u8 inter_run(void) {
    u8 px, py, f, want;

    in_t = in_t + 1;
    f = 0;
    if (in_t & 8) f = 1;

    /* --- scene 3: the crab rave. No house, no seal, just a party. --- */
    if (in_scene == 3) {
        u8 beat_i;
        beat_i = (u8)(in_t & 31);
        rave_phase = 0;
        if (beat_i >= 16) rave_phase = 1;
        if (beat_i == 0)  PlaySound(SND_KICK);
        if (beat_i == 16) PlaySound(SND_BLIP);
        if ((in_t & 127) == 0) {
            u8 k;
            k = (u8)((in_t >> 7) & 3);
            if (k == 0)      SetPalette(SPRITE_PLANE, SP_BEE, 0,
                                        RGB(15, 3, 2), RGB(7, 0, 0), RGB(15, 15, 15));
            else if (k == 1) SetPalette(SPRITE_PLANE, SP_BEE, 0,
                                        RGB(15, 9, 2), RGB(7, 2, 0), RGB(15, 15, 15));
            else if (k == 2) SetPalette(SPRITE_PLANE, SP_BEE, 0,
                                        RGB(13, 2, 9), RGB(6, 0, 4), RGB(15, 15, 15));
            else             SetPalette(SPRITE_PLANE, SP_BEE, 0,
                                        RGB(3, 12, 13), RGB(0, 5, 6), RGB(15, 15, 15));
        }
        draw_rave_crabs();
        /* penguin walks in, watches, edges off */
        if (in_t < 90)       draw_peng((u8)(in_t >> 1), 112, FACE_RIGHT, f);
        else if (in_t < 480) draw_peng(45, 112, FACE_UP, 0);
        else                 draw_peng((u8)(45 + ((in_t - 480) >> 1)), 112,
                                       FACE_RIGHT, f);
        if (in_t >= RAVE_END) return 1;
        return 0;
    }

    py = PF_PY0 + (HOUSE_Y << 4) + 8;

    /* --- walk in --- */
    if (in_t < IN_WALK_END) {
        px = (u8)(in_t >> 1);
        draw_peng(px, py, FACE_RIGHT, f);
        return 0;
    }

    /* --- build, one piece at a time --- */
    if (in_t < IN_BUILD_END) {
        /* Shift, never divide - cc900's divide codegen is not trustworthy
         * and the gnu90 preflight will happily accept a '/ 12' that breaks
         * on hardware. */
        want = (u8)((in_t - IN_WALK_END) >> 3);
        if (want > 6) want = 6;
        if (want > in_built) {
            in_built = want;
            draw_house(in_built);
            PlaySound(SND_LAND);
        }
        draw_peng(IN_WALK_END >> 1, py, FACE_RIGHT, f);
        return 0;
    }

    /* --- seal slides in from the right --- */
    if (in_t < IN_SEAL_END) {
        px = (u8)(160 - ((in_t - IN_BUILD_END) >> 1));
        if (px < SEAL_X) px = SEAL_X;
        draw_seal(px, SEAL_Y, 0);
        draw_peng(IN_WALK_END >> 1, py, FACE_RIGHT, f);
        return 0;
    }

    /* --- the wind-up. Long on purpose: the length is the joke. --- */
    if (in_t < IN_DRAW_END) {
        draw_seal(SEAL_X, SEAL_Y, 1);
        draw_peng(IN_WALK_END >> 1, py, FACE_RIGHT, f);
        if (in_t == IN_SEAL_END + 2) PlaySound(SND_HATCH);
        return 0;
    }

    /* --- gust sweeps right to left --- */
    if (in_t < IN_GUST_END) {
        draw_seal(SEAL_X, SEAL_Y, 1);
        px = (u8)(SEAL_X - ((in_t - IN_DRAW_END) << 1));
        if (px > SEAL_X) px = 0;
        draw_gust(px);
        draw_peng(IN_WALK_END >> 1, py, FACE_RIGHT, f);
        if (in_t == IN_DRAW_END + 1) PlaySound(SND_STUN);
        return 0;
    }

    /* --- the result --- */
    if (in_t < IN_BEAT_END) {
        hide_gust();
        if (in_scene == 2) {
            /* diamonds hold. The seal does itself an injury. */
            draw_seal(SEAL_X, SEAL_Y + (u8)((in_t - IN_GUST_END) >> 2), 0);
            draw_house(6);
            draw_peng(IN_WALK_END >> 1, py, FACE_RIGHT, f);
            if (in_t == IN_GUST_END + 1) PlaySound(SND_DIE);
        } else {
            /* the house goes. penguin legs it. */
            draw_seal(SEAL_X, SEAL_Y, 0);
            if (in_t == IN_GUST_END + 1) {
                wipe_house();
                PlaySound(SND_CRUSH);
            }
            px = (u8)(IN_WALK_END >> 1);
            if (in_t > IN_GUST_END + 8) {
                want = (u8)((in_t - IN_GUST_END - 8) >> 1);
                if (want > px) px = 0;
                else           px = px - want;
            }
            draw_peng(px, py, FACE_LEFT, f);
        }
        return 0;
    }

    /* --- hold on the punchline --- */
    if (in_t < IN_HOLD_END) return 0;

    return 1;
}
