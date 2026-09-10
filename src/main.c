/* main.c - Pengu (NGPC) - Studio So Not Kansai / underscore42
 *
 * A Pengo homage.  Single screen, 10x9 cells of 16px, four fields, one tile
 * set retinted four ways.  Walk into a block and it slides; if it is jammed,
 * A crushes it.  Kill Sno-Bees by sliding a block into them, crushing their
 * egg before it hatches, or punching the wall to stun one and walking over
 * it.  Clear every Sno-Bee AND every egg to take the round.
 *
 * Link order matters: main.c first.
 */
#define CARTHDR_IMPL
#include "carthdr.h"

#include "game.h"
#include "screen.h"
#include "entities.h"
#include "snobee.h"
#include "orca.h"
#include "scenes.h"
#include "tiles.h"
#include "kana.h"
#include "sound.h"
#include "save.h"

/* ---- Konami code: Up Up Down Down Left Right Left Right B A ----
 * J_UP=0x01 J_DOWN=0x02 J_LEFT=0x04 J_RIGHT=0x08 J_A=0x10 J_B=0x20
 * Same shape as Asteroids and Blue Print.  Entered on the title screen; it
 * unlocks the whack-a-orca round after level 4 instead of wrapping to 1.
 */
static u8 konami_pos;
static const u8 konami_seq[10] = {
    0x01, 0x01, 0x02, 0x02, 0x04, 0x08, 0x04, 0x08, 0x20, 0x10
};

static void check_konami(void);

/* ---- forward declarations: every function, or cc900 assumes int and then
 * conflicts with the void definition ---- */
void main(void);
static void enter_title(void);
static void enter_scores(void);
static void enter_level(void);
static void enter_dead(void);
static void enter_clear(void);
static void enter_over(void);
static void run_title(void);
static void run_scores(void);
static void run_game(void);
static void run_dead(void);
static void run_clear(void);
static void run_over(void);
static void start_bees(void);
static void start_new_game(void);
static void enter_bonus(void);
static void run_bonus(void);
static u8 from_bonus;
static void enter_inter(void);
static void run_inter(void);

static void check_konami(void) {
    u8 expected;
    if (konami_pos >= 10) return;
    if (pad_press == 0) return;
    expected = konami_seq[konami_pos];
    if (pad_press & expected) {
        konami_pos++;
        if (konami_pos >= 10) {
            konami_on  = 1;
            konami_pos = 0;
            PlaySound(SND_CLEAR);
            draw_title();
        }
    } else {
        konami_pos = 0;
    }
}

/* Hatch the round's opening Sno-Bees straight out of egg blocks, arcade
 * style - they are not placed, they emerge. */
static void start_bees(void) {
    u8 i, n;
    bees_reset();
    n = level_start_bees();
    for (i = 0; i < n; i++) spawn_from_surf();
}

static void start_new_game(void) {
    new_game();
    enter_level();
}

static void enter_title(void) {
    state = STATE_TITLE;
    skip  = 10;
    state_tmr = 0;
    clear_all_sprites();
    /* The tropical set is banked OVER the title logo, so coming back from a
     * game the logo is still trunk/frond/pineapple data - PE and N render as
     * garbage while GU survives, because the tropical set is only 7 of the
     * logo's 12 tiles. Put the logo back before drawing it. */
    set_canopy_front(0);
    FillScreen(SCR_2_PLANE, ' ', PAL_FROND);
    install_title_tiles();
    draw_title();
    anim_title();   /* same reason as enter_level: the debounce frames return
                     * before run_title() draws, so seed the chase sprites */
}

static void enter_scores(void) {
    set_canopy_front(0);
    FillScreen(SCR_2_PLANE, ' ', PAL_FROND);
    install_title_tiles();
    state = STATE_SCORES;
    skip  = 10;
    state_tmr = 200;
    draw_scores();
}

static void enter_level(void) {
    state = STATE_GAME;
    skip  = 10;
    clear_all_sprites();
    init_level();
    apply_theme(level);
    install_tropic_tiles();   /* banked over the title logo */
    wipe_screen();
    draw_playfield();
    draw_canopy();
    set_canopy_front(1);      /* plane 2 in FRONT: the only way to draw over
                               * a sprite on this hardware */
    init_hud();
    draw_hud();
    start_bees();
    /* The skip frames below return before run_game() draws, so put the
     * actors on screen once here - otherwise the field sits empty for the
     * length of the debounce. */
    draw_player();
    draw_bees();
    PlaySound(SND_START);
}

