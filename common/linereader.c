/* linereader.c — turn a stream of bytes into whole lines.
 *
 * This exists because read() has no idea what a line is. It returns whatever
 * bytes are available at that moment, and a line boundary almost never falls
 * at the end of one read. A 64-byte read of a file of 30-byte lines gives two
 * lines and part of a third. The leftover part must be kept and joined to the
 * front of the next read, or it is lost.
 *
 * On a regular file with a large buffer the problem is easy to miss. With a
 * small buffer, or on a device that returns whatever has arrived so far,
 * partial lines are the normal case.
 *
 * FOLLOWING A FILE THAT IS STILL GROWING. On a regular file, read() returning
 * 0 means "there is nothing after this offset NOW". It does not mean nothing
 * will ever be there: another process can append, and the next read() from
 * the same offset returns the new bytes. tail -f works this way, and so does
 * Lab 1's live feed. A reader opened with lr_open_follow therefore never
 * latches end of file. It reports EAGAIN, the answer a non-blocking pipe
 * already gives, and keeps a half-written last line until its '\n' arrives,
 * because the writer may be part way through it.
 *
 * Why not getline() or fgets()? Both work on a FILE*, which brings stdio's
 * own buffer. Later parts wait on raw descriptors, and a stdio buffer in
 * front of a descriptor hides a complete line that is already read.
 *
 * The buffer layout is a simple window:
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
    size_t  start;      /* first unread byte            */
    size_t  end;        /* one past the last valid byte */
    int     eof;        /* read() has returned 0        */
    int     follow;     /* 0 from read() is "not yet", not "never" */
    int     discarding; /* inside an over-long line; skip to the next '\n' */
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

    /* One extra byte so a line filling the buffer exactly still has room for
       the '\0' that makes it a C string. */
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
        /* Is a complete line already in the buffer? */
        char *nl = memchr(lr->buf + lr->start, '\n', lr->end - lr->start);
        if (nl != NULL) {
            if (lr->discarding) {
                /* The tail of an over-long line. Step past its '\n' and pick
                   up at the next line, which is intact. */
                lr->start      = (size_t)(nl - lr->buf) + 1;
                lr->discarding = 0;
                continue;
            }

            size_t llen = (size_t)(nl - (lr->buf + lr->start));

            /* Overwrite the '\n' with '\0'. This is safe because the byte
               belongs to the buffer and the caller only borrows the line
               until the next call. */
            *nl   = '\0';
            *line = lr->buf + lr->start;
            if (len != NULL) {
                *len = llen;
            }
            lr->start += llen + 1;
            return 1;
        }

        /* Still inside the over-long line, so every byte held is part of it. */
        if (lr->discarding) {
            lr->start = 0;
            lr->end   = 0;
            if (lr->eof) {
                /* The stream ended inside it. There is no next line. */
                lr->discarding = 0;
                return 0;
            }
        }

        if (lr->eof) {
            /* No newline left. Anything still here is a final line without a
               terminator, which is a real line and must be returned once. */
            if (lr->end > lr->start) {
                size_t llen = lr->end - lr->start;
                lr->buf[lr->end] = '\0';        /* the spare byte earns itself */
                *line = lr->buf + lr->start;
                if (len != NULL) {
                    *len = llen;
                }
                lr->start = lr->end;
                return 1;
            }
            return 0;
        }

        /* No complete line yet, so more bytes are needed. Make room first. */
        if (lr->start > 0) {
            /* Slide the partial line to the front. memmove, not memcpy: the
               regions overlap whenever the partial line is longer than the
               distance it moves. */
            size_t remain = lr->end - lr->start;
            memmove(lr->buf, lr->buf + lr->start, remain);
            lr->start = 0;
            lr->end   = remain;
        }

        if (lr->end == lr->cap) {
            /* The buffer is full and still holds no newline, so this line is
               longer than the reader can ever hold. Report it rather than
               grow without limit -- a stream with no newlines at all would
               otherwise consume all memory. SENTINEL's 128-byte line limit
               means this signals malformed input.
             *
             * Report it ONCE, then drop the line and resynchronise on the next
             * '\n'. Returning the error without dropping anything leaves the
             * buffer full with no newline, so every later call finds the same
             * state and fails the same way: one hostile line would silence the
             * channel for the rest of the run, and the caller could not tell
             * that from a quiet sensor. Losing one line is the lesser harm, and
             * the caller is told, so it can count what it lost.
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
                /* The end of what exists so far. Anything held is the front
                   of a line the writer has not finished, so it stays. */
                errno = EAGAIN;
                return -1;
            }
            lr->eof = 1;
            continue;                   /* loop once more to flush the tail */
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
        /* close(), not the library's retry wrapper: this file is given code and
           must not depend on libsyscall. On Linux a close() that returns EINTR
           has already released the descriptor, so a retry would close whatever
           number was handed out next. There is nothing to retry. */
        close(lr->fd);
    }
    free(lr->buf);
    free(lr);
}
