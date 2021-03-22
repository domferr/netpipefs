/**
 * Definitions for fuse version, command line options, usage text.
 */

#ifndef NETPIPEFS_OPTIONS_H
#define NETPIPEFS_OPTIONS_H

#define FUSE_USE_VERSION 29 //fuse version 2.9. Needed by fuse.h
#include <fuse.h>

/** Definition fro command line options */
struct netpipefs_options {
    char *mountpoint;
    int show_help;
    int debug;
    int multithreaded;
    int foreground;
    long timeout;
    int port;
    char *hostip;
    int hostport;
    int delayconnect;
    size_t pipecapacity;
};

/** Command line options. Declared in options.c */
extern struct netpipefs_options netpipefs_options;

/** Debug utility to print debug strings when the debug flag is given */
#define DEBUG(...)						\
	do { if (netpipefs_options.debug) fprintf(stderr, ##__VA_ARGS__); } while(0)

/**
 * Parse netpipefs's options.
 *
 * @param progname program's name
 * @param args it will be filled with the arguments that will be passed to FUSE
 *
 * @return -1 on error, 0 if the options are correct, 1 if the options are not correct and prints the error
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
