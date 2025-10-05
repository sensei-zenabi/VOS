cmake_minimum_required(VERSION 3.16)
project(CoolRetroTerminalClone VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2TTF REQUIRED SDL2_ttf)

add_executable(cool_retro_terminal
    src/main.cpp
    src/terminal.cpp
    src/ansi_parser.cpp
    src/crt_effect.cpp
    src/font_atlas.cpp
)

target_include_directories(cool_retro_terminal
    PRIVATE
        include
        ${SDL2_INCLUDE_DIRS}
        ${SDL2TTF_INCLUDE_DIRS}
)

target_compile_options(cool_retro_terminal PRIVATE ${SDL2_CFLAGS_OTHER} ${SDL2TTF_CFLAGS_OTHER})

target_link_libraries(cool_retro_terminal
    PRIVATE
        ${SDL2_LIBRARIES}
        ${SDL2TTF_LIBRARIES}
)

if(MSVC)
    target_compile_options(cool_retro_terminal PRIVATE /W4 /permissive-)
else()
    target_compile_options(cool_retro_terminal PRIVATE -Wall -Wextra -Wpedantic)
endif()
