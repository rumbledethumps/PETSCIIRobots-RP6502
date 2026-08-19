#ifndef PETSCII_PROBE_H
#define PETSCII_PROBE_H

/* A fixed block the game refreshes once a frame, so tests can assert on state
 * without knowing where the linker put anything.
 *
 * It lives in XRAM rather than RAM because XRAM addresses are chosen by the
 * program: XR_PROBE is a constant in xram.h, whereas a RAM symbol moves every
 * time the code above it changes size, which would make every `peek` in
 * tests/emu a maintenance burden.
 *
 * Sixteen XRAM writes a frame is nothing, so it stays in release builds and a
 * shipped ROM is debuggable in the field.
 */

#define PROBE_MAGIC_0 'P'
#define PROBE_MAGIC_1 'R'

/* Byte offsets from XR_PROBE. */
#define PROBE_MAGIC     0   /* 2 bytes, 'P' 'R'                     */
#define PROBE_STATE     2   /* PROBE_STATE_* below                  */
#define PROBE_LEVEL     3   /* 0..13                                */
#define PROBE_PLAYER_X  4
#define PROBE_PLAYER_Y  5
#define PROBE_HEALTH    6
#define PROBE_ROBOTS    7   /* live units in slots 1..27            */
#define PROBE_RNG       8   /* current LFSR value                   */
#define PROBE_FRAME     9   /* wraps at 256                         */
#define PROBE_WINDOW_X 10
#define PROBE_WINDOW_Y 11
#define PROBE_SIZE     16

#define PROBE_STATE_BOOT    0
#define PROBE_STATE_TITLE   1
#define PROBE_STATE_PLAYING 2
#define PROBE_STATE_DEAD    3
#define PROBE_STATE_WON     4

#endif
