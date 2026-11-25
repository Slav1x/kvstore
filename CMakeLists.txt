cmake_minimum_required(VERSION 3.16)
project(kvstore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

# The storage engine itself, shared by the CLI, tests, and benchmark.
add_library(kvcore src/kvstore.cpp)
target_include_directories(kvcore PUBLIC src)

add_executable(kvstore src/main.cpp)
target_link_libraries(kvstore PRIVATE kvcore)

add_executable(kvstore_bench bench/bench.cpp)
target_link_libraries(kvstore_bench PRIVATE kvcore)

enable_testing()
add_executable(kvstore_tests tests/test_kvstore.cpp)
target_link_libraries(kvstore_tests PRIVATE kvcore)
add_test(NAME kvstore_tests COMMAND kvstore_tests)
