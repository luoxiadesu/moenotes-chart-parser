cmake_minimum_required(VERSION 3.16)
project(moenotes-chart-parser C)

option(MS_BUILD_CLI "Build the music-score command line tool" ON)
option(MS_BUILD_TESTS "Build C regression tests" ON)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(ZLIB REQUIRED)

add_library(moenotes_chart_parser STATIC
    src/music_score.c
    third_party/yyjson.c)
target_include_directories(moenotes_chart_parser
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_link_libraries(moenotes_chart_parser PUBLIC ZLIB::ZLIB)
if(MSVC)
    target_compile_definitions(moenotes_chart_parser PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(moenotes_chart_parser PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(moenotes_chart_parser PUBLIC m)
endif()

if(MS_BUILD_CLI)
    add_executable(moenotes-chart-parser cli/music_score_cli.c)
    target_link_libraries(moenotes-chart-parser PRIVATE moenotes_chart_parser)
endif()

if(MS_BUILD_TESTS)
    enable_testing()
    add_executable(moenotes-chart-parser-test tests/test_music_score.c)
    target_link_libraries(moenotes-chart-parser-test PRIVATE moenotes_chart_parser)
    add_test(NAME moenotes-chart-parser-regression COMMAND moenotes-chart-parser-test)
endif()
