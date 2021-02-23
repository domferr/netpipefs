#include "../include/socketconn.h"
#include "../include/utils.h"
#include "../include/scfiles.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>

static struct sockaddr_un socket_get_address(void) {
    struct sockaddr_un sa;
    strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;
    return sa;
}

int socket_listen(void) {
    struct sockaddr_un sa = socket_get_address();
    int fd_skt;
    MINUS1(fd_skt = socket(AF_UNIX, SOCK_STREAM, 0), return -1)
    MINUS1(bind(fd_skt, (struct sockaddr *) &sa, sizeof(sa)), return -1)
    MINUS1(listen(fd_skt, SOMAXCONN), return -1)

    return fd_skt;
}

int socket_accept(int fd_skt) {
    int fd_client, err;
    struct timeval timeout = { MS_TO_SEC(CONN_TIMEOUT), MS_TO_USEC(CONN_TIMEOUT) };
    fd_set set;

    FD_ZERO(&set);
    FD_SET(fd_skt, &set);
    //aspetto di instaurare una connessione ma se scade il timeout termino
    MINUS1(err = select(fd_skt + 1, &set, NULL, NULL, &timeout), return -1)
    if (err == 0) {
        errno = ETIMEDOUT;
        fd_client = -1;
    } else {
        MINUS1(fd_client = accept(fd_skt, NULL, 0), return -1)
    }

    return fd_client;
}

int socket_connect(void) {
    int fd_skt;
    struct sockaddr_un sa = socket_get_address();
    MINUS1(fd_skt = socket(AF_UNIX, SOCK_STREAM, 0), return -1)
    //Avvia una connessione con il server via socket AF_UNIX
    while (connect(fd_skt, (struct sockaddr *) &sa, sizeof(sa)) == -1 ) {
        if (errno == ENOENT) {
            sleep(1); /* sock ancora non esiste, aspetto e poi riprovo */
        } else {
            return -1;
        }
    }

    return fd_skt;
}

/*int socket_send(int fd_skt, const char *data, size_t size) {
    MINUS1(writen(fd_skt, &size, sizeof(size_t)), return -1)
    MINUS1(writen(fd_skt, (void*) data, size), return -1)
    return 0;
}

int socket_read(int fd_skt, char *buf) {
    size_t size;
    MINUS1(readn(fd_skt, &size, sizeof(size_t)), return -1)
    return readn(fd_skt, (void*) buf, size);
}*/

int socket_close(int fd_skt) {
    return close(fd_skt);
}

int socket_destroy(void) {
    return unlink(SOCKNAME);
}