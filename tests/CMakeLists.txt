# Everything CTest runs.
#
# The two Python checks were CI-only workflow steps, which meant `ctest` locally
# said "all tests passed" while leaving the port's real safety net unrun. They
# are ordinary tests now and CI just runs ctest.

# The converter is what the whole port rests on: it assembles the converted
# sources and checks they reproduce David Murray's shipped X16 binaries byte for
# byte, so every later difference in src/ is one we made on purpose. It needs
# ca65 and the reference sources, nothing that has been built here.
add_test(NAME reference.conversion
         COMMAND ${Python3_EXECUTABLE}
                 ${CMAKE_CURRENT_LIST_DIR}/reference/verify_conversion.py
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(reference.conversion PROPERTIES LABELS reference TIMEOUT 300)

# The invariants the platform layer assumes of the converted assets. These read
# assets/gen, which the ROM build produces, so this cannot run before it.
add_test(NAME assets.invariants
         COMMAND ${Python3_EXECUTABLE}
                 ${CMAKE_CURRENT_LIST_DIR}/assets/verify_assets.py
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(assets.invariants PROPERTIES LABELS assets TIMEOUT 120)

add_subdirectory(emu)
