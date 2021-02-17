#include "../include/socketconn.h"
#include "../include/utils.h"
#include "../include/scfiles.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>

int socket_accept(void) {
    int fd_skt, fd_store, err;
    struct timeval timeout = { MS_TO_SEC(CONN_TIMEOUT), MS_TO_USEC(CONN_TIMEOUT) };
    fd_set set;
    struct sockaddr_un sa;
    strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    MINUS1(fd_skt = socket(AF_UNIX, SOCK_STREAM, 0), return -1)
    MINUS1(bind(fd_skt, (struct sockaddr *) &sa, sizeof(sa)), return -1)
    MINUS1(listen(fd_skt, SOMAXCONN), return -1)
    FD_ZERO(&set);
    FD_SET(fd_skt, &set);
    //aspetto di instaurare una connessione ma se scade il timeout termino
    MINUS1(err = select(fd_skt + 1, &set, NULL, NULL, &timeout), return -1)
    if (err == 0) {
        errno = ETIMEDOUT;
        fd_store = -1;
    } else {
        MINUS1(fd_store = accept(fd_skt, NULL, 0), return -1)
    }

    close(fd_skt);
    unlink(SOCKNAME);
    return fd_store;
}

int socket_connect(void) {
    int fd_skt;
    struct sockaddr_un sa;
    strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    MINUS1(fd_skt = socket(AF_UNIX, SOCK_STREAM, 0), return -1)
    //Avvia una connessione con il server via socket AF_UNIX
    while (connect(fd_skt, (struct sockaddr *) &sa, sizeof(sa)) == -1 ) {
        if (errno == ENOENT) {
            //TODO cosa fare al posto di msleep?
            //MINUS1(msleep(1000L),return -1) /* sock ancora non esiste, aspetto CONN_INTERVAL millisecondi e poi riprovo */
        } else {
            return -1;
        }
    }
    return fd_skt;
}
/*
int socket_send(char *data);
int socket_read(char *buf);*/