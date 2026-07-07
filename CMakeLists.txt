cmake_minimum_required(VERSION 3.24)
project(raptor LANGUAGES C)

# SDL3 SDK location (prebuilt VC development libraries)
set(RAPTOR_SDL3_ROOT "D:/Programs/SDL3-3.4.12" CACHE PATH "SDL3 SDK root")
list(APPEND CMAKE_PREFIX_PATH "${RAPTOR_SDL3_ROOT}")

set(RAPTOR_DATADIR "C:/Projects/SDLRaptor/data" CACHE PATH "Default game data directory")

if(NOT MSVC)
    message(FATAL_ERROR "This port currently targets MSVC (VS Build Tools).")
endif()

# CMake treats uppercase .C as C++; the DOS sources are C. Force the language.
function(raptor_mark_as_c)
    set_source_files_properties(${ARGV} PROPERTIES LANGUAGE C)
endfunction()

# ---------------------------------------------------------------------------
# rapcore: portable pieces of the original GFX library + compat layer.
# Shared by the game and the headless tools.
# ---------------------------------------------------------------------------
set(RAPCORE_SOURCES
    GFX/GLBAPI.C
    GFX/VMEMAPI.C
    GFX/PREFAPI.C
    GFX/exitapi.c
    port/doscompat.c
    port/dpmiapi_win.c
    # here (not in the raptor target) because it includes <windows.h>,
    # which the SOURCE include dir would shadow with the game's WINDOWS.H
    port/crash_win.c
)
raptor_mark_as_c(${RAPCORE_SOURCES})
add_library(rapcore STATIC ${RAPCORE_SOURCES})
# port/compat must shadow the MSVC dos.h/conio.h
target_include_directories(rapcore BEFORE PUBLIC port/compat)
target_include_directories(rapcore PUBLIC GFX port)
target_compile_definitions(rapcore PUBLIC
    _CRT_SECURE_NO_WARNINGS
    _CRT_NONSTDC_NO_WARNINGS
)
# /J: plain char is unsigned, matching the Watcom default the game was built with
target_compile_options(rapcore PUBLIC /J)

# ---------------------------------------------------------------------------
# glbtool: headless GLB archive inspector (milestone M0 verification)
# ---------------------------------------------------------------------------
add_executable(glbtool port/tools/glbtool.c)
target_link_libraries(glbtool PRIVATE rapcore)

# ---------------------------------------------------------------------------
# raptor: the game
# ---------------------------------------------------------------------------
find_package(SDL3 REQUIRED CONFIG)

set(GAME_SOURCES
    SOURCE/ANIMS.C
    SOURCE/BONUS.C
    SOURCE/ENEMY.C
    SOURCE/ESHOT.C
    SOURCE/FLAME.C
    SOURCE/FX.C
    SOURCE/HELP.C
    SOURCE/INPUT.C
    SOURCE/INTRO.C
    SOURCE/LOADSAVE.C
    SOURCE/MOVIE.C
    SOURCE/OBJECTS.C
    SOURCE/RAP.C
    SOURCE/SHADOWS.C
    SOURCE/SHOTS.C
    SOURCE/STORE.C
    SOURCE/TILE.C
    SOURCE/WINDOWS.C
)
set(GFX_SOURCES
    GFX/GFXAPI.C
    GFX/IMSAPI.C
    GFX/swdapi.c
)
set(PORT_SOURCES
    port/plat_main.c
    port/plat_video.c
    port/plat_pump.c
    port/tsm_svc.c
    port/gfx_blit.c
    port/tile_blit.c
    port/anim_blit.c
    port/ptr_blit.c
    port/kbd_sdl.c
    port/ptr_sdl.c
)
# Compiled unmodified from the original engine (audiolib's FM synth +
# MIDI sequencer, plus apodmx's MUS->MIDI converter); everything else in
# audiolib/apodmx (hardware drivers, task manager, MUSIC.C's wiring) is
# replaced by the audio/*.c files below.
set(AUDIOLIB_SOURCES
    audiolib/SOURCE/MIDI.C
    audiolib/SOURCE/AL_MIDI.C
    audiolib/SOURCE/LL_MAN.C
    # default built-in GM/AdLib timbre bank storage (ADLIB_TimbreBank[]);
    # AL_SetCard immediately overwrites its contents with Raptor's own
    # GENMIDI_OP2 data, so only its storage/version-gating shape matters -
    # this is the one file in the set that still carries the same
    # LIBVER_ASSREV conditionals as the rest of our restoration build.
    audiolib/SOURCE/GMTIMBRE.C
    apodmx/MUS2MID.C
    apodmx/USRHOOKS.C
)
set(AUDIO_SOURCES
    audio/audio_lock.c
    audio/task_audio.c
    audio/sfx_mixer.c
    audio/dmx_sdl.c
    audio/dmx_music.c
    audio/audio_cb.c
    audio/opl/opl3.c
)
raptor_mark_as_c(${GAME_SOURCES} ${GFX_SOURCES} ${AUDIOLIB_SOURCES})

add_executable(raptor ${GAME_SOURCES} ${GFX_SOURCES} ${PORT_SOURCES}
                       ${AUDIOLIB_SOURCES} ${AUDIO_SOURCES})
target_include_directories(raptor PRIVATE
    SOURCE apodmx audio audio/opl audiolib/SOURCE)
target_link_libraries(raptor PRIVATE rapcore SDL3::SDL3)
target_compile_definitions(raptor PRIVATE
    RAPTOR_DEFAULT_DATADIR="${RAPTOR_DATADIR}"
    LIBVER_ASSREV=19960108L)

# the port supplies the real main(); the game's becomes raptor_main
set_property(SOURCE SOURCE/RAP.C APPEND PROPERTY COMPILE_DEFINITIONS main=raptor_main)

add_custom_command(TARGET raptor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared> $<TARGET_FILE_DIR:raptor>)
