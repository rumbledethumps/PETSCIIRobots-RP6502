/* The RIA PSG, playing the PET engine's notes.
 *
 * Eight oscillators of eight bytes each, live in XRAM. The engine only uses one
 * -- the PET had a single voice and sharing it is what makes a sound effect
 * interrupt the music -- but the block is enabled whole.
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

/* Portal 1 throughout. plat_psg_note is only ever called from the VSYNC
 * interrupt, which saves and restores portal 1 around everything it does, so
 * the main loop's portal 0 work is never disturbed. psg_init runs before the
 * interrupt is enabled, which is why it may use portal 1 too. */

/* A square wave, struck hard and held: instant attack, sustain equal to peak,
 * short release. That is as close as a PSG gets to the PET's raw gated square. */
#define DUTY        128
#define VOL_ATTACK  0x20        /* attenuation 2, fastest attack   */
#define VOL_DECAY   0x20        /* sustain at the same level       */
#define WAVE_RELEASE 0x11       /* square, 24 ms release           */
#define PAN_CENTRE  0x00        /* centre, gate clear              */

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

/* The engine's only output. Note 0 releases the voice; anything else starts a
 * new note at that pitch. */
void __fastcall__ plat_psg_note(unsigned char note)
{
    unsigned freq;

    if (!note || note > 36) {
        RIA.addr1 = XR_PSG + OSC_MUSIC * OSC_BYTES + 6;
        RIA.step1 = 1;
        RIA.rw1 = PAN_CENTRE;           /* gate low: release */
        return;
    }

    freq = ((unsigned *)NOTE_FREQ_PSG)[note];

    RIA.addr1 = XR_PSG + OSC_MUSIC * OSC_BYTES;
    RIA.step1 = 1;
    RIA.rw1 = (unsigned char)(freq & 0xFF);
    RIA.rw1 = (unsigned char)(freq >> 8);
    RIA.rw1 = DUTY;
    RIA.rw1 = VOL_ATTACK;
    RIA.rw1 = VOL_DECAY;
    RIA.rw1 = WAVE_RELEASE;
    RIA.rw1 = PAN_CENTRE;               /* gate low... */
    RIA.addr1 = XR_PSG + OSC_MUSIC * OSC_BYTES + 6;
    RIA.rw1 = PAN_CENTRE | 1;           /* ...then high, which strikes the note */
}
