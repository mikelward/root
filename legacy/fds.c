#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "fds.h"

/*
 * Open /dev/null on any of stdin, stdout and stderr that is closed.
 *
 * A setuid program started with one of them closed would otherwise hand
 * that number to the next file or socket it opens (the syslog socket, say),
 * and its own messages, or the command it runs as root, would write there.
 * The Rust runtime does the same before main().
 *
 * Returns 0 on success, -1 with errno set if a closed descriptor cannot be
 * replaced. Logs nothing: with the descriptors in doubt, the caller decides.
 */
int ensure_standard_fds_open(void)
{
    for (int fd = STDIN_FILENO; fd <= STDERR_FILENO; fd++) {
        if (fcntl(fd, F_GETFD) != -1 || errno != EBADF) {
            continue;
        }
        /* open() returns the lowest free number, which is this one. */
        int opened = open("/dev/null", O_RDWR);
        if (opened == -1) {
            return -1;
        }
        if (opened != fd) {
            close(opened);
            errno = EBADF;
            return -1;
        }
    }
    return 0;
}

/* vim: set ts=4 sw=4 tw=0 et:*/
