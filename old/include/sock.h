/** @file
 * Utility functions used in netpipefs to establish a double connection and to communicate via sockets by sending
 * data length before the data itself.
 */

#ifndef SOCKETCONN_H
#define SOCKETCONN_H

#include <sys/un.h>
#include <netinet/in.h>

/**
 * Establish a double connection with the host: one by connect to the remote host and another one by accepting a
 * connection from remote host. If the remote host has not yet called this function, the connection will be tried
 * again after a certain amount of milliseconds and until the timeout. Returns the file descriptor got by accept.
 * If time is out then it returns -1 and sets errno to ETIMEDOUT.
 *
 * @param fdconn file descriptor used by connect
 * @param fdacc file descriptor used by accept
 * @param conn_sa socket address used by connect
 * @param acc_sa socket address used by accept
 * @param timeout maximum time allowed to establish the connection. Expressed in milliseconds.
 * @param interval how much time should wait before trying connect again. Expressed in milliseconds.
 *
 * @return the file descriptor got by accept or -1 on error and sets errno. On timeout it returns -1 and errno is set
 * to ETIMEDOUT
 */
int sock_connect_while_accept(int fdconn, int fdacc, struct sockaddr *conn_sa, struct sockaddr *acc_sa, long timeout, long interval);

/**
 * Invia i dati passati per argomento attraverso il file descriptor fornito. I dati vengono preceduti da un unsigned
 * integer che rappresenta la dimensione in bytes dei dati.
 *
 * @param fd_skt file descriptor sul quale scrivere i dati
 * @param data dati da inviare
 * @param size quanti bytes inviare
 *
 * @return numero di bytes scritti in caso di successo, -1 altrimenti ed imposta errno, 0 se il socket è stato chiudo
 */
int sock_write_h(int fd_skt, void *data, size_t size);

/**
 * Legge dal socket i dati attraverso il file descriptor fornito, allocando abbastanza memoria. ptr punterà all'aria di
 * memoria allocata ed il chiamante deve occuparsi di liberarla quando non gli serve più per evitare memory leaks.
 *
 * @param fd_skt file descriptor sul quale scrivere i dati
 * @param ptr puntatore all'area di memoria allocata che contiene i dati letti
 * @return numero di bytes letti in caso di successo, -1 altrimenti ed imposta errno oppure 0 se il socket è chiuso
 */
int sock_read_h(int fd_skt, void **ptr);

#endif //SOCKETCONN_H
