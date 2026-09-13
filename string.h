/*
 * RinOS libc - string.h
 * 文字列・メモリ操作
 */

#ifndef _STRING_H
#define _STRING_H

#include "stddef.h"
#if defined(__cplusplus) && !defined(RIN_FREESTANDING) && \
    defined(__STDC_HOSTED__) && __STDC_HOSTED__
#include <errno.h>
#else
#include "errno.h"
#endif
#include "limits.h"
#include "internal/rin_memory_fast.h"

#ifdef __cplusplus
extern "C" {
#define RIN_STRING_INLINE inline
#define RIN_STRING_THREAD_LOCAL thread_local
#else
#define RIN_STRING_INLINE static inline
#define RIN_STRING_THREAD_LOCAL _Thread_local
#endif

/* ═══════════════════════════════════════════════════════════════
 * メモリ操作
 * rin.hでマクロ定義済みの場合はスキップ
 * ═══════════════════════════════════════════════════════════════*/

#ifndef memcpy
RIN_STRING_INLINE void* memcpy(void* dest, const void* src, size_t n) {
    return rin_memory_copy_fast(dest, src, n);
}

RIN_STRING_INLINE void* memmove(void* dest, const void* src, size_t n) {
    return rin_memory_move_fast(dest, src, n);
}

RIN_STRING_INLINE void* memset(void* s, int c, size_t n) {
    return rin_memory_set_fast(s, c, n);
}

RIN_STRING_INLINE int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++; p2++;
    }
    return 0;
}

RIN_STRING_INLINE void* memchr(const void* s, int c, size_t n) {
    const unsigned char* p = (const unsigned char*)s;
    while (n--) {
        if (*p == (unsigned char)c) return (void*)(__UINTPTR_TYPE__)p;
        p++;
    }
    return NULL;
}
#endif /* memcpy */

/* ═══════════════════════════════════════════════════════════════
 * 文字列操作
 * rin.hでマクロ定義済みの場合はスキップ
 * ═══════════════════════════════════════════════════════════════*/

#ifndef strlen
RIN_STRING_INLINE size_t strlen(const char* s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

RIN_STRING_INLINE char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

RIN_STRING_INLINE char* strncpy(char* dest, const char* src, size_t n) {
    char* d = dest;
    while (n && (*d++ = *src++)) n--;
    while (n--) *d++ = '\0';
    return dest;
}

RIN_STRING_INLINE char* strcat(char* dest, const char* src) {
    char* d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

RIN_STRING_INLINE char* strncat(char* dest, const char* src, size_t n) {
    char* d = dest;
    while (*d) d++;
    while (n-- && (*d++ = *src++));
    *d = '\0';
    return dest;
}

RIN_STRING_INLINE int strcmp(const char* s1, const char* s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

RIN_STRING_INLINE int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && *s1 == *s2) { s1++; s2++; n--; }
    return n ? *(const unsigned char*)s1 - *(const unsigned char*)s2 : 0;
}

RIN_STRING_INLINE char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) return (char*)(__UINTPTR_TYPE__)s;
        s++;
    }
    return (c == '\0') ? (char*)(__UINTPTR_TYPE__)s : NULL;
}

RIN_STRING_INLINE char* strrchr(const char* s, int c) {
    const char* last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    return (c == '\0') ? (char*)(__UINTPTR_TYPE__)s : (char*)(__UINTPTR_TYPE__)last;
}

RIN_STRING_INLINE char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)(__UINTPTR_TYPE__)haystack;
    for (; *haystack; haystack++) {
        const char* h = haystack;
        const char* n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char*)(__UINTPTR_TYPE__)haystack;
    }
    return NULL;
}

RIN_STRING_INLINE size_t strspn(const char* s, const char* accept) {
    size_t count = 0;
    while (*s) {
        const char* a = accept;
        int found = 0;
        while (*a) { if (*s == *a++) { found = 1; break; } }
        if (!found) break;
        s++; count++;
    }
    return count;
}

