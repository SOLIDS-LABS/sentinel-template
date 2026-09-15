/* libsyscall — the operating system interface of SENTINEL. Lab 1.
 *
 * This header is given. Do not change the declarations: other code, and the
 * tests that mark your lab, call these functions exactly as declared here.
 * You write every function in src/.
 *
 * Each comment says what a function must do, not how to do it. When a
 * function fails, it returns -1 (or NULL) and sets errno, unless its comment
 * says something else.
 */

#ifndef SENTINEL_SYSCALL_H
#define SENTINEL_SYSCALL_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

/* ------------------------------------------------------------------ io.c */

/* Open path for reading. Returns a file descriptor. */
int sc_open_read(const char *path);

/* Open path for writing. Create the file if it does not exist, with the
 * permission bits 0644.
 *   append != 0   every write goes to the end of the file, also when other
 *                 processes write the same file at the same time.
 *   append == 0   the existing content is removed when the file opens.
 */
int sc_open_write(const char *path, int append);

/* The same as sc_open_write, but a new file gets the permission bits in mode. */
int sc_open_write_mode(const char *path, int append, mode_t mode);

/* Close fd. Returns 0. */
int sc_close(int fd);

/* Write all n bytes of buf to fd. Returns 0 only when every byte is written.
 * n == 0 writes nothing and returns 0, whatever fd is.
 */
int sc_write_all(int fd, const void *buf, size_t n);

/* Read from fd into buf until n bytes arrive or the input ends.
 * Returns the number of bytes read: n, or fewer only at the end of the input.
 * n == 0 reads nothing and returns 0, whatever fd is.
 */
ssize_t sc_read_full(int fd, void *buf, size_t n);

/* --------------------------------------------------------------- clock.c */

/* Bytes that a timestamp from sc_stamp needs, including the '\0'. */
#define SC_STAMP_MAX 32

/* Nanoseconds from a clock that never goes backwards. The count starts at an
 * arbitrary point, so the value is not a date. Use it to measure durations.
 */
int64_t sc_now_ns(void);

/* The current date and time: seconds since the epoch, and microseconds from
 * 0 to 999999. A NULL pointer is allowed, and nothing is written to it.
 */
void sc_now_wall(int64_t *sec, int32_t *usec);

/* Write the current date and time into buf as "<seconds>.<microseconds>",
 * with exactly 6 digits after the point: "1755534061.000042".
 * Returns the length written, or -1 if buflen is too small.
 */
int sc_stamp(char *buf, size_t buflen);

/* ----------------------------------------------------------------- log.c */

/* The mission log. Every action of SENTINEL is timestamped and written to it
 * (requirement R6). */

typedef enum {
    LOG_SYSTEM = 0,   /* start, stop, configuration          */
    LOG_INFO,         /* a routine report was received       */
    LOG_PENDING,      /* queued, waiting for a worker        */
    LOG_PRIORITY,     /* a priority contact was promoted     */
    LOG_ALERT,        /* a threat assessment was raised      */
    LOG_TRACK         /* the track picture was updated       */
} log_level_t;

typedef struct log log_t;   /* opaque: you define struct log in log.c */

/* Open the mission log at path. New lines always go to the end of the file,
 * and several processes may write the same log at the same time.
 * Only the owner may read or write the file (mode 0600). This is also true
 * when the file existed before with other permissions.
 * Returns NULL on failure.
 */
log_t *log_open(const char *path);

/* Write one line to the log, with a message formatted like printf:
 *
 *     <timestamp> <LEVEL> <message>\n
 *     1755534061.482913 INFO     contact ACCF-4587 from NWS
 *
 * The timestamp is the sc_stamp format. The level name is left-aligned in a
 * field of 8 characters, and one space follows the field.
 * A line is at most 512 bytes, including its '\n'. A longer message is cut,
 * and the line still ends with '\n'.
 * A line never mixes with a line from another writer, and no line is lost.
 * Returns 0. lg == NULL or an unknown level gives -1 with errno EINVAL.
 */
int log_writef(log_t *lg, log_level_t level, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

/* Close the log and free the handle. lg == NULL is allowed and returns 0. */
int log_close(log_t *lg);

/* The name of a level, for example "ALERT", or NULL if level is not a
 * log_level_t value. */
const char *log_level_name(log_level_t level);

/* ---------------------------------------------------------------- proc.c */

/* What the kernel knows about a running process, read from /proc. */
typedef struct {
    pid_t pid;
    pid_t ppid;
    char  name[64];      /* the executable name, with no '\n' */
    long  vm_rss_kb;     /* resident memory, in kilobytes     */
    long  vm_size_kb;    /* total virtual memory, in kilobytes */
    int   threads;       /* the number of threads             */
    int   open_fds;      /* the number of open descriptors    */

    /* How often this process stopped running, and who decided.
     *
     * vol_ctxt   it gave the processor up itself, because it had to wait for
     *            something: a read, a lock, a sleep.
     * invol_ctxt the operating system TOOK the processor away, because
     *            another task was owed a turn.
     *
     * Both are in /proc/<pid>/status. A rising involuntary count is the
     * operating system sharing one processor between several tasks. */
    long  vol_ctxt;
    long  invol_ctxt;
} proc_info_t;

/* Fill out for process pid. pid 0 means the calling process.
 * open_fds is the number of descriptors that the process holds. For the
 * calling process, it does not count a descriptor that proc_report itself
 * opens to do its work.
 * out == NULL gives -1 with errno EINVAL. A pid with no process gives -1.
 */
int proc_report(pid_t pid, proc_info_t *out);

/* Write info to fd as text, in exactly this layout (%d, %s and %ld as in
 * printf):
 *
 *     process report
 *       pid        %d
 *       ppid       %d
 *       name       %s
 *       vm_size    %ld kB
 *       vm_rss     %ld kB
 *       threads    %d
 *       open_fds   %d
 *       ctxt_vol   %ld
 *       ctxt_invol %ld
 *
 * info == NULL gives -1 with errno EINVAL.
 */
int proc_report_write(int fd, const proc_info_t *info);

#endif /* SENTINEL_SYSCALL_H */
