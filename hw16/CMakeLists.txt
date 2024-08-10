cmake_minimum_required(VERSION 3.10)

project(logger_test LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

set (CMAKE_ENABLE_EXPORTS TRUE)

add_compile_options(-Wall -Wextra -Wpedantic)

add_library(logger STATIC logger.c)

add_executable(logger_test test.c)

target_link_libraries(logger_test PUBLIC logger)
