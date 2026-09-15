/* device.h — open a serial device as a stream of bytes.
 *
 * This is GIVEN code. No lab asks you to write it.
 *
 * A device IS a file: open, read and close are the same calls, and the line
 * reader beside this file cannot tell the difference. What differs is
 * behaviour. A device may block where a regular file never does, it has no
 * size, and it cannot be seeked.
 *
 * It is given for the same reason the line reader is: it carries input into
 * the system and no requirement names it. R1 to R6 are met whether a report
 * arrives from a file, a pipe, a socket or a serial port. This is the seam
 * the bench board plugs into, and the bench is an instrument.
 */

#ifndef SENTINEL_DEVICE_H
#define SENTINEL_DEVICE_H

/* Open a device for reading, in raw mode. nonblock != 0 adds O_NONBLOCK.
 *
 * Two things this does that a plain open does not:
 *
 *   O_NOCTTY   opening a terminal device without it can make that terminal
 *              the process's CONTROLLING terminal, so a Ctrl-C typed at the
 *              sensor's port would signal the centre. A sensor must not be
 *              able to do that to the centre it reports to.
 *
 *   raw mode   a serial port arrives cooked: the line discipline echoes
 *              input, rewrites carriage returns, and waits for a whole line.
 *              All three corrupt a byte stream. A device that is not a
 *              terminal has no line discipline, so nothing is configured and
 *              isatty says so.
 *
 * The path may be a serial port or a pseudo-terminal. That matters for
 * teaching with one board: most students point this at a replay device and
 * the bench points it at the real sensor, and it is the same call.
 *
 * Returns a descriptor, or -1 with errno set. */
int device_open(const char *path, int nonblock);

#endif /* SENTINEL_DEVICE_H */
