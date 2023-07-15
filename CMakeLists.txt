# (c) Copyright Mario "Neo" Sieg 2023. All rights reserved. mario.sieg.64@gmail.com

# The NEO runtime is written in C 99 and is designed to be portable.
# The NEO compiler is written in C++ 20 and requires a recent C++ toolchain.
# The unit tests are also written in C++ and require googletest.
# So if you have no C++ compiler for your platform, you can still run precompiled NEO bytecode using the runtime.

cmake_minimum_required(VERSION 3.14)

project(NEO LANGUAGES C)

message("Configuring NEO...")

option(NEO_BUILD_RUNTIME "Build NEO runtime" ON) # Requires C 99 capable compiler.
option(NEO_BUILD_COMPILER "Build NEO compiler" ON) # Requires C++ 20 capable compiler.
option(NEO_BUILD_TESTS "Build NEO tests" ON) # Requires googletest and C++ compiler.

if (WIN32)
    set(COMPILE_OPTIONS /W3 /WX)
else()
    set(COMPILE_OPTIONS -Werror -Wall -Wextra -pedantic -Wpedantic -Wswitch -Wimplicit-fallthrough -Wconversion -Wmissing-field-initializers -std=gnu99 -DNOEL=1 -Wno-gnu-auto-type)
endif(WIN32)

if (${NEO_BUILD_COMPILER} OR ${NEO_BUILD_TESTS}) # These targets require a C++ 20 compiler.
    enable_language(CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if (${NEO_BUILD_RUNTIME})
    file(GLOB_RECURSE neo_runtime_sources runtime/*.h runtime/*.c)
    add_library(libneort STATIC ${neo_runtime_sources}) # NEO runtime environment.
    target_compile_options(libneort PUBLIC "${COMPILE_OPTIONS}")
endif()

if (${NEO_BUILD_COMPILER}) # Requires C++ 20
    file(GLOB_RECURSE neo_compiler_sources compiler/*.hpp compiler/*.cpp)
    add_library(libneoc STATIC ${neo_compiler_sources}) # NEO compiler.
    target_compile_options(libneoc PUBLIC "${COMPILE_OPTIONS}")
endif()

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
    target_include_directories(neo_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
    target_include_directories(neo_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/compiler)
    target_link_libraries(neo_test GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(neo_test)
    target_link_libraries(neo_test libneoc)
endif()

message("neo configured successfully!")
