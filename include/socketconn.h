#ifndef SOCKETCONN_H
#define SOCKETCONN_H

#define DEFAULT_ENDPOINT "127.0.0.1"
#define DEFAULT_PORT 6789
#define CONN_TIMEOUT 10000  //Massimo tempo, espresso in millisecondi, per avviare una connessione socket af unix
#define UNIX_PATH_MAX 108
#define SOCKNAME "./sockfile.sock"

int socket_accept(void);
int socket_connect(void);
int socket_send(char *data);
int socket_read(char *buf);

#endif //SOCKETCONN_H
