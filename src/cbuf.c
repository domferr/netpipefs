#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "../include/cbuf.h"

struct cbuf_s {
    char *data;
    size_t head;
    size_t tail;
    size_t capacity;
    int isfull;
};

cbuf_t *cbuf_alloc(size_t capacity) {
    struct cbuf_s *cbuf = (struct cbuf_s*) malloc(sizeof(struct cbuf_s));
    if (cbuf == NULL) return NULL;

    cbuf->capacity = capacity;
    cbuf->head = 0;
    cbuf->tail = 0;
    cbuf->isfull = 0;
    if (capacity == 0) {
        cbuf->data = NULL;
    } else {
        cbuf->data = (char *) malloc(sizeof(char) * capacity);
        if (cbuf->data == NULL) {
            free(cbuf);
            return NULL;
        }
    }

    return cbuf;
}

void cbuf_free(cbuf_t *cbuf) {
    free(cbuf->data);
    free(cbuf);
}

size_t cbuf_put(cbuf_t *cbuf, const char *data, size_t size) {
    size_t put = 0;
    if (cbuf->capacity == 0) return 0;

    while(put < size && !cbuf->isfull) {
        cbuf->data[cbuf->head] = *(data + put);
        (cbuf->head)++;
        if (cbuf->head == cbuf->capacity) cbuf->head = 0;
        put++;
        cbuf->isfull = cbuf->head == cbuf->tail;
    }
    return put;
}

size_t cbuf_get(cbuf_t *cbuf, char *data, size_t size) {
    size_t got = 0;
    if (cbuf->capacity == 0) return 0;

    while(got < size && !cbuf_empty(cbuf)) {
        *(data+got) = cbuf->data[cbuf->tail];
        (cbuf->tail)++;
        if (cbuf->tail == cbuf->capacity) cbuf->tail = 0;
        got++;
        cbuf->isfull = 0;
    }
    return got;
}

size_t cbuf_get_memcpy(cbuf_t *cbuf, char *data, size_t size) {
    if (cbuf->capacity == 0) return 0;

    char *dataptr = data;
    char *bufptr;
    size_t   nleft;
    size_t linear_len;

    nleft = size;
    while (nleft > 0 && !cbuf_empty(cbuf)) {
        if (cbuf->head > cbuf->tail) linear_len = cbuf->head - cbuf->tail;
        else linear_len = cbuf->capacity - cbuf->tail;
        if (linear_len > nleft) linear_len = nleft;

        bufptr = cbuf->data + cbuf->tail;
        memcpy(dataptr, bufptr, linear_len);

        nleft -= linear_len;
        dataptr += linear_len;
        cbuf->tail += linear_len;
        if (cbuf->tail >= cbuf->capacity) cbuf->tail = 0;
        cbuf->isfull = 0;
    }

    return(size - nleft); /* return >= 0 */
}

size_t cbuf_put_memcpy(cbuf_t *cbuf, const char *data, size_t size) {
    if (cbuf->capacity == 0) return 0;
    char *dataptr = (char *) data;
    char *bufptr;
    size_t   nleft;
    size_t linear_len;

    nleft = size;
    while (nleft > 0 && !cbuf->isfull) {
        if (cbuf->head >= cbuf->tail) linear_len = cbuf->capacity - cbuf->head;
        else linear_len = cbuf->tail - cbuf->head;
        if (linear_len > nleft) linear_len = nleft;

        bufptr = cbuf->data + cbuf->head;
        memcpy(bufptr, dataptr, linear_len);

        nleft -= linear_len;
        dataptr += linear_len;
        cbuf->head += linear_len;
        if (cbuf->head >= cbuf->capacity) cbuf->head = 0;
        cbuf->isfull = cbuf->head == cbuf->tail;
    }

    return (size - nleft);
}

ssize_t cbuf_writen(int fd, cbuf_t *cbuf, size_t n) {
    char *dataptr;
    size_t   nleft;
    ssize_t  nwritten;
    size_t linear_len;
    if (cbuf->capacity == 0) return 0;

    nleft = n;
    while (nleft > 0 && !cbuf_empty(cbuf)) {
        if (cbuf->head > cbuf->tail) linear_len = cbuf->head - cbuf->tail;
        else linear_len = cbuf->capacity - cbuf->tail;
        if (linear_len > nleft) linear_len = nleft;

        dataptr = cbuf->data + cbuf->tail;
        if((nwritten = write(fd, dataptr, linear_len)) < 0) {
            if (nleft == n) return -1; /* error, return -1 */
            else break; /* error, return amount written so far */
        } else if (nwritten == 0) break;

        nleft -= nwritten;
        cbuf->tail += nwritten;
        if (cbuf->tail >= cbuf->capacity) cbuf->tail = 0;
        cbuf->isfull = 0;
    }

    return(n - nleft); /* return >= 0 */
}

ssize_t cbuf_readn(int fd, cbuf_t *cbuf, size_t n) {
    char *dataptr;
    if (cbuf->capacity == 0) return 0;

    size_t linear_len;
    size_t   nleft;
    ssize_t  nread;

    nleft = n;
    while (nleft > 0 && !cbuf->isfull) {
        if (cbuf->head >= cbuf->tail) linear_len = cbuf->capacity - cbuf->head;
        else linear_len = cbuf->tail - cbuf->head;
        if (linear_len > nleft) linear_len = nleft;

        dataptr = cbuf->data + cbuf->head;
        if((nread = read(fd, dataptr, linear_len)) < 0) {
            if (nleft == n) return -1; /* error, return -1 */
            else break; /* error, return amount read so far */
        } else if (nread == 0) break; /* EOF */

        nleft -= nread;
        cbuf->head += nread;
        if (cbuf->head >= cbuf->capacity) cbuf->head = 0;
        cbuf->isfull = cbuf->head == cbuf->tail;
    }

    return(n - nleft); /* return >= 0 */

}

int cbuf_full(cbuf_t *cbuf) {
    return cbuf->isfull;
}

int cbuf_empty(cbuf_t *cbuf) {
    return !cbuf->isfull && (cbuf->head == cbuf->tail);
}

size_t cbuf_size(cbuf_t *cbuf) {
    if (cbuf->isfull) return cbuf->capacity;

    if (cbuf->head >= cbuf->tail) return cbuf->head - cbuf->tail;

    return cbuf->capacity + cbuf->head - cbuf->tail;
}

size_t cbuf_capacity(cbuf_t *cbuf) {
    return cbuf->capacity;
}