RIN_STRING_INLINE size_t strcspn(const char* s, const char* reject) {
    size_t count = 0;
    while (*s) {
        const char* r = reject;
        while (*r) { if (*s == *r++) return count; }
        s++; count++;
    }
    return count;
}

RIN_STRING_INLINE char* strpbrk(const char* s, const char* accept) {
    while (*s) {
        const char* a = accept;
        while (*a) { if (*s == *a++) return (char*)(__UINTPTR_TYPE__)s; }
        s++;
    }
    return NULL;
}

extern RIN_STRING_THREAD_LOCAL char* _rin_strtok_last;

RIN_STRING_INLINE char* strtok(char* str, const char* delim) {
    if (str) _rin_strtok_last = str;
    if (!_rin_strtok_last) return NULL;

    /* 先頭のデリミタをスキップ */
    while (*_rin_strtok_last) {
        const char* d = delim;
        int is_delim = 0;
        while (*d) {
            if (*_rin_strtok_last == *d++) { is_delim = 1; break; }
        }
        if (!is_delim) break;
        _rin_strtok_last++;
    }

    if (!*_rin_strtok_last) {
        _rin_strtok_last = NULL;
        return NULL;
    }

    char* start = _rin_strtok_last;

    /* トークンの終端を探す */
    while (*_rin_strtok_last) {
        const char* d = delim;
        while (*d) {
            if (*_rin_strtok_last == *d++) {
                *_rin_strtok_last++ = '\0';
                return start;
            }
        }
        _rin_strtok_last++;
    }

    _rin_strtok_last = NULL;
    return start;
}

/* Caller-owned reentrant tokenizer.  The save pointer is the only mutable
 * state, so two parsers can be interleaved without touching the process-wide
 * compatibility cursor used by strtok(). */
RIN_STRING_INLINE char* strtok_r(char* str, const char* delim,
                                char** saveptr) {
    char* cursor;
    char* start;

    if (!delim || !saveptr) {
        errno = EINVAL;
        return NULL;
    }
    cursor = str ? str : *saveptr;
    if (!cursor) return NULL;

    while (*cursor) {
        const char* d = delim;
        int is_delim = 0;
        while (*d) {
            if (*cursor == *d++) { is_delim = 1; break; }
        }
        if (!is_delim) break;
        cursor++;
    }
    if (!*cursor) {
        *saveptr = NULL;
        return NULL;
    }

    start = cursor;
    while (*cursor) {
        const char* d = delim;
        while (*d) {
            if (*cursor == *d++) {
                *cursor++ = '\0';
                *saveptr = cursor;
                return start;
            }
        }
        cursor++;
    }
    *saveptr = NULL;
    return start;
}
#endif /* strlen */

/* ═══════════════════════════════════════════════════════════════
 * 文字列複製 (POSIX)
 * Note: strdup/strndup are defined in stdlib.h after malloc is defined
 * ═══════════════════════════════════════════════════════════════*/

/* ═══════════════════════════════════════════════════════════════
 * ロケール依存文字列比較 (カーネルlocale.c連携)
 * ═══════════════════════════════════════════════════════════════*/

/* カーネルAPI宣言 */
int rin_strcoll(const char* s1, const char* s2);
size_t rin_strxfrm(char* dest, const char* src, size_t n);

/* strcoll - ロケール依存文字列比較 */
RIN_STRING_INLINE int strcoll(const char* s1, const char* s2) {
    return rin_strcoll(s1, s2);
}

/* strxfrm - 照合用文字列変換 */
RIN_STRING_INLINE size_t strxfrm(char* dest, const char* src, size_t n) {
    return rin_strxfrm(dest, src, n);
}

/* ═══════════════════════════════════════════════════════════════
 * エラーメッセージ
 * ═══════════════════════════════════════════════════════════════*/

