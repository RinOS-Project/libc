/*
 * RinOS libc - arpa/inet.h
 * インターネットアドレス操作
 */

#ifndef _ARPA_INET_H
#define _ARPA_INET_H

#include "../errno.h"
#include "../sys/socket.h"
#include "../netinet/in.h"
#include <rin/net/ipv6_text.h>

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * バイトオーダー変換 (sys/socket.hで定義済み)
 * ═══════════════════════════════════════════════════════════════*/

/* htons, htonl, ntohs, ntohl は sys/socket.h で定義 */

/* ═══════════════════════════════════════════════════════════════
 * アドレス変換関数
 * ═══════════════════════════════════════════════════════════════*/

static inline int _inet_parse_legacy_ipv4(const char* text,
                                           in_addr_t* address_out) {
    unsigned int part[4] = {0u, 0u, 0u, 0u};
    unsigned int count = 0u;
    const char* cursor = text;
    unsigned int address;

    if (!text || !address_out) return 0;
    for (;;) {
        unsigned int value = 0u;
        unsigned int base = 10u;
        unsigned int digits = 0u;
        if (count >= 4u || *cursor < '0' || *cursor > '9') return 0;
        if (cursor[0] == '0') {
            if (cursor[1] == 'x' || cursor[1] == 'X') {
                base = 16u;
                cursor += 2;
            } else {
                base = 8u;
            }
        }
        for (;;) {
            unsigned int digit;
            if (*cursor >= '0' && *cursor <= '9') {
                digit = (unsigned int)(*cursor - '0');
            } else if (base == 16u && *cursor >= 'a' && *cursor <= 'f') {
                digit = (unsigned int)(*cursor - 'a') + 10u;
            } else if (base == 16u && *cursor >= 'A' && *cursor <= 'F') {
                digit = (unsigned int)(*cursor - 'A') + 10u;
            } else {
                break;
            }
            if (digit >= base || value > (0xffffffffu - digit) / base)
                return 0;
            value = value * base + digit;
            ++digits;
            ++cursor;
        }
        if (digits == 0u) return 0;
        part[count++] = value;
        if (*cursor == '\0') break;
        if (*cursor++ != '.') return 0;
    }

    switch (count) {
    case 1u:
        address = part[0];
        break;
    case 2u:
        if (part[0] > 0xffu || part[1] > 0xffffffu) return 0;
        address = (part[0] << 24) | part[1];
        break;
    case 3u:
        if (part[0] > 0xffu || part[1] > 0xffu || part[2] > 0xffffu)
            return 0;
        address = (part[0] << 24) | (part[1] << 16) | part[2];
        break;
    case 4u:
        if (part[0] > 0xffu || part[1] > 0xffu ||
            part[2] > 0xffu || part[3] > 0xffu)
            return 0;
        address = (part[0] << 24) | (part[1] << 16) |
                  (part[2] << 8) | part[3];
        break;
    default:
        return 0;
    }

    *address_out = htonl(address);
    return 1;
}

/* inet_addr - ドット形式文字列をネットワークバイトオーダーに変換 */
static inline in_addr_t inet_addr(const char* cp) {
    in_addr_t address;
    return _inet_parse_legacy_ipv4(cp, &address) ? address : INADDR_NONE;
}

/* inet_aton - inet_addrと同様だが成功/失敗を返す */
static inline int inet_aton(const char* cp, struct in_addr* inp) {
    in_addr_t address;
    if (!_inet_parse_legacy_ipv4(cp, &address)) return 0;
    if (inp) inp->s_addr = address;
    return 1;
}

/* inet_ntoa - ネットワークバイトオーダーをドット形式文字列に変換 */
static inline char* inet_ntoa(struct in_addr in) {
    /* POSIX permits the returned text to be overwritten by this thread's
     * next conversion, but a process-global buffer would let another thread
     * rewrite a still-live result. */
#if defined(__cplusplus)
#define RIN_INET_THREAD_LOCAL thread_local
#else
#define RIN_INET_THREAD_LOCAL _Thread_local
#endif
    static RIN_INET_THREAD_LOCAL char buf[16];
#undef RIN_INET_THREAD_LOCAL
    unsigned int addr = ntohl(in.s_addr);
    unsigned int a = (addr >> 24) & 0xFF;
    unsigned int b = (addr >> 16) & 0xFF;
    unsigned int c = (addr >> 8) & 0xFF;
    unsigned int d = addr & 0xFF;

    char* p = buf;

    /* a */
    if (a >= 100) { *p++ = '0' + a / 100; a %= 100; *p++ = '0' + a / 10; a %= 10; }
    else if (a >= 10) { *p++ = '0' + a / 10; a %= 10; }
    *p++ = '0' + a;
    *p++ = '.';

    /* b */
    if (b >= 100) { *p++ = '0' + b / 100; b %= 100; *p++ = '0' + b / 10; b %= 10; }
    else if (b >= 10) { *p++ = '0' + b / 10; b %= 10; }
    *p++ = '0' + b;
    *p++ = '.';

    /* c */
    if (c >= 100) { *p++ = '0' + c / 100; c %= 100; *p++ = '0' + c / 10; c %= 10; }
    else if (c >= 10) { *p++ = '0' + c / 10; c %= 10; }
    *p++ = '0' + c;
    *p++ = '.';

    /* d */
    if (d >= 100) { *p++ = '0' + d / 100; d %= 100; *p++ = '0' + d / 10; d %= 10; }
    else if (d >= 10) { *p++ = '0' + d / 10; d %= 10; }
    *p++ = '0' + d;

    *p = '\0';
    return buf;
}

