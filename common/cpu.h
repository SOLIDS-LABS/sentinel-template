/* cpu.h — the processor, as an instrument.
 *
 * This is GIVEN code. No lab asks you to write it.
 *
 * Everything here answers one question: what is the processor doing to this
 * program? Which processors may it use, how much processor time has this
 * thread actually spent, and how often was it taken off.
 *
 * None of the six requirements needs any of it. SENTINEL would meet R1 to R6
 * with this file deleted. The COURSE needs it, to create the one-processor
 * condition the labs are measured under and to report what a run cost. That
 * is what makes it an instrument rather than a lab deliverable, and why it
 * lives here beside the contact record and not in a library a student writes.
 *
 * Three properties of the affinity mask matter, and none is obvious:
 *
 *   1. The mask survives fork(). A child starts with its parent's mask, and
 *      nothing tells you. The centre pins itself before it spawns sensors, so
 *      every sensor would be trapped on the centre's core unless the child
 *      clears it. That is why the centre calls cpu_unpin() in each child it
 *      starts, after the fork and before the exec.
 *
 *   2. The mask survives exec(). Replacing the program image does not reset
 *      it, so even contactfeed, which knows nothing about any of this, would
 *      inherit the restriction.
 *
 *   3. Threads inherit it from the thread that created them. Pinning the main
 *      thread before any thread is created is enough to pin all of them.
 *
 * drills/04a_affinity.c shows all three with sched_setaffinity directly.
 */

#ifndef SENTINEL_CPU_H
#define SENTINEL_CPU_H

#include <stdint.h>
#include <sys/types.h>

/* How many processors the machine has online. Returns -1 on failure. */
int cpu_count_online(void);

/* How many processors this program may CURRENTLY use. Returns -1 on failure.
 *
 * Read this back rather than trusting the value you asked for. Asking for a
 * processor is not the same as being given it: a container, a cpuset or an
 * administrator can allow fewer, and the call still succeeds with a narrower
 * mask. The centre reports what it read, never what it requested. */
int cpu_allowed(void);

/* Restrict this program to n_cpus processors, chosen from those it is already
 * allowed to use, lowest first. n_cpus above the online count is clamped;
 * n_cpus below 1 gives -1 and EINVAL. The first call saves the mask the
 * program started with, so cpu_unpin can put it back exactly. */
int cpu_pin(int n_cpus);

/* Put back the mask this program started with. When cpu_pin was never called
 * there is nothing saved, so this offers every online processor instead. */
int cpu_unpin(void);

/* Nanoseconds of PROCESSOR time this thread has used, or -1 on failure.
 *
 * This is not elapsed time, and the difference is the whole point.
 * CLOCK_THREAD_CPUTIME_ID stops while the thread is off the processor, so two
 * readings either side of a sleep differ by almost nothing however long the
 * sleep was. On one processor the CPU time of every thread added together can
 * never exceed the elapsed time, which is what makes concurrency measurable
 * and tells a thread that waits for work from a thread that waits for a
 * processor. Use sc_now_ns for a duration. */
int64_t cpu_thread_ns(void);

/* The two context switch counters of one thread of THIS process, from
 * /proc/self/task/<tid>/status. Returns 0, or -1 with errno set; ESRCH once
 * the thread has exited.
 *
 * A voluntary switch is the thread giving the processor up, normally because
 * it blocked. An involuntary one is the operating system taking the processor
 * away. The second number is the one that shows contention. */
int cpu_thread_ctxt(pid_t tid, long *vol, long *invol);

#endif /* SENTINEL_CPU_H */