static void enter_dead(void) {
    state = STATE_DEAD;
    skip  = 6;
    state_tmr = 90;
    PlaySound(SND_DIE);
}

static void enter_clear(void) {
    state = STATE_CLEAR;
    skip  = 6;
    state_tmr = 120;
    /* time bonus: what's left of the sudden-death clock */
    if (level_frames < SUDDEN_AT)
        score = score + ((SUDDEN_AT - level_frames) >> 5);
    show_banner(s_clear);
    PlaySound(SND_CLEAR);
}

static void enter_over(void) {
    hide_player();
    state = STATE_OVER;
    skip  = 10;
    state_tmr = 150;
    show_banner(s_gameover);
    if (is_high_score(score)) {
        insert_high_score(score);
        save_high_scores();
    }
}

static void run_title(void) {
    check_konami();
    anim_title();
    if (pad_press & J_A)      start_new_game();
    if (pad_press & J_OPTION) enter_scores();
}

static void run_scores(void) {
    if (state_tmr > 0) state_tmr = state_tmr - 1;
    if (state_tmr == 0)        enter_title();
    if (pad_press & J_OPTION)  enter_title();
    if (pad_press & J_A)       enter_title();
}

static void run_game(void) {
    if (pad_press & J_OPTION) {
        if (paused) paused = 0;
        else        paused = 1;
        show_pause(paused);
    }
    if (paused) return;

    level_frames = level_frames + 1;
    if (level_frames >= SUDDEN_AT) sudden = 1;

    pulse_egg_palette();

    /* Mid-round hatching keeps the pressure on, and makes crushing an egg
     * block a real decision rather than a fallback. */
    /* Crabs crawl out of the surf on a clock. No eggs in Pengu 2, so the
     * pressure is continuous rather than something you can pre-empt. */
    if (spawn_tmr > 0) spawn_tmr = spawn_tmr - 1;
    if (spawn_tmr == 0) {
        spawn_from_surf();
        spawn_tmr = SPAWN_TIME;
        if (sudden) spawn_tmr = SPAWN_TIME >> 1;
    }

    if (lift_tmr > 0) lift_tmr = lift_tmr - 1;
    sway_canopy();

    update_player();
    update_slide();
    update_crush();
    update_bees();

    draw_player();
    draw_slide();
    draw_crush();
    draw_bees();
    draw_hud();

    if (!p_alive) {
        enter_dead();
        return;
    }
    /* round is won only when every Sno-Bee AND every egg is gone */
    /* Round ends when the sea has sent its quota and you have killed them
     * all. Pineapples are optional - they never block a clear. */
    if (spawns_left == 0 && bees_alive() == 0) enter_clear();
}

static void enter_bonus(void) {
    from_bonus = 0;
    set_canopy_front(0);
    FillScreen(SCR_2_PLANE, ' ', PAL_FROND);
    state = STATE_BONUS;
    skip  = 10;
    clear_all_sprites();
    bees_reset();
    init_bonus();
    apply_theme(1);           /* night ice - the water reads better dark */
    wipe_screen();
    draw_playfield();
    init_bonus_hud();
    draw_bonus_hud();
    orca_reset();
    draw_player();
    PlaySound(SND_START);
}

static void run_bonus(void) {
    if (pad_press & J_OPTION) {
        if (paused) paused = 0;
        else        paused = 1;
        show_pause(paused);
    }
    if (paused) return;

    if (bonus_tmr > 0) bonus_tmr = bonus_tmr - 1;
    if (bonus_sixty > 0) bonus_sixty = bonus_sixty - 1;
    if (bonus_sixty == 0) {
        bonus_sixty = 60;
        if (bonus_secs > 0) bonus_secs = bonus_secs - 1;
    }

    update_player();
    update_slide();
    update_crush();
    update_orca();

    draw_player();
    draw_slide();
    draw_crush();
    draw_orca();
    draw_bonus_hud();

    /* Three ways out: holes gone, ammunition gone, or time gone. */
    if (holes_left() == 0 || bonus_tmr == 0 ||
        (blocks_left() == 0 && !sl_on)) {
        if (whacks >= holes_total) score = score + PERFECT_BONUS;
        bonus_round = bonus_round + 1;
        hide_orca();
        from_bonus = 1;
        state      = STATE_CLEAR;
        skip       = 6;
        state_tmr  = 120;
        show_banner(s_clear);
        PlaySound(SND_CLEAR);
    }
}

