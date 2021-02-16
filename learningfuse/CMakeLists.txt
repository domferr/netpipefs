cmake_minimum_required(VERSION 3.10)
project(learningfuse C)

set(CMAKE_C_STANDARD 99)

add_executable(learningfuse src/main.c include/utils.h include/client.h)