static inline int _inet_pton_ipv4_strict(const char* src, struct in_addr* addr) {
    unsigned int octets[4] = {0u, 0u, 0u, 0u};
    unsigned int part = 0u;
    unsigned int value = 0u;
    unsigned int digits = 0u;
    int leading_zero = 0;
    const char* cursor;
    if (!src || !addr) return 0;
    for (cursor = src; ; ++cursor) {
        char character = *cursor;
        if (character >= '0' && character <= '9') {
            if (digits >= 3u || (digits != 0u && leading_zero)) return 0;
            if (digits == 0u) leading_zero = character == '0';
            value = value * 10u + (unsigned int)(character - '0');
            if (value > 255u) return 0;
            ++digits;
            continue;
        }
        if ((character != '.' && character != '\0') || digits == 0u ||
            part >= 4u) {
            return 0;
        }
        octets[part++] = value;
        value = 0u;
        digits = 0u;
        leading_zero = 0;
        if (character == '\0') break;
    }
    if (part != 4u) return 0;
    addr->s_addr = htonl((octets[0] << 24) | (octets[1] << 16) |
                         (octets[2] << 8) | octets[3]);
    return 1;
}

/* inet_pton - 文字列をバイナリアドレスに変換 */
static inline int inet_pton(int af, const char* src, void* dst) {
    if (!src || !dst) {
        errno = EINVAL;
        return -1;
    }
    if (af == AF_INET) {
        return _inet_pton_ipv4_strict(src, (struct in_addr*)dst);
    }
    if (af == AF_INET6) {
        return rin_ipv6_text_parse(src,
                                   ((struct in6_addr*)dst)->s6_addr) == 0
            ? 1 : 0;
    }
    errno = EAFNOSUPPORT;
    return -1;
}

/* inet_ntop - バイナリアドレスを文字列に変換 */
static inline const char* inet_ntop(int af, const void* src, char* dst, socklen_t size) {
    if (!src || !dst) {
        errno = EINVAL;
        return NULL;
    }
    if (af == AF_INET) {
        if (size < 16) {
            errno = ENOSPC;
            return NULL;
        }
        const struct in_addr* addr = (const struct in_addr*)src;
        char* buf = inet_ntoa(*addr);
        char* p = dst;
        while (*buf && (size_t)(p - dst) < size - 1) {
            *p++ = *buf++;
        }
        *p = '\0';
        return dst;
    }
    if (af == AF_INET6) {
        if (rin_ipv6_text_format(((const struct in6_addr*)src)->s6_addr,
                                 dst, (size_t)size) != 0) {
            errno = ENOSPC;
            return NULL;
        }
        return dst;
    }
    errno = EAFNOSUPPORT;
    return NULL;
}

/* inet_network - ホストバイトオーダーのネットワーク番号を返す */
static inline in_addr_t inet_network(const char* cp) {
    return ntohl(inet_addr(cp));
}

/* inet_makeaddr - ネットワーク番号とホスト番号からアドレスを作成 */
static inline struct in_addr inet_makeaddr(in_addr_t net, in_addr_t host) {
    struct in_addr addr;
    if (net < 128) {
        addr.s_addr = htonl((net << 24) | (host & 0xffffff));
    } else if (net < 65536) {
        addr.s_addr = htonl((net << 16) | (host & 0xffff));
    } else {
        addr.s_addr = htonl((net << 8) | (host & 0xff));
    }
    return addr;
}

/* inet_lnaof - ローカルネットワーク部を取得 */
static inline in_addr_t inet_lnaof(struct in_addr in) {
    in_addr_t a = ntohl(in.s_addr);
    if (IN_CLASSA(a)) return a & IN_CLASSA_HOST;
    if (IN_CLASSB(a)) return a & IN_CLASSB_HOST;
    return a & IN_CLASSC_HOST;
}

/* inet_netof - ネットワーク部を取得 */
static inline in_addr_t inet_netof(struct in_addr in) {
    in_addr_t a = ntohl(in.s_addr);
    if (IN_CLASSA(a)) return (a & IN_CLASSA_NET) >> 24;
    if (IN_CLASSB(a)) return (a & IN_CLASSB_NET) >> 16;
    return (a & IN_CLASSC_NET) >> 8;
}

#ifdef __cplusplus
}
#endif

#endif /* _ARPA_INET_H */