static void enter_inter(void) {
    set_canopy_front(0);
    FillScreen(SCR_2_PLANE, ' ', PAL_FROND);
    state = STATE_INTER;
    skip  = 10;
    inter_start(inter_done - 1);
}

static void run_inter(void) {
    /* A skips. Nobody should be made to sit through the same gag twice. */
    if ((pad_press & J_A) || inter_run()) {
        clear_all_sprites();
        install_tiles();          /* put the kana font back */
        enter_level();
    }
}

static void run_dead(void) {
    /* penguin blinks out */
    if (state_tmr & 4) {
        UnsetSprite(SPR_PENG);
        UnsetSprite(SPR_PENG + 1);
        UnsetSprite(SPR_PENG + 2);
        UnsetSprite(SPR_PENG + 3);
    } else {
        draw_player();
    }

    if (state_tmr > 0) state_tmr = state_tmr - 1;
    if (state_tmr > 0) return;

    hide_player();   /* he is dead; do not leave him standing under the banner */

    /* DEATH REBUILDS THE WHOLE FIELD - deliberate, not laziness.
     *
     * Blocks are ammunition, and you can wreck your own board by shoving them
     * all flat against the walls. That state is still winnable (wall-punch
     * stun is a kill method that needs no blocks) but it is far worse to play.
     * Preserving block positions across a death would mean losing a life AND
     * being stuck with your own mistake - the death compounds the error
     * instead of clearing it.
     *
     * Resetting also keeps each round a self-contained puzzle re-attempted
     * from a known layout, which is what the arcade does, and in Pengu 1 it
     * sidesteps the egg-inside-block coupling entirely.
     */
    if (lives > 0) {
        lives = lives - 1;
        enter_level();
    } else {
        enter_over();
    }
}

static void run_clear(void) {
    if (state_tmr > 0) state_tmr = state_tmr - 1;
    if (state_tmr > 0) return;
    clear_sprite_kana();

    /* Coming out of the bonus round: straight into the next field, and do
     * not count it as a field clear or the bonus would chain. */
    if (state_tmr == 0 && from_bonus) {
        from_bonus = 0;
        enter_level();
        return;
    }

    level = level + 1;

    /* Bonus round every 5th stage: four fields, then the orca.  The Konami
     * code makes it every field instead. */
    /* Intermission first: its stages never coincide with a bonus round, so
     * this ordering only matters if the schedules are ever retuned. */
    if (inter_due()) {
        enter_inter();
        return;
    }

    if (bonus_due()) {
        enter_bonus();
        return;
    }

    enter_level();
}

static void run_over(void) {
    if (state_tmr > 0) state_tmr = state_tmr - 1;
    if (state_tmr > 0) return;
    clear_sprite_kana();
    enter_scores();
}

void main(void) {
    InitNGPC();

    /* Order is load bearing: palettes, then the BIOS font, then our tiles,
     * then wipe, then draw.  Wiping before the font call bleeds the
     * Japanese charset through. */
    setup_palettes();
    SysSetSystemFont();
    install_tiles();
    wipe_screen();

    sound_init();
    load_high_scores();

    pad_cur   = 0;
    pad_prev  = 0;
    pad_press = 0;
    skip      = 0;
    konami_pos = 0;
    konami_on  = 0;
    from_bonus = 0;
    score     = 0;
    frame     = 0;
    level     = 0;
    lives     = START_LIVES;
    rand_seed = 42;

    clear_all_sprites();
    enter_title();

    for (;;) {
        WaitVsync();

        pad_prev  = pad_cur;
        pad_cur   = JOYPAD & 0x7F;
        pad_press = pad_cur & ~pad_prev;

        frame = frame + 1;
        /* seed the RNG from how long the player took to press start */
        if (state == STATE_TITLE) rand_seed = rand_seed + 1;

        /* Input is read ABOVE this line so pad_prev stays correct across the
         * gap; then swallow the debounce frames.  Stops one A press walking
         * through title -> game, or clear -> next level. */
        if (skip > 0) {
            skip = skip - 1;
            continue;
        }

        if (state == STATE_TITLE)       run_title();
        else if (state == STATE_SCORES) run_scores();
        else if (state == STATE_GAME)   run_game();
        else if (state == STATE_BONUS)  run_bonus();
        else if (state == STATE_INTER)  run_inter();
        else if (state == STATE_DEAD)   run_dead();
        else if (state == STATE_CLEAR)  run_clear();
        else                            run_over();
    }
}