static inline const char* _rin_strerror_known_message(int errnum) {
    switch (errnum) {
        case 0:  return "Success";
        case 1:  return "Operation not permitted";
        case 2:  return "No such file or directory";
        case 3:  return "No such process";
        case 4:  return "Interrupted system call";
        case 5:  return "I/O error";
        case 6:  return "No such device or address";
        case 7:  return "Argument list too long";
        case 8:  return "Exec format error";
        case 9:  return "Bad file descriptor";
        case 10: return "No child processes";
        case 11: return "Resource temporarily unavailable";
        case 12: return "Out of memory";
        case 13: return "Permission denied";
        case 14: return "Bad address";
        case 15: return "Block device required";
        case 16: return "Device or resource busy";
        case 17: return "File exists";
        case 18: return "Cross-device link";
        case 19: return "No such device";
        case 20: return "Not a directory";
        case 21: return "Is a directory";
        case 22: return "Invalid argument";
        case 23: return "File table overflow";
        case 24: return "Too many open files";
        case 25: return "Inappropriate ioctl for device";
        case 26: return "Text file busy";
        case 27: return "File too large";
        case 28: return "No space left on device";
        case 29: return "Illegal seek";
        case 30: return "Read-only file system";
        case 31: return "Too many links";
        case 32: return "Broken pipe";
        case 33: return "Mathematics argument out of domain of function";
        case 34: return "Numerical result out of range";
        case 35: return "Resource deadlock would occur";
        case 36: return "File name too long";
        case 37: return "No locks available";
        case 38: return "Function not implemented";
        case 39: return "Directory not empty";
        case 40: return "Too many levels of symbolic links";
        case 42: return "No message of desired type";
        case 43: return "Identifier removed";
        case 44: return "Channel number out of range";
        case 45: return "Level 2 not synchronized";
        case 46: return "Level 3 halted";
        case 47: return "Level 3 reset";
        case 48: return "Link number out of range";
        case 49: return "Protocol driver not attached";
        case 50: return "No CSI structure available";
        case 51: return "Level 2 halted";
        case 52: return "Invalid exchange";
        case 53: return "Invalid request descriptor";
        case 54: return "Exchange full";
        case 55: return "No anode";
        case 56: return "Invalid request code";
        case 57: return "Invalid slot";
        case 59: return "Bad font file format";
        case 60: return "Device not a stream";
        case 61: return "No data available";
        case 62: return "Timer expired";
        case 63: return "Out of streams resources";
        case 64: return "Machine is not on the network";
        case 65: return "Package not installed";
        case 66: return "Object is remote";
        case 67: return "Link has been severed";
        case 68: return "Advertise error";
        case 69: return "Srmount error";
        case 70: return "Communication error on send";
        case 71: return "Protocol error";
        case 74: return "Multihop attempted";
        case 76: return "RFS specific error";
        case 77: return "Bad message";
        case 78: return "Value too large for defined data type";
        case 80: return "Name not unique on network";
        case 81: return "File descriptor in bad state";
        case 82: return "Remote address changed";
        case 83: return "Can not access a needed shared library";
        case 84: return "Accessing a corrupted shared library";
        case 85: return ".lib section in a.out corrupted";
        case 86: return "Attempting to link in too many shared libraries";
        case 87: return "Cannot exec a shared library directly";
        case 88: return "Illegal byte sequence";
        case 89: return "Interrupted system call should be restarted";
        case 90: return "Streams pipe error";
        case 91: return "Too many users";
        case 116: return "Stale file handle";
        case 117: return "Structure needs cleaning";
        case 118: return "Not a XENIX named type file";
        case 119: return "No XENIX semaphores available";
        case 120: return "Is a named type file";
        case 121: return "Remote I/O error";
        case 122: return "Quota exceeded";
        case 123: return "No medium found";
        case 124: return "Wrong medium type";
        case 125: return "Operation canceled";
        case 126: return "Required key not available";
        case 127: return "Key has expired";
        case 128: return "Key has been revoked";
        case 129: return "Key was rejected by service";
        case 130: return "Owner died";
        case 131: return "State not recoverable";
        case 200: return "Socket operation on non-socket";
        case 201: return "Destination address required";
        case 202: return "Message too long";
        case 203: return "Protocol wrong type for socket";
        case 204: return "Protocol not available";
        case 205: return "Protocol not supported";
        case 206: return "Socket type not supported";
        case 207: return "Operation not supported";
        case 208: return "Protocol family not supported";
        case 209: return "Address family not supported by protocol";
        case 210: return "Address already in use";
        case 211: return "Cannot assign requested address";
        case 212: return "Network is down";
        case 213: return "Network is unreachable";
        case 214: return "Network dropped connection on reset";
        case 215: return "Software caused connection abort";
        case 216: return "Connection reset by peer";
        case 217: return "No buffer space available";
        case 218: return "Transport endpoint is already connected";
        case 219: return "Transport endpoint is not connected";
        case 220: return "Cannot send after transport endpoint shutdown";
        case 221: return "Too many references";
        case 222: return "Connection timed out";
        case 223: return "Connection refused";
        case 224: return "Host is down";
        case 225: return "No route to host";
        case 226: return "Operation already in progress";
        case 227: return "Operation now in progress";
        default:
            return NULL;
    }
}

