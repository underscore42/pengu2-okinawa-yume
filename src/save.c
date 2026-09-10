/* save.c - Pengu high score table  ** FLASH WRITES ARE STUBBED OUT **
 *
 * WHY THIS IS STUBBED
 * -------------------
 * library.c's Flash() hardcodes:
 *
 *     __ASM("SAVEOFFSET  EQU  0x1e0000");
 *     __ASM("BLOCK_NB    EQU  30");
 *
 * 0x1E0000 is ~1.9MB into what is a 256KB ROM image, and block 30 does not
 * exist on that part. So the write lands nowhere and, worse, the routine
 * ERASES the block first - issuing an erase for a block that isn't there is
 * not something worth testing on a cart you own.
 *
 * The symptom to expect if it were left live: scores appear to persist
 * within a session (GetSavedData reads back RAM) and silently vanish on a
 * power cycle.
 *
 * This affects every title in the catalogue that calls Flash(), not just
 * Pengu. NOT YET CONFIRMED ON SILICON - the test is to save a score, power
 * cycle, and see whether it survives.
 *
 * THE FIX, when someone gets to it: a custom flash routine with SAVEOFFSET
 * at 0x30000 (block 3, the last 64KB of a 256KB image) and a block number
 * that exists on the part. Until then the table lives in RAM, resets on
 * power-up, and nothing touches the chip.
 *
 * The interface is unchanged, so restoring persistence is a change to this
 * file alone.
 */
#include "save.h"

/* ---- forward declarations ---- */
void load_high_scores(void);
void save_high_scores(void);
void insert_high_score(u16 s);
u8   is_high_score(u16 s);
static void set_defaults(void);

static void set_defaults(void) {
    high_scores[0] = 5000;
    high_scores[1] = 4000;
    high_scores[2] = 3000;
    high_scores[3] = 2000;
    high_scores[4] = 1000;
}

void load_high_scores(void) {
    /* No GetSavedData() call: there is nothing valid to read back, and the
     * table it would populate is the one we are about to default anyway. */
    set_defaults();
}

void save_high_scores(void) {
    /* Deliberately empty. Do NOT call Flash() until the offset is fixed -
     * it erases a block that does not exist on a 256KB cart. */
}

u8 is_high_score(u16 s) {
    if (s > high_scores[4]) return 1;
    return 0;
}

void insert_high_score(u16 s) {
    u8 i, j;
    for (i = 0; i < 5; i++) {
        if (s > high_scores[i]) {
            j = 4;
            while (j > i) {
                high_scores[j] = high_scores[j - 1];
                j--;
            }
            high_scores[i] = s;
            return;
        }
    }
}
