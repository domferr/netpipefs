#ifndef SOCKETCONN_H
#define SOCKETCONN_H

#include <stddef.h>

#define DEFAULT_PORT 6789
#define CONN_TIMEOUT 6000  //Massimo tempo, espresso in millisecondi, per avviare una connessione socket
#define UNIX_PATH_MAX 108
#define SOCKNAME "/tmp/sockfile.sock"

int socket_listen(void);
int socket_accept(int fd_skt);
int socket_connect(void);
int socket_send(int fd_skt, const char *data, size_t size);
int socket_read(int fd_skt, char *buf);
int socket_close(int fd_skt);
int socket_destroy(void);

#endif //SOCKETCONN_H
