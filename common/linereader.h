/* linereader.h — make whole lines from a stream of bytes.
 *
 * This is GIVEN code. No lab asks you to write it.
 *
 * It is here, next to the contact record, because every part of SENTINEL
 * reads its input through it, and none of the six requirements is about it.
 * It brings input into the system, as contact_parse makes a record from a
 * line.
 *
 * read() gives back the bytes that are available at that time, and the end
 * of a line is almost never at the end of those bytes. The line reader keeps
 * the part of a line that is left from one read, and joins it to the start
 * of the next read. So the caller gets only whole lines.
 */

#ifndef SENTINEL_LINEREADER_H
#define SENTINEL_LINEREADER_H

#include <stddef.h>

typedef struct linereader linereader_t;   /* opaque */

/* Read lines from fd. own_fd != 0 means that lr_close also closes fd.
 * bufsize is the size of each read; 0 gives a default size. */
linereader_t *lr_open(int fd, size_t bufsize, int own_fd);

/* The same, for a file to which another process still adds data.
 *
 * On a regular file, read() returning 0 means "nothing after this offset
 * yet". It does not mean "nothing ever". A follow reader never reports the
 * end of the input. Where the reader of lr_open returns 0, lr_next_line
 * returns -1 with errno EAGAIN, and the caller decides how long to wait
 * before it asks again. The reader keeps a last line that is not complete
 * until its '\n' arrives, and never returns it as a line. A line that is too
 * long gives EMSGSIZE, and the reader recovers, as with lr_open. */
linereader_t *lr_open_follow(int fd, size_t bufsize, int own_fd);

/* Return the next line, without its '\n', and with a '\0' at its end.
 * The pointer stays valid until the next call of lr_next_line on the same
 * reader.
 *
 *   1  a line is in *line, and its length is in *len
 *   0  the end of the input, with no part of a line left (never, for a
 *      follow reader)
 *  -1  an error, with errno set. A line longer than the limit of the reader
 *      gives EMSGSIZE. A follow reader at the end of the file, as the file
 *      is now, gives EAGAIN.
 *
 * A last line with no '\n' at its end is still returned, one time (lr_open
 * only).
 *
 * EMSGSIZE is reported one time for each line that is too long, and the
 * reader RECOVERS: it drops that line, and the next call returns the line
 * after it. So the error means "one line was lost". It never means "this
 * reader is finished". A caller must count it and continue to read. If the
 * caller stops, one bad line stops the whole channel.
 */
int lr_next_line(linereader_t *lr, const char **line, size_t *len);

/* Free the reader. Close fd if lr_open was told to own it. */
void lr_close(linereader_t *lr);

#endif /* SENTINEL_LINEREADER_H */
