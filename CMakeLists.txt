cmake_minimum_required(VERSION 3.10)
project(netpipefs C)

set(CMAKE_C_STANDARD 99)

add_executable(netpipefs include/utils.h src/main.c src/socketconn.c include/socketconn.h src/scfiles.c include/scfiles.h examples/simpleprodcons.c src/utils.c src/dispatcher.c include/dispatcher.h src/options.c include/options.h src/netpipefs_file.c include/netpipefs_file.h src/icl_hash.c include/icl_hash.h test/utils.test.c test/testutilities.h src/openfiles.c include/openfiles.h test/openfiles.test.c src/cbuf.c include/cbuf.h test/cbuf.test.c examples/variabledata.c)