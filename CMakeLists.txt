# (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

# The NEO runtime is written in C 99 and is designed to be portable.
# The NEO compiler is written in C++ 20 and requires a recent C++ toolchain.
# The unit tests are also written in C++ and require googletest.
# So if you have no C++ compiler for your platform, you can still run precompiled NEO bytecode using the runtime.

cmake_minimum_required(VERSION 3.14)

project(NEO LANGUAGES C)

message("Configuring NEO...")

option(NEO_BUILD_TESTS "Build NEO tests" ON) # Requires googletest and C++ compiler.

if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    message("Detected release mode")
    set (IS_RELEASE true)
    set (IS_DEBUG false)
else ()
    message("Detected debug mode")
    set (IS_RELEASE false)
    set (IS_DEBUG true)
endif ()

if (WIN32)
    set(COMPILE_OPTIONS /W3 /WX /utf-8) # basic compile options
else()
    set(COMPILE_OPTIONS -std=gnu99 -Wall -Wextra -Werror -Wswitch -Wimplicit-fallthrough -Wconversion -Wmissing-field-initializers -Wno-error=type-limits) # basic compile options
    if (${IS_RELEASE})
        set(COMPILE_OPTIONS ${COMPILE_OPTIONS} -O3 -fvisibility=hidden -fomit-frame-pointer) # advanced options
        if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            add_link_options(-fuse-ld=lld)
            add_link_options("-Wl,--thinlto-cache-dir=${PROJECT_BINARY_DIR}/lto_cache")
            add_link_options("-Wl,--thinlto-jobs=64")
            add_compile_options(-flto=thin)
            add_link_options(-flto=thin)
        elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            add_compile_options(flto=64)
            add_link_options(-flto=64)
        endif()
    endif()
endif(WIN32)

if (${NEO_BUILD_TESTS}) # These targets require a C++ 20 compiler.
    enable_language(CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

file(GLOB_RECURSE neo_sources src/*.h src/*.c)
add_library(libneo STATIC ${neo_sources}) # NEO compiler.
target_compile_options(libneo PRIVATE "${COMPILE_OPTIONS}")

add_executable(neo driver/neo_driver.c)
target_link_libraries(neo PRIVATE libneo)
target_compile_options(neo PRIVATE "${COMPILE_OPTIONS}")
target_include_directories(neo PRIVATE src)

if (${NEO_BUILD_TESTS}) # Requires C++ 20
    set(DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
    file(GLOB_RECURSE neo_test_sources test/*.hpp test/*.cpp)
    add_executable(neo_test ${neo_test_sources})
    target_include_directories(neo_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(neo_test GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(neo_test)
    target_link_libraries(neo_test libneo)
endif()

message("neo configured successfully!")
