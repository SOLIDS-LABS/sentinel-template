/* cpu.h — the processor, as an instrument.
 *
 * This is GIVEN code. No lab asks you to write it.
 *
 * Each function here answers one question: what does the processor do to
 * this program? Which processors can the program use? How much processor
 * time did this thread really use? How often was the processor taken away
 * from it?
 *
 * None of the six requirements needs this file. SENTINEL meets R1 to R6
 * without it. The COURSE needs it, for two tasks: to make the one-processor
 * condition in which the labs are measured, and to report the cost of a run.
 * So it is an instrument and not a lab deliverable. That is why it is here,
 * next to the contact record, and not in a library that a student writes.
 *
 * Three properties of the affinity mask are important, and none is obvious:
 *
 *   1. The mask stays after fork(). A child starts with the mask of its
 *      parent, and nothing tells you. The centre pins itself before it
 *      starts the sensors. If the child does not clear the mask, every
 *      sensor can use only the core of the centre. That is why the centre
 *      calls cpu_unpin() in each child that it starts, after the fork and
 *      before the exec.
 *
 *   2. The mask stays after exec(). A new program image does not reset it.
 *      So contactfeed, which knows nothing about the mask, also gets the
 *      limit.
 *
 *   3. A thread gets the mask from the thread that created it. If the main
 *      thread is pinned before it creates a thread, all threads are pinned.
 */

#ifndef SENTINEL_CPU_H
#define SENTINEL_CPU_H

#include <stdint.h>
#include <sys/types.h>

/* The number of processors that are online on the machine. Returns -1 on
 * failure. */
int cpu_count_online(void);

/* The number of processors that this program can use NOW. Returns -1 on
 * failure.
 *
 * Read this value. Do not trust the value that you asked for. A request for
 * a processor does not always give that processor: a container, a cpuset or
 * an administrator can allow fewer, and the call still succeeds with a
 * smaller mask. The centre reports the value that it read, never the value
 * that it asked for. */
int cpu_allowed(void);

/* Limit this program to n_cpus processors. They are the lowest-numbered
 * processors from those that the program can use already. If n_cpus is
 * more than the online count, the online count is used. n_cpus below 1
 * gives -1 and EINVAL. The first call saves the mask with which the program
 * started, so cpu_unpin can put it back exactly. */
int cpu_pin(int n_cpus);

/* Put back the mask with which this program started. If cpu_pin was never
 * called, nothing is saved, so this gives every online processor. */
int cpu_unpin(void);

/* Nanoseconds of PROCESSOR time that this thread has used, or -1 on failure.
 *
 * This is not elapsed time, and that difference is the purpose of the
 * function. CLOCK_THREAD_CPUTIME_ID stops while the thread is off the
 * processor. So two readings before and after a sleep are almost the same,
 * however long the sleep was. On one processor, the CPU time of all threads
 * added together cannot be more than the elapsed time. This makes
 * concurrency measurable. It also shows the difference between a thread
 * that waits for work and a thread that waits for a processor. Use
 * sc_now_ns for a duration. */
int64_t cpu_thread_ns(void);

/* The two context switch counters of one thread of THIS process, from
 * /proc/self/task/<tid>/status. Returns 0, or -1 with errno set. ESRCH
 * means that the thread has stopped.
 *
 * In a voluntary switch, the thread gives the processor up, usually because
 * it must wait. In an involuntary switch, the operating system takes the
 * processor away. The second number shows that threads compete for the
 * processor. */
int cpu_thread_ctxt(pid_t tid, long *vol, long *invol);

#endif /* SENTINEL_CPU_H */
