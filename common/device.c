/* device.c — open a serial device as a stream of bytes. See device.h.
 *
 * Opening is one call. Making the port stop behaving like a terminal is the
 * rest of the file, and it is the part that a plain open() leaves undone.
 *
 * close(), not libsyscall's sc_close: common/ sits UNDER libsyscall and must
 * not depend on it. On Linux a close() that reports EINTR has already
 * released the descriptor, so there is nothing to retry.
 */

#include "device.h"

#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

int device_open(const char *path, int nonblock)
{
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }

    /* A DEVICE IS A FILE. open, read and close are the same calls the feed
       uses, and lr_next_line, beside this file, cannot tell the difference. What differs is
       behaviour, not interface: a device may block when a regular file would
       not, it has no size, and it cannot be seeked.

       O_NOCTTY is the flag nobody expects. Opening a terminal device without
       it can make that terminal this process's CONTROLLING terminal, and
       then a Ctrl-C typed at it signals SENTINEL. A sensor must not be able
       to do that to the centre it reports to. */
    int flags = O_RDONLY | O_NOCTTY | (nonblock ? O_NONBLOCK : 0);
    int fd;
    do {
        fd = open(path, flags);
    } while (fd < 0 && errno == EINTR);
    if (fd < 0) {
        return -1;
    }

    /* A serial port arrives in "cooked" mode: the line discipline echoes what
       it reads, translates carriage returns into newlines, and waits for a
       line before returning anything. All three corrupt a byte stream. A
       device that is not a terminal has no line discipline, so there is
       nothing to configure and isatty says so. */
    if (isatty(fd)) {
        struct termios tio;
        if (tcgetattr(fd, &tio) != 0) {
            int saved = errno;
            close(fd);
            errno = saved;
            return -1;
        }
        cfmakeraw(&tio);
        cfsetispeed(&tio, B115200);
        cfsetospeed(&tio, B115200);

        /* Return as soon as any byte is available. The default makes read()
           wait for a full buffer, which turns a steady feed into nothing
           followed by a burst. */
        tio.c_cc[VMIN]  = 0;
        tio.c_cc[VTIME] = 1;            /* tenths of a second */

        if (tcsetattr(fd, TCSANOW, &tio) != 0) {
            int saved = errno;
            close(fd);
            errno = saved;
            return -1;
        }
    }
    return fd;
}
