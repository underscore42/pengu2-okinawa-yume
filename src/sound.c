/* sound.c - Pengu sound effects
 *
 * COMPLIANCE NOTES - these are verified, not guesses.  Audited by
 * tools/checksound.py; run it after any edit here.
 *
 * 1. NO WaitVsync between InstallSoundDriver() and InstallSounds().  An
 *    earlier "Z80 boot race guard" note said to add two - it is WRONG for
 *    this driver and produces NO SOUND AT ALL.  Match the working example
 *    exactly: two calls, back to back.
 * 2. sound_init() is called AFTER graphics setup in main().  Calling it
 *    first can also kill the driver.
 * 3. The stop-all-sounds library call is declared in library.h but NOT
 *    exported by system.lib - referencing it fails at tulink with an
 *    unresolved external.  gnu90 preflight cannot see this.  Never call it.
 *    Silence comes from the volume decaying to 0, see 4.
 * 4. There is no hardware envelope generator.  Volume is software-stepped
 *    and a channel HOLDS its last level forever, so every effect MUST reach
 *    volume 0 within its Length or the channel drones.  Every row below
 *    satisfies ceil(InitialVol/VolStep) * VolSpeed <= Length, with
 *    VolLowerLimit 0 for true silence.
 * 5. The library abstraction is 15 = loud, 0 = silent (the Z80 driver
 *    inverts it for the chip, where $0F is max attenuation).
 * 6. ToneStep must be non-zero.  The driver only writes the tone-period
 *    register when a sound SWEEPS; a steady tone never sets the pitch and
 *    plays as static.  That is why sweeps work and flat notes don't.
 * 7. Channels 1 and 2 only.  Channel 3 is noise - leave it silent.
 * 8. PlaySound(n) is 1-BASED: PlaySound(1) plays game_sounds[0], so the
 *    SND_* defines in sound.h run 1..11 and index this table directly.
 *
 * 9. The array MUST be const.  This is the one that made Pengu silent
 *    through v1-v5: const data stays in ROM, but a non-const initialised
 *    array needs a .data section copied ROM->RAM at startup, and it isn't.
 *    game_sounds was the only non-const initialised table in the project -
 *    every other table (tiles, maps, themes, kana) is const, which is
 *    exactly why graphics worked and sound never did.  Explicitly sized as
 *    well; the working example leaves it unsized, sizing is just defensive.
 *
 * Field order:
 *   Channel, Length, Repeat, InitialTone, ToneStep, ToneSpeed, ToneOWB,
 *   ToneLowerLimit, ToneUpperLimit, InitialVol, VolStep, VolSpeed, VolOWB,
 *   VolLowerLimit, VolUpperLimit
 */
#include "sound.h"

static const SOUNDEFFECT game_sounds[13] = {
/*  1 push   */ { 1,  8, 0, 0x0180, 0x0030, 1, 0, 0x0120, 0x0300, 12, 2, 1, 0, 0, 15 },
/*  2 crush  */ { 2, 14, 0, 0x0060, 0x0018, 1, 0, 0x0040, 0x0200, 15, 2, 1, 0, 0, 15 },
/*  3 land   */ { 2,  8, 0, 0x0200, 0x0020, 1, 0, 0x0180, 0x0300, 11, 2, 1, 0, 0, 15 },
/*  4 squash */ { 2, 16, 0, 0x0080, 0x0014, 1, 0, 0x0060, 0x02C0, 15, 1, 1, 0, 0, 15 },
/*  5 hatch  */ { 1, 12, 0, 0x0220, 0x0040, 1, 0, 0x0140, 0x0380, 12, 2, 1, 0, 0, 15 },
/*  6 stun   */ { 1, 14, 0, 0x0100, 0x0028, 1, 0, 0x00A0, 0x0300, 14, 2, 1, 0, 0, 15 },
/*  7 thud   */ { 2,  6, 0, 0x02A0, 0x0018, 1, 0, 0x0240, 0x0340,  9, 2, 1, 0, 0, 15 },
/*  8 stomp  */ { 1,  8, 0, 0x0120, 0x0038, 1, 0, 0x00C0, 0x0280, 13, 2, 1, 0, 0, 15 },
/*  9 die    */ { 1, 20, 0, 0x0080, 0x0018, 2, 0, 0x0060, 0x0360, 15, 1, 1, 0, 0, 15 },
/* 10 clear  */ { 1, 20, 0, 0x0300, 0x0028, 1, 0, 0x00A0, 0x0380, 14, 1, 1, 0, 0, 15 },
/* 11 start  */ { 2, 16, 0, 0x0280, 0x0030, 1, 0, 0x00C0, 0x0340, 13, 1, 1, 0, 0, 15 },
/* 12 kick   */ { 2,  6, 0, 0x02E0, 0x0040, 1, 0, 0x0200, 0x0380, 15, 3, 1, 0, 0, 15 },
/* 13 blip   */ { 1,  4, 0, 0x00C0, 0x0030, 1, 0, 0x0080, 0x0200, 10, 3, 1, 0, 0, 15 }
};

void sound_init(void) {
    InstallSoundDriver();
    InstallSounds(game_sounds, 13);
}
