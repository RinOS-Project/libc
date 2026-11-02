#ifndef RIN_LIBC_INTERNAL_POSIX_CLOCK_H
#define RIN_LIBC_INTERNAL_POSIX_CLOCK_H

#include <rin/time/abi.h>

/* POSIX times() の公開単位。カーネル内部のタイマー周波数とは独立。 */
#define RIN_POSIX_CLK_TCK ((long)RIN_TIMES_CLOCK_TICKS_PER_SECOND)

#endif
