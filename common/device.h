/* device.h — open a serial device as a stream of bytes.
 *
 * This is GIVEN code. No lab asks you to write it.
 *
 * A device IS a file: open, read and close are the same calls, and the line
 * reader next to this file cannot see a difference. The behaviour is
 * different. A device can block where a regular file never blocks, it has no
 * size, and lseek does not work on it.
 *
 * It is given for the same reason as the line reader: it brings input into
 * the system, and no requirement names it. R1 to R6 are met when a report
 * comes from a file, a pipe, a socket or a serial port. This is the point
 * where the bench board connects, and the bench is an instrument.
 */

#ifndef SENTINEL_DEVICE_H
#define SENTINEL_DEVICE_H

/* Open a device for reading, in raw mode. nonblock != 0 adds O_NONBLOCK.
 *
 * This function does two things that a plain open does not do:
 *
 *   O_NOCTTY   without it, the open of a terminal device can make that
 *              terminal the CONTROLLING terminal of the process. Then a
 *              Ctrl-C typed at the port of the sensor sends a signal to the
 *              centre. A sensor must not be able to do that to the centre.
 *
 *   raw mode   a serial port starts in "cooked" mode: the line discipline
 *              echoes the input, changes carriage returns, and waits for a
 *              whole line. All three damage a byte stream. A device that is
 *              not a terminal has no line discipline, so nothing is set, and
 *              isatty shows this.
 *
 * The path can be a serial port or a pseudo-terminal. This is important
 * when a course has one board: most students use a replay device, the bench
 * uses the real sensor, and both use the same call.
 *
 * Returns a descriptor, or -1 with errno set. */
int device_open(const char *path, int nonblock);

#endif /* SENTINEL_DEVICE_H */
