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

#define NOTZERO(w, then)        \
        if ((w) != 0) {         \
            then;               \
        }

#define MINUS1ERR(w, then)      \
        if ((w) == -1) {        \
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
 * Conversione da millisecondi a secondi
 */
#define MS_TO_SEC(ms) ((ms)/1000L)

/**
 * Quanti microsecondi ci sono nei millisecondi specificati
 */
#define MS_TO_USEC(ms) (((ms) % 1000L) * 1000L)

//https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html
#ifdef DEBUGGING
#if DEBUGGING
#define DEBUG(...) \
    do { printf(__VA_ARGS__); } while(0);
#else
#define DEBUG(str, ...)
#endif
#else
#define DEBUG(str, ...)
#endif

#endif //UTILS_H
