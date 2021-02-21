cmake_minimum_required(VERSION 3.10)
project(fspipe C)

set(CMAKE_C_STANDARD 99)

add_executable(fspipe include/utils.h src/main.c src/socketconn.c include/socketconn.h src/scfiles.c include/scfiles.h src/fscons.c src/fsprod.c)