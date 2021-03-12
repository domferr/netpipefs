#ifndef SCFILES_H
#define SCFILES_H

#include <sys/types.h>

/**
 * Read "n" bytes from the given file descriptor.
 * (from “Advanced Programming In the UNIX Environment” by W. Richard Stevens
 * and Stephen A. Rago, 2013, 3rd Edition, Addison-Wesley)
 * @param fd file descriptor
 * @param ptr buffer pointer
 * @param n how many bytes to read
 * @return number of bytes read or -1 on error or 0 on end of file
 */
ssize_t readn(int fd, void *ptr, size_t n);

/**
 * Write "n" bytes into the given file descriptor.
 * (from “Advanced Programming In the UNIX Environment” by W. Richard Stevens
 * and Stephen A. Rago, 2013, 3rd Edition, Addison-Wesley)
 * @param fd file descriptor
 * @param ptr buffer pointer
 * @param n how many bytes to write
 * @return number of written bytes or -1 on error
 */
ssize_t writen(int fd, void *ptr, size_t n);

#endif //SCFILES_H
