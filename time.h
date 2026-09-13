/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - time.h
 * 時刻・日付操作
 */

#ifndef _TIME_H
#define _TIME_H

#include "stddef.h"
#include "stdint.h"
#include "limits.h"
#include "sys/syscall.h"
#include "../../src/shared/rin_cpu_time_abi.h"
#include "../../src/shared/rin_timer_abi.h"

#if defined(RIN_USERSPACE)
#include "locale.h"
typedef locale_t RinStrptimeLocale;
#else
typedef void* RinStrptimeLocale;
#endif

/* A caller-provided time hook is an explicit request for Rin's target
 * implementation even in a hosted test process. */
#if defined(_RIN_TIME_SYSCALL1) || defined(_RIN_TIME_SYSCALL2) || \
    defined(_RIN_TIME_SLEEP) || defined(_RIN_TIME_GETTIMEOFDAY) || \
    defined(_RIN_TIME_NS64) || defined(_RIN_TIME_NS32) || \
    defined(_RIN_TIME_REALTIME_NS64) || defined(_RIN_TIME_REALTIME_NS32) || \
    defined(_RIN_TIME_CPU_GET) || defined(_RIN_TIME_SETITIMER) || \
    defined(_RIN_TIME_GETITIMER) || \
    defined(_RIN_TIME_TIMER_CREATE) || defined(_RIN_TIME_TIMER_SETTIME) || \
    defined(_RIN_TIME_TIMER_GETTIME) || defined(_RIN_TIME_TIMER_DELETE) || \
    defined(_RIN_TIME_TIMER_GETOVERRUN) || defined(_RIN_TIME_TIMER_POLL) || \
    defined(_RIN_TIME_CLOCK_SETTIME)
#define RIN_TIME_CUSTOM_SYSCALL_HOOK 1
#endif

#if (defined(RIN_FREESTANDING) && RIN_FREESTANDING) || \
    !defined(__STDC_HOSTED__) || !__STDC_HOSTED__ || \
    defined(RIN_TIME_CUSTOM_SYSCALL_HOOK) || !defined(WIN_PTHREADS_H)
#define RIN_TIME_TARGET_FUNCTIONS 1
#endif

/* Some freestanding contract tests intentionally provide their own syscall
 * adapter and suppress the public sys/syscall.h include guard.  The shared
 * ABI remains available through rin_cpu_time_abi.h, so retain the canonical
 * realtime number without requiring the legacy SYS_* alias to be visible. */
#ifndef SYS_REALTIME_NS
#define SYS_REALTIME_NS RIN_SYS_REALTIME_NS
#endif
#ifndef SYS_TIMER_CREATE
#define SYS_TIMER_CREATE RIN_SYS_TIMER_CREATE
#endif
#ifndef SYS_TIMER_SETTIME
#define SYS_TIMER_SETTIME RIN_SYS_TIMER_SETTIME
#endif
#ifndef SYS_TIMER_GETTIME
#define SYS_TIMER_GETTIME RIN_SYS_TIMER_GETTIME
#endif
#ifndef SYS_TIMER_DELETE
#define SYS_TIMER_DELETE RIN_SYS_TIMER_DELETE
#endif
#ifndef SYS_TIMER_GETOVERRUN
#define SYS_TIMER_GETOVERRUN RIN_SYS_TIMER_GETOVERRUN
#endif
#ifndef SYS_TIMER_POLL
#define SYS_TIMER_POLL RIN_SYS_TIMER_POLL
#endif
#ifndef SYS_CLOCK_SETTIME
#define SYS_CLOCK_SETTIME RIN_SYS_CLOCK_SETTIME
#endif

#ifndef _RIN_TIME_SYSCALL1
#define _RIN_TIME_SYSCALL1(number, argument1) \
    _syscall1((uintptr_t)(number), (uintptr_t)(argument1))
#endif

#ifndef _RIN_TIME_SYSCALL2
#define _RIN_TIME_SYSCALL2(number, argument1, argument2) \
    _syscall2((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2))
#endif

#ifndef _RIN_TIME_SLEEP
#define _RIN_TIME_SLEEP(milliseconds) \
    _RIN_TIME_SYSCALL1(SYS_SLEEP, (uintptr_t)(milliseconds))
#endif

#ifndef _RIN_TIME_GETTIMEOFDAY
#define _RIN_TIME_GETTIMEOFDAY(output) \
    _RIN_TIME_SYSCALL2(SYS_GETTIMEOFDAY, (uintptr_t)(output), 0u)
#endif

#ifndef _RIN_TIME_NS64
#define _RIN_TIME_NS64() _RIN_TIME_SYSCALL1(SYS_TIME_NS, 0u)
#endif

#ifndef _RIN_TIME_NS32
#define _RIN_TIME_NS32(output) \
    _RIN_TIME_SYSCALL1(SYS_TIME_NS, (uintptr_t)(output))
#endif

#ifndef _RIN_TIME_REALTIME_NS64
#define _RIN_TIME_REALTIME_NS64() \
    _RIN_TIME_SYSCALL1(SYS_REALTIME_NS, 0u)
#endif

#ifndef _RIN_TIME_REALTIME_NS32
#define _RIN_TIME_REALTIME_NS32(output) \
    _RIN_TIME_SYSCALL1(SYS_REALTIME_NS, (uintptr_t)(output))
#endif

#ifndef _RIN_TIME_CPU_GET
#define _RIN_TIME_CPU_GET(output) \
    _RIN_TIME_SYSCALL1(SYS_CPU_TIME_GET, (uintptr_t)(output))
#endif

#ifndef _RIN_TIME_TIMER_CREATE
#define _RIN_TIME_TIMER_CREATE(request) \
    _RIN_TIME_SYSCALL1(SYS_TIMER_CREATE, (uintptr_t)(request))
#endif

#ifndef _RIN_TIME_TIMER_SETTIME
#define _RIN_TIME_TIMER_SETTIME(timer_id, flags, new_value, old_value) \
    _syscall4((uintptr_t)SYS_TIMER_SETTIME, (uintptr_t)(timer_id), \
              (uintptr_t)(flags), (uintptr_t)(new_value), \
              (uintptr_t)(old_value))
#endif

#ifndef _RIN_TIME_TIMER_GETTIME
#define _RIN_TIME_TIMER_GETTIME(timer_id, value) \
    _RIN_TIME_SYSCALL2(SYS_TIMER_GETTIME, (uintptr_t)(timer_id), \
                       (uintptr_t)(value))
#endif

#ifndef _RIN_TIME_TIMER_DELETE
#define _RIN_TIME_TIMER_DELETE(timer_id) \
    _RIN_TIME_SYSCALL1(SYS_TIMER_DELETE, (uintptr_t)(timer_id))
#endif

#ifndef _RIN_TIME_TIMER_GETOVERRUN
#define _RIN_TIME_TIMER_GETOVERRUN(timer_id) \
    _RIN_TIME_SYSCALL1(SYS_TIMER_GETOVERRUN, (uintptr_t)(timer_id))
#endif

#ifndef _RIN_TIME_TIMER_POLL
#define _RIN_TIME_TIMER_POLL(timer_id) \
    _RIN_TIME_SYSCALL1(SYS_TIMER_POLL, (uintptr_t)(timer_id))
#endif

#ifndef _RIN_TIME_CLOCK_SETTIME
#define _RIN_TIME_CLOCK_SETTIME(request) \
    _RIN_TIME_SYSCALL2(SYS_CLOCK_SETTIME, \
                       (uintptr_t)((request)->clock_id), \
                       (uintptr_t)(request))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * 型定義
 * ═══════════════════════════════════════════════════════════════*/

#ifndef _TIME_T_DEFINED
typedef long time_t;
#define _TIME_T_DEFINED
#endif
#ifndef _CLOCK_T_DEFINED
typedef long clock_t;
#define _CLOCK_T_DEFINED
#endif
#ifndef _CLOCKID_T_DEFINED
typedef int clockid_t;
#define _CLOCKID_T_DEFINED
#endif
#ifndef _TIMER_T_DEFINED
typedef int timer_t;
#define _TIMER_T_DEFINED
#endif

#ifndef _TM_DEFINED
#define _TM_DEFINED
struct tm {
    int tm_sec;     /* 秒 (0-59) */
    int tm_min;     /* 分 (0-59) */
    int tm_hour;    /* 時 (0-23) */
    int tm_mday;    /* 日 (1-31) */
    int tm_mon;     /* 月 (0-11) */
    int tm_year;    /* 年 (1900年からの年数) */
    int tm_wday;    /* 曜日 (0-6, 日曜=0) */
    int tm_yday;    /* 年間通算日 (0-365) */
    int tm_isdst;   /* 夏時間フラグ */
    /* GNU拡張 */
    long tm_gmtoff; /* UTCからのオフセット(秒) */
    const char* tm_zone; /* タイムゾーン名 */
};
#define RIN_TM_HAS_GNU_EXTENSIONS 1
#endif

#if defined(_TIMESPEC_DEFINED)
/* MinGW's sys/types.h publishes itimerspec under the broader timespec
 * guard.  Remember that an external definition owns both structures so the
 * target header does not redeclare itimerspec after a hosted pthread include. */
#define RIN_LIBC_TIME_EXTERNAL_TIMESPEC 1
#endif

#ifndef _TIMESPEC_DEFINED
#define _TIMESPEC_DEFINED
struct timespec {
    time_t tv_sec;   /* 秒 */
    long   tv_nsec;  /* ナノ秒 */
};
#endif

#ifndef _TIMEVAL_DEFINED
#define _TIMEVAL_DEFINED
struct timeval {
    time_t tv_sec;   /* 秒 */
    long   tv_usec;  /* マイクロ秒 */
};
#endif

#ifndef _ITIMERSPEC_DEFINED
#if !defined(RIN_LIBC_TIME_EXTERNAL_TIMESPEC)
#define _ITIMERSPEC_DEFINED
struct itimerspec {
    struct timespec it_interval;
    struct timespec it_value;
};
#endif
#endif

#ifndef TIMER_ABSTIME
#define TIMER_ABSTIME ((int)RIN_TIMER_ABSTIME)
#endif
#ifndef SIGEV_SIGNAL
#define SIGEV_SIGNAL ((int)RIN_TIMER_NOTIFY_SIGNAL)
#endif

