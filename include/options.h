/**
 * Definitions for fuse version, command line options, usage text.
 */

#ifndef NETPIPEFS_OPTIONS_H
#define NETPIPEFS_OPTIONS_H

#define FUSE_USE_VERSION 29 //fuse version 2.9. Needed by fuse.h
#include <fuse.h>

/**
 * Command line options
 */
struct netpipefs_options {
    int show_help;
    int debug;
    long timeout;
    size_t pipecapacity;
    int port;
    char *hostip;
    int hostport;
};

extern struct netpipefs_options netpipefs_options;

#define DEBUG(...)						\
	do { if (netpipefs_options.debug) fprintf(stderr, ##__VA_ARGS__); } while(0)

/**
 * Parse netpipefs's options.
 *
 * @param progname program's name
 * @param args it will be filled with the arguments that will be passed to FUSE
 *
 * @return
 */
int netpipefs_opt_parse(const char *progname, struct fuse_args *args);

/**
 * Free netpipefs's options.
 *
 * @param args arguments passed to FUSE
 */
void netpipefs_opt_free(struct fuse_args *args);

/**
 * Print NetpipeFS's usage.
 *
 * @param progname program name
 */
void netpipefs_usage(const char *progname);

#endif //NETPIPEFS_OPTIONS_H
