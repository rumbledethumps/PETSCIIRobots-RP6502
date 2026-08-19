add_executable(robots)
target_sources(robots PRIVATE
    main.c
    platform.c
    input.c
    psg.c
    plot.s                    # the tile blitter, in assembly because the profile said so
    # David Murray's game logic, converted from ACME to ca65. These are the
    # original instructions, not a transliteration, so they behave exactly as
    # the X16 build does.
    game/background_tasks.s   # the AI dispatcher and all 24 unit types
    game/window.s             # what should be on screen, not how to draw it
    game/transporter.s        # unit type 23, kept apart as the X16 did
    game/music.s              # the PET sound engine, recovered
    game/music_data.s         # its patterns and the note table
    game/messages.s
    game/screens.s            # the four screen layouts and the intro artwork
    game/items.s              # using, searching, pushing, firing
    game/irq.s                # the VSYNC interrupt: timers and the game clock
    game/globals.s
    game/capi.s               # cc65 name aliasing, the only file that knows C exists
    game/platform_bridge.s    # the AI's calls into the platform layer
)
target_include_directories(robots PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# The PET had one voice, so a sound effect had to stop the music and wait for
# its row. This machine has eight oscillators and does not, which is the better
# game but not the original one. Turn this on to put the PET's behaviour back
# and A/B the two against real hardware; see the header of game/music.s.
option(PETSCII_AUTHENTIC_AUDIO "Play music and sound effects on one voice, as the PET did" OFF)
if(PETSCII_AUTHENTIC_AUDIO)
    # Not target_compile_definitions: that becomes cl65's -D, which defines a
    # symbol for the C preprocessor and never reaches ca65. Assembler symbols
    # are a separate option, and SHELL: keeps its two words from being merged.
    target_compile_options(robots PRIVATE
        "$<$<COMPILE_LANGUAGE:ASM>:SHELL:--asm-define PETSCII_AUTHENTIC_AUDIO>")
endif()
add_dependencies(robots petscii_assets)

# GET_TILE_FROM_MAP builds the map address arithmetically rather than by
# indexing, so MAP has to start on a page boundary. The stock rp6502.cfg has no
# aligned segment, so the project ships one with LEVELDATA added.
target_link_options(robots PRIVATE -C ${CMAKE_CURRENT_LIST_DIR}/rp6502-petscii.cfg)
# ...and relink when it changes, which CMake does not infer from a link option.
set_property(TARGET robots PROPERTY LINK_DEPENDS
             ${CMAKE_CURRENT_LIST_DIR}/rp6502-petscii.cfg)

# Static XRAM: loaded straight into place when the ROM loads, costing the 6502
# nothing at run time. Addresses are the XRAM map in xram.h plus 0x10000.
rp6502_asset(robots 0x1A000 ${AGEN}/font.bin)         # XR_FONT
rp6502_asset(robots 0x1A800 ${AGEN}/spr_player.bin)   # XR_SPR_PLAYER
rp6502_asset(robots 0x1C200 ${AGEN}/spr_cursors.bin)  # XR_SPR_CURSOR
rp6502_asset(robots 0x1C800 ${AGEN}/spr_hud.bin)      # XR_SPR_HUD
rp6502_asset(robots 0x1E000 ${AGEN}/palettes.bin)     # XR_PAL_BITMAP, _CHARS, _PLAYER

# Named files in the ROM's micro filesystem, opened at run time. The two 320x240
# screens share one 38400-byte region of XRAM, and 14 levels are 122 KB, so
# neither can be resident.
rp6502_asset(robots tiles    ${AGEN}/tiles.bin)
rp6502_asset(robots gamepic  ${AGEN}/gamepic.bin)
rp6502_asset(robots intropic ${AGEN}/intropic.bin)
foreach(L a b c d e f g h i j k l m n)
    rp6502_asset(robots level-${L} ${AGEN}/level-${L}.bin)
endforeach()

rp6502_executable(robots DATA default RESET default)

# rp6502_executable() writes ${name}.rp6502 into the current binary dir but does
# not export the path; tests/emu needs it.
set(PETSCII_ROM ${CMAKE_CURRENT_BINARY_DIR}/robots.rp6502
    CACHE INTERNAL "packaged ROM")