#ifndef SIGEV_NONE
#define SIGEV_NONE 1
#endif
#ifndef SIGEV_THREAD
#define SIGEV_THREAD 2
#endif
#ifndef RIN_TIMER_THREAD_SIGNAL
#define RIN_TIMER_THREAD_SIGNAL 31
#endif
#ifndef _SIGVAL_DEFINED
#define _SIGVAL_DEFINED
union sigval {
    int sival_int;
    void* sival_ptr;
};
#endif
#ifndef _SIGEVENT_DEFINED
#define _SIGEVENT_DEFINED
struct sigevent {
    int sigev_notify;
    int sigev_signo;
    union sigval sigev_value;
    void (*sigev_notify_function)(union sigval);
    void* sigev_notify_attributes;
};

#if defined(RIN_USERSPACE)
extern int rin_timer_thread_register(timer_t timer_id,
                                     void (*function)(union sigval),
                                     union sigval value)
    __attribute__((weak));
extern void rin_timer_thread_unregister(timer_t timer_id)
    __attribute__((weak));
#endif
#endif
#ifndef SIGALRM
#define SIGALRM 14
#endif
#ifndef SIGKILL
#define SIGKILL 9
#endif
#ifndef SIGSTOP
#define SIGSTOP 19
#endif
#ifndef NSIG
#define NSIG 32
#endif

/* A target syscall result is a signed machine word.  Keep it in intptr_t
 * until the public API has checked its narrower destination type. */
#if defined(RIN_TIME_TARGET_FUNCTIONS)
static inline intptr_t _rin_time_result(intptr_t result) {
    result = __rin_syscall_posixize(result);
    if (result < 0) {
        if (result != -1) errno = EIO;
        return -1;
    }
    return result;
}

static inline int _rin_time_status(intptr_t result) {
    result = _rin_time_result(result);
    if (result < 0) return -1;
    if (result != 0) {
        errno = (uintptr_t)result > (uintptr_t)INT_MAX ? EOVERFLOW : EIO;
        return -1;
    }
    return 0;
}

static inline int _rin_time_gettimeofday(struct timeval* output) {
    return _rin_time_status(_RIN_TIME_GETTIMEOFDAY(output));
}

/* `time_t` is Rin's signed long ABI.  Do the bound check before modifying
 * the caller's timespec so LLP64 builds cannot silently truncate seconds. */
