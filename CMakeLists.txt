cmake_minimum_required(VERSION 3.24)
project(TheGreatTrek VERSION 0.1.1 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(TREK_BUILD_GAME "Build the SDL3 game (off = core, tools and tests only)" ON)
option(TREK_USE_SYSTEM_SDL "Use installed SDL3, SDL3_ttf and SDL3_image instead of fetching them" OFF)
option(TREK_BUILD_TESTS "Build the unit tests" ON)

# Every program (game, tools, tests) lands in one folder, next to data/ and assets/.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

function(trek_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_subdirectory(third_party/cjson)
add_subdirectory(src/core)
add_subdirectory(tools)

if(TREK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(TREK_BUILD_GAME)
    include(cmake/Dependencies.cmake)
    add_subdirectory(src/ui)
endif()

# Copy data/ and assets/ next to the programs so they run from the build folder.
add_custom_target(trek_resources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/data ${CMAKE_BINARY_DIR}/bin/data
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/bin/assets
    COMMENT "Copying data and assets")
