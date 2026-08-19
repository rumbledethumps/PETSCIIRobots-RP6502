include(EmuTest)

petscii_add_emu_test(menu    ROM ${PETSCII_ROM})
petscii_add_emu_test(bringup ROM ${PETSCII_ROM})
petscii_add_emu_test(walk    ROM ${PETSCII_ROM})
petscii_add_emu_test(ai      ROM ${PETSCII_ROM})
petscii_add_emu_test(search  ROM ${PETSCII_ROM})
petscii_add_emu_test(livemap ROM ${PETSCII_ROM})
petscii_add_emu_test(audio   ROM ${PETSCII_ROM})
petscii_add_emu_test(audio_channels ROM ${PETSCII_ROM})
petscii_add_emu_test(irq_zp  ROM ${PETSCII_ROM})
petscii_add_emu_test(animate ROM ${PETSCII_ROM})
petscii_add_emu_test(overlay ROM ${PETSCII_ROM})
petscii_add_emu_test(flash   ROM ${PETSCII_ROM} NEEDS_POKE)
petscii_add_emu_test(keys    ROM ${PETSCII_ROM})
petscii_add_emu_test(gamepad ROM ${PETSCII_ROM})
petscii_add_emu_test(controls ROM ${PETSCII_ROM})
petscii_add_emu_test(endgame ROM ${PETSCII_ROM} NEEDS_POKE TIMEOUT 180)
petscii_add_emu_test(elevator ROM ${PETSCII_ROM} NEEDS_POKE)
petscii_add_emu_test(gameover ROM ${PETSCII_ROM} NEEDS_POKE TIMEOUT 180)

# The generated scripts are inputs to ctest, not to any target, so something has
# to ask for them to exist.
add_custom_target(petscii_emu_scripts ALL DEPENDS ${PETSCII_EMU_SCRIPTS})
