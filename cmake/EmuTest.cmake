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
#
# Asked of the usage text rather than by running a script: at configure time the
# ROM has not been built, and without one the emulator exits non-zero whatever
# the script says, so a run cannot tell "no poke" from "no ROM".
if(RP6502_EMU AND NOT DEFINED RP6502_EMU_HAS_POKE)
    execute_process(COMMAND ${RP6502_EMU} --help
                    OUTPUT_VARIABLE _usage ERROR_VARIABLE _usage_err)
    string(APPEND _usage "${_usage_err}")
    if(_usage MATCHES "[\r\n]  poke ")
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

    # Scripts run from a generated copy with @SYMBOL@ replaced by the address
    # the linker actually chose. Doing it at build time rather than by hand is
    # what stops a test breaking every time the code above a variable changes
    # size -- which it did, four at once, when the gamepad code landed.
    set(_script ${CMAKE_CURRENT_BINARY_DIR}/scripts/${name}.txt)
    add_custom_command(
        OUTPUT ${_script}
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/expand_symbols.py
                $<TARGET_FILE:robots>.map
                ${CMAKE_CURRENT_LIST_DIR}/${name}.txt ${_script}
        DEPENDS ${CMAKE_CURRENT_LIST_DIR}/${name}.txt
                ${CMAKE_SOURCE_DIR}/tools/expand_symbols.py robots
        COMMENT "Resolving symbols in ${name}.txt"
        VERBATIM)
    list(APPEND PETSCII_EMU_SCRIPTS ${_script})
    set(PETSCII_EMU_SCRIPTS ${PETSCII_EMU_SCRIPTS} PARENT_SCOPE)

    add_test(NAME emu.${name}
             COMMAND ${RP6502_EMU} --mute --seed ${E_SEED} --tmpdrive
                     --script ${_script} ${E_ROM}
             WORKING_DIRECTORY ${_scratch})
    set_tests_properties(emu.${name} PROPERTIES TIMEOUT ${E_TIMEOUT} LABELS emu)
endfunction()
