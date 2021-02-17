#ifndef SCFILES_H
#define SCFILES_H

#include <sys/types.h>

/** Implementazione di readn e writen (by R. Stevens et al.)
 * http://didawiki.di.unipi.it/doku.php/informatica/sol/laboratorio20/esercitazionib/readnwriten
 * (tratto da “Advanced Programming In the UNIX Environment” by W. Richard Stevens
 * and Stephen A. Rago, 2013, 3rd Edition, Addison-Wesley)
 * */

/**
 * Legge "n" bytes da un descrittore di bytes.
 * (tratto da “Advanced Programming In the UNIX Environment” by W. Richard Stevens
 * and Stephen A. Rago, 2013, 3rd Edition, Addison-Wesley)
 * @param fd descrittore del file
 * @param ptr puntatore al buffer dentro il quale inserire i dati letti
 * @param n quanti bytes leggere
 * @return numero di bytes letti oppure -1 in caso di errore oppure 0 se la fine del file è stata raggiunta
 */
ssize_t readn(int fd, void *ptr, size_t n);

/**
 * Scrive "n" bytes in un descrittore di file.
 * (tratto da “Advanced Programming In the UNIX Environment” by W. Richard Stevens
 * and Stephen A. Rago, 2013, 3rd Edition, Addison-Wesley)
 * @param fd descrittore del file
 * @param ptr puntatore al buffer dal quale scrivere
 * @param n quanti bytes scrivere
 * @return numero di bytes scritti oppure -1 in caso di errore
 */
ssize_t writen(int fd, void *ptr, size_t n);

#endif //SCFILES_H
