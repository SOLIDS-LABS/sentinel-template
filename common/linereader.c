/* linereader.c — make whole lines from a stream of bytes.
 *
 * This file is necessary because read() does not know what a line is. It
 * returns the bytes that are available at that time, and the end of a line
 * is almost never at the end of one read. A 64-byte read of a file of
 * 30-byte lines gives two lines and a part of a third line. The reader must
 * keep that part and join it to the start of the next read. If it does not,
 * the part is lost.
 *
 * On a regular file with a large buffer, this problem is easy to miss. With a
 * small buffer, or on a device that returns the bytes that have arrived up to
 * now, a part of a line is the normal case.
 *
 * A FILE THAT STILL GROWS. On a regular file, read() returning 0 means "there
 * is nothing after this offset NOW". It does not mean that nothing will come:
 * another process can add data, and the next read() from the same offset
 * returns the new bytes. tail -f works in this way, and so does the live feed
 * of Lab 1. So a reader from lr_open_follow never records the end of the
 * file. It reports EAGAIN, which is the answer that a non-blocking pipe gives
 * already. It keeps a last line that is not complete until its '\n' arrives,
 * because the writer can be in the middle of that line.
 *
 * Why not getline() or fgets()? Both use a FILE*, which adds the buffer of
 * stdio. Later parts wait on raw descriptors. A stdio buffer in front of a
 * descriptor hides a complete line that the program has read already.
 *
 * The buffer is a window:
 *
 *     buf: [ consumed | unread bytes | free space ]
 *                     ^start         ^end         ^cap
 */

#include "linereader.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define LR_DEFAULT_BUFSIZE 4096
#define LR_MIN_BUFSIZE       16

struct linereader {
    int     fd;
    int     own_fd;
    char   *buf;
    size_t  cap;
    size_t  start;      /* the first byte not read by the caller */
    size_t  end;        /* one byte after the last valid byte    */
    int     eof;        /* read() has returned 0                 */
    int     follow;     /* 0 from read() is "not yet", not "never" */
    int     discarding; /* in a line that is too long; skip to the next '\n' */
};

static linereader_t *open_reader(int fd, size_t bufsize, int own_fd, int follow)
{
    if (fd < 0) {
        errno = EBADF;
        return NULL;
    }
    if (bufsize == 0) {
        bufsize = LR_DEFAULT_BUFSIZE;
    }
    if (bufsize < LR_MIN_BUFSIZE) {
        bufsize = LR_MIN_BUFSIZE;
    }

    linereader_t *lr = malloc(sizeof *lr);
    if (lr == NULL) {
        return NULL;
    }

    /* One more byte, so that a line that fills the buffer exactly still has
       space for the '\0' that makes it a C string. */
    lr->buf = malloc(bufsize + 1);
    if (lr->buf == NULL) {
        free(lr);
        return NULL;
    }

    lr->fd         = fd;
    lr->own_fd     = own_fd;
    lr->cap        = bufsize;
    lr->start      = 0;
    lr->end        = 0;
    lr->eof        = 0;
    lr->follow     = follow;
    lr->discarding = 0;
    return lr;
}

linereader_t *lr_open(int fd, size_t bufsize, int own_fd)
{
    return open_reader(fd, bufsize, own_fd, 0);
}

linereader_t *lr_open_follow(int fd, size_t bufsize, int own_fd)
{
    return open_reader(fd, bufsize, own_fd, 1);
}

int lr_next_line(linereader_t *lr, const char **line, size_t *len)
{
    if (lr == NULL || line == NULL) {
        errno = EINVAL;
        return -1;
    }

    for (;;) {
        /* Does the buffer hold a complete line already? */
        char *nl = memchr(lr->buf + lr->start, '\n', lr->end - lr->start);
        if (nl != NULL) {
            if (lr->discarding) {
                /* The end of a line that is too long. Move past its '\n' and
                   continue at the next line, which is complete. */
                lr->start      = (size_t)(nl - lr->buf) + 1;
                lr->discarding = 0;
                continue;
            }

            size_t llen = (size_t)(nl - (lr->buf + lr->start));

            /* Write '\0' on the '\n'. This is safe, because the byte is in
               the buffer, and the caller uses the line only until the next
               call. */
            *nl   = '\0';
            *line = lr->buf + lr->start;
            if (len != NULL) {
                *len = llen;
            }
            lr->start += llen + 1;
            return 1;
        }

        /* Still in the line that is too long, so every byte here is part of
           it. */
        if (lr->discarding) {
            lr->start = 0;
            lr->end   = 0;
            if (lr->eof) {
                /* The stream ended in that line. There is no next line. */
                lr->discarding = 0;
                return 0;
            }
        }

        if (lr->eof) {
            /* No newline is left. The bytes here are a last line with no
               '\n'. That is a real line, and the reader returns it one
               time. */
            if (lr->end > lr->start) {
                size_t llen = lr->end - lr->start;
                lr->buf[lr->end] = '\0';        /* the one extra byte is used here */
                *line = lr->buf + lr->start;
                if (len != NULL) {
                    *len = llen;
                }
                lr->start = lr->end;
                return 1;
            }
            return 0;
        }

        /* There is no complete line yet, so more bytes are necessary. First
           make space. */
        if (lr->start > 0) {
            /* Move the part of a line to the front. Use memmove, not memcpy:
               the two areas overlap when the part is longer than the
               distance that it moves. */
            size_t remain = lr->end - lr->start;
            memmove(lr->buf, lr->buf + lr->start, remain);
            lr->start = 0;
            lr->end   = remain;
        }

        if (lr->end == lr->cap) {
            /* The buffer is full and has no newline, so this line is longer
               than the reader can hold. Report it. Do not make the buffer
               larger: a stream with no newlines would then use all the
               memory. The line limit of SENTINEL is 128 bytes, so this
               error means that the input is not correct.
             *
             * Report it ONE time, then drop the line and continue at the
             * next '\n'. If the reader returns the error and drops nothing,
             * the buffer stays full with no newline. Every later call then
             * finds the same state and fails in the same way. One hostile
             * line then stops the channel for the rest of the run, and the
             * caller cannot tell that from a sensor that sends nothing. The
             * loss of one line is the smaller fault, and the caller gets the
             * error, so it can count the lines that it lost.
             */
            lr->discarding = 1;
            lr->start      = 0;
            lr->end        = 0;
            errno = EMSGSIZE;
            return -1;
        }

        ssize_t n = read(lr->fd, lr->buf + lr->end, lr->cap - lr->end);
        if (n < 0) {
            if (errno == EINTR) {
                continue;               /* a signal, not a failure */
            }
            return -1;
        }
        if (n == 0) {
            if (lr->follow) {
                /* The end of the data that exists now. The bytes here are
                   the start of a line that the writer has not finished, so
                   they stay. */
                errno = EAGAIN;
                return -1;
            }
            lr->eof = 1;
            continue;                   /* one more pass returns the last line */
        }
        lr->end += (size_t)n;
    }
}

void lr_close(linereader_t *lr)
{
    if (lr == NULL) {
        return;
    }
    if (lr->own_fd) {
        /* Use close(), not the retry function of the library: this file is
           given code, and must not depend on libsyscall. On Linux, a close()
           that returns EINTR has released the descriptor already, so a
           retry can close the next descriptor that has the same number.
           There is nothing to retry. */
        close(lr->fd);
    }
    free(lr->buf);
    free(lr);
}
