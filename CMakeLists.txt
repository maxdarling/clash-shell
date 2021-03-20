cmake_minimum_required(VERSION 3.10)
project(clash)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_compile_options(-Wall -Werror)

# for use with clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SRCS
    src/loguru/loguru.cpp
    src/util/string_utils.cpp
    src/Executor.cpp
    src/Clash.cpp)

set(HDRS
    src/loguru/loguru.hpp
    src/Executor.h
    src/Clash.h
    src/test/ExecutorTestHarness.h)

add_executable(executor_tests src/test/executor_tests.cpp 
    ${SRCS} ${HDRS})

add_executable(clash src/clash_main.cpp ${SRCS} ${HDRS})
