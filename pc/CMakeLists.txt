cmake_minimum_required(VERSION 3.21)
project(psion3d_pc LANGUAGES C CXX)

# A native build of the game for development only. The Psion build is still the
# shipping target and is driven entirely by ../unnamed.pr and ../make.bat, which
# know nothing about this directory.
#
# The whole port rests on one trick: fp_types.h does `#include <plib.h>`, so
# putting pc/compat/ on the include path -- and NOT the SIBO SDK -- resolves the
# SDK headers to the replacements here. No game source needs to change.

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

set(GAME_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Mirrors the #compile list in ../unnamed.pr, minus the platform modules:
# psion3d.c (WLIB windows and the event pump), debug.c (gPrintText and PLIB's
# software float package), videomem.a and fpasm.a (JPI assembler).
# Listed explicitly, never globbed -- a glob would sweep psion3d.c back in.
set(GAME_SOURCES
    ${GAME_ROOT}/bitmap.c
    ${GAME_ROOT}/enemy.c
    ${GAME_ROOT}/player.c
    ${GAME_ROOT}/draw.c
    ${GAME_ROOT}/walls.c
    ${GAME_ROOT}/labwall.c
    ${GAME_ROOT}/sprite.c
    ${GAME_ROOT}/game_map.c
    ${GAME_ROOT}/pickup.c
    ${GAME_ROOT}/fp_math.c
    ${GAME_ROOT}/gameloop.c
)

set(PC_SOURCES
    src/pcclock.c       # monotonic clock; the only file that sees <windows.h>
    src/plib_pc.c       # file I/O, segments, ticks, string helpers
    src/fpasm_pc.c      # fpmul, plus the layout assertions fpdiv depends on
    src/debug_pc.c      # stands in for debug.c
    src/host.c          # the frame driver; the only PC file that includes game headers
    src/main_pc.cpp
    src/GameView.cpp
    src/MainWindow.cpp
)

qt_add_executable(psion3d_pc ${GAME_SOURCES} ${PC_SOURCES})

target_include_directories(psion3d_pc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/compat   # <plib.h> and <wlib.h> resolve HERE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GAME_ROOT}
)

target_compile_definitions(psion3d_pc PRIVATE
    PSION3D_PC=1
    PSION3D_DEFAULT_ASSET_ROOT="${GAME_ROOT}"
)

target_link_libraries(psion3d_pc PRIVATE Qt6::Widgets)

# Keep the console attached: p_open and the segment shims report failures there,
# and a missing asset is the commonest PC-side problem.
set_target_properties(psion3d_pc PROPERTIES WIN32_EXECUTABLE OFF)

if(MSVC)
    # C4068 unknown pragma  - fp_types.h's `#pragma call` is for TopSpeed only.
    # C4505 unused local fn - the static helpers in game_map.h, fp_math.h and
    #                         units.h are unused in most translation units.
    target_compile_options(psion3d_pc PRIVATE /W3 /wd4068 /wd4505)
else()
    target_compile_options(psion3d_pc PRIVATE
        -Wall -Wextra
        -Wno-unknown-pragmas
        -Wno-unused-function
        -Wno-unused-const-variable
        # Uniform signatures behind the wall_draw_fn contract mean several
        # wall and pickup handlers ignore parameters they must still accept.
        -Wno-unused-parameter)
endif()

# Noisy, but flags exactly the implicit narrowings where the 16-bit target would
# have wrapped and this one will not. Worth a look after renderer changes.
option(PSION3D_AUDIT_CONVERSIONS "Enable -Wconversion on the game sources" OFF)

if(PSION3D_AUDIT_CONVERSIONS AND NOT MSVC)
    set_source_files_properties(${GAME_SOURCES} PROPERTIES
        COMPILE_OPTIONS "-Wconversion;-Wsign-conversion")
endif()
