#ifndef CBUF_H
#define CBUF_H

#include <stddef.h>

/** Circular buffer data type */
typedef struct cbuf_s cbuf_t;

/**
 * Creates a new buffer with a given capacity
 *
 * @param capacity how much data the buffer can have
 * @return the created buffer, NULL on error
 */
cbuf_t *cbuf_alloc(size_t capacity);

/**
 * Destroys the given buffer. The buffer structure and the remaining data are freed.
 *
 * @param cbuf the buffer to be destroyed
 */
void cbuf_free(cbuf_t *cbuf);

/**
 * Put data into the buffer. Only puts data until the buffer is full
 *
 * @param cbuf the buffer
 * @param data the data
 * @param size how much of the data should be put
 * @return how much data was put
 */
size_t cbuf_put(cbuf_t *cbuf, const char *data, size_t size);

/**
 * Get data from the buffer. If the amount required is greater than the available data, all
 * the data is got. Data got is also removed from the buffer.
 *
 * @param cbuf the buffer
 * @param data data got will be added into this pointer
 * @param size how much the data should get
 * @return how much data was got
 */
size_t cbuf_get(cbuf_t *cbuf, char *data, size_t size);

/**
 * Read "n" bytes from the given file descriptor and puts data into the circular buffer.
 *
 * @param fd file descriptor
 * @param cbuf buffer pointer
 * @param n how many bytes to read
 * @return number of bytes read or -1 on error or 0 on end of file
 */
ssize_t cbuf_readn(int fd, cbuf_t *cbuf, size_t n);

/**
 * Check if the given buffer is full or not.
 *
 * @param cbuf the buffer
 * @return 1 if the buffer is full, 0 otherwise
 */
int cbuf_full(cbuf_t *cbuf);

/**
 * Check if the given buffer is empty or not.
 *
 * @param cbuf the buffer
 * @return 1 if the buffer is empty, 0 otherwise
 */
int cbuf_empty(cbuf_t *cbuf);

/**
 * Get how much data is inside the buffer
 *
 * @param cbuf the buffer
 * @return how much data there is into the buffer
 */
size_t cbuf_size(cbuf_t *cbuf);

/**
 * Get the buffer capacity
 *
 * @param cbuf the buffer
 * @return the buffer's capacity
 */
size_t cbuf_capacity(cbuf_t *cbuf);

#endif //CBUF_H
