cmake_minimum_required(VERSION 3.20)
project(LimitOrderBook CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

add_library(lob STATIC
    src/OrderBook.cpp
)
target_include_directories(lob PUBLIC include)
target_compile_options(lob PRIVATE -Wall -Wextra)

add_executable(lob_benchmark benchmark/main.cpp)
target_link_libraries(lob_benchmark PRIVATE lob)

option(LOB_BUILD_TESTS "Build unit tests" ON)
if(LOB_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    add_executable(lob_tests tests/OrderBookTest.cpp)
    target_link_libraries(lob_tests PRIVATE lob GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(lob_tests)
endif()
