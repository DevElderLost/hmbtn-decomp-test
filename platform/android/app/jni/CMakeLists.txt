# Native library of the Android app: libmain.so (the game, loaded by SDL) and libSDL2.so.
cmake_minimum_required(VERSION 3.18)
project(hmbtn_android C)

get_filename_component(HMBTN_PLATFORM "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(HMBTN_NATIVE "${HMBTN_PLATFORM}/native")
set(HMBTN_COMPAT "${HMBTN_PLATFORM}/compat")

if(NOT SDL_DIR)
    set(SDL_DIR "${HMBTN_PLATFORM}/android/SDL")
endif()
if(NOT EXISTS "${SDL_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "SDL2 not found at ${SDL_DIR}. Clone it first:\n"
        "  git clone --depth 1 --branch release-2.30.9 https://github.com/libsdl-org/SDL.git platform/android/SDL")
endif()

# Only the shared library is needed.
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory("${SDL_DIR}" SDL2-build)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# The console's hardware: address space, software GTE and GPU, controller shim.
add_library(psx_core STATIC
    "${HMBTN_NATIVE}/psx_mem.c"
    "${HMBTN_NATIVE}/psx_gte.c"
    "${HMBTN_NATIVE}/psx_gpu.c"
    "${HMBTN_NATIVE}/psx_pad.c")
target_include_directories(psx_core PUBLIC "${HMBTN_NATIVE}" "${HMBTN_COMPAT}")
target_compile_options(psx_core PRIVATE -Wall -Wextra)

# The PSY-Q compatibility libraries (libgte, libgpu, libgs, libcd, libsnd, libmcrd, ...), the
# demo scene, and the platform hooks. They see the compat headers ahead of the system ones: see
# platform/compat/psyq.
add_library(psx_compat STATIC
    "${HMBTN_NATIVE}/psx_libgte.c"
    "${HMBTN_NATIVE}/psx_libgpu.c"
    "${HMBTN_NATIVE}/psx_cd.c"
    "${HMBTN_NATIVE}/psx_mcard.c"
    "${HMBTN_NATIVE}/psx_snd.c"
    "${HMBTN_NATIVE}/psx_system.c"
    "${HMBTN_NATIVE}/psx_platform.c"
    "${HMBTN_NATIVE}/demo_cube.c")
target_include_directories(psx_compat BEFORE PRIVATE
    "${HMBTN_COMPAT}/override"
    "${HMBTN_COMPAT}/psyq")
target_include_directories(psx_compat PUBLIC "${HMBTN_NATIVE}")
target_compile_options(psx_compat PRIVATE -funsigned-char -Wall -Wextra)
set_target_properties(psx_compat PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
target_link_libraries(psx_compat PUBLIC psx_core)

# The game itself: the decompiled sources, unchanged except for the HMBTN_NATIVE branches of a
# few lines. The build replaces two files:
#   src/main/overlay_regions.c  -> platform/native/overlay_native.c (the load regions as RAM addresses)
#   src/main/text/text.c        -> a copy with three tables defined plainly: text.h declares them
#                                  extern and text.c defines them static, which the original
#                                  compiler accepted and a current one does not.
# The game's main() is renamed so SDL's main can start it.
get_filename_component(HMBTN_ROOT "${HMBTN_PLATFORM}/.." ABSOLUTE)
file(GLOB_RECURSE HMBTN_GAME_SOURCES CONFIGURE_DEPENDS
    "${HMBTN_ROOT}/src/main/*.c" "${HMBTN_ROOT}/src/overlay/*.c")
list(FILTER HMBTN_GAME_SOURCES EXCLUDE REGEX "/src/main/overlay_regions\\.c$|/src/main/text/text\\.c$")

file(READ "${HMBTN_ROOT}/src/main/text/text.c" HMBTN_TEXT_C)
string(REGEX REPLACE "(^|\n)static char (text_fmt_(dec|hex|int)\\[4\\])" "\\1char \\2" HMBTN_TEXT_C "${HMBTN_TEXT_C}")
set(HMBTN_TEXT_C_PATH "${CMAKE_CURRENT_BINARY_DIR}/generated/text.c")
file(WRITE "${HMBTN_TEXT_C_PATH}" "${HMBTN_TEXT_C}")

# Every directory under src/ with a header is on the include path, as in the original build.
file(GLOB_RECURSE HMBTN_HEADERS "${HMBTN_ROOT}/src/*.h")
set(HMBTN_GAME_INCLUDES "")
foreach(header ${HMBTN_HEADERS})
    get_filename_component(dir "${header}" DIRECTORY)
    list(APPEND HMBTN_GAME_INCLUDES "${dir}")
endforeach()
list(REMOVE_DUPLICATES HMBTN_GAME_INCLUDES)

add_library(hmbtn_game STATIC
    ${HMBTN_GAME_SOURCES}
    "${HMBTN_TEXT_C_PATH}"
    "${HMBTN_NATIVE}/overlay_native.c")
target_include_directories(hmbtn_game BEFORE PRIVATE
    "${HMBTN_COMPAT}/override"
    "${HMBTN_COMPAT}/psyq"
    "${HMBTN_COMPAT}"
    "${HMBTN_NATIVE}")
target_include_directories(hmbtn_game PRIVATE ${HMBTN_GAME_INCLUDES} "${HMBTN_ROOT}/src")
target_compile_definitions(hmbtn_game PRIVATE HMBTN_NATIVE _LANGUAGE_C __CHAR_UNSIGNED__)
target_compile_options(hmbtn_game PRIVATE -funsigned-char -w)
set_target_properties(hmbtn_game PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
set_source_files_properties("${HMBTN_ROOT}/src/main/main.c" PROPERTIES COMPILE_DEFINITIONS "main=hmbtn_game_main")
target_link_libraries(hmbtn_game PUBLIC psx_compat)

# SDL loads this library and calls its SDL_main (main() in main_sdl.c).
add_library(main SHARED
    "${HMBTN_NATIVE}/main_sdl.c"
    "${HMBTN_NATIVE}/touch_pad.c")
target_compile_definitions(main PRIVATE HMBTN_GAME)
target_compile_options(main PRIVATE -Wall -Wextra)
target_link_libraries(main hmbtn_game psx_compat psx_core SDL2)
