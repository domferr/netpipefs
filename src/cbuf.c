#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include "../include/cbuf.h"

cbuf_t *cbuf_alloc(size_t capacity) {
    if (capacity <= 0) {
        errno = EINVAL;
        return NULL;
    }

    struct cbuf_s *cbuf = (struct cbuf_s*) malloc(sizeof(struct cbuf_s));
    if (cbuf == NULL) return NULL;

    cbuf->capacity = capacity;
    cbuf->data = (char*) malloc(sizeof(char) * capacity);
    if (cbuf->data == NULL) {
        free(cbuf);
        return NULL;
    }

    cbuf->head = 0;
    cbuf->tail = 0;
    cbuf->isfull = 0;

    return cbuf;
}

void cbuf_free(cbuf_t *cbuf) {
    free(cbuf->data);
    free(cbuf);
}

size_t cbuf_put(cbuf_t *cbuf, const char *data, size_t size) {
    size_t available = cbuf->capacity - cbuf_size(cbuf);
    if (available == 0) return 0;
    size_t willput = available < size ? available:size;

    for(size_t i = 0; i<willput; i++) {
        cbuf->data[cbuf->head] = *(data+i);
        (cbuf->head)++;
        if (cbuf->head == cbuf->capacity) cbuf->head = 0;
    }

    cbuf->isfull = cbuf->head == cbuf->tail;

    return willput;
}

size_t cbuf_get(cbuf_t *cbuf, char *data, size_t size) {
    size_t willget = cbuf_size(cbuf) < size ? cbuf_size(cbuf):size;
    if (willget == 0) return 0;

    cbuf->isfull = 0;
    for(size_t i = 0; i<willget; i++) {
        *(data+i) = cbuf->data[cbuf->tail];
        (cbuf->tail)++;
        if (cbuf->tail == cbuf->capacity) cbuf->tail = 0;
    }

    return willget;
}

size_t cbuf_readn(int fd, cbuf_t *cbuf, size_t n) {
    char *dataptr = cbuf->data + cbuf->head;
    size_t emptyspace = cbuf->capacity - cbuf_size(cbuf);

    size_t linear_len;
    size_t   nleft;
    ssize_t  nread;

    nleft = n > emptyspace ? emptyspace:n;
    while (nleft > 0) {
        if (cbuf->head < cbuf->tail) linear_len = cbuf->tail - cbuf->head;
        else linear_len = cbuf->capacity - cbuf->head;
        if (linear_len > nleft) linear_len = nleft;

        if((nread = read(fd, dataptr, linear_len)) < 0) {
            if (nleft == n) return -1; /* error, return -1 */
            else break; /* error, return amount read so far */
        } else if (nread == 0) break; /* EOF */
        nleft -= nread;
        dataptr = dataptr + nread;
        cbuf->head += nread;
        if (cbuf->head >= cbuf->capacity) cbuf->head = 0;
    }
    cbuf->isfull = cbuf->head == cbuf->tail;

    return(n - nleft); /* return >= 0 */

}

size_t cbuf_size(cbuf_t *cbuf) {
    if (cbuf->isfull) return cbuf->capacity;

    if (cbuf->head >= cbuf->tail) return cbuf->head - cbuf->tail;

    return cbuf->capacity + cbuf->head - cbuf->tail;
}

size_t cbuf_capacity(cbuf_t *cbuf) {
    return cbuf->capacity;
}