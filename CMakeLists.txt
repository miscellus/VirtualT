cmake_minimum_required(VERSION 3.11)
project(slappy C)

include(FetchContent)

set(SOURCES
    src/main_sdl.c
    src/doins.c
    src/io.c
    src/memory.c
    src/sound.c
    src/intelhex.c
    src/kc85rom.c
    src/m100rom.c
    src/m10rom.c
    src/m200rom.c
    src/n8201rom.c
    src/n8300rom.c
)

add_executable(slappy ${SOURCES})

# Fetch SDL3 and build as static library
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.8
)

# Static build
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)

# Disable unnecessary features for WebAssembly 2D rendering
# set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
set(SDL_RENDER_D3D OFF CACHE BOOL "" FORCE)
set(SDL_RENDER_METAL OFF CACHE BOOL "" FORCE)
set(SDL_RENDER_VULKAN OFF CACHE BOOL "" FORCE)
set(SDL_RENDER_OPENGL OFF CACHE BOOL "" FORCE)
set(SDL_RENDER_OPENGLES OFF CACHE BOOL "" FORCE)
set(SDL_RENDER_OPENGLES2 OFF CACHE BOOL "" FORCE)
set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_POWER OFF CACHE BOOL "" FORCE)
# set(SDL_FILE OFF CACHE BOOL "" FORCE)
# set(SDL_CORE_VIDEO OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)


FetchContent_MakeAvailable(SDL3)

target_link_libraries(slappy PRIVATE SDL3::SDL3)
target_include_directories(slappy PRIVATE src)

set_target_properties(slappy PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
)

target_compile_definitions(slappy PRIVATE _CRT_SECURE_NO_WARNINGS)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
    # target_compile_options(slappy PRIVATE /Zi /Od /RTC1)
    # target_link_options(slappy PRIVATE /DEBUG /SUBSYSTEM:WINDOWS /MACHINE:X64)
    target_compile_options(slappy PRIVATE /O2)
    target_link_options(slappy PRIVATE /RELEASE /SUBSYSTEM:WINDOWS /MACHINE:X64)
endif()

# Add this block for WebAssembly compilation
if(EMSCRIPTEN)
    # Output an HTML file (Emscripten will also generate slappy.js and slappy.wasm)
    set_target_properties(slappy PROPERTIES SUFFIX ".html")

    target_link_options(slappy PRIVATE
        "SHELL:-s ALLOW_MEMORY_GROWTH=1"
        "SHELL:-s INITIAL_MEMORY=67108864"
        # "SHELL:-s ASYNCIFY=1"
        # "SHELL:-s ASYNCIFY_STACK_SIZE=65536"
    )

    # target_link_options(slappy PRIVATE
    #     "SHELL:-s ALLOW_MEMORY_GROWTH=1"

    #     # If your game uses a standard `while(running)` loop, you need ASYNCIFY
    #     # to prevent freezing the browser tab. (Alternatively, refactor your C code
    #     # to use emscripten_set_main_loop).
    #         # "SHELL:-s ASYNCIFY=1"

    #     # Optional: Set the initial memory size (e.g., 64MB)
    #     "SHELL:-s INITIAL_MEMORY=67108864"
    # )
endif()