cmake_minimum_required(VERSION 3.24)
project(raptor LANGUAGES C)

set(RAPTOR_DATADIR "C:/Projects/SDLRaptor/data" CACHE PATH "Default game data directory")

if(NOT MSVC AND NOT UNIX)
    message(FATAL_ERROR "This port targets MSVC (Windows) or a UNIX-like GCC/Clang toolchain (Linux).")
endif()

# CMake treats uppercase .C as C++; the DOS sources are C. Force the language.
function(raptor_mark_as_c)
    set_source_files_properties(${ARGV} PROPERTIES LANGUAGE C)
endfunction()

# ---------------------------------------------------------------------------
# The original DOS/Watcom sources (GFX, SOURCE, audiolib/SOURCE, apodmx) use
# uppercase filenames but lowercase #include "foo.h" directives throughout -
# relying on Windows' case-insensitive filesystem. On a case-sensitive
# filesystem (Linux, ...) build a parallel tree of lowercase symlinks so
# those includes still resolve, without renaming the historical uppercase
# sources themselves (which are still referenced by their real names in the
# source lists below).
# ---------------------------------------------------------------------------
function(raptor_case_shim_dir SRC_DIR OUT_VAR)
    if(WIN32)
        set(${OUT_VAR} "${CMAKE_CURRENT_SOURCE_DIR}/${SRC_DIR}" PARENT_SCOPE)
        return()
    endif()
    set(shim_dir "${CMAKE_BINARY_DIR}/case_shim/${SRC_DIR}")
    file(REMOVE_RECURSE "${shim_dir}")
    file(MAKE_DIRECTORY "${shim_dir}")
    file(GLOB entries "${CMAKE_CURRENT_SOURCE_DIR}/${SRC_DIR}/*")
    foreach(entry ${entries})
        if(NOT IS_DIRECTORY "${entry}")
            get_filename_component(fname "${entry}" NAME)
            string(TOLOWER "${fname}" fname_lower)
            file(CREATE_LINK "${entry}" "${shim_dir}/${fname_lower}" SYMBOLIC)
        endif()
    endforeach()
    set(${OUT_VAR} "${shim_dir}" PARENT_SCOPE)
endfunction()

raptor_case_shim_dir(GFX             RAPTOR_GFX_INCDIR)
raptor_case_shim_dir(SOURCE          RAPTOR_SOURCE_INCDIR)
raptor_case_shim_dir(audiolib/SOURCE RAPTOR_AUDIOLIB_INCDIR)
raptor_case_shim_dir(apodmx          RAPTOR_APODMX_INCDIR)

# ---------------------------------------------------------------------------
# rapcore: portable pieces of the original GFX library + compat layer.
# Shared by the game and the headless tools.
# ---------------------------------------------------------------------------
if(WIN32)
    set(RAPTOR_CRASH_SOURCE port/crash_win.c)
else()
    set(RAPTOR_CRASH_SOURCE port/crash_posix.c)
endif()

set(RAPCORE_SOURCES
    GFX/GLBAPI.C
    GFX/VMEMAPI.C
    GFX/PREFAPI.C
    GFX/exitapi.c
    port/doscompat.c
    port/dpmiapi_win.c
    # here (not in the raptor target) because crash_win.c includes
    # <windows.h>, which the SOURCE include dir would shadow with the
    # game's WINDOWS.H
    ${RAPTOR_CRASH_SOURCE}
)
raptor_mark_as_c(${RAPCORE_SOURCES})
add_library(rapcore STATIC ${RAPCORE_SOURCES})
# port/compat must shadow the MSVC dos.h/conio.h
target_include_directories(rapcore BEFORE PUBLIC port/compat)
target_include_directories(rapcore PUBLIC ${RAPTOR_GFX_INCDIR} port)
if(MSVC)
    target_compile_definitions(rapcore PUBLIC
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_WARNINGS
    )
    # /J: plain char is unsigned, matching the Watcom default the game was built with
    target_compile_options(rapcore PUBLIC /J)
else()
    # -funsigned-char: same rationale as /J above
    target_compile_options(rapcore PUBLIC -funsigned-char)
endif()

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
    port/shop_ui.c
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
    ${RAPTOR_SOURCE_INCDIR} ${RAPTOR_APODMX_INCDIR} audio audio/opl ${RAPTOR_AUDIOLIB_INCDIR})
target_link_libraries(raptor PRIVATE rapcore SDL3::SDL3)
if(UNIX)
    target_link_libraries(raptor PRIVATE m)
endif()
target_compile_definitions(raptor PRIVATE
    RAPTOR_DEFAULT_DATADIR="${RAPTOR_DATADIR}"
    LIBVER_ASSREV=19960108L)
# F5 from the VS IDE otherwise starts in the solution dir, not next to the
# GLB files copied alongside raptor.exe below.
set_property(TARGET raptor PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:raptor>")

# the port supplies the real main(); the game's becomes raptor_main
set_property(SOURCE SOURCE/RAP.C APPEND PROPERTY COMPILE_DEFINITIONS main=raptor_main)

add_custom_command(TARGET raptor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared> $<TARGET_FILE_DIR:raptor>)

# Copy the game's GLB archives next to raptor.exe, so it finds them via its
# own-directory fallback (port/plat_main.c) with no --data/RAPTOR_DATA needed.
file(GLOB RAPTOR_GLB_FILES "${CMAKE_SOURCE_DIR}/data/*.GLB" "${CMAKE_SOURCE_DIR}/data/*.glb")
if(RAPTOR_GLB_FILES)
    add_custom_command(TARGET raptor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${RAPTOR_GLB_FILES} $<TARGET_FILE_DIR:raptor>)
endif()
