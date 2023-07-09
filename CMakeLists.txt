# (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

cmake_minimum_required(VERSION 3.0)

project(NEO LANGUAGES C)

message("Configuring NEO...")

file(GLOB_RECURSE neo_sources src/*.h src/*.c)
add_library(libneo STATIC ${neo_sources})

if (WIN32)
    set(COMPILE_OPTIONS /W4 /WX)
else()
    set(COMPILE_OPTIONS -Werror -Wall -Wextra -pedantic -Wpedantic -Wswitch -Wimplicit-fallthrough -Wconversion -Wmissing-field-initializers -std=gnu99 -DNOEL=1 -Wno-gnu-auto-type)
endif(WIN32)

add_executable(neo driver/neo_driver.c)
target_link_libraries(neo libneo)

target_compile_options(libneo PUBLIC "${COMPILE_OPTIONS}")
target_compile_options(neo PUBLIC "${COMPILE_OPTIONS}")

message("neo configured successfully!")
