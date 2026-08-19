# Attack of the PETSCII Robots — RP6502 port.
#
# One source tree, two kinds of build:
#
#   llvm-mos/*, cc65/*   cross-compiled ROMs
#   host/*               native test binaries (utest.h + CTest)
#
# A CMake toolchain is a property of the build tree, not a directory, so these
# cannot share one. They share this file instead: tools/rp6502.cmake sets
# CMAKE_TOOLCHAIN_FILE (so it must run before project()) and fatals when neither
# compiler variable is defined, so the host tree simply does not include it.
# `cmake --list-presets` shows both.

cmake_minimum_required(VERSION 3.21)

if(DEFINED CC65_TARGET_SYSTEM OR DEFINED LLVM_MOS_PLATFORM)
    set(PETSCII_TREE rp6502)
    include(${CMAKE_CURRENT_LIST_DIR}/tools/rp6502.cmake)
else()
    set(PETSCII_TREE host)
endif()

project(petscii-robots-rp6502 VERSION 0.1.0 LANGUAGES C CXX ASM)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)
set(PETSCII_ASSET_DIR ${CMAKE_CURRENT_LIST_DIR}/assets CACHE PATH "Game data")

if(PETSCII_TREE STREQUAL "rp6502")
    include(Assets)
    add_subdirectory(src)

    # The emulator is a host program driven from outside, and the asset checks
    # read what this tree generates, so the whole suite belongs to the tree that
    # builds the ROM.
    enable_testing()
    add_subdirectory(tests)
endif()
