# Convert the X16 game data into the shapes the RP6502 wants.
#
# Nothing here edits a binary by hand: every asset in assets/gen/ is produced
# from assets/src/ by a script in tools/convert/, so touching a source file
# regenerates it on the next build.

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(ASRC ${CMAKE_SOURCE_DIR}/assets/src)
set(AGEN ${CMAKE_SOURCE_DIR}/assets/gen)
set(CONV ${CMAKE_SOURCE_DIR}/tools/convert)

file(MAKE_DIRECTORY ${AGEN})

# petscii_convert(<output> <script> <inputs...>)
function(petscii_convert out script)
    add_custom_command(
        OUTPUT ${AGEN}/${out}
        DEPENDS ${CONV}/${script} ${ARGN}
        COMMAND ${Python3_EXECUTABLE} ${CONV}/${script} ${ARGN} ${AGEN}/${out}
        COMMENT "asset ${out}"
        VERBATIM)
    set(PETSCII_ASSETS ${PETSCII_ASSETS} ${AGEN}/${out} PARENT_SCOPE)
endfunction()

petscii_convert(font.bin     conv_font.py     ${ASRC}/gfxfont.bin)
petscii_convert(tiles.bin    conv_tiles.py    ${ASRC}/TILESET.GFX)
petscii_convert(palettes.bin conv_palette.py  ${CMAKE_SOURCE_DIR}/reference/x16/x16Robots.ASM)
petscii_convert(intropic.bin conv_rle.py      ${ASRC}/intropic.rle)
petscii_convert(gamepic.bin  conv_rle.py      ${ASRC}/gamepic.rle)

foreach(L a b c d e f g h i j k l m n)
    petscii_convert(level-${L}.bin conv_level.py ${ASRC}/levels/level-${L})
endforeach()

# Three outputs from three inputs, so it does not fit the one-output helper.
add_custom_command(
    OUTPUT ${AGEN}/spr_player.bin ${AGEN}/spr_cursors.bin ${AGEN}/spr_hud.bin
    DEPENDS ${CONV}/conv_sprites.py
            ${ASRC}/PSPRITE_DATA.BIN ${ASRC}/CURSORS4BIT.BIN ${ASRC}/SPRITE_DATA.BIN
    COMMAND ${Python3_EXECUTABLE} ${CONV}/conv_sprites.py
            --player ${ASRC}/PSPRITE_DATA.BIN
            --cursors ${ASRC}/CURSORS4BIT.BIN
            --hud ${ASRC}/SPRITE_DATA.BIN
            --out ${AGEN}
    COMMENT "asset sprites"
    VERBATIM)
list(APPEND PETSCII_ASSETS
     ${AGEN}/spr_player.bin ${AGEN}/spr_cursors.bin ${AGEN}/spr_hud.bin)

add_custom_target(petscii_assets DEPENDS ${PETSCII_ASSETS})
