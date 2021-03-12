cmake_minimum_required(VERSION 3.10)
project(clash)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_compile_options(-Wall -Werror)

set(EXECUTOR_TEST_SRCS
    src/loguru/loguru.cpp
    src/string_util.cpp
    src/Executor.cpp)

set(EXECUTOR_TEST_HDRS
    src/loguru/loguru.hpp
    src/Executor.h)

add_executable(executor_tests src/executor_tests.cpp 
    ${EXECUTOR_TEST_SRCS} ${EXECUTOR_TEST_HDRS})
