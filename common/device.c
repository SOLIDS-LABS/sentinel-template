/* device.c — open a serial device as a stream of bytes. See device.h.
 *
 * The open is one call. The rest of the file stops the port from behaving
 * as a terminal. A plain open() does not do that part.
 *
 * Use close(), not sc_close of libsyscall: common/ is UNDER libsyscall and
 * must not depend on it. On Linux, a close() that reports EINTR has released
 * the descriptor already, so there is nothing to retry.
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

    /* A DEVICE IS A FILE. open, read and close are the same calls that the
       feed uses, and lr_next_line, next to this file, cannot see a
       difference. The behaviour is different, not the interface: a device
       can block where a regular file does not, it has no size, and lseek
       does not work on it.

       O_NOCTTY is easy to forget. Without it, the open of a
       terminal device can make that terminal the CONTROLLING terminal of
       this process. Then a Ctrl-C typed at it sends a signal to SENTINEL. A
       sensor must not be able to do that to the centre. */
    int flags = O_RDONLY | O_NOCTTY | (nonblock ? O_NONBLOCK : 0);
    int fd;
    do {
        fd = open(path, flags);
    } while (fd < 0 && errno == EINTR);
    if (fd < 0) {
        return -1;
    }

    /* A serial port starts in "cooked" mode: the line discipline echoes what
       it reads, changes carriage returns into newlines, and waits for a
       whole line before it returns anything. All three damage a byte
       stream. A device that is not a terminal has no line discipline, so
       there is nothing to set, and isatty shows this. */
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

        /* Return when one byte is available. With the default, read()
           waits for a full buffer. A steady feed then gives nothing for a
           time, and then a burst. */
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
