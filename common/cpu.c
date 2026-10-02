/* cpu.c — the processor, as an instrument. cpu.h tells why this is given.
 *
 * The operating system keeps an affinity mask for every task: the set of
 * processors on which the task can run. sched_setaffinity replaces the mask,
 * and sched_getaffinity reads it.
 *
 * Two clocks are NOT here. They are in libsyscall, because a lab asks for
 * them: CLOCK_REALTIME for a timestamp, and CLOCK_MONOTONIC for a duration.
 * Only the third clock, the processor clock of each thread, is a
 * measurement.
 */

#include "cpu.h"

#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* The mask with which this program started. The first cpu_pin saves it, so
   that cpu_unpin can put it back exactly.

   To put back "every online processor" is not correct. A run that started
   under `taskset -c 2,3` then gets processors 0 and 1, which its operator
   did not give it. A sensor from fork then runs on them too. */
static cpu_set_t g_original;
static int       g_saved = 0;

int cpu_count_online(void)
{
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    if (n < 1) {
        return -1;
    }
    return (int)n;
}

int cpu_allowed(void)
{
    cpu_set_t set;
    CPU_ZERO(&set);
    if (sched_getaffinity(0, sizeof set, &set) != 0) {
        return -1;
    }
    return CPU_COUNT(&set);
}

int cpu_pin(int n_cpus)
{
    if (n_cpus < 1) {
        errno = EINVAL;
        return -1;
    }

    if (!g_saved) {
        CPU_ZERO(&g_original);
        if (sched_getaffinity(0, sizeof g_original, &g_original) != 0) {
            return -1;
        }
        g_saved = 1;
    }

    int online = cpu_count_online();
    if (online < 1) {
        return -1;
    }
    if (n_cpus > online) {
        n_cpus = online;
    }

    /* Choose from the processors that the program CAN use, lowest first,
       and not from 0..n_cpus-1. Under `taskset -c 4,5`, the low-numbered
       processors are not available, and the kernel refuses a mask that
       names them with EINVAL. This loop reads the saved mask, so "one core"
       means "the first core that the program got". That is true in both
       cases. */
    cpu_set_t want;
    CPU_ZERO(&want);
    int chosen = 0;
    for (int cpu = 0; cpu < CPU_SETSIZE && chosen < n_cpus; cpu++) {
        if (CPU_ISSET(cpu, &g_original)) {
            CPU_SET(cpu, &want);
            chosen++;
        }
    }
    if (chosen == 0) {
        errno = EINVAL;
        return -1;
    }

    return sched_setaffinity(0, sizeof want, &want);
}

int cpu_unpin(void)
{
    if (g_saved) {
        return sched_setaffinity(0, sizeof g_original, &g_original);
    }

    /* The program was never pinned, so no mask is saved. Give every online
       processor. */
    int online = cpu_count_online();
    if (online < 1) {
        return -1;
    }
    cpu_set_t all;
    CPU_ZERO(&all);
    for (int cpu = 0; cpu < CPU_SETSIZE && cpu < online; cpu++) {
        CPU_SET(cpu, &all);
    }
    return sched_setaffinity(0, sizeof all, &all);
}

/* The same arithmetic as sc_now_ns, on a different clock. The clock id
   makes it a different measurement: CLOCK_THREAD_CPUTIME_ID stops while the
   thread is off the processor. So two readings before and after a sleep are
   almost the same, however long the sleep was. */
int64_t cpu_thread_ns(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0) {
        return -1;
    }
    return (int64_t)ts.tv_sec * 1000000000 + (int64_t)ts.tv_nsec;
}

/* Return the value after "key:" in one line of /proc status text. Return
   NULL if the line has a different key. */
static const char *field(const char *line, const char *key)
{
    size_t klen = strlen(key);
    if (strncmp(line, key, klen) != 0 || line[klen] != ':') {
        return NULL;
    }
    const char *p = line + klen + 1;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return p;
}

int cpu_thread_ctxt(pid_t tid, long *vol, long *invol)
{
    if (tid <= 0) {
        errno = EINVAL;
        return -1;
    }
    if (vol != NULL) {
        *vol = 0;
    }
    if (invol != NULL) {
        *invol = 0;
    }

    /* "self" again, so the caller does not need its own pid. A thread id has
       a meaning only in its own process, so no other process is possible
       here. */
    char path[128];
    snprintf(path, sizeof path, "/proc/self/task/%d/status", (int)tid);

    FILE *f = fopen(path, "r");
    if (f == NULL) {
        return -1;                     /* ESRCH when the thread has stopped */
    }

    char line[512];
    while (fgets(line, sizeof line, f)) {
        const char *v;
        if ((v = field(line, "voluntary_ctxt_switches")) != NULL) {
            if (vol != NULL) {
                *vol = strtol(v, NULL, 10);
            }
        } else if ((v = field(line, "nonvoluntary_ctxt_switches")) != NULL) {
            if (invol != NULL) {
                *invol = strtol(v, NULL, 10);
            }
        }
    }
    fclose(f);
    return 0;
}
