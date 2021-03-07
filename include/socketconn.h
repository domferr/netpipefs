#ifndef SOCKETCONN_H
#define SOCKETCONN_H

#define DEFAULT_PORT 7000
#define DEFAULT_TIMEOUT 8000    // Massimo tempo, espresso in millisecondi, per avviare una connessione socket
#define CONNECT_INTERVAL 1000    // Ogni quanti millisecondi riprovare la connect se fallisce
#define UNIX_PATH_MAX 108
#define SOCKNAME "/tmp/sockfile.sock"

struct fspipe_socket {
    int fd_server;  // used to accept socket connections
    int fd_skt;     // used to communicate via sockets
    pthread_mutex_t writesktmtx;
};

enum socket_message {
    OPEN = 100,
    OPEN_CONFIRM,
    CLOSE
};

/**
 * Crea un socket AF_UNIX ed esegue il binding del socket e la chiamata di sistema listen() sul socket.
 *
 * @return file descriptor del socket creato, -1 in caso di errore e imposta errno
 */
int socket_listen(void);

/**
 * Accetta una connessione sul socket passato per argomento. Ritorna il file descriptor del client che ha accettato
 * la connessione. Se scade il timeout allora la funzione ritorna -1 ed errno viene impostato a ETIMEDOUT.
 *
 * @param fd_skt file descriptor sul quale accettare la connessione
 * @param timeout tempo massimo, espresso in millisecondi, per instaurare una connessione. Se negativo viene utilizzato
 * tempo di default DEFAULT_TIMEOUT
 * @return file descriptor del client con il quale è iniziata la connessione, -1 in caso di errore e imposta errno
 */
int socket_accept(int fd_skt, long timeout);

/**
 * Cerca di connettersi via socket AF_UNIX. Il tentativo di connessione viene svolto ad intervalli di CONNECT_INTERVAL
 * millisecondi. Ritorna il file descriptor da utilizzare per la comunicazione con il server oppure -1 in caso di errore
 * ed imposta errno. Se scade il timeout allora la funzione ritorna -1 ed errno viene impostato a ETIMEDOUT.
 *
 *
 * @param timeout tempo massimo, espresso in millisecondi, per instaurare una connessione. Se negativo viene utilizzato
 * tempo di default DEFAULT_TIMEOUT
 * @return il file descriptor per comunicare con il server oppure -1 in caso di errore ed imposta errno
 */
int socket_connect(long timeout);

/**
 * Invia i dati passati per argomento attraverso il file descriptor fornito. I dati vengono preceduti da un unsigned
 * integer che rappresenta la dimensione in bytes dei dati.
 *
 * @param fd_skt file descriptor sul quale scrivere i dati
 * @param data dati da inviare
 * @param size quanti bytes inviare
 *
 * @return 0 in caso di successo, -1 altrimenti ed imposta errno
 */
int socket_write_h(int fd_skt, void *data, size_t size);

/**TODO change this doc
 * Legge dal socket i dati attraverso il file descriptor fornito e li ritorna al chiamante.
 *
 * @param fd_skt file descriptor sul quale scrivere i dati
 * @return i dati letti in caso di successo, NULL altrimenti ed imposta errno
 */
int socket_read_h(int fd_skt, void **ptr);

/**
 * Legge dal socket il numero di bytes indicati. Se la lettura si blocca per più di timeout millisecondi, ritorna -1
 * e imposta errno a ETIMEDOUT.
 *
 * @param fd_skt file descriptor sul quale leggere i dati
 * @param buf buffer sul quale scrivere i dati lettere
 * @param timeout tempo massimo per la lettura
 * @return quanti bytes sono stati letti. In caso di errore ritorna -1 ed imposta errno. Se scade il tempo, ritorna -1
 * ed imposta errno a ETIMEDOUT.
 */
int socket_read_t(int fd, void *buf, size_t size, long timeout);

/**
 * Unlink the file used for the socket communication
 *
 * @return 0 on success, -1 otherwise and errno is set
 */
int socket_destroy(void);

#endif //SOCKETCONN_H
