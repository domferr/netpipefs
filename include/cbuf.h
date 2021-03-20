#ifndef CBUF_H
#define CBUF_H

#include <stddef.h>

typedef struct cbuf_s cbuf_t;

cbuf_t *cbuf_alloc(size_t capacity);

void cbuf_free(cbuf_t *cbuf);

size_t cbuf_put(cbuf_t *cbuf, const char *data, size_t size);

size_t cbuf_get(cbuf_t *cbuf, char *data, size_t size);

ssize_t cbuf_readn(int fd, cbuf_t *cbuf, size_t n);

int cbuf_full(cbuf_t *cbuf);

int cbuf_empty(cbuf_t *cbuf);

size_t cbuf_size(cbuf_t *cbuf);

size_t cbuf_capacity(cbuf_t *cbuf);

#endif //CBUF_H
