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

/* The message pointer PRINT_INFO reads. Two zero page bytes, so it is a real
 * pointer here rather than a pair of bytes. */
extern unsigned char *SOURCE;
#pragma zpsym("SOURCE")

extern unsigned char MAP_WINDOW_X, MAP_WINDOW_Y;  /* top-left of the window   */
extern unsigned char REDRAW_WINDOW;               /* 1 = redraw is due        */
extern unsigned char SCREEN_SHAKE;                /* 1 = shake this frame     */
#pragma zpsym("MAP_WINDOW_X")
#pragma zpsym("MAP_WINDOW_Y")
#pragma zpsym("REDRAW_WINDOW")
#pragma zpsym("SCREEN_SHAKE")

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

/* ---- per-unit working state ------------------------------------------ */
extern unsigned char UNIT_TIMER_A[64], UNIT_TIMER_B[64];
extern unsigned char UNIT_TILE[32];     /* the tile each visible unit draws as */
extern unsigned char MAP_PRECALC[77];   /* units inside the 11x7 window        */

/* Set once a frame by the main loop; BACKGROUND_TASKS consumes it. On the X16
 * this was the IRQ's job. */
extern unsigned char BGTIMER1, BGTIMER2;

/* Maintained by the VSYNC interrupt in src/game/irq.s. IRQ_FRAME is the frame
 * reference anything outside the interrupt uses to know time has passed. */
extern unsigned char IRQ_FRAME, KEYTIMER, CLOCK_ACTIVE;
extern unsigned char CYCLES, SECONDS, MINUTES, HOURS;
extern unsigned char KEYS, INV_MAGNET;

/* Only one of each may be running at a time. */
extern unsigned char BIG_EXP_ACT, MAGNET_ACT, PLASMA_ACT;

/* ---- inventory and selection ----------------------------------------- */
extern unsigned char AMMO_PISTOL, AMMO_PLASMA;
extern unsigned char INV_BOMBS, INV_EMP, INV_MEDKIT, INV_MAGNET;
extern unsigned char SELECTED_WEAPON;   /* 0 none, 1 pistol, 2 plasma        */
extern unsigned char SELECTED_ITEM;     /* 0 none, 1 bomb, 2 EMP, 3 medkit,
                                           4 magnet                          */
extern unsigned char PLAYER_DIRECTION;  /* 0 up, 3 down, 6 left, 9 right     */
extern unsigned char PLAYER_ANIMATE;    /* 0..2, added for the sprite frame  */
extern unsigned char CONTROL;           /* 0 keyboard, 1 custom, 2 gamepad   */

extern unsigned char CURSOR_X, CURSOR_Y;  /* selection cursor, window cells  */
extern unsigned char CURSOR_ON;           /* 0 off, 1 compass, 2 lens, 3 hand */
#pragma zpsym("CURSOR_X")
#pragma zpsym("CURSOR_Y")
#pragma zpsym("CURSOR_ON")

/* The thirteen bindings, in STANDARD_CONTROLS order. */
extern unsigned char KEY_MOVE_UP[13];
extern unsigned char STANDARD_CONTROLS[13];

/* Sound effect numbers, from reference/x16/sounds.inc. The X16 played these as
 * digitised samples; the RP6502 will play the original PET engine's patterns on
 * the PSG. The numbers are unchanged either way. */
#define SFX_BEEP2         0
#define SFX_BEEP          1
#define SFX_CYCLEITEM     2
#define SFX_CYCLEWEAPON   3
#define SFX_DOOR          4
#define SFX_EMP           5
#define SFX_ERROR         6
#define SFX_EXPLOSION     7
#define SFX_FOUNDITEM     8
#define SFX_MAGNET2       9
#define SFX_MAGNET       10
#define SFX_MEDKIT       11
#define SFX_MOVE         12
#define SFX_PISTOL       13
#define SFX_PLASMA       14
#define SFX_SHOCK        15

/* ---- the intro menu -------------------------------------------------- */
extern unsigned char MENUY;             /* 0..3, which option is selected    */
extern unsigned char MENUCOL;
extern unsigned char SELECTED_MAP;      /* 0..13                             */
extern unsigned char DIFF_LEVEL;        /* 0 easy, 1 normal, 2 hard          */
extern unsigned char SPRITECOLSTATE, SPRITECOLTIMER;
extern unsigned char SPRITECOLCHART[8];
extern unsigned char MAP_NAMES[];       /* fourteen sixteen-byte names       */

/* Screen layouts, run-length encoded 40x30 grids. */
extern unsigned char INTRO_TEXT[], SCR_TEXT[], SCR_ENDGAME[], SCR_CUSTOM_KEYS[];
extern unsigned char CINEMA_MESSAGE[], THREE_FACES[];

/* Screen-code strings: 0 ends one, 255 forces a new line. */
extern unsigned char INTRO_MESSAGE[];
extern unsigned char MSG_SEARCHING[];

/* ---- the actions the player can take --------------------------------- */
void USE_ITEM(void);
void SEARCH_OBJECT(void);
void MOVE_OBJECT(void);
void FIRE_UP(void);
void FIRE_DOWN(void);
void FIRE_LEFT(void);
void FIRE_RIGHT(void);

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

/* One pass of unit AI, gated on BGTIMER1, plus the window redraw it may ask
 * for. Every unit in slots 1..63 whose timer has expired runs its routine. */
void BACKGROUND_TASKS(void);
void MAP_PRE_CALCULATE(void);      /* collect visible units into MAP_PRECALC */
void CACULATE_AND_REDRAW(void);    /* centre the window on the player (sic)  */

#endif