static inline int _rin_time_ns_to_timespec(uint64_t nanoseconds,
                                           struct timespec* output) {
    uint64_t seconds = nanoseconds / UINT64_C(1000000000);
    if (seconds > (uint64_t)LONG_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    output->tv_sec = (time_t)seconds;
    output->tv_nsec = (long)(nanoseconds % UINT64_C(1000000000));
    return 0;
}

/* クロックID */
#define CLOCK_REALTIME           0
#define CLOCK_MONOTONIC          1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID  3
#define CLOCK_REALTIME_COARSE    CLOCK_REALTIME
#define CLOCK_MONOTONIC_COARSE   CLOCK_MONOTONIC

/* ═══════════════════════════════════════════════════════════════
 * 定数
 * ═══════════════════════════════════════════════════════════════*/

#define CLOCKS_PER_SEC 1000

/* CPU clocks are acquired through the fixed 32-byte RinCpuTimeV1 ABI on
 * target userspace.  The kernel path uses the same scheduler source directly
 * while avoiding a syscall instruction in ring 0.  Host builds remain
 * fail-closed so a native test binary never accidentally invokes Rin syscall
 * numbers on its host kernel. */
static inline int _rin_cpu_time_ns(uint16_t scope, uint64_t* output) {
    if (!output) {
        errno = EFAULT;
        return -1;
    }
#if defined(RIN_FREESTANDING) && defined(RIN_KERNEL)
    extern int sched_cpu_time_current(uint16_t, uint64_t, uint64_t*);
    extern uint64_t platform_get_time_ns(void);
    int result = sched_cpu_time_current(scope, platform_get_time_ns(), output);
    if (result < 0) {
        /* Scheduler errors use the POSIX errno range when representable.
         * Do not negate an arbitrary backend value: INT_MIN would overflow
         * before errno could be published. */
        errno = (result >= -4095) ? -result : EIO;
        return -1;
    }
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
#elif defined(RIN_FREESTANDING) && defined(RIN_USERSPACE)
    /* List every ABI field instead of `{0}` so strict C++ builds do not
     * downgrade the required complete request initialization to a warning.
     * The kernel validates the two reserved fields remain zero. */
    RinCpuTimeV1 request = { 0u, 0u, 0u, 0u, 0u, 0u };
    intptr_t result;
    request.struct_size = (uint32_t)sizeof(request);
    request.version = RIN_CPU_TIME_ABI_VERSION;
    request.scope = scope;
    result = _RIN_TIME_CPU_GET(&request);
    if (_rin_time_status(result) != 0) return -1;
    *output = request.cpu_time_ns;
    return 0;
#else
    (void)scope;
    errno = ENOSYS;
    return -1;
#endif
}

/* ═══════════════════════════════════════════════════════════════
 * RTC読み取り (カーネル空間用)
 * Note: RIN_KERNEL is only defined in kernel builds, not user apps
 * ═══════════════════════════════════════════════════════════════*/

#if defined(RIN_FREESTANDING) && defined(RIN_KERNEL)
#define RTC_INDEX_PORT  0x70
#define RTC_DATA_PORT   0x71
#define RTC_SECONDS     0x00
#define RTC_MINUTES     0x02
#define RTC_HOURS       0x04
#define RTC_DAY         0x07
#define RTC_MONTH       0x08
#define RTC_YEAR        0x09
#define RTC_STATUS_A    0x0A

static inline uint8_t _time_inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline void _time_outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline int _bcd_to_bin(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

static inline int _rtc_updating(void) {
    _time_outb(RTC_INDEX_PORT, RTC_STATUS_A);
    return _time_inb(RTC_DATA_PORT) & 0x80;
}

static inline uint8_t _rtc_read(uint8_t reg) {
    _time_outb(RTC_INDEX_PORT, reg);
    return _time_inb(RTC_DATA_PORT);
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * 時刻計算ヘルパー
 * ═══════════════════════════════════════════════════════════════*/

static const int _days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static inline int _is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/* ═══════════════════════════════════════════════════════════════
 * 時刻取得関数
 * ═══════════════════════════════════════════════════════════════*/

#if defined(_STDIO_GLOBAL_IMPL)
/* rin_runtime.c emits the process-wide ABI symbol in the same translation
 * unit.  Keep the declaration visible without also emitting the header-only
 * fallback implementation. */
time_t time(time_t* tloc);
#else
static inline time_t time(time_t* tloc) {
#if defined(RIN_FREESTANDING) && defined(RIN_KERNEL)
    /* カーネル空間: RTCから直接読み取り */
    while (_rtc_updating());

    int sec = _bcd_to_bin(_rtc_read(RTC_SECONDS));
    int min = _bcd_to_bin(_rtc_read(RTC_MINUTES));
    int hour = _bcd_to_bin(_rtc_read(RTC_HOURS));
    int day = _bcd_to_bin(_rtc_read(RTC_DAY));
    int month = _bcd_to_bin(_rtc_read(RTC_MONTH));
    int year = _bcd_to_bin(_rtc_read(RTC_YEAR)) + 2000;

    /* UNIX時間計算 */
    time_t t = 0;
    for (int y = 1970; y < year; y++) {
        t += _is_leap_year(y) ? 366 : 365;
    }
    t *= 86400;
    for (int m = 0; m < month - 1; m++) {
        t += _days_in_month[m] * 86400;
        if (m == 1 && _is_leap_year(year)) t += 86400;
    }
    t += (day - 1) * 86400;
    t += hour * 3600 + min * 60 + sec;

    if (tloc) *tloc = t;
    return t;
#else
    struct timeval tv = {0, 0};
    if (_rin_time_gettimeofday(&tv) != 0) {
        if (tloc) *tloc = (time_t)-1;
        return (time_t)-1;
    }
    if (tloc) *tloc = tv.tv_sec;
    return tv.tv_sec;
#endif
}
#endif

static inline clock_t clock(void) {
    uint64_t nanoseconds;
    uint64_t ticks;
    const uint64_t maximum_clock =
        (UINT64_C(1) << (sizeof(clock_t) * 8u - 1u)) - UINT64_C(1);
    if (_rin_cpu_time_ns(RIN_CPU_TIME_SCOPE_PROCESS, &nanoseconds) != 0)
        return (clock_t)-1;
    ticks = nanoseconds / (UINT64_C(1000000000) / CLOCKS_PER_SEC);
    if (ticks > maximum_clock) {
        errno = EOVERFLOW;
        return (clock_t)-1;
    }
    return (clock_t)ticks;
}

static inline int gettimeofday(struct timeval* tv, void* tz) {
    (void)tz;
    if (!tv) {
        errno = EFAULT;
        return -1;
    }
#if defined(RIN_FREESTANDING) && defined(RIN_KERNEL)
    time_t t = time(NULL);
    tv->tv_sec = t;
    tv->tv_usec = 0;
    return 0;
#else
    return _rin_time_gettimeofday(tv);
#endif
}

/* ═══════════════════════════════════════════════════════════════
 * tm構造体操作
 * ═══════════════════════════════════════════════════════════════*/

#if defined(RIN_USERSPACE)

extern char* tzname[2];
extern long timezone;
extern int daylight;

/* Product timezone owners may bind one immutable, read-only default source.
 * The callback must write one NUL-terminated POSIX TZ expression into the
 * supplied bounded buffer and return non-zero.  The capacity passed by libc
 * is RIN_TIME_ZONE_PROVIDER_TEXT_MAX + 1, including the terminator.  TZ
 * remains the process-level override; a failed callback is reported without
 * changing the active zone. */
#define RIN_TIME_ZONE_PROVIDER_TEXT_MAX 127u
typedef int (*RinTimeZoneSystemProvider)(char* buffer, size_t capacity,
                                         void* context);
int rin_time_zone_bind_system_provider(RinTimeZoneSystemProvider provider,
                                       void* context);

/* Internal bounded `%Z` bridge supplied by time_zone.c.  The result strings
 * have static storage and the function changes no process timezone state. */
int rin_time_zone_match_abbreviation(const char* input, size_t* consumed,
                                     long* offset, const char** zone);

/* Validate one complete `%Z` token against the immutable abbreviation and
 * named-zone catalog.  `length` excludes any terminator; the function only
 * reports the catalog's standard offset and never changes TZ state. */
int rin_time_zone_resolve_name(const char* input, size_t length, long* offset);

void tzset(void);
struct tm* gmtime(const time_t* timer);
struct tm* localtime(const time_t* timer);
struct tm* gmtime_r(const time_t* timer, struct tm* result);
struct tm* localtime_r(const time_t* timer, struct tm* result);
time_t mktime(struct tm* value);
time_t timegm(struct tm* value);

#else

#if defined(__cplusplus)
#define RIN_TIME_FALLBACK_THREAD_LOCAL thread_local
#else
#define RIN_TIME_FALLBACK_THREAD_LOCAL _Thread_local
#endif
static RIN_TIME_FALLBACK_THREAD_LOCAL struct tm _tm_buf;
#undef RIN_TIME_FALLBACK_THREAD_LOCAL
static char _tzname_utc[] = "UTC";
/* This header provides an internal UTC-only fallback. Individual consumers
 * need not reference tzname, so keep its internal linkage without creating a
 * freestanding -Werror unused-variable failure in every translation unit. */
static char* tzname[2] __attribute__((unused)) = { _tzname_utc, _tzname_utc };

static inline struct tm* gmtime(const time_t* timer) {
    if (!timer) return NULL;

    time_t t = *timer;
    int days = (int)(t / 86400);
    int rem = (int)(t % 86400);

    if (rem < 0) { rem += 86400; days--; }

    _tm_buf.tm_sec = rem % 60; rem /= 60;
    _tm_buf.tm_min = rem % 60;
    _tm_buf.tm_hour = rem / 60;

    _tm_buf.tm_wday = (4 + days) % 7;
    if (_tm_buf.tm_wday < 0) _tm_buf.tm_wday += 7;

    int year = 1970;
    while (days >= (_is_leap_year(year) ? 366 : 365)) {
        days -= _is_leap_year(year) ? 366 : 365;
        year++;
    }
    while (days < 0) {
        year--;
        days += _is_leap_year(year) ? 366 : 365;
    }

    _tm_buf.tm_year = year - 1900;
    _tm_buf.tm_yday = days;

    int month = 0;
    while (month < 11) {
        int mdays = _days_in_month[month];
        if (month == 1 && _is_leap_year(year)) mdays++;
        if (days < mdays) break;
        days -= mdays;
        month++;
    }

    _tm_buf.tm_mon = month;
    _tm_buf.tm_mday = days + 1;
    _tm_buf.tm_isdst = 0;
    _tm_buf.tm_gmtoff = 0;
    _tm_buf.tm_zone = "UTC";

    return &_tm_buf;
}

static inline struct tm* localtime(const time_t* timer) {
    return gmtime(timer);  /* タイムゾーン未対応 */
}

/* リエントラント版 */
static inline struct tm* gmtime_r(const time_t* timer, struct tm* result) {
    if (!timer || !result) return NULL;

    time_t t = *timer;
    int days = (int)(t / 86400);
    int rem = (int)(t % 86400);

    if (rem < 0) { rem += 86400; days--; }

    result->tm_sec = rem % 60; rem /= 60;
    result->tm_min = rem % 60;
    result->tm_hour = rem / 60;

    result->tm_wday = (4 + days) % 7;
    if (result->tm_wday < 0) result->tm_wday += 7;

    int year = 1970;
    while (days >= (_is_leap_year(year) ? 366 : 365)) {
        days -= _is_leap_year(year) ? 366 : 365;
        year++;
    }
    while (days < 0) {
        year--;
        days += _is_leap_year(year) ? 366 : 365;
    }

    result->tm_year = year - 1900;
    result->tm_yday = days;

    int month = 0;
    while (month < 11) {
        int mdays = _days_in_month[month];
        if (month == 1 && _is_leap_year(year)) mdays++;
        if (days < mdays) break;
        days -= mdays;
        month++;
    }

    result->tm_mon = month;
    result->tm_mday = days + 1;
    result->tm_isdst = 0;
    result->tm_gmtoff = 0;
    result->tm_zone = "UTC";

    return result;
}

static inline struct tm* localtime_r(const time_t* timer, struct tm* result) {
    return gmtime_r(timer, result);  /* タイムゾーン未対応 */
}

static inline time_t mktime(struct tm* tm) {
    if (!tm) return -1;

    int year = tm->tm_year + 1900;
    int month = tm->tm_mon;
    int day = tm->tm_mday;

    time_t days = 0;
    for (int y = 1970; y < year; y++) {
        days += _is_leap_year(y) ? 366 : 365;
    }

    for (int m = 0; m < month; m++) {
        days += _days_in_month[m];
        if (m == 1 && _is_leap_year(year)) days++;
    }

    days += day - 1;

    return days * 86400 + tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;
}

static inline time_t timegm(struct tm* tm) {
    return mktime(tm);
}

#endif /* RIN_USERSPACE */

static inline double difftime(time_t time1, time_t time0) {
    return (double)(time1 - time0);
}

/* ═══════════════════════════════════════════════════════════════
 * 文字列変換
 * ═══════════════════════════════════════════════════════════════*/

#define RIN_ASCTIME_TEXT_SIZE 26u

/* The legacy asctime()/ctime() API returns storage owned by the caller's
 * thread until its next call.  A process-global buffer lets an unrelated
 * thread overwrite a still-live result, which is especially surprising when
 * a formatter is used from logging or signal-adjacent code.  Keep the
 * standard static-storage lifetime while making the owner explicit and
 * independent for C and C++ hosted/freestanding builds. */
#if defined(__cplusplus)
#define RIN_ASCTIME_THREAD_LOCAL thread_local
#else
#define RIN_ASCTIME_THREAD_LOCAL _Thread_local
#endif
static RIN_ASCTIME_THREAD_LOCAL char _asctime_buf[RIN_ASCTIME_TEXT_SIZE];
#undef RIN_ASCTIME_THREAD_LOCAL

static inline int _rin_asctime_tm_valid(const struct tm* value) {
    return value && value->tm_sec >= 0 && value->tm_sec <= 60 &&
           value->tm_min >= 0 && value->tm_min <= 59 &&
           value->tm_hour >= 0 && value->tm_hour <= 23 &&
           value->tm_mday >= 1 && value->tm_mday <= 31 &&
           value->tm_mon >= 0 && value->tm_mon <= 11 &&
           value->tm_year >= -1900 && value->tm_year <= 8099 &&
           value->tm_wday >= 0 && value->tm_wday <= 6;
}

static inline char* asctime_r(const struct tm* tm, char* output) {
    static const char* const days[7] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    static const char* const months[12] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    char candidate[RIN_ASCTIME_TEXT_SIZE];
    int year;
    unsigned int index;

    if (!tm || !output || !_rin_asctime_tm_valid(tm)) {
        errno = EINVAL;
        return NULL;
    }
    for (index = 0u; index < 3u; ++index) candidate[index] = days[tm->tm_wday][index];
    candidate[3] = ' ';
    for (index = 0u; index < 3u; ++index) candidate[4u + index] = months[tm->tm_mon][index];
    candidate[7] = ' ';
    candidate[8] = tm->tm_mday < 10 ? ' ' : (char)('0' + tm->tm_mday / 10);
    candidate[9] = (char)('0' + tm->tm_mday % 10);
    candidate[10] = ' ';
    candidate[11] = (char)('0' + tm->tm_hour / 10);
    candidate[12] = (char)('0' + tm->tm_hour % 10);
    candidate[13] = ':';
    candidate[14] = (char)('0' + tm->tm_min / 10);
    candidate[15] = (char)('0' + tm->tm_min % 10);
    candidate[16] = ':';
    candidate[17] = (char)('0' + tm->tm_sec / 10);
    candidate[18] = (char)('0' + tm->tm_sec % 10);
    candidate[19] = ' ';
    year = tm->tm_year + 1900;
    candidate[20] = (char)('0' + (year / 1000) % 10);
    candidate[21] = (char)('0' + (year / 100) % 10);
    candidate[22] = (char)('0' + (year / 10) % 10);
    candidate[23] = (char)('0' + year % 10);
    candidate[24] = '\n';
    candidate[25] = '\0';
    for (index = 0u; index < RIN_ASCTIME_TEXT_SIZE; ++index)
        output[index] = candidate[index];
    return output;
}

static inline char* asctime(const struct tm* tm) {
    return asctime_r(tm, _asctime_buf);
}

static inline char* ctime_r(const time_t* timer, char* output) {
    struct tm local;
    if (!timer || !output) {
        errno = EINVAL;
        return NULL;
    }
    if (!localtime_r(timer, &local)) return NULL;
    return asctime_r(&local, output);
}

static inline char* ctime(const time_t* timer) {
    return ctime_r(timer, _asctime_buf);
}

#if defined(RIN_USERSPACE)
const char* rin_locale_weekday(int weekday, int abbreviated);
const char* rin_locale_month(int month, int abbreviated);
const char* rin_locale_am_pm(int hour);
const char* rin_locale_time_format(char conversion);
#endif

static inline const char* _rin_time_weekday(int weekday, int abbreviated) {
#if defined(RIN_USERSPACE)
    return rin_locale_weekday(weekday, abbreviated);
#else
    static const char* short_names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char* long_names[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                                       "Thursday", "Friday", "Saturday"};
    return abbreviated ? short_names[weekday % 7] : long_names[weekday % 7];
#endif
}

static inline const char* _rin_time_month(int month, int abbreviated) {
#if defined(RIN_USERSPACE)
    return rin_locale_month(month, abbreviated);
#else
    static const char* short_names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    static const char* long_names[] = {"January", "February", "March", "April", "May", "June",
                                       "July", "August", "September", "October", "November", "December"};
    return abbreviated ? short_names[month % 12] : long_names[month % 12];
#endif
}


size_t strftime(char* s, size_t maxsize, const char* format,
                const struct tm* tm);
#if defined(RIN_USERSPACE)
/* POSIX locale-specific formatter.  The locale object is immutable for the
 * duration of this call; LC_GLOBAL_LOCALE selects the process locale. */
size_t strftime_l(char* s, size_t maxsize, const char* format,
                  const struct tm* tm, locale_t locale);
#endif

/* ═══════════════════════════════════════════════════════════════
 * POSIX時刻関数
 * ═══════════════════════════════════════════════════════════════*/

static inline int clock_gettime(clockid_t clk_id, struct timespec* tp) {
    if (!tp) {
        errno = EFAULT;
        return -1;
    }

    switch (clk_id) {
        case CLOCK_MONOTONIC:
        {
            /* SYS_TIME_NS (260): TSC-based monotonic nanoseconds */
#if defined(__x86_64__)
            intptr_t result = _rin_time_result(_RIN_TIME_NS64());
            uint64_t ns;
            if (result < 0) return -1;
            ns = (uint64_t)result;
#else
            /* 32-bit: return value is truncated to 32 bits, so pass a
             * pointer and let the kernel write the full u64 there. */
            uint64_t ns = 0;
            if (_rin_time_status(_RIN_TIME_NS32(&ns)) != 0) return -1;
#endif
            return _rin_time_ns_to_timespec(ns, tp);
        }
        case CLOCK_PROCESS_CPUTIME_ID:
        case CLOCK_THREAD_CPUTIME_ID:
        {
            uint64_t ns;
            uint16_t scope = clk_id == CLOCK_PROCESS_CPUTIME_ID
                ? RIN_CPU_TIME_SCOPE_PROCESS : RIN_CPU_TIME_SCOPE_THREAD;
            if (_rin_cpu_time_ns(scope, &ns) != 0) return -1;
            return _rin_time_ns_to_timespec(ns, tp);
        }
        case CLOCK_REALTIME:
        {
#if defined(RIN_FREESTANDING)
            /* The product kernel owns a nanosecond realtime source.  Keep
             * CLOCK_REALTIME independent from the legacy microsecond
             * gettimeofday ABI so callers can preserve sub-microsecond data. */
            uint64_t ns;
#if defined(__x86_64__)
            intptr_t result =
                _rin_time_result(_RIN_TIME_REALTIME_NS64());
            if (result < 0) return -1;
            ns = (uint64_t)result;
#else
            ns = 0u;
            if (_rin_time_status(_RIN_TIME_REALTIME_NS32(&ns)) != 0)
                return -1;
#endif
            return _rin_time_ns_to_timespec(ns, tp);
#else
            struct timeval tv;
            int ret = gettimeofday(&tv, NULL);
            if (ret < 0) return -1;
            tp->tv_sec = tv.tv_sec;
            tp->tv_nsec = tv.tv_usec * 1000;
            return 0;
#endif
        }
        default:
            errno = EINVAL;
            return -1;
    }
}

static inline int clock_getres(clockid_t clk_id, struct timespec* res) {
    if (clk_id == CLOCK_PROCESS_CPUTIME_ID ||
        clk_id == CLOCK_THREAD_CPUTIME_ID) {
#if (defined(RIN_FREESTANDING) && defined(RIN_KERNEL)) || \
    (defined(RIN_FREESTANDING) && defined(RIN_USERSPACE))
        if (res) {
            res->tv_sec = 0;
            /* CPU intervals are sampled from the monotonic target clock. */
            res->tv_nsec = 1000000;
        }
        return 0;
#else
        errno = ENOSYS;
        return -1;
#endif
    }
    if (clk_id != CLOCK_REALTIME && clk_id != CLOCK_MONOTONIC) {
        errno = EINVAL;
        return -1;
    }
    if (res) {
        res->tv_sec = 0;
        res->tv_nsec = 1000000;  /* 1ms resolution */
    }
    return 0;
}

static inline int _rin_time_timespec_to_ns(const struct timespec* input,
                                           uint64_t* output) {
    uint64_t seconds;
    uint64_t nanoseconds;
    if (!input || !output) return EFAULT;
    if (input->tv_sec < 0 || input->tv_nsec < 0 ||
        input->tv_nsec >= 1000000000L) return EINVAL;
    seconds = (uint64_t)input->tv_sec;
    nanoseconds = (uint64_t)input->tv_nsec;
    if (seconds > (UINT64_MAX - nanoseconds) / UINT64_C(1000000000))
        return EOVERFLOW;
    *output = seconds * UINT64_C(1000000000) + nanoseconds;
    return 0;
}

/* Set the shared wall clock only through the authenticated, capability-gated
 * kernel owner.  CLOCK_MONOTONIC and CPU clocks are immutable. */
static inline int clock_settime(clockid_t clk_id,
                                const struct timespec* tp) {
    RinClockSettimeV1 request = { 0u, 0u, 0u, 0, 0, 0, 0 };
    uint64_t unix_ns;
    intptr_t result;
    int conversion;
    if (!tp) {
        errno = EFAULT;
        return -1;
    }
    if (clk_id != CLOCK_REALTIME) {
        errno = EINVAL;
        return -1;
    }
    conversion = _rin_time_timespec_to_ns(tp, &unix_ns);
    if (conversion != 0) {
        errno = conversion;
        return -1;
    }
    if (unix_ns == 0u) {
        /* The product realtime owner reserves zero for an unavailable RTC. */
        errno = EINVAL;
        return -1;
    }
    request.struct_size = (uint32_t)sizeof(request);
    request.version = RIN_CLOCK_SETTIME_ABI_VERSION;
    request.clock_id = (int32_t)clk_id;
    request.tv_sec = (int64_t)tp->tv_sec;
    request.tv_nsec = (int64_t)tp->tv_nsec;
    (void)unix_ns;
    result = _rin_time_result(_RIN_TIME_CLOCK_SETTIME(&request));
    if (result < 0) return -1;
    if (result != 0) {
        errno = (uintptr_t)result > (uintptr_t)INT_MAX ? EOVERFLOW : EIO;
        return -1;
    }
    return 0;
}

static inline int timer_create(clockid_t clock_id,
                               const struct sigevent* event,
                               timer_t* timer_id) {
    RinTimerCreateV1 request;
    intptr_t result;
    int notify = SIGEV_SIGNAL;
    int signo = SIGALRM;
    if (!timer_id) {
        errno = EFAULT;
        return -1;
    }
    if (clock_id != CLOCK_REALTIME && clock_id != CLOCK_MONOTONIC) {
        errno = EINVAL;
        return -1;
    }
    if (event) {
        notify = event->sigev_notify;
        if (notify != SIGEV_SIGNAL && notify != SIGEV_NONE &&
            notify != SIGEV_THREAD) {
            errno = EINVAL;
            return -1;
        }
        if (notify == SIGEV_SIGNAL) {
            if (event->sigev_signo != 0) signo = event->sigev_signo;
        } else if (notify == SIGEV_THREAD) {
            if (!event->sigev_notify_function ||
                event->sigev_notify_attributes != (void*)0) {
                errno = EINVAL;
                return -1;
            }
            signo = RIN_TIMER_THREAD_SIGNAL;
        } else {
            signo = 0;
        }
    }
    if (notify == SIGEV_SIGNAL &&
        (signo <= 0 || signo >= NSIG || signo == SIGKILL || signo == SIGSTOP)) {
        errno = EINVAL;
        return -1;
    }
    request.struct_size = (uint32_t)sizeof(request);
    request.version = (uint16_t)RIN_TIMER_ABI_VERSION;
    request.flags = 0u;
    request.clock_id = (int32_t)clock_id;
    request.notify = notify;
    request.signo = (int32_t)signo;
    request.reserved0 = 0;
    request.value = 0u;
#if defined(RIN_TIME_TARGET_FUNCTIONS)
    result = _rin_time_result(_RIN_TIME_TIMER_CREATE(&request));
    if (result < 0) return -1;
    if (result == 0 || (uintmax_t)result > (uintmax_t)INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    *timer_id = (timer_t)result;
#if defined(RIN_USERSPACE)
    if (notify == SIGEV_THREAD) {
        if (!rin_timer_thread_register ||
            rin_timer_thread_register(*timer_id, event->sigev_notify_function,
                                      event->sigev_value) != 0) {
            (void)_RIN_TIME_TIMER_DELETE(*timer_id);
            errno = EAGAIN;
            return -1;
        }
    }
#endif
    return 0;
#else
    (void)result;
    errno = ENOSYS;
    return -1;
#endif
}

static inline int timer_settime(timer_t timer_id, int flags,
                                const struct itimerspec* value,
                                struct itimerspec* old_value) {
    RinTimerSpecV1 request;
    RinTimerSpecV1 previous;
    struct itimerspec converted;
    uint64_t value_ns;
    uint64_t interval_ns;
    intptr_t result;
    int error;
    if (timer_id <= 0 || !value) {
        errno = !value ? EFAULT : EINVAL;
        return -1;
    }
    if ((flags & ~TIMER_ABSTIME) != 0) {
        errno = EINVAL;
        return -1;
    }
    error = _rin_time_timespec_to_ns(&value->it_value, &value_ns);
    if (error != 0) {
        errno = error;
        return -1;
    }
    error = _rin_time_timespec_to_ns(&value->it_interval, &interval_ns);
    if (error != 0) {
        errno = error;
        return -1;
    }
    request.struct_size = (uint32_t)sizeof(request);
    request.version = (uint16_t)RIN_TIMER_ABI_VERSION;
    request.flags = 0u;
    request.value_ns = value_ns;
    request.interval_ns = interval_ns;
    request.reserved0 = 0u;
    request.reserved1 = 0u;
#if defined(RIN_TIME_TARGET_FUNCTIONS)
    result = _rin_time_result(_RIN_TIME_TIMER_SETTIME(
        timer_id, (uint32_t)flags, &request,
        old_value ? &previous : (void*)0));
    if (result < 0) return -1;
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    if (old_value) {
        error = _rin_time_ns_to_timespec(previous.value_ns, &converted.it_value);
        if (error == 0)
            error = _rin_time_ns_to_timespec(previous.interval_ns,
                                             &converted.it_interval);
        if (error != 0) {
            errno = error;
            return -1;
        }
        *old_value = converted;
    }
    return 0;
#else
    (void)result;
    (void)old_value;
    errno = ENOSYS;
    return -1;
#endif
}

static inline int timer_gettime(timer_t timer_id,
                                struct itimerspec* value) {
    RinTimerSpecV1 result_wire;
    struct itimerspec converted;
    intptr_t result;
    int error;
    if (timer_id <= 0 || !value) {
        errno = !value ? EFAULT : EINVAL;
        return -1;
    }
#if defined(RIN_TIME_TARGET_FUNCTIONS)
    result = _rin_time_result(_RIN_TIME_TIMER_GETTIME(timer_id, &result_wire));
    if (result < 0) return -1;
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    if (!rin_timer_spec_v1_valid(&result_wire)) {
        errno = EIO;
        return -1;
    }
    error = _rin_time_ns_to_timespec(result_wire.value_ns,
                                     &converted.it_value);
    if (error == 0)
        error = _rin_time_ns_to_timespec(result_wire.interval_ns,
                                         &converted.it_interval);
    if (error != 0) {
        errno = error;
        return -1;
    }
    *value = converted;
    return 0;
#else
    (void)result;
    errno = ENOSYS;
    return -1;
#endif
}

static inline int timer_delete(timer_t timer_id) {
    intptr_t result;
    if (timer_id <= 0) {
        errno = EINVAL;
        return -1;
    }
#if defined(RIN_TIME_TARGET_FUNCTIONS)
    result = _rin_time_result(_RIN_TIME_TIMER_DELETE(timer_id));
    if (result < 0) return -1;
    if (result != 0) {
        errno = EIO;
        return -1;
    }
#if defined(RIN_USERSPACE)
    if (rin_timer_thread_unregister) rin_timer_thread_unregister(timer_id);
#endif
    return 0;
#else
    (void)result;
    errno = ENOSYS;
    return -1;
#endif
}

/* Return and clear the bounded number of expirations that elapsed before the
 * most recently published periodic notification.  A positive syscall result
 * is a valid count here, unlike the status-only timer operations above. */
static inline int timer_getoverrun(timer_t timer_id) {
    intptr_t result;
    if (timer_id <= 0) {
        errno = EINVAL;
        return -1;
    }
#if defined(RIN_TIME_TARGET_FUNCTIONS)
    result = __rin_syscall_posixize(_RIN_TIME_TIMER_GETOVERRUN(timer_id));
    if (result < 0) return -1;
    if ((uintmax_t)result > (uintmax_t)INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (int)result;
#else
    (void)result;
    errno = ENOSYS;
    return -1;
#endif
}

static inline int nanosleep(const struct timespec* req, struct timespec* rem) {
    unsigned long long total_ms;
    if (!req) {
        errno = EFAULT;
        return -1;
    }
    if (req->tv_sec < 0 || req->tv_nsec < 0 ||
        req->tv_nsec >= 1000000000L) {
        errno = EINVAL;
        return -1;
    }
    if ((unsigned long long)req->tv_sec >
        (~0ULL - 999999ULL) / 1000ULL) {
        errno = EOVERFLOW;
        return -1;
    }

    /* The kernel sleep ABI is millisecond based. Round up so a non-zero
     * request is never reported complete after sleeping for less time. */
    total_ms = (unsigned long long)req->tv_sec * 1000ULL +
               ((unsigned long long)req->tv_nsec + 999999ULL) / 1000000ULL;

    while (total_ms != 0) {
        uintptr_t chunk = total_ms > UINT64_C(0xffffffff)
            ? (uintptr_t)UINT64_C(0xffffffff) : (uintptr_t)total_ms;
#if defined(RIN_FREESTANDING) && !defined(RIN_USERSPACE)
        extern void platform_sleep_ms(unsigned int ms);
        platform_sleep_ms((unsigned int)chunk);
#else
        if (_rin_time_status(_RIN_TIME_SLEEP(chunk)) != 0) {
            if (rem) {
                rem->tv_sec = (time_t)(total_ms / 1000ULL);
                rem->tv_nsec = (long)((total_ms % 1000ULL) * 1000000ULL);
            }
            return -1;
        }
#endif
        total_ms -= (uint64_t)chunk;
    }

    if (rem) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }
    return 0;
}

/* POSIX clock_nanosleep returns an errno value directly rather than using
 * errno as its result channel.  Keep the existing nanosleep owner as the
 * single relative-delay backend, and derive an absolute request from one
 * clock snapshot so a realtime adjustment cannot be observed halfway through
 * the subtraction. */
static inline int clock_nanosleep(clockid_t clock_id, int flags,
                                  const struct timespec* request,
                                  struct timespec* remain) {
    struct timespec now;
    struct timespec relative;
    int saved_errno;
    int result;

    if (!request) return EFAULT;
    if ((flags & ~TIMER_ABSTIME) != 0) return EINVAL;
    if (request->tv_sec < 0 || request->tv_nsec < 0 ||
        request->tv_nsec >= 1000000000L)
        return EINVAL;
    if (clock_id != CLOCK_REALTIME && clock_id != CLOCK_MONOTONIC)
        return EINVAL;

    if ((flags & TIMER_ABSTIME) == 0) {
        result = nanosleep(request, remain);
        if (result == 0) return 0;
        saved_errno = errno;
        return saved_errno != 0 ? saved_errno : EIO;
    }

    if (clock_gettime(clock_id, &now) != 0) {
        saved_errno = errno;
        return saved_errno != 0 ? saved_errno : EIO;
    }
    if (now.tv_sec < 0 || now.tv_nsec < 0 ||
        now.tv_nsec >= 1000000000L)
        return EIO;
    if (now.tv_sec > request->tv_sec ||
        (now.tv_sec == request->tv_sec &&
         now.tv_nsec >= request->tv_nsec)) {
        if (remain) {
            remain->tv_sec = 0;
            remain->tv_nsec = 0;
        }
        return 0;
    }

    relative.tv_sec = request->tv_sec - now.tv_sec;
    if (request->tv_nsec < now.tv_nsec) {
        --relative.tv_sec;
        relative.tv_nsec = (long)(request->tv_nsec + 1000000000L -
                                  now.tv_nsec);
    } else {
        relative.tv_nsec = request->tv_nsec - now.tv_nsec;
    }
    result = nanosleep(&relative, remain);
    if (result == 0) return 0;
    saved_errno = errno;
    return saved_errno != 0 ? saved_errno : EIO;
}

/* ═══════════════════════════════════════════════════════════════
 * strptime - 文字列から時刻への変換 (POSIX)
 * ═══════════════════════════════════════════════════════════════*/

static inline int _strptime_isdigit(int c) {
    return c >= '0' && c <= '9';
}

typedef struct {
    struct tm value;
    RinStrptimeLocale locale;
    unsigned int fields;
    int century;
    int year_two_digits;
    int hour_twelve;
    int has_century;
    int has_year_two_digits;
    int has_meridiem;
    int meridiem_is_pm;
    const char* parsed_zone;
    int week_number;
    int week_starts_monday;
    int iso_year;
    int iso_week;
    int has_iso_year;
    int has_iso_weekday;
} RinStrptimeState;

#if defined(RIN_USERSPACE)
static inline const char* _strptime_locale_weekday(
    RinStrptimeLocale locale, int weekday, int abbreviated) {
    return locale == LC_GLOBAL_LOCALE
               ? rin_locale_weekday(weekday, abbreviated)
               : rin_locale_l_weekday(locale, weekday, abbreviated);
}

static inline const char* _strptime_locale_month(
    RinStrptimeLocale locale, int month, int abbreviated) {
    return locale == LC_GLOBAL_LOCALE
               ? rin_locale_month(month, abbreviated)
               : rin_locale_l_month(locale, month, abbreviated);
}

static inline const char* _strptime_locale_am_pm(
    RinStrptimeLocale locale, int hour) {
    return locale == LC_GLOBAL_LOCALE ? rin_locale_am_pm(hour)
                                      : rin_locale_l_am_pm(locale, hour);
}

static inline const char* _strptime_locale_time_format(
    RinStrptimeLocale locale, char conversion) {
    return locale == LC_GLOBAL_LOCALE
               ? rin_locale_time_format(conversion)
               : rin_locale_l_time_format(locale, conversion);
}
#endif

enum {
    RIN_STRPTIME_YEAR = 1u << 0,
    RIN_STRPTIME_MONTH = 1u << 1,
    RIN_STRPTIME_MDAY = 1u << 2,
    RIN_STRPTIME_HOUR_24 = 1u << 3,
    RIN_STRPTIME_HOUR_12 = 1u << 4,
    RIN_STRPTIME_MINUTE = 1u << 5,
    RIN_STRPTIME_SECOND = 1u << 6,
    RIN_STRPTIME_YDAY = 1u << 7,
    RIN_STRPTIME_WDAY = 1u << 8,
    RIN_STRPTIME_OFFSET = 1u << 9,
    RIN_STRPTIME_ZONE = 1u << 10,
    RIN_STRPTIME_WEEK_NUMBER = 1u << 11,
    RIN_STRPTIME_ISO_WEEK = 1u << 12
};

static inline int _strptime_whitespace(char value) {
    return value == ' ' || value == '\t' || value == '\n' ||
           value == '\r' || value == '\f' || value == '\v';
}

static inline int _strptime_lower(int value) {
    return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value;
}

static inline int _strptime_parse_int(const char** text, int minimum_digits,
                                      int maximum_digits, int minimum,
                                      int maximum, int* result) {
    int value = 0;
    int digits = 0;
    while (_strptime_isdigit(**text) && digits < maximum_digits) {
        value = value * 10 + (**text - '0');
        ++*text;
        ++digits;
    }
    if (digits < minimum_digits || value < minimum || value > maximum)
        return 0;
    *result = value;
    return 1;
}

static inline int _strptime_assign(RinStrptimeState* state,
                                   unsigned int field, int* destination,
                                   int value) {
    if ((state->fields & field) != 0u && *destination != value) return 0;
    state->fields |= field;
    *destination = value;
    return 1;
}

static inline int _strptime_match_name(const char** text,
                                       const char* const* names,
                                       unsigned int count, int* result) {
    unsigned int index;
    for (index = 0u; index < count; ++index) {
        const char* input = *text;
        const char* name = names[index];
        while (*name != '\0' &&
               _strptime_lower((unsigned char)*input) ==
               _strptime_lower((unsigned char)*name)) {
            ++input;
            ++name;
        }
        if (*name == '\0') {
            *text = input;
            *result = (int)index;
            return 1;
        }
    }
    return 0;
}

static inline int _strptime_match_text(const char** text, const char* value) {
    const char* input = *text;
    while (*value != '\0' && *input == *value) {
        ++input;
        ++value;
    }
    if (*value != '\0') return 0;
    *text = input;
    return 1;
}

/* Parse an ISO-8601/RFC-822 numeric UTC offset without narrowing through a
 * host-specific time type.  `%z` accepts the traditional `+hhmm` spelling
 * and its colon form; `%:z` requires the colon.  The parser consumes the
 * complete offset before mutating the destination state so malformed input
 * cannot publish a partial offset. */
static inline int _strptime_parse_numeric_offset(
    const char** text, int require_colon, RinStrptimeState* state) {
    const char* input = *text;
    int sign;
    int hour;
    int minute;
    long offset;
    int has_colon = 0;

    if (*input != '+' && *input != '-') return 0;
    sign = *input++ == '-' ? -1 : 1;
    if (!_strptime_parse_int(&input, 2, 2, 0, 23, &hour)) return 0;
    if (*input == ':') {
        has_colon = 1;
        ++input;
    }
    if (require_colon && !has_colon) return 0;
    if (!_strptime_parse_int(&input, 2, 2, 0, 59, &minute)) return 0;
    offset = (long)sign * (long)(hour * 3600 + minute * 60);
    if ((state->fields & RIN_STRPTIME_OFFSET) != 0u &&
        state->value.tm_gmtoff != offset)
        return 0;
    *text = input;
    state->fields |= RIN_STRPTIME_OFFSET;
    state->value.tm_gmtoff = offset;
    return 1;
}

/* Parse a signed Unix epoch count without relying on the host time_t width.
 * The input cursor is published only after the complete decimal value has
 * passed the int64_t range check. */
static inline int _strptime_parse_epoch_seconds(const char** text,
                                                int64_t* result) {
    const char* input = *text;
    int negative = 0;
    uint64_t magnitude = 0u;
    uint64_t limit;
    int digits = 0;

    if (*input == '+' || *input == '-') {
        negative = *input == '-';
        ++input;
    }
    limit = (uint64_t)INT64_MAX + (negative ? UINT64_C(1) : UINT64_C(0));
    while (_strptime_isdigit(*input)) {
        uint64_t digit = (uint64_t)(*input - '0');
        if (magnitude > (limit - digit) / UINT64_C(10)) return 0;
        magnitude = magnitude * UINT64_C(10) + digit;
        ++input;
        ++digits;
    }
    if (digits == 0) return 0;
    if (negative) {
        if (magnitude == (uint64_t)INT64_MAX + UINT64_C(1))
            *result = INT64_MIN;
        else
            *result = -(int64_t)magnitude;
    } else {
        *result = (int64_t)magnitude;
    }
    *text = input;
    return 1;
}

static inline int _strptime_leap_year(int year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static inline int _strptime_month_days(int year, int month) {
    static const unsigned char days[12] = {
        31u, 28u, 31u, 30u, 31u, 30u,
        31u, 31u, 30u, 31u, 30u, 31u
    };
    return days[month] + (month == 1 && _strptime_leap_year(year));
}

static inline int _strptime_weekday(int year, int month, int day) {
    static const int month_offset[12] = {
        0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4
    };
    if (month < 2) --year;
    return (year + year / 4 - year / 100 + year / 400 +
            month_offset[month] + day) % 7;
}

static inline int _strptime_epoch_to_calendar(int64_t seconds, int* year,
                                              int* month, int* day,
                                              int* hour, int* minute,
                                              int* second, int* yday,
                                              int* wday);

static inline int _strptime_iso_weeks_in_year(int year) {
    int january_first = _strptime_weekday(year, 0, 1);
    return (january_first == 4 ||
            (january_first == 3 && _strptime_leap_year(year))) ? 53 : 52;
}

static inline int _strptime_parse(const char** input_output,
                                  const char* format,
                                  RinStrptimeState* state,
                                  unsigned int depth) {
    static const char* const c_short_months[12] = {
        "jan", "feb", "mar", "apr", "may", "jun",
        "jul", "aug", "sep", "oct", "nov", "dec"
    };
    static const char* const c_long_months[12] = {
        "january", "february", "march", "april", "may", "june",
        "july", "august", "september", "october", "november", "december"
    };
    static const char* const c_short_days[7] = {
        "sun", "mon", "tue", "wed", "thu", "fri", "sat"
    };
    static const char* const c_long_days[7] = {
        "sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"
    };
    static const char* const c_am_pm[2] = { "am", "pm" };
    const char* const* short_months = c_short_months;
    const char* const* long_months = c_long_months;
    const char* const* short_days = c_short_days;
    const char* const* long_days = c_long_days;
    const char* const* am_pm = c_am_pm;
#if defined(RIN_USERSPACE)
    const char* date_time_format;
    const char* date_format;
    const char* time_format;
#else
    const char* date_time_format = "%a %b %e %H:%M:%S %Y";
    const char* date_format = "%m/%d/%y";
    const char* time_format = "%H:%M:%S";
#endif
    const char* input = *input_output;

#if defined(RIN_USERSPACE)
    const char* locale_short_months[12];
    const char* locale_long_months[12];
    const char* locale_short_days[7];
    const char* locale_long_days[7];
    const char* locale_am_pm[2];
    unsigned int locale_index;
    for (locale_index = 0u; locale_index < 12u; ++locale_index) {
        locale_short_months[locale_index] = _strptime_locale_month(
            state->locale, (int)locale_index, 1);
        locale_long_months[locale_index] = _strptime_locale_month(
            state->locale, (int)locale_index, 0);
        if (!locale_short_months[locale_index] ||
            !locale_long_months[locale_index] ||
            locale_short_months[locale_index][0] == '\0' ||
            locale_long_months[locale_index][0] == '\0')
            return 0;
    }
    for (locale_index = 0u; locale_index < 7u; ++locale_index) {
        locale_short_days[locale_index] = _strptime_locale_weekday(
            state->locale, (int)locale_index, 1);
        locale_long_days[locale_index] = _strptime_locale_weekday(
            state->locale, (int)locale_index, 0);
        if (!locale_short_days[locale_index] || !locale_long_days[locale_index] ||
            locale_short_days[locale_index][0] == '\0' ||
            locale_long_days[locale_index][0] == '\0')
            return 0;
    }
    locale_am_pm[0] = _strptime_locale_am_pm(state->locale, 0);
    locale_am_pm[1] = _strptime_locale_am_pm(state->locale, 12);
    date_time_format = _strptime_locale_time_format(state->locale, 'c');
    date_format = _strptime_locale_time_format(state->locale, 'x');
    time_format = _strptime_locale_time_format(state->locale, 'X');
    if (!locale_am_pm[0] || !locale_am_pm[1] ||
        locale_am_pm[0][0] == '\0' || locale_am_pm[1][0] == '\0' ||
        !date_time_format || !date_format || !time_format ||
        date_time_format[0] == '\0' || date_format[0] == '\0' ||
        time_format[0] == '\0')
        return 0;
    short_months = locale_short_months;
    long_months = locale_long_months;
    short_days = locale_short_days;
    long_days = locale_long_days;
    am_pm = locale_am_pm;
#endif

    if (depth > 4u) return 0;
    while (*format != '\0') {
        int parsed;
        char modifier = '\0';
        if (*format != '%') {
            if (_strptime_whitespace(*format)) {
                while (_strptime_whitespace(*input)) ++input;
            } else {
                if (*input != *format) return 0;
                ++input;
            }
            ++format;
            continue;
        }

        ++format;
        if (*format == '\0') return 0;
        if (*format == 'E' || *format == 'O') {
            modifier = *format++;
            if (*format == '\0') return 0;
            if ((modifier == 'E' && *format != 'c' && *format != 'C' &&
                 *format != 'x' && *format != 'X' && *format != 'y' &&
                 *format != 'Y') ||
                (modifier == 'O' && *format != 'd' && *format != 'e' &&
                 *format != 'H' && *format != 'I' && *format != 'm' &&
                 *format != 'M' && *format != 'S' && *format != 'U' &&
                 *format != 'V' && *format != 'u' && *format != 'w' &&
                 *format != 'W' &&
                 *format != 'y'))
                return 0;
        }
        switch (*format) {
            case '%':
                if (*input != '%') return 0;
                ++input;
                break;
            case 'Y':
                if ((state->fields & RIN_STRPTIME_YEAR) != 0u ||
                    state->has_century || state->has_year_two_digits ||
                    !_strptime_parse_int(&input, 1, 4, 0, 9999, &parsed))
                    return 0;
                state->value.tm_year = parsed - 1900;
                state->fields |= RIN_STRPTIME_YEAR;
                break;
            case 'C':
                if ((state->fields & RIN_STRPTIME_YEAR) != 0u ||
                    state->has_century ||
                    !_strptime_parse_int(&input, 1, 2, 0, 99, &parsed))
                    return 0;
                state->century = parsed;
                state->has_century = 1;
                break;
            case 'y':
                if ((state->fields & RIN_STRPTIME_YEAR) != 0u ||
                    state->has_year_two_digits ||
                    !_strptime_parse_int(&input, 1, 2, 0, 99, &parsed))
                    return 0;
                state->year_two_digits = parsed;
                state->has_year_two_digits = 1;
                break;
            case 'm':
                if (!_strptime_parse_int(&input, 1, 2, 1, 12, &parsed) ||
                    !_strptime_assign(state, RIN_STRPTIME_MONTH,
                                      &state->value.tm_mon, parsed - 1))
                    return 0;
                break;
            case 'b':
            case 'h':
                if (!_strptime_match_name(&input, short_months, 12u, &parsed) ||
                    !_strptime_assign(state, RIN_STRPTIME_MONTH,
                                      &state->value.tm_mon, parsed))
                    return 0;
                break;
            case 'B':
                if (!_strptime_match_name(&input, long_months, 12u, &parsed) ||
                    !_strptime_assign(state, RIN_STRPTIME_MONTH,
                                      &state->value.tm_mon, parsed))
                    return 0;
                break;
            case 'd':
                if (!_strptime_parse_int(&input, 1, 2, 1, 31, &parsed) ||
                    !_strptime_assign(state, RIN_STRPTIME_MDAY,
                                      &state->value.tm_mday, parsed))
                    return 0;
                break;
            case 'e':
                if (*input == ' ') ++input;
                if (!_strptime_parse_int(&input, 1, 2, 1, 31, &parsed) ||
                    !_strptime_assign(state, RIN_STRPTIME_MDAY,
                                      &state->value.tm_mday, parsed))
                    return 0;
                break;
            case 'H':
                if ((state->fields & RIN_STRPTIME_HOUR_12) != 0u ||
                    !_strptime_parse_int(&input, 1, 2, 0, 23, &parsed) ||
                    !_strptime_assign(state, RIN_STRPTIME_HOUR_24,
                                      &state->value.tm_hour, parsed))
                    return 0;
                break;
            case 'I':
                if ((state->fields & RIN_STRPTIME_HOUR_24) != 0u ||
                    !_strptime_parse_int(&input, 1, 2, 1, 12, &parsed) ||
                    ((state->fields & RIN_STRPTIME_HOUR_12) != 0u &&
                     state->hour_twelve != parsed))
                    return 0;
                state->fields |= RIN_STRPTIME_HOUR_12;
                state->hour_twelve = parsed;
                break;
            case 'M':
                if (!_strptime_parse_int(&input, 1, 2, 0, 59, &parsed) ||
                    !_strptime_assign(state, RIN_STRPTIME_MINUTE,
                                      &state->value.tm_min, parsed))
                    return 0;
                break;
            case 'S':
                if (!_strptime_parse_int(&input, 1, 2, 0, 60, &parsed) ||
                    !_strptime_assign(state, RIN_STRPTIME_SECOND,
                                      &state->value.tm_sec, parsed))
                    return 0;
                break;
            case 's': {
                int64_t epoch;
                int epoch_year;
                int epoch_month;
                int epoch_mday;
                int epoch_hour;
                int epoch_minute;
                int epoch_second;
                int epoch_yday;
                int epoch_wday;
                if (state->has_century || state->has_year_two_digits ||
                    (state->fields & RIN_STRPTIME_HOUR_12) != 0u ||
                    state->has_meridiem ||
                    !_strptime_parse_epoch_seconds(&input, &epoch) ||
                    !_strptime_epoch_to_calendar(
                        epoch, &epoch_year, &epoch_month, &epoch_mday,
                        &epoch_hour, &epoch_minute, &epoch_second,
                        &epoch_yday, &epoch_wday) ||
                    !_strptime_assign(state, RIN_STRPTIME_YEAR,
                                      &state->value.tm_year,
                                      epoch_year - 1900) ||
                    !_strptime_assign(state, RIN_STRPTIME_MONTH,
                                      &state->value.tm_mon, epoch_month) ||
                    !_strptime_assign(state, RIN_STRPTIME_MDAY,
                                      &state->value.tm_mday, epoch_mday) ||
                    !_strptime_assign(state, RIN_STRPTIME_HOUR_24,
                                      &state->value.tm_hour, epoch_hour) ||
                    !_strptime_assign(state, RIN_STRPTIME_MINUTE,
                                      &state->value.tm_min, epoch_minute) ||
                    !_strptime_assign(state, RIN_STRPTIME_SECOND,
                                      &state->value.tm_sec, epoch_second) ||
                    !_strptime_assign(state, RIN_STRPTIME_YDAY,
                                      &state->value.tm_yday, epoch_yday) ||
                    !_strptime_assign(state, RIN_STRPTIME_WDAY,
                                      &state->value.tm_wday, epoch_wday))
                    return 0;
                break;
            }
            case 'k':
                if (*input == ' ') ++input;
                if ((state->fields & RIN_STRPTIME_HOUR_12) != 0u ||
                    !_strptime_parse_int(&input, 1, 2, 0, 23, &parsed) ||
                    !_strptime_assign(state, RIN_STRPTIME_HOUR_24,
                                      &state->value.tm_hour, parsed))
                    return 0;
                break;
            case 'l':
                if (*input == ' ') ++input;
                if ((state->fields & RIN_STRPTIME_HOUR_24) != 0u ||
                    !_strptime_parse_int(&input, 1, 2, 1, 12, &parsed) ||
                    ((state->fields & RIN_STRPTIME_HOUR_12) != 0u &&
                     state->hour_twelve != parsed))
                    return 0;
                state->fields |= RIN_STRPTIME_HOUR_12;
                state->hour_twelve = parsed;
                break;
            case 'p':
            case 'P':
                if (!_strptime_match_name(&input, am_pm, 2u, &parsed))
                    return 0;
                if (state->has_meridiem && state->meridiem_is_pm != parsed)
                    return 0;
                state->has_meridiem = 1;
                state->meridiem_is_pm = parsed;
                break;
            case 'z': {
                if (!_strptime_parse_numeric_offset(&input, 0, state))
                    return 0;
                break;
            }
            case ':':
                /* `%:z` is the widely deployed GNU spelling for a required
                 * colon.  Do not accept a bare `%:` or arbitrary modifiers. */
                if (format[1] != 'z' ||
                    !_strptime_parse_numeric_offset(&input, 1, state))
                    return 0;
                ++format;
                break;
            case 'Z': {
                const char* zone;
                long offset;
#if defined(RIN_USERSPACE)
                size_t consumed;
                if (!rin_time_zone_match_abbreviation(input, &consumed,
                                                      &offset, &zone))
                    return 0;
                input += consumed;
#else
                if (_strptime_match_text(&input, "UTC")) {
                    zone = "UTC";
                    offset = 0L;
                } else if (_strptime_match_text(&input, "GMT")) {
                    zone = "GMT";
                    offset = 0L;
                } else {
                    return 0;
                }
#endif
                if (((state->fields & RIN_STRPTIME_OFFSET) != 0u &&
                     state->value.tm_gmtoff != offset) ||
                    ((state->fields & RIN_STRPTIME_ZONE) != 0u &&
                     state->parsed_zone != zone))
                    return 0;
                state->fields |= RIN_STRPTIME_OFFSET | RIN_STRPTIME_ZONE;
                state->value.tm_gmtoff = offset;
                state->value.tm_zone = zone;
                state->parsed_zone = zone;
                break;
            }
            case 'U':
            case 'W':
                if (!_strptime_parse_int(&input, 1, 2, 0, 53, &parsed) ||
                    ((state->fields & RIN_STRPTIME_WEEK_NUMBER) != 0u &&
                     (state->week_number != parsed ||
                      state->week_starts_monday != (*format == 'W'))))
                    return 0;
                state->fields |= RIN_STRPTIME_WEEK_NUMBER;
                state->week_number = parsed;
                state->week_starts_monday = *format == 'W';
                break;
            case 'G':
                if (!_strptime_parse_int(&input, 1, 4, 1, 9999, &parsed) ||
                    (state->has_iso_year && state->iso_year != parsed))
                    return 0;
                state->has_iso_year = 1;
                state->iso_year = parsed;
                break;
            case 'g':
                if (!_strptime_parse_int(&input, 1, 2, 0, 99, &parsed))
                    return 0;
                parsed = parsed >= 69 ? 1900 + parsed : 2000 + parsed;
                if (state->has_iso_year && state->iso_year != parsed)
                    return 0;
                state->has_iso_year = 1;
                state->iso_year = parsed;
                break;
            case 'V':
                if (!_strptime_parse_int(&input, 1, 2, 1, 53, &parsed) ||
                    ((state->fields & RIN_STRPTIME_ISO_WEEK) != 0u &&
                     state->iso_week != parsed))
                    return 0;
                state->fields |= RIN_STRPTIME_ISO_WEEK;
                state->iso_week = parsed;
                break;
            case 'j':
                if (!_strptime_parse_int(&input, 1, 3, 1, 366, &parsed) ||
                    !_strptime_assign(state, RIN_STRPTIME_YDAY,
                                      &state->value.tm_yday, parsed - 1))
                    return 0;
                break;
            case 'w':
                if (!_strptime_parse_int(&input, 1, 1, 0, 6, &parsed) ||
                    !_strptime_assign(state, RIN_STRPTIME_WDAY,
                                      &state->value.tm_wday, parsed))
                    return 0;
                break;
            case 'u':
                if (!_strptime_parse_int(&input, 1, 1, 1, 7, &parsed) ||
                    !_strptime_assign(state, RIN_STRPTIME_WDAY,
                                      &state->value.tm_wday,
                                      parsed == 7 ? 0 : parsed))
                    return 0;
                state->has_iso_weekday = 1;
                break;
            case 'a':
                if (!_strptime_match_name(&input, short_days, 7u, &parsed) ||
                    !_strptime_assign(state, RIN_STRPTIME_WDAY,
                                      &state->value.tm_wday, parsed))
                    return 0;
                break;
            case 'A':
                if (!_strptime_match_name(&input, long_days, 7u, &parsed) ||
                    !_strptime_assign(state, RIN_STRPTIME_WDAY,
                                      &state->value.tm_wday, parsed))
                    return 0;
                break;
            case 'n':
            case 't':
                while (_strptime_whitespace(*input)) ++input;
                break;
            case 'D':
                if (!_strptime_parse(&input, "%m/%d/%y", state, depth + 1u))
                    return 0;
                break;
            case 'c':
                if (!_strptime_parse(&input, date_time_format,
                                     state, depth + 1u))
                    return 0;
                break;
            case 'F':
                if (!_strptime_parse(&input, "%Y-%m-%d", state, depth + 1u))
                    return 0;
                break;
            case 'R':
                if (!_strptime_parse(&input, "%H:%M", state, depth + 1u))
                    return 0;
                break;
            case 'T':
                if (!_strptime_parse(&input, "%H:%M:%S", state, depth + 1u))
                    return 0;
                break;
            case 'X':
                if (!_strptime_parse(&input, time_format, state, depth + 1u))
                    return 0;
                break;
            case 'r':
                if (!_strptime_parse(&input, "%I:%M:%S %p", state,
                                     depth + 1u))
                    return 0;
                break;
            case 'x':
                if (!_strptime_parse(&input, date_format, state, depth + 1u))
                    return 0;
                break;
            default:
                return 0;
        }
        ++format;
    }
    *input_output = input;
    return 1;
}

static inline int _strptime_finish(RinStrptimeState* state) {
    int days_in_year;
    int year;
    if (state->has_century || state->has_year_two_digits) {
        if (state->has_century) {
            year = state->century * 100;
            if (state->has_year_two_digits) year += state->year_two_digits;
        } else {
            year = state->year_two_digits >= 69
                ? 1900 + state->year_two_digits
                : 2000 + state->year_two_digits;
        }
        state->value.tm_year = year - 1900;
        state->fields |= RIN_STRPTIME_YEAR;
    }
    if (state->has_meridiem) {
        if ((state->fields & RIN_STRPTIME_HOUR_12) == 0u) return 0;
        state->value.tm_hour = state->hour_twelve % 12 +
                               (state->meridiem_is_pm ? 12 : 0);
    }
    if ((state->fields & RIN_STRPTIME_ISO_WEEK) != 0u) {
        int iso_weekday;
        int january_fourth_weekday;
        int monday_of_week_one;
        int calendar_year;
        int month;
        int yday;
        if (!state->has_iso_year || !state->has_iso_weekday ||
            state->iso_week > _strptime_iso_weeks_in_year(state->iso_year))
            return 0;
        iso_weekday = state->value.tm_wday == 0 ? 7 : state->value.tm_wday;
        january_fourth_weekday = _strptime_weekday(state->iso_year, 0, 4);
        monday_of_week_one = 3 - ((january_fourth_weekday + 6) % 7);
        yday = monday_of_week_one + (state->iso_week - 1) * 7 +
               iso_weekday - 1;
        calendar_year = state->iso_year;
        if (yday < 0) {
            --calendar_year;
            yday += _strptime_leap_year(calendar_year) ? 366 : 365;
        } else if (yday >= (_strptime_leap_year(calendar_year) ? 366 : 365)) {
            yday -= _strptime_leap_year(calendar_year) ? 366 : 365;
            ++calendar_year;
        }
        if (calendar_year < 1 || calendar_year > 9999 ||
            ((state->fields & RIN_STRPTIME_YDAY) != 0u &&
             state->value.tm_yday != yday) ||
            !_strptime_assign(state, RIN_STRPTIME_YEAR,
                              &state->value.tm_year, calendar_year - 1900))
            return 0;
        state->fields |= RIN_STRPTIME_YDAY;
        state->value.tm_yday = yday;
        month = 0;
        while (yday >= _strptime_month_days(calendar_year, month)) {
            yday -= _strptime_month_days(calendar_year, month);
            ++month;
        }
        if (!_strptime_assign(state, RIN_STRPTIME_MONTH,
                              &state->value.tm_mon, month) ||
            !_strptime_assign(state, RIN_STRPTIME_MDAY,
                              &state->value.tm_mday, yday + 1))
            return 0;
    }
    if ((state->fields & RIN_STRPTIME_WEEK_NUMBER) != 0u) {
        int day_of_week;
        int first_weekday;
        int month;
        int yday;
        if ((state->fields & (RIN_STRPTIME_YEAR | RIN_STRPTIME_WDAY)) !=
                (RIN_STRPTIME_YEAR | RIN_STRPTIME_WDAY) ||
            state->value.tm_year < -1899)
            return 0;
        year = state->value.tm_year + 1900;
        days_in_year = _strptime_leap_year(year) ? 366 : 365;
        first_weekday = _strptime_weekday(year, 0, 1);
        if (state->week_starts_monday) {
            int first_monday = (8 - first_weekday) % 7;
            day_of_week = (state->value.tm_wday + 6) % 7;
            if (state->week_number == 0) {
                yday = (state->value.tm_wday - first_weekday + 7) % 7;
                if (yday >= first_monday) return 0;
            } else {
                yday = first_monday + (state->week_number - 1) * 7 +
                       day_of_week;
            }
        } else {
            int first_sunday = (7 - first_weekday) % 7;
            if (state->week_number == 0) {
                yday = state->value.tm_wday - first_weekday;
                if (yday < 0) return 0;
            } else {
                yday = first_sunday + (state->week_number - 1) * 7 +
                       state->value.tm_wday;
            }
        }
        if (yday < 0 || yday >= days_in_year ||
            ((state->fields & RIN_STRPTIME_YDAY) != 0u &&
             state->value.tm_yday != yday))
            return 0;
        state->fields |= RIN_STRPTIME_YDAY;
        state->value.tm_yday = yday;
        month = 0;
        while (yday >= _strptime_month_days(year, month)) {
            yday -= _strptime_month_days(year, month);
            ++month;
        }
        if (!_strptime_assign(state, RIN_STRPTIME_MONTH,
                              &state->value.tm_mon, month) ||
            !_strptime_assign(state, RIN_STRPTIME_MDAY,
                              &state->value.tm_mday, yday + 1))
            return 0;
    }
    if ((state->fields & (RIN_STRPTIME_YEAR | RIN_STRPTIME_MONTH |
                          RIN_STRPTIME_MDAY)) ==
        (RIN_STRPTIME_YEAR | RIN_STRPTIME_MONTH | RIN_STRPTIME_MDAY)) {
        int month;
        int yday = 0;
        year = state->value.tm_year + 1900;
        if (state->value.tm_mday >
            _strptime_month_days(year, state->value.tm_mon))
            return 0;
        for (month = 0; month < state->value.tm_mon; ++month)
            yday += _strptime_month_days(year, month);
        yday += state->value.tm_mday - 1;
        if ((state->fields & RIN_STRPTIME_YDAY) != 0u &&
            state->value.tm_yday != yday)
            return 0;
        if ((state->fields & RIN_STRPTIME_WDAY) != 0u && year >= 1 &&
            state->value.tm_wday != _strptime_weekday(
                year, state->value.tm_mon, state->value.tm_mday))
            return 0;
    }
    if ((state->fields & (RIN_STRPTIME_YEAR | RIN_STRPTIME_YDAY)) ==
        (RIN_STRPTIME_YEAR | RIN_STRPTIME_YDAY) &&
        state->value.tm_yday == 365 &&
        !_strptime_leap_year(state->value.tm_year + 1900))
        return 0;
    return 1;
}

static inline char* strptime(const char* s, const char* format, struct tm* tm) {
    RinStrptimeState state;
    const char* input = s;
    if (!s || !format || !tm) return NULL;
    state.value = *tm;
#if defined(RIN_USERSPACE)
    state.locale = LC_GLOBAL_LOCALE;
#else
    state.locale = NULL;
#endif
    state.fields = 0u;
    state.century = 0;
    state.year_two_digits = 0;
    state.hour_twelve = 0;
    state.has_century = 0;
    state.has_year_two_digits = 0;
    state.has_meridiem = 0;
    state.meridiem_is_pm = 0;
    state.parsed_zone = NULL;
    state.week_number = 0;
    state.week_starts_monday = 0;
    state.iso_year = 0;
    state.iso_week = 0;
    state.has_iso_year = 0;
    state.has_iso_weekday = 0;
    if (!_strptime_parse(&input, format, &state, 0u) ||
        !_strptime_finish(&state))
        return NULL;
    *tm = state.value;
    return (char*)(__UINTPTR_TYPE__)input;
}

#if defined(RIN_USERSPACE)
/* POSIX locale-specific parser.  The locale object is snapshotted by the
 * locale-owned data provider; no process-global LC_TIME mutation occurs. */
static inline char* strptime_l(const char* s, const char* format,
                               struct tm* tm, locale_t locale) {
    RinStrptimeState state;
    const char* input = s;
    if (!s || !format || !tm || !locale) return NULL;
    state.value = *tm;
    state.locale = locale;
    state.fields = 0u;
    state.century = 0;
    state.year_two_digits = 0;
    state.hour_twelve = 0;
    state.has_century = 0;
    state.has_year_two_digits = 0;
    state.has_meridiem = 0;
    state.meridiem_is_pm = 0;
    state.parsed_zone = NULL;
    state.week_number = 0;
    state.week_starts_monday = 0;
    state.iso_year = 0;
    state.iso_week = 0;
    state.has_iso_year = 0;
    state.has_iso_weekday = 0;
    if (!_strptime_parse(&input, format, &state, 0u) ||
        !_strptime_finish(&state))
        return NULL;
    *tm = state.value;
    return (char*)(__UINTPTR_TYPE__)input;
}
#endif

/* Convert a Unix epoch count to a proleptic Gregorian UTC calendar value.
 * Rin's strict parser deliberately bounds the result to the representable
 * year range used by its tm validation (0001..9999). */
static inline int _strptime_epoch_to_calendar(int64_t seconds, int* year,
                                              int* month, int* day,
                                              int* hour, int* minute,
                                              int* second, int* yday,
                                              int* wday) {
    const int64_t minimum = -INT64_C(62135596800);
    const int64_t maximum = INT64_C(253402300799);
    int64_t days;
    int64_t remainder;
    int64_t z;
    int64_t era;
    int64_t day_of_era;
    int64_t year_of_era;
    int64_t day_of_year;
    int64_t month_part;
    int computed_year;
    int computed_month;
    int computed_day;
    int index;

    if (seconds < minimum || seconds > maximum) return 0;
    days = seconds / INT64_C(86400);
    remainder = seconds % INT64_C(86400);
    if (remainder < 0) {
        remainder += INT64_C(86400);
        --days;
    }
    z = days + INT64_C(719468);
    era = (z >= 0 ? z : z - INT64_C(146096)) / INT64_C(146097);
    day_of_era = z - era * INT64_C(146097);
    year_of_era = (day_of_era - day_of_era / 1460 +
                   day_of_era / 36524 - day_of_era / 146096) / 365;
    computed_year = (int)(year_of_era + era * 400);
    day_of_year = day_of_era -
                  (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    month_part = (5 * day_of_year + 2) / 153;
    computed_day = (int)(day_of_year - (153 * month_part + 2) / 5 + 1);
    computed_month = (int)(month_part + (month_part < 10 ? 3 : -9));
    computed_year += computed_month <= 2;
    if (computed_year < 1 || computed_year > 9999) return 0;
    *year = computed_year;
    *month = computed_month - 1;
    *day = computed_day;
    *hour = (int)(remainder / 3600);
    remainder %= 3600;
    *minute = (int)(remainder / 60);
    *second = (int)(remainder % 60);
    *yday = 0;
    for (index = 0; index < *month; ++index)
        *yday += _strptime_month_days(*year, index);
    *yday += *day - 1;
    *wday = _strptime_weekday(*year, *month, *day);
    return 1;
}
#endif /* RIN_TIME_TARGET_FUNCTIONS */

#ifdef __cplusplus
}
#endif

#undef RIN_TIME_TARGET_FUNCTIONS
#undef RIN_TIME_CUSTOM_SYSCALL_HOOK

#endif /* _TIME_H */
