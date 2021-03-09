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
 * Conversione da millisecondi a secondi
 */
#define MS_TO_SEC(ms) ((ms)/1000L)

/**
 * Quanti microsecondi ci sono nei millisecondi specificati
 */
#define MS_TO_USEC(ms) (((ms) % 1000L) * 1000L)

/**
 * Quanti nanosecondi ci sono nei millisecondi specificati
 */
#define MS_TO_NANOSEC(ms) (((ms)%1000L)*1000000L)

/**
 * Mette il thread in attesa per un tempo pari ai millisecondi specificati.
 *
 * @param milliseconds quanti millisecondi attendere
 * @return 0 in caso di successo, -1 altrimenti
 */
int msleep(int milliseconds);

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

#endif //UTILS_H
