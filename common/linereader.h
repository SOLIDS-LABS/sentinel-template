/* linereader.h — turn a stream of bytes into whole lines.
 *
 * This is GIVEN code. No lab asks you to write it.
 *
 * It lives here, beside the contact record, because every part of SENTINEL
 * reads its input through it and none of the six requirements is about it. It
 * carries input into the system, the same way contact_parse turns a line into
 * a record.
 *
 * read() hands back a chunk of whatever size happens to be available, and a
 * line boundary almost never falls at the end of one. The line reader keeps
 * the partial line left over from one read and joins it to the front of the
 * next, so the caller sees only whole lines.
 */

#ifndef SENTINEL_LINEREADER_H
#define SENTINEL_LINEREADER_H

#include <stddef.h>

typedef struct linereader linereader_t;   /* opaque */

/* Take ownership of fd and read lines from it. own_fd != 0 means lr_close
 * also closes fd. bufsize is the read chunk size; 0 selects a default. */
linereader_t *lr_open(int fd, size_t bufsize, int own_fd);

/* The same, for a file that another process is still appending to.
 *
 * On a regular file, read() returning 0 means "nothing after this offset
 * yet", not "nothing ever". A follow reader never reports end of input:
 * where lr_open's reader would return 0, lr_next_line returns -1 with errno
 * EAGAIN, and the caller decides how long to wait before asking again. A
 * half-written last line is kept until its '\n' arrives and is never returned
 * as a line. Over-long lines give EMSGSIZE and recover, as with lr_open. */
linereader_t *lr_open_follow(int fd, size_t bufsize, int own_fd);

/* Return the next line, without its '\n', NUL-terminated.
 * The pointer is valid until the next lr_next_line call on the same reader.
 *
 *   1  a line was returned in *line (and its length in *len)
 *   0  end of input, with no partial line left over (never, for a follow reader)
 *  -1  error, errno set. A line longer than the reader's limit gives EMSGSIZE.
 *      A follow reader that has reached the end of the file so far gives EAGAIN.
 *
 * A final line with no trailing '\n' is still returned, once (lr_open only).
 *
 * EMSGSIZE is reported once per over-long line, and the reader RECOVERS: it
 * drops that line and the next call returns the line after it. So the error
 * means "one line was lost", never "this reader is finished". A caller should
 * count it and carry on, not stop reading -- stopping turns one bad line into
 * a dead channel.
 */
int lr_next_line(linereader_t *lr, const char **line, size_t *len);

/* Free the reader, and close the fd if lr_open was told to own it. */
void lr_close(linereader_t *lr);

#endif /* SENTINEL_LINEREADER_H */
