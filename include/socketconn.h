#ifndef SOCKETCONN_H
#define SOCKETCONN_H

#define DEFAULT_PORT 6789
#define CONN_TIMEOUT 6000  //Massimo tempo, espresso in millisecondi, per avviare una connessione socket
#define UNIX_PATH_MAX 108
#define SOCKNAME "/tmp/sockfile.sock"

int socket_init(void);
int socket_listen(int fd_skt);
int socket_accept(int fd_skt);
int socket_connect(int fd_skt);
int socket_send(char *data);
int socket_read(char *buf);
void socket_close(int fd_skt);

#endif //SOCKETCONN_H
