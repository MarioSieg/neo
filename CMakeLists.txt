# (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

# The NEO runtime is written in C 99 and is designed to be portable.
# The NEO compiler is written in C++ 20 and requires a recent C++ toolchain.
# The unit tests are also written in C++ and require googletest.
# So if you have no C++ compiler for your platform, you can still run precompiled NEO bytecode using the runtime.

cmake_minimum_required(VERSION 3.14)

project(NEO LANGUAGES C)

message("Configuring NEO...")

option(NEO_BUILD_TESTS "Build NEO tests" OFF) # Requires googletest and C++ compiler.
option(NEO_BUILD_FUZZER "Build NEO fuzzer" OFF) # Requires libFuzzer and C++ compiler.

option(NEO_ENABLE_SSE41 "Enable SSE 4.1 Penryn instructions" ON) # Requires CPU >= x86-64 Intel Core 2 2 (Penryn) CPU.

# The following extensions are useful to debug and investigate the NEO compiler and runtime, but not to the end user.
option(NEO_EXTENSION_AST_RENDERING "Enable AST rendering support" OFF)
option(NEO_EXTENSION_DISASSEMBLER "Enable disassembling of JIT code" OFF)

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
    if (${NEO_ENABLE_SSE41})
        set(COMPILE_OPTIONS ${COMPILE_OPTIONS} -msse -msse2 -msse3 -mssse3 -msse4.1 -mcx16) # Enable advanced instructions
    endif()
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

if (${NEO_BUILD_TESTS} OR ${NEO_BUILD_FUZZER}) # These targets require a C++ 20 compiler.
    enable_language(CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

file(GLOB_RECURSE NEO_CORE_SOURCES src/*.h src/*.c)
add_library(libneo STATIC ${NEO_CORE_SOURCES}) # NEO compiler.
target_compile_options(libneo PRIVATE "${COMPILE_OPTIONS}")

if (${NEO_EXTENSION_AST_RENDERING} OR ${NEO_BUILD_TESTS})
    message("[EXTENSION] Enabled AST rendering support")
    target_link_libraries(libneo PRIVATE cgraph gvc)
    target_compile_definitions(libneo PUBLIC NEO_EXTENSION_AST_RENDERING)
endif()

if (${NEO_EXTENSION_DISASSEMBLER} OR ${NEO_BUILD_TESTS})
    message("[EXTENSION] Enabled disassembling of JIT code")
    add_subdirectory(ext/zydis)
    target_link_libraries(libneo PRIVATE Zydis)
    target_include_directories(libneo PRIVATE ext/zydis/include)
    target_compile_definitions(libneo PUBLIC NEO_EXTENSION_DISASSEMBLER)
endif()

add_executable(neo driver/neo_driver.c)
target_link_libraries(neo PRIVATE libneo)
target_compile_options(neo PRIVATE "${COMPILE_OPTIONS}")
target_include_directories(neo PRIVATE src)

if (${NEO_BUILD_TESTS}) # Requires C++ 20
    add_subdirectory(ext/googletest)
    enable_testing()
    file(GLOB_RECURSE NEO_TEST_SOURCES test/*.hpp test/*.cpp test/*.c src/*.hpp)
    add_executable(neo_test ${NEO_TEST_SOURCES})
    target_include_directories(neo_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(neo_test PUBLIC -fpermissive -Wno-address-of-temporary)
    target_link_libraries(neo_test GTest::gtest_main)
    target_link_libraries(neo_test Zydis)
    target_include_directories(neo_test PUBLIC ext/zydis/include)
    include(GoogleTest)
    gtest_discover_tests(neo_test)
    target_link_libraries(neo_test libneo)
endif()

if (${NEO_BUILD_FUZZER}) # Fuzzer requires clang
    if (NOT (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
        message(FATAL_ERROR "NEO fuzzer must be compiled with LLVM Clang because of libFuzz!")
    endif()
    message("Building NEO fuzzer...")
    file(GLOB_RECURSE NEO_FUZZ_SOURCES fuzz/*.hpp fuzz/*.cpp)
    add_executable(neo_fuzzer ${NEO_FUZZ_SOURCES})
    target_include_directories(neo_fuzzer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(neo_fuzzer PRIVATE "-fsanitize=fuzzer,address")
    target_compile_options(neo_fuzzer PRIVATE "-march=native")
    target_link_options(neo_fuzzer PRIVATE "-fsanitize=fuzzer,address")
    target_link_libraries(neo_fuzzer libneo)
endif ()

message("neo configured successfully!")
