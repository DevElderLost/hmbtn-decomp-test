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

# The PSY-Q compatibility libraries (libgte, libgpu, libgs) and the demo scene that uses them.
# They see the compat headers ahead of the system ones: see platform/compat/psyq.
add_library(psx_compat STATIC
    "${HMBTN_NATIVE}/psx_libgte.c"
    "${HMBTN_NATIVE}/psx_libgpu.c"
    "${HMBTN_NATIVE}/demo_cube.c")
target_include_directories(psx_compat BEFORE PRIVATE
    "${HMBTN_COMPAT}/override"
    "${HMBTN_COMPAT}/psyq")
target_include_directories(psx_compat PUBLIC "${HMBTN_NATIVE}")
target_compile_options(psx_compat PRIVATE -funsigned-char -Wall -Wextra)
set_target_properties(psx_compat PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
target_link_libraries(psx_compat PUBLIC psx_core)

# SDL loads this library and calls its SDL_main (main() in main_sdl.c).
add_library(main SHARED
    "${HMBTN_NATIVE}/main_sdl.c"
    "${HMBTN_NATIVE}/touch_pad.c")
target_compile_options(main PRIVATE -Wall -Wextra)
target_link_libraries(main psx_compat psx_core SDL2)
