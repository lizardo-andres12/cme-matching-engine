cmake_minimum_required(VERSION 3.14)
project(CME_MatchingEngine VERSION 1.0)
find_package(Threads REQUIRED)

# --- Project Settings ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 

add_compile_options(-march=native -Wall -pedantic-errors)

option(ENABLE_TSAN "Enable Thread Sanitizer for race condition detection" OFF)
if(ENABLE_TSAN)
    message(STATUS "Thread Sanitizer enabled, performance will drop")
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
endif()


# --- GTest Setup ---
include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/lib/googletest
  DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)

FetchContent_Declare(
    googlebenchmark
    URL https://github.com/google/benchmark/archive/refs/tags/v1.9.3.zip
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/lib/googlebenchmark
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)

# prevent gtest from overriding our compiler warning flags
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# disable benchmark's internal tests to reduce build time 
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)
FetchContent_MakeAvailable(googlebenchmark)

# --- Main Application ---
add_executable(main.out src/main.cpp)


# --- Test Suite ---
enable_testing()

file(GLOB_RECURSE TEST_SOURCES "tests/*.cpp")
if(TEST_SOURCES)
    add_executable(run_tests ${TEST_SOURCES})
    target_include_directories(run_tests PUBLIC include)
    target_link_libraries(run_tests PRIVATE 
        gtest_main
	Threads::Threads
    )

    # register tests for ctest
    include(GoogleTest)
    gtest_discover_tests(run_tests)
else()
    message(STATUS "No test files found in 'tests/' directory.")
endif()

