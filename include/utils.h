#ifndef UTILS_H
#define UTILS_H

#define EQNULL(w, then)     \
        if ((w) == NULL) {  \
            then;           \
        }

#define EQNULLERR(w, then)      \
        if ((w) == NULL) {      \
            perror(#w);         \
            then;               \
        }

#define MINUS1(w, then)         \
        if ((w) == -1) {        \
            then;               \
        }

#define MINUS1ERR(w, then)      \
        if ((w) == -1) {        \
            perror(#w);         \
            then;               \
        }

#define NOTZERO(w, then)        \
        if ((w) != 0) {         \
            then;               \
        }

#define ISNEGATIVE(w, then)     \
        if ((w) < 0) {          \
            then;               \
        }

#define ISNEGATIVEERR(w, then)  \
        if ((w) < 0) {          \
            perror(#w);         \
            then;               \
        }

#define PTH(e, pcall, then)         \
        if (((e) = (pcall)) != 0) { \
            errno = e;              \
            then;                   \
        }

#define PTHERR(e, pcall, then)      \
        if (((e) = (pcall)) != 0) { \
            errno = e;              \
            perror(#pcall);         \
            then;                   \
        }

/**
 * Convert milliseconds to seconds
 */
#define MS_TO_SEC(ms) ((ms)/1000L)

/**
 * Convert milliseconds to macroseconds
 */
#define MS_TO_USEC(ms) (((ms) % 1000L) * 1000L)

/**
 * Convert milliseconds to nanoseconds
 */
#define MS_TO_NANOSEC(ms) (((ms)%1000L)*1000000L)

/**
 * Sleep for the given milliseconds
 *
 * @param milliseconds how many milliseconds to wait
 * @return 0 on success, -1 otherwise
 */
int msleep(long milliseconds);

/**
 * Parses the given ip address ipstr which is represented in dot notation and as a string and converts it into an
 * array.
 *
 * @param ipstr ip address in dot notation
 * @param pointer to the array used to give the conversion result
 *
 * @return 0 on success, -1 if the ip address is not a valid address
 */
int ipv4_address_to_array(const char *ipstr, int *res);

/**
 * Returns the elapsed time from the given start to now.
 *
 * @param start starting time
 * @return elapsed time as struct timespec, -1 on error and sets errno
 */
struct timespec elapsed_time(struct timespec *start);

#endif //UTILS_H