#define _RIN_STRERROR_UNKNOWN_MESSAGE_CAPACITY \
    (sizeof("Error ") + 1u + sizeof(unsigned int) * CHAR_BIT)

static inline size_t _rin_strerror_unknown_message(int errnum, char* buffer) {
    static const char prefix[] = "Error ";
    char reverse_digits[sizeof(unsigned int) * CHAR_BIT];
    unsigned int magnitude;
    size_t output = 0u;
    size_t digits = 0u;
    size_t index;

    for (index = 0u; index < sizeof(prefix) - 1u; ++index)
        buffer[output++] = prefix[index];
    if (errnum < 0) {
        buffer[output++] = '-';
        magnitude = 0u - (unsigned int)errnum;
    } else {
        magnitude = (unsigned int)errnum;
    }
    do {
        reverse_digits[digits++] = (char)('0' + magnitude % 10u);
        magnitude /= 10u;
    } while (magnitude != 0u);
    while (digits != 0u)
        buffer[output++] = reverse_digits[--digits];
    buffer[output] = '\0';
    return output;
}

RIN_STRING_INLINE const char* strerror(int errnum) {
    const char* known = _rin_strerror_known_message(errnum);
    /* The returned pointer remains valid until this thread's next strerror
     * call.  A process-global buffer would let a concurrent lookup rewrite a
     * caller's still-live diagnostic. */
#if defined(__cplusplus)
#define RIN_STRING_THREAD_LOCAL thread_local
#else
#define RIN_STRING_THREAD_LOCAL _Thread_local
#endif
    static RIN_STRING_THREAD_LOCAL char
        unknown[_RIN_STRERROR_UNKNOWN_MESSAGE_CAPACITY];
#undef RIN_STRING_THREAD_LOCAL

    if (known) return known;
    (void)_rin_strerror_unknown_message(errnum, unknown);
    return unknown;
}

/* POSIX/XSI strerror_r: caller-owned output and an explicit ERANGE result. */
RIN_STRING_INLINE int strerror_r(int errnum, char* buf, size_t buflen) {
    const char* message = _rin_strerror_known_message(errnum);
    char unknown[_RIN_STRERROR_UNKNOWN_MESSAGE_CAPACITY];
    size_t length;
    size_t copied;

    if (!buf) return EINVAL;
    if (message) {
        length = 0u;
        while (message[length]) ++length;
    } else {
        length = _rin_strerror_unknown_message(errnum, unknown);
        message = unknown;
    }
    if (buflen == 0u) return ERANGE;
    copied = length;
    if (copied >= buflen) copied = buflen - 1u;
    for (size_t index = 0u; index < copied; ++index) buf[index] = message[index];
    buf[copied] = '\0';
    return copied == length ? 0 : ERANGE;
}

#ifdef __cplusplus
}
#endif

#include "fortify.h"

#undef RIN_STRING_INLINE
#undef RIN_STRING_THREAD_LOCAL

#endif /* _STRING_H */
