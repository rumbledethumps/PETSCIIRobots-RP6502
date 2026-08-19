# Drive the RP6502 emulator from CTest.
#
# The emulator is fetched, not built: `cmake -P tools/rp6502.cmake` puts it in
# tools/. A plain `cmake --preset` never does, because rp6502_fetch_emulator()
# runs only under RP6502_TOOLS_FETCHED and only script mode sets that. So a
# fresh clone and CI both need the explicit fetch, and this module says so
# rather than silently registering nothing.

if(CMAKE_HOST_SYSTEM_VERSION MATCHES "[Mm]icrosoft" OR CMAKE_HOST_WIN32)
    set(_emu_names rp6502-emu rp6502-emu.exe)
else()
    set(_emu_names rp6502-emu)
endif()
find_program(RP6502_EMU NAMES ${_emu_names}
             HINTS ${CMAKE_SOURCE_DIR}/tools NO_DEFAULT_PATH)
if(NOT RP6502_EMU)
    message(WARNING
        "No rp6502-emu, so the script tests will not be registered. Fetch it with:\n"
        "    cmake -P ${CMAKE_SOURCE_DIR}/tools/rp6502.cmake")
endif()

# Does this emulator have `poke`? Some tests reach states that no sequence of
# keys does -- the endgame needs every robot dead, the elevator panel is behind
# a locked door -- and the way to those is to write the state and let the game
# read it back. `poke` is newer than the emulator CI currently fetches, so the
# tests that need it are registered only where it exists rather than failing
# everywhere it does not. They light up on their own once CI catches up.
if(RP6502_EMU AND NOT DEFINED RP6502_EMU_HAS_POKE)
    set(_probe ${CMAKE_CURRENT_BINARY_DIR}/poke-probe.txt)
    file(WRITE ${_probe} "poke $0002 $00\n")
    execute_process(COMMAND ${RP6502_EMU} --mute --seed 1 --tmpdrive
                            --script ${_probe} ${PETSCII_ROM}
                    RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_QUIET)
    if(_rc EQUAL 0)
        set(RP6502_EMU_HAS_POKE ON CACHE INTERNAL "emulator supports poke")
        message(STATUS "rp6502-emu supports poke")
    else()
        set(RP6502_EMU_HAS_POKE OFF CACHE INTERNAL "emulator supports poke")
        message(STATUS "rp6502-emu has no poke; skipping the tests that need it")
    endif()
endif()

# petscii_add_emu_test(<name> ROM <path> [TIMEOUT <s>] [SEED <n>] [NEEDS_POKE])
#
# --mute so CI opens no audio device, --seed so anything asking for entropy gets
# the same entropy every run, --tmpdrive so MSC0: is a fresh throwaway and a
# previous run's saves cannot change the outcome. The emulator is headless under
# --script: it drives sys_run_frame() itself and never opens a window, so no X
# server is involved. A failed check names the script line and exits 1.
function(petscii_add_emu_test name)
    cmake_parse_arguments(E "NEEDS_POKE" "ROM;TIMEOUT;SEED" "" ${ARGN})
    if(NOT RP6502_EMU)
        return()
    endif()
    if(E_NEEDS_POKE AND NOT RP6502_EMU_HAS_POKE)
        return()
    endif()
    if(NOT E_SEED)
        set(E_SEED 1)
    endif()
    if(NOT E_TIMEOUT)
        set(E_TIMEOUT 120)
    endif()
    set(_scratch ${CMAKE_CURRENT_BINARY_DIR}/scratch/${name})
    file(MAKE_DIRECTORY ${_scratch})
    add_test(NAME emu.${name}
             COMMAND ${RP6502_EMU} --mute --seed ${E_SEED} --tmpdrive
                     --script ${CMAKE_CURRENT_LIST_DIR}/${name}.txt ${E_ROM}
             WORKING_DIRECTORY ${_scratch})
    set_tests_properties(emu.${name} PROPERTIES TIMEOUT ${E_TIMEOUT} LABELS emu)
endfunction()
