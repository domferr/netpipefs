cmake_minimum_required(VERSION 3.10)
project(netpipefs C)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
add_definitions(-D_FILE_OFFSET_BITS=64)
set(CMAKE_C_STANDARD 99)

# netpipefs
add_executable(netpipefs src/main.c src/sock.c include/sock.h src/scfiles.c include/scfiles.h
        src/utils.c include/utils.h src/dispatcher.c include/dispatcher.h src/options.c include/options.h
        src/netpipe.c include/netpipe.h src/icl_hash.c include/icl_hash.h
        src/openfiles.c include/openfiles.h src/cbuf.c include/cbuf.h src/netpipefs_socket.c include/netpipefs_socket.h
        src/signal_handler.c include/signal_handler.h)
target_link_libraries(netpipefs PRIVATE Threads::Threads)

# TESTS
# utils.test
add_executable(utils.test test/utils.test.c src/utils.c include/utils.h test/testutilities.h)
# openfiles.test
add_executable(openfiles.test src/openfiles.c include/openfiles.h test/openfiles.test.c test/testutilities.h
        src/utils.c include/utils.h src/icl_hash.c include/icl_hash.h src/netpipe.c include/netpipe.h
        src/options.c include/options.h src/cbuf.c include/cbuf.h src/netpipefs_socket.c include/netpipefs_socket.h
        src/scfiles.c include/scfiles.h src/sock.c include/sock.h)
# cbuf.test
add_executable(cbuf.test test/cbuf.test.c src/cbuf.c include/cbuf.h test/testutilities.h )

# EXAMPLES
# simpleprodcons
add_executable(simpleprodcons examples/simpleprodcons.c src/scfiles.c include/scfiles.h)
# variabledata
add_executable(variabledata examples/variabledata.c src/scfiles.c include/scfiles.h)
# benchmark
add_executable(benchmark examples/benchmark.c src/scfiles.c include/scfiles.h src/utils.c include/utils.h)
target_link_libraries(benchmark PRIVATE Threads::Threads)
# nonblockingio
add_executable(nonblockingio examples/nonblockingio.c src/scfiles.c include/scfiles.h)
# select
add_executable(select examples/select.c src/scfiles.c include/scfiles.h)