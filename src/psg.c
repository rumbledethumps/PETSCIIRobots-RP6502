/* The RIA PSG, playing the PET engine's notes.
 *
 * Eight oscillators of eight bytes each, live in XRAM. The engine uses two:
 * oscillator 0 for the music and oscillator 1 for sound effects, so an effect
 * no longer has to take the music's voice the way it did on the PET. An
 * PETSCII_AUTHENTIC_AUDIO build routes both to oscillator 0 instead.
 *
 * Per oscillator:
 *
 *     0-1  frequency, hertz times three
 *     2    duty, compared against the top byte of the phase accumulator
 *     3    peak attenuation in the high nibble, attack rate in the low
 *     4    sustain attenuation in the high nibble, decay rate in the low
 *     5    waveform in the high nibble, release rate in the low
 *     6    pan in bits 7-1, gate in bit 0
 *
 * Attenuation runs the other way from volume: 0 is loudest and 15 is silence.
 *
 * The gate is edge-triggered. Writing a 1 while the oscillator is already
 * sounding does nothing at all, so a new note has to clear the gate and set it
 * again -- which is what makes each row of a pattern audible rather than one
 * long slur.
 */
#include <rp6502.h>

#include "game/game.h"
#include "psg.h"
#include "xram.h"

#define OSC_BYTES 8
#define OSC_MUSIC 0
#define OSC_EFFECT 1

/* Portal 1 throughout. plat_psg_note is only ever called from the VSYNC
 * interrupt, which saves and restores portal 1 around everything it does, so
 * the main loop's portal 0 work is never disturbed. psg_init runs before the
 * interrupt is enabled, which is why it may use portal 1 too. */

/* A square wave, struck hard and held: instant attack, sustain equal to peak,
 * short release. That is as close as a PSG gets to the PET's raw gated square,
 * and it is what the music uses.
 *
 * Effects get their own voice now, which means they no longer announce
 * themselves by the music stopping. A narrower pulse and a faster release give
 * them a harder edge so they cut through a held note instead of blending into
 * it -- the one piece of timbre design the PET data cannot supply, since it
 * only ever named notes. */
#define PAN_CENTRE  0x00        /* centre, gate clear              */

#define MUSIC_DUTY          128 /* square                          */
#define MUSIC_VOL_ATTACK    0x20        /* attenuation 2, fastest attack */
#define MUSIC_VOL_DECAY     0x20        /* sustain at the same level     */
#define MUSIC_WAVE_RELEASE  0x11        /* square, 24 ms release         */

#define EFFECT_DUTY         48  /* a thin pulse: brighter, more harmonics */
#define EFFECT_VOL_ATTACK   0x10        /* attenuation 1, fastest attack */
#define EFFECT_VOL_DECAY    0x30        /* decays to a quieter sustain   */
#define EFFECT_WAVE_RELEASE 0x13        /* square, ~6 ms release         */

void psg_init(void)
{
    unsigned char i;
    /* Clear the block before enabling it: the VGA starts reading as soon as the
     * register is set, and whatever XRAM held would be eight oscillators of
     * noise. */
    RIA.addr1 = XR_PSG;
    RIA.step1 = 1;
    for (i = 0; i < 8 * OSC_BYTES; i++)
        RIA.rw1 = 0;
    xreg(0, 1, 0x00, XR_PSG);
}

/* Note 0 releases the voice; anything else starts a new note at that pitch.
 * osc is a constant at both call sites, so cc65 folds the address arithmetic
 * and this costs no more than the single-oscillator version did. */
static void psg_note(unsigned char osc, unsigned char note,
                     unsigned char duty, unsigned char vol_attack,
                     unsigned char vol_decay, unsigned char wave_release)
{
    unsigned freq;
    unsigned base = XR_PSG + osc * OSC_BYTES;

    if (!note || note > 36) {
        RIA.addr1 = base + 6;
        RIA.step1 = 1;
        RIA.rw1 = PAN_CENTRE;           /* gate low: release */
        return;
    }

    freq = ((unsigned *)NOTE_FREQ_PSG)[note];

    RIA.addr1 = base;
    RIA.step1 = 1;
    RIA.rw1 = (unsigned char)(freq & 0xFF);
    RIA.rw1 = (unsigned char)(freq >> 8);
    RIA.rw1 = duty;
    RIA.rw1 = vol_attack;
    RIA.rw1 = vol_decay;
    RIA.rw1 = wave_release;
    RIA.rw1 = PAN_CENTRE;               /* gate low... */
    RIA.addr1 = base + 6;
    RIA.rw1 = PAN_CENTRE | 1;           /* ...then high, which strikes the note */
}

/* The music channel. */
void __fastcall__ plat_psg_note(unsigned char note)
{
    psg_note(OSC_MUSIC, note, MUSIC_DUTY, MUSIC_VOL_ATTACK,
             MUSIC_VOL_DECAY, MUSIC_WAVE_RELEASE);
}

/* The sound effect channel. */
void __fastcall__ plat_psg_effect(unsigned char note)
{
    psg_note(OSC_EFFECT, note, EFFECT_DUTY, EFFECT_VOL_ATTACK,
             EFFECT_VOL_DECAY, EFFECT_WAVE_RELEASE);
}
