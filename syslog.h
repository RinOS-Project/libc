/*
 * RinOS libc - syslog.h
 * システムログ
 */

#ifndef _SYSLOG_H
#define _SYSLOG_H

#include "errno.h"
#include "limits.h"
#include "stddef.h"
#include "stdint.h"
#include "stdarg.h"
#include "stdio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * 優先度レベル (severity)
 * ═══════════════════════════════════════════════════════════════*/

#define LOG_EMERG   0   /* システム使用不能 */
#define LOG_ALERT   1   /* 即座に対処が必要 */
#define LOG_CRIT    2   /* 致命的状態 */
#define LOG_ERR     3   /* エラー状態 */
#define LOG_WARNING 4   /* 警告状態 */
#define LOG_NOTICE  5   /* 正常だが重要 */
#define LOG_INFO    6   /* 情報メッセージ */
#define LOG_DEBUG   7   /* デバッグメッセージ */

/* 優先度マスク */
#define LOG_PRIMASK 0x07

#define LOG_PRI(p)    ((p) & LOG_PRIMASK)
#define LOG_MAKEPRI(fac, pri) (((fac) << 3) | (pri))

/* ═══════════════════════════════════════════════════════════════
 * ファシリティ (facility)
 * ═══════════════════════════════════════════════════════════════*/

#define LOG_KERN     (0 << 3)   /* カーネルメッセージ */
#define LOG_USER     (1 << 3)   /* ユーザーレベルメッセージ */
#define LOG_MAIL     (2 << 3)   /* メールシステム */
#define LOG_DAEMON   (3 << 3)   /* システムデーモン */
#define LOG_AUTH     (4 << 3)   /* セキュリティ/認証 */
#define LOG_SYSLOG   (5 << 3)   /* syslogd内部 */
#define LOG_LPR      (6 << 3)   /* 印刷システム */
#define LOG_NEWS     (7 << 3)   /* ネットニュース */
#define LOG_UUCP     (8 << 3)   /* UUCP */
#define LOG_CRON     (9 << 3)   /* クロックデーモン */
#define LOG_AUTHPRIV (10 << 3)  /* セキュリティ/認証 (プライベート) */
#define LOG_FTP      (11 << 3)  /* FTPデーモン */

/* ローカル用 */
#define LOG_LOCAL0   (16 << 3)
#define LOG_LOCAL1   (17 << 3)
#define LOG_LOCAL2   (18 << 3)
#define LOG_LOCAL3   (19 << 3)
#define LOG_LOCAL4   (20 << 3)
#define LOG_LOCAL5   (21 << 3)
#define LOG_LOCAL6   (22 << 3)
#define LOG_LOCAL7   (23 << 3)

#define LOG_NFACILITIES 24
#define LOG_FACMASK  0x03F8

#define LOG_FAC(p)   (((p) & LOG_FACMASK) >> 3)

/* ═══════════════════════════════════════════════════════════════
 * openlogオプション
 * ═══════════════════════════════════════════════════════════════*/

#define LOG_PID     0x01  /* PIDを記録 */
#define LOG_CONS    0x02  /* コンソールにも出力 */
#define LOG_ODELAY  0x04  /* 接続を遅延 */
#define LOG_NDELAY  0x08  /* 即座に接続 */
#define LOG_NOWAIT  0x10  /* 子プロセスを待たない */
#define LOG_PERROR  0x20  /* stderrにも出力 */

/* ═══════════════════════════════════════════════════════════════
 * ログマスク
 * ═══════════════════════════════════════════════════════════════*/

#define LOG_MASK(pri)  (1 << (pri))
#define LOG_UPTO(pri)  ((1 << ((pri) + 1)) - 1)

/* ═══════════════════════════════════════════════════════════════
 * 内部状態
 * ═══════════════════════════════════════════════════════════════*/

/* syslog configuration is process-owned.  Defining it in this header made
 * openlog() in one translation unit invisible to syslog() in another. */
extern const char* _syslog_ident;
extern int _syslog_option;
extern int _syslog_facility;
extern int _syslog_mask;

#ifndef _RIN_SYSLOG_SYSCALL0
#define _RIN_SYSLOG_SYSCALL0(number) _syscall0((uintptr_t)(number))
#endif
#ifndef _RIN_SYSLOG_SYSCALL3
#define _RIN_SYSLOG_SYSCALL3(number, arg1, arg2, arg3) \
    _syscall3((uintptr_t)(number), (uintptr_t)(arg1), \
              (uintptr_t)(arg2), (uintptr_t)(arg3))
