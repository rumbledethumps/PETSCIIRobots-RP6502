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

# petscii_add_emu_test(<name> ROM <path> [TIMEOUT <s>] [SEED <n>])
#
# --mute so CI opens no audio device, --seed so anything asking for entropy gets
# the same entropy every run, --tmpdrive so MSC0: is a fresh throwaway and a
# previous run's saves cannot change the outcome. The emulator is headless under
# --script: it drives sys_run_frame() itself and never opens a window, so no X
# server is involved. A failed check names the script line and exits 1.
function(petscii_add_emu_test name)
    cmake_parse_arguments(E "" "ROM;TIMEOUT;SEED" "" ${ARGN})
    if(NOT RP6502_EMU)
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
