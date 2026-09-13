/* SPDX-License-Identifier: MIT */
#ifndef RIN_THREAD_TIMEOUT_POLICY_H
#define RIN_THREAD_TIMEOUT_POLICY_H

struct rin_thread_relative_timeout {
    long seconds;
    long nanoseconds;
};

enum rin_thread_timeout_outcome {
    RIN_THREAD_TIMEOUT_INVALID = -1,
    RIN_THREAD_TIMEOUT_EXPIRED = 0,
    RIN_THREAD_TIMEOUT_READY = 1
};

/* Convert an absolute deadline without signed subtraction or nanosecond
 * multiplication.  Very distant deadlines are clamped; a spurious wakeup
 * will recompute the remaining interval. */
static inline enum rin_thread_timeout_outcome
rin_thread_relative_timeout(long deadline_seconds, long deadline_nanoseconds,
                            long now_seconds, long now_nanoseconds,
                            long maximum_seconds,
                            struct rin_thread_relative_timeout* output)
{
    unsigned long seconds;
    long nanoseconds;

    if (!output || maximum_seconds < 0 || deadline_nanoseconds < 0 ||
        deadline_nanoseconds >= 1000000000L || now_nanoseconds < 0 ||
        now_nanoseconds >= 1000000000L) {
        return RIN_THREAD_TIMEOUT_INVALID;
    }
    if (deadline_seconds < now_seconds ||
        (deadline_seconds == now_seconds &&
         deadline_nanoseconds <= now_nanoseconds)) {
        output->seconds = 0;
        output->nanoseconds = 0;
        return RIN_THREAD_TIMEOUT_EXPIRED;
    }

    /* The signed comparison above establishes a non-negative mathematical
     * difference.  Unsigned subtraction therefore computes it without UB,
     * including deadlines that straddle the signed time_t boundary. */
    seconds = (unsigned long)deadline_seconds - (unsigned long)now_seconds;
    nanoseconds = deadline_nanoseconds - now_nanoseconds;
    if (nanoseconds < 0) {
        --seconds;
        nanoseconds += 1000000000L;
    }

    if (seconds > (unsigned long)maximum_seconds) {
        output->seconds = maximum_seconds;
        output->nanoseconds = 999999999L;
    } else {
        output->seconds = (long)seconds;
        output->nanoseconds = nanoseconds;
    }
    return RIN_THREAD_TIMEOUT_READY;
}

#endif /* RIN_THREAD_TIMEOUT_POLICY_H */
