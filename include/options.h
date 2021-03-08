/*
 * Definitions for fuse version, command line options, usage text.
 */

#ifndef FSPIPE_OPTIONS_H
#define FSPIPE_OPTIONS_H

#define FUSE_USE_VERSION 29 //fuse version 2.9. Needed by fuse.h
#include <fuse.h>

/**
 * Command line options
 */
struct fspipe_options {
    const char *host;
    int port;
    int remote_port;
    int show_help;
    int debug;
    long timeout;
    int is_server;
};

extern struct fspipe_options fspipe_options;

#define DEBUG(...)						\
	do { if (fspipe_options.debug) fprintf(stderr, ##__VA_ARGS__); } while(0)

int fspipe_opt_parse(const char *progname, struct fuse_args *args);
void fspipe_opt_free(struct fuse_args *args);

/**
 * Prints FSPipe's usage.
 *
 * @param progname program name
 */
void fspipe_usage(const char *progname);

#endif //FSPIPE_OPTIONS_H
