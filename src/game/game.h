#ifndef PETSCII_GAME_H
#define PETSCII_GAME_H

/* The ported game logic, as the C platform layer sees it.
 *
 * These are David Murray's routines converted from the X16 assembly, not a
 * transliteration: the same instructions, so the same behaviour. They talk
 * through globals exactly as the original does -- set the inputs, call, read
 * the result -- which is why none of them take arguments.
 *
 * The aliasing that makes cc65 able to call them lives in src/game/capi.s.
 */

/* ---- state the routines read and write ------------------------------- */
extern unsigned char TILE;         /* tile being fetched or plotted        */
extern unsigned char MAP_X, MAP_Y; /* map coordinates, 0..127 and 0..63    */
extern unsigned char UNIT;         /* which unit a routine acts on         */
extern unsigned char MOVE_TYPE;    /* MOVE_WALK, MOVE_HOVER, or both       */
extern unsigned char MOVE_RESULT;  /* 1 if the move happened               */
extern unsigned char UNIT_FIND;    /* 255 if no unit was there             */
extern unsigned char RANDOM;       /* LFSR state; seed it non-zero         */

/* These really are in zero page, and saying so is worth a byte and a cycle at
 * every access. Without it cc65 emits absolute addressing and ld65 warns about
 * the address-size mismatch against capi.s. */
#pragma zpsym("TILE")
#pragma zpsym("MAP_X")
#pragma zpsym("MAP_Y")
#pragma zpsym("UNIT")
#pragma zpsym("MOVE_TYPE")
#pragma zpsym("MOVE_RESULT")
#pragma zpsym("UNIT_FIND")
#pragma zpsym("RANDOM")

#define MOVE_WALK  0x01
#define MOVE_HOVER 0x02

/* TILE_ATTRIB bits. The move test is (attrib & MOVE_TYPE) == MOVE_TYPE. */
#define ATTR_WALKABLE     0x01
#define ATTR_HOVERABLE    0x02
#define ATTR_PUSHABLE     0x04
#define ATTR_DESTRUCTIBLE 0x08
#define ATTR_SEETHROUGH   0x10
#define ATTR_DESTINATION  0x20
#define ATTR_SEARCHABLE   0x40

/* ---- level data ------------------------------------------------------ */
/* Contiguous and in this order, so a level file is one read() into
 * UNIT_TYPE. MAP is page-aligned; see src/rp6502-petscii.cfg. */
extern unsigned char UNIT_TYPE[64], UNIT_LOC_X[64], UNIT_LOC_Y[64];
extern unsigned char UNIT_A[64], UNIT_B[64], UNIT_C[64], UNIT_D[64];
extern unsigned char UNIT_HEALTH[64];
extern unsigned char MAP[128 * 64];

#define LEVEL_BYTES (8 * 64 + 128 * 64)     /* 8704, the whole level file */

/* ---- tileset --------------------------------------------------------- */
extern unsigned char DESTRUCT_PATH[256];
extern unsigned char TILE_ATTRIB[256];

/* ---- routines -------------------------------------------------------- */
void GET_TILE_FROM_MAP(void);      /* MAP_X, MAP_Y -> TILE                 */
void PLOT_TILE_TO_MAP(void);       /* TILE -> MAP_X, MAP_Y                 */
void CHECK_FOR_UNIT(void);         /* MAP_X, MAP_Y -> UNIT_FIND            */
void REQUEST_WALK_UP(void);        /* UNIT, MOVE_TYPE -> MOVE_RESULT       */
void REQUEST_WALK_DOWN(void);
void REQUEST_WALK_LEFT(void);
void REQUEST_WALK_RIGHT(void);
void GENERATE_RANDOM_NUMBER(void); /* advances RANDOM                      */

#endif
