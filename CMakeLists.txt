cmake_minimum_required(VERSION 3.10)
project(fspipe C)

set(CMAKE_C_STANDARD 99)

add_executable(fspipe include/utils.h src/main.c src/socketconn.c include/socketconn.h src/scfiles.c include/scfiles.h examples/simpleprodcons.c src/utils.c src/dispatcher.c include/dispatcher.h src/options.c include/options.h src/fspipe_file.c include/fspipe_file.h src/icl_hash.c include/icl_hash.h test/utils.test.c test/fspipe.test.c test/testutilities.h)