#endif

/* ═══════════════════════════════════════════════════════════════
 * syslog関数
 * ═══════════════════════════════════════════════════════════════*/

/* openlog - syslog接続を開く */
static inline void openlog(const char* ident, int option, int facility) {
    _syslog_ident = ident;
    _syslog_option = option;
    _syslog_facility = facility;
}

/* closelog - syslog接続を閉じる */
static inline void closelog(void) {
    _syslog_ident = NULL;
    _syslog_option = 0;
    _syslog_facility = LOG_USER;
}

/* setlogmask - ログマスクを設定 */
static inline int setlogmask(int mask) {
    int old = _syslog_mask;
    if (mask != 0) {
        _syslog_mask = mask;
    }
    return old;
}

static inline void _syslog_write_stderr(const char* message, size_t length) {
    size_t offset = 0;
    while (offset < length) {
        size_t remaining = length - offset;
        intptr_t written = _RIN_SYSLOG_SYSCALL3(
            SYS_WRITE, 2, (uintptr_t)(message + offset),
            (uintptr_t)remaining);
        if (written == -EINTR) continue;
        if (written <= 0) break;
        if ((uintptr_t)written > (uintptr_t)remaining) break;
        offset += (size_t)(uintptr_t)written;
    }
}

/* LOG_PID is advisory formatting, but its getpid result still crosses a
 * target-width boundary.  Keep logging from publishing a negative errno or
 * truncating a wide PID, and preserve the caller's errno because syslog is a
 * void API. */
static inline int _rin_syslog_pid_value(intptr_t raw_result, int* output) {
    int saved_errno = errno;
    intptr_t result = raw_result;
    errno = saved_errno;
    if (result < 0 || result > (intptr_t)INT_MAX || !output) return -1;
    *output = (int)result;
    return 0;
}

/* vsyslog - va_list版syslog。ログサービスが未接続でも、メッセージを
 * 捨てずにstderrへ有界出力する。 */
static inline void vsyslog(int priority, const char* format, va_list ap) {
    int pri = LOG_PRI(priority);
    int effective_priority = priority;
    int saved_errno = errno;
    char message[1024];
    size_t used = 0;

    /* マスクチェック */
    if (!(LOG_MASK(pri) & _syslog_mask)) {
        return;
    }

    static const char* level_names[] = {
        "EMERG", "ALERT", "CRIT", "ERR",
        "WARNING", "NOTICE", "INFO", "DEBUG"
    };
    if ((effective_priority & LOG_FACMASK) == 0)
        effective_priority |= _syslog_facility;

    int prefix = snprintf(message, sizeof(message), "<%d>[%s]%s%s",
                          effective_priority & (LOG_FACMASK | LOG_PRIMASK),
                          level_names[pri],
                          _syslog_ident ? " " : "",
                          _syslog_ident ? _syslog_ident : "");
    if (prefix < 0) {
        errno = saved_errno;
        return;
    }
    used = (size_t)prefix;
    if (used >= sizeof(message)) used = sizeof(message) - 1;

    if ((_syslog_option & LOG_PID) != 0 && used < sizeof(message) - 1) {
        int pid;
        if (_rin_syslog_pid_value(_RIN_SYSLOG_SYSCALL0(SYS_GETPID), &pid) == 0) {
            int count = snprintf(message + used, sizeof(message) - used,
                                 "[%d]", pid);
            if (count > 0) {
                size_t appended = (size_t)count;
                size_t available = sizeof(message) - used - 1;
                used += appended < available ? appended : available;
            }
        }
    }
    if (used < sizeof(message) - 1) message[used++] = ':';
    if (used < sizeof(message) - 1) message[used++] = ' ';

    if (used < sizeof(message) - 1) {
        int count = vsnprintf(message + used, sizeof(message) - used,
                              format ? format : "(null)", ap);
        if (count > 0) {
            size_t appended = (size_t)count;
            size_t available = sizeof(message) - used - 1;
            used += appended < available ? appended : available;
        }
    }
    if (used == 0 || message[used - 1] != '\n') {
        if (used < sizeof(message) - 1) message[used++] = '\n';
        else message[sizeof(message) - 2] = '\n';
    }
    message[used] = '\0';
    _syslog_write_stderr(message, used);
    errno = saved_errno;
}

/* syslog - ログメッセージを出力 */
static inline void syslog(int priority, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    vsyslog(priority, format, ap);
    va_end(ap);
}

#ifdef __cplusplus
}
#endif

#endif /* _SYSLOG_H */
