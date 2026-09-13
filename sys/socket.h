/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - sys/socket.h
 * ソケットAPI
 */

#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

#include "types.h"
#include "syscall.h"
#include "../stdint.h"
#include "../limits.h"
#define RIN_SOCKET_ABI_TYPES_PROVIDED 1
#include "../../../RinOS-SDK/include/rin/socket_abi.h"
#undef RIN_SOCKET_ABI_TYPES_PROVIDED

/* Shared ABI headers may pull the hosted CRT's stddef, which in turn exposes
 * a second `errno` macro on MinGW.  Socket result helpers are part of the Rin
 * libc contract and must publish errors through its overridable
 * __errno_location seam; do not let that transitive CRT include silently
 * retarget the inline functions. */
#if !defined(RIN_LIBC_HOSTED_ERRNO_OWNER)
#ifdef errno
#undef errno
#endif
#ifdef __cplusplus
extern "C" {
#endif
extern int* __errno_location(void);
#ifdef __cplusplus
}
#endif
#define errno (*__errno_location())
#endif

/* Keep every direct socket syscall at the target-word boundary.  These
 * override points also let the header contract be exercised without issuing
 * RinOS syscall numbers on a hosted test process. */
#ifndef _RIN_SOCKET_SYSCALL2
#define _RIN_SOCKET_SYSCALL2(number, argument1, argument2) \
    _syscall2((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2))
#endif
#ifndef _RIN_SOCKET_SYSCALL1
#define _RIN_SOCKET_SYSCALL1(number, argument1) \
    _syscall1((uintptr_t)(number), (uintptr_t)(argument1))
#endif
#ifndef _RIN_SOCKET_SYSCALL3
#define _RIN_SOCKET_SYSCALL3(number, argument1, argument2, argument3) \
    _syscall3((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3))
#endif
#ifndef _RIN_SOCKET_SYSCALL4
#define _RIN_SOCKET_SYSCALL4(number, argument1, argument2, argument3, argument4) \
    _syscall4((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3), \
              (uintptr_t)(argument4))
#endif
#ifndef _RIN_SOCKET_SYSCALL5
#define _RIN_SOCKET_SYSCALL5(number, argument1, argument2, argument3, argument4, argument5) \
    _syscall5((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3), \
              (uintptr_t)(argument4), (uintptr_t)(argument5))
#endif
#ifndef _RIN_SOCKET_SYSCALL6
#define _RIN_SOCKET_SYSCALL6(number, argument1, argument2, argument3, argument4, argument5, argument6) \
    _syscall6((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3), \
              (uintptr_t)(argument4), (uintptr_t)(argument5), \
              (uintptr_t)(argument6))
#endif

static inline int __rin_socket_raw_error(intptr_t result) {
    if (result >= 0) return 0;
    errno = result >= -4095 ? (int)-result : EIO;
    return -1;
}

static inline int __rin_socket_status_result(intptr_t result) {
    if (__rin_socket_raw_error(result) != 0) return -1;
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

static inline int __rin_socket_fd_result(intptr_t result) {
    if (__rin_socket_raw_error(result) != 0) return -1;
    if ((uintptr_t)result > (uintptr_t)INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (int)result;
}

static inline ssize_t __rin_socket_ssize_result(intptr_t result) {
    if (__rin_socket_raw_error(result) != 0) return (ssize_t)-1;
    if ((uintptr_t)result > (uintptr_t)SSIZE_MAX) {
        errno = EOVERFLOW;
        return (ssize_t)-1;
    }
    return (ssize_t)result;
}

/* Stream and datagram I/O may not report more bytes than the caller offered.
 * Keep the target-width result signed until both the public ssize_t bound and
 * the request-length bound have been checked.  A malformed positive kernel
 * word must never become a successful over-report. */
static inline ssize_t __rin_socket_count_result(intptr_t result,
                                                size_t requested) {
    if (__rin_socket_raw_error(result) != 0) return (ssize_t)-1;
    if ((uintptr_t)result > (uintptr_t)SSIZE_MAX) {
        errno = EOVERFLOW;
        return (ssize_t)-1;
    }
    if ((uintptr_t)result > (uintptr_t)requested) {
        errno = EIO;
        return (ssize_t)-1;
    }
    return (ssize_t)result;
}

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * 型定義
 * ═══════════════════════════════════════════════════════════════*/

typedef unsigned int socklen_t;
typedef unsigned short sa_family_t;

/* ═══════════════════════════════════════════════════════════════
 * アドレスファミリ
 * ═══════════════════════════════════════════════════════════════*/

#define AF_UNSPEC    0   /* 未指定 */
#define AF_LOCAL     1   /* ローカル通信 */
#define AF_UNIX      AF_LOCAL
#define AF_INET      2   /* IPv4 */
#define AF_INET6     10  /* IPv6 */
#define AF_PACKET    17  /* 低レベルパケット */

/* プロトコルファミリ (アドレスファミリと同じ) */
#define PF_UNSPEC    AF_UNSPEC
#define PF_LOCAL     AF_LOCAL
#define PF_UNIX      AF_UNIX
#define PF_INET      AF_INET
#define PF_INET6     AF_INET6
#define PF_PACKET    AF_PACKET

/* ═══════════════════════════════════════════════════════════════
 * ソケットタイプ
 * ═══════════════════════════════════════════════════════════════*/

#define SOCK_STREAM    1   /* TCP (順序保証、コネクション型) */
#define SOCK_DGRAM     2   /* UDP (データグラム) */
#define SOCK_RAW       3   /* 生ソケット */
#define SOCK_SEQPACKET 5   /* シーケンシャルパケット */

/* ソケットフラグ (ORで組み合わせ可能) */
#define SOCK_NONBLOCK  0x800   /* ノンブロッキング */
#define SOCK_CLOEXEC   0x80000 /* exec時にクローズ */

/* ═══════════════════════════════════════════════════════════════
 * プロトコル番号
 * ═══════════════════════════════════════════════════════════════*/

#define IPPROTO_IP      0   /* ダミー */
#define IPPROTO_ICMP    1   /* ICMP */
#define IPPROTO_TCP     6   /* TCP */
#define IPPROTO_UDP     17  /* UDP */
#define IPPROTO_RAW     255 /* 生IP */

/* ═══════════════════════════════════════════════════════════════
 * アドレス構造体
 * ═══════════════════════════════════════════════════════════════*/

/* 汎用ソケットアドレス */
struct sockaddr {
    sa_family_t sa_family;
    char        sa_data[14];
};

/* ソケットアドレスストレージ (十分なサイズ) */
struct sockaddr_storage {
    sa_family_t ss_family;
    char        __ss_padding[126];
    unsigned long __ss_align;
};

/* IPv4アドレス */
typedef uint32_t in_addr_t;

struct in_addr {
    in_addr_t s_addr;
};

/* IPv4ソケットアドレス */
struct sockaddr_in {
    sa_family_t    sin_family;  /* AF_INET */
    uint16_t       sin_port;    /* ポート (ネットワークバイトオーダー) */
    struct in_addr sin_addr;    /* IPアドレス */
    unsigned char  sin_zero[8]; /* パディング */
};

/* IPv6アドレス */
struct in6_addr {
    union {
        uint8_t  s6_addr[16];
        uint16_t s6_addr16[8];
        uint32_t s6_addr32[4];
    };
};

/* IPv6ソケットアドレス */
struct sockaddr_in6 {
    sa_family_t     sin6_family;   /* AF_INET6 */
    uint16_t        sin6_port;     /* ポート */
    uint32_t        sin6_flowinfo; /* フロー情報 */
    struct in6_addr sin6_addr;     /* IPv6アドレス */
    uint32_t        sin6_scope_id; /* スコープID */
};

/* Stable 20-byte IPv6 multicast membership ABI shared with the kernel's
 * setsockopt boundary. */
struct ipv6_mreq {
    struct in6_addr ipv6mr_multiaddr;
    uint32_t        ipv6mr_interface;
};

/* UNIXドメインソケットアドレス */
#ifndef _SOCKADDR_UN_DEFINED
#define _SOCKADDR_UN_DEFINED
struct sockaddr_un {
    sa_family_t sun_family;    /* AF_UNIX */
    char        sun_path[108]; /* パス名 */
};
#endif

/* ═══════════════════════════════════════════════════════════════
 * 特殊アドレス
 * ═══════════════════════════════════════════════════════════════*/

#define INADDR_ANY       ((in_addr_t)0x00000000)
#define INADDR_BROADCAST ((in_addr_t)0xFFFFFFFF)
#define INADDR_LOOPBACK  ((in_addr_t)0x7F000001)
#define INADDR_NONE      ((in_addr_t)0xFFFFFFFF)

extern const struct in6_addr in6addr_any;
extern const struct in6_addr in6addr_loopback;

#define IN6ADDR_ANY_INIT      {{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }}
#define IN6ADDR_LOOPBACK_INIT {{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 }}

/* ═══════════════════════════════════════════════════════════════
 * バイトオーダー変換
 * ═══════════════════════════════════════════════════════════════*/

static inline uint16_t htons(uint16_t hostshort) {
    return ((hostshort & 0xFF) << 8) | ((hostshort >> 8) & 0xFF);
}

static inline uint16_t ntohs(uint16_t netshort) {
    return htons(netshort);
}

static inline uint32_t htonl(uint32_t hostlong) {
    return ((hostlong & 0xFF) << 24) |
           ((hostlong & 0xFF00) << 8) |
           ((hostlong >> 8) & 0xFF00) |
           ((hostlong >> 24) & 0xFF);
}

static inline uint32_t ntohl(uint32_t netlong) {
    return htonl(netlong);
}

/* ═══════════════════════════════════════════════════════════════
 * ソケットオプション
 * ═══════════════════════════════════════════════════════════════*/

/* ソケットレベル */
#define SOL_SOCKET  1

/* ソケットオプション */
#define SO_DEBUG        1   /* デバッグ情報 */
#define SO_REUSEADDR    2   /* アドレス再利用 */
#define SO_TYPE         3   /* ソケットタイプ取得 */
#define SO_ERROR        4   /* エラー取得 */
#define SO_DONTROUTE    5   /* ルーティング回避 */
#define SO_BROADCAST    6   /* ブロードキャスト許可 */
#define SO_SNDBUF       7   /* 送信バッファサイズ */
#define SO_RCVBUF       8   /* 受信バッファサイズ */
#define SO_KEEPALIVE    9   /* キープアライブ */
#define SO_OOBINLINE    10  /* OOBデータをインライン */
#define SO_LINGER       13  /* close時のlinger */
#define SO_RCVLOWAT     18  /* 受信低水位 */
#define SO_SNDLOWAT     19  /* 送信低水位 */
#define SO_RCVTIMEO     20  /* 受信タイムアウト */
#define SO_SNDTIMEO     21  /* 送信タイムアウト */
#define SO_ACCEPTCONN   30  /* accept可能か */
#define SO_REUSEPORT    15  /* ポート再利用 */
#define SO_NONBLOCK               0x5003 /* kernel socket nonblocking */

/* TCPオプション */
#ifndef IPPROTO_TCP
#define IPPROTO_TCP     6
#endif
#define TCP_NODELAY     1   /* Nagleアルゴリズム無効化 */
#define TCP_MAXSEG      2   /* 最大セグメントサイズ */
#define TCP_KEEPIDLE    4   /* キープアライブ開始時間 */
#define TCP_KEEPINTVL   5   /* キープアライブ間隔 */
#define TCP_KEEPCNT     6   /* キープアライブ回数 */

/* ═══════════════════════════════════════════════════════════════
 * send/recvフラグ
 * ═══════════════════════════════════════════════════════════════*/

#define MSG_OOB         0x01    /* 帯域外データ */
#define MSG_PEEK        0x02    /* データを読むがキューから削除しない */
#define MSG_DONTROUTE   0x04    /* ルーティング回避 */
#define MSG_TRUNC       0x20    /* 切り詰めたdatagramの元の長さを返す */
#define MSG_DONTWAIT    0x40    /* ノンブロッキング操作 */
#define MSG_WAITALL     0x100   /* 全データを待つ */
#define MSG_NOSIGNAL    0x4000  /* SIGPIPEを送らない */
#define MSG_CTRUNC      0x08    /* 補助データが制御バッファで切り詰められた */

/* ═══════════════════════════════════════════════════════════════
 * shutdownフラグ
 * ═══════════════════════════════════════════════════════════════*/

#define SHUT_RD   0  /* 受信を停止 */
#define SHUT_WR   1  /* 送信を停止 */
#define SHUT_RDWR 2  /* 送受信を停止 */

/* ═══════════════════════════════════════════════════════════════
 * linger構造体
 * ═══════════════════════════════════════════════════════════════*/

struct linger {
    int l_onoff;   /* linger有効 */
    int l_linger;  /* linger時間(秒) */
};

/* ═══════════════════════════════════════════════════════════════
 * msghdr構造体 (sendmsg/recvmsg用)
 * ═══════════════════════════════════════════════════════════════*/

struct iovec {
    void*  iov_base;  /* バッファ開始 */
    size_t iov_len;   /* バッファ長 */
};

struct msghdr {
    void*         msg_name;       /* オプションアドレス */
    socklen_t     msg_namelen;    /* アドレス長 */
    struct iovec* msg_iov;        /* scatter/gather配列 */
    size_t        msg_iovlen;     /* 配列要素数 */
    void*         msg_control;    /* 補助データ */
    size_t        msg_controllen; /* 補助データ長 */
    int           msg_flags;      /* フラグ */
};

struct cmsghdr {
    size_t cmsg_len;   /* データ長 (ヘッダ含む) */
    int    cmsg_level; /* プロトコル */
    int    cmsg_type;  /* プロトコル固有タイプ */
    /* 後にデータが続く */
};

/* The kernel socket message owner accepts at most 64 iovecs.  Compute the
 * aggregate request before issuing sendmsg/recvmsg so a malformed result
 * cannot claim more bytes than the caller supplied across the vector. */
#define RIN_SOCKET_MAX_IOV 64u
static inline int __rin_socket_message_length(const struct msghdr* message,
                                              size_t* length_out) {
    size_t total = 0;
    size_t index;

    if (!message || !length_out) {
        errno = EFAULT;
        return -1;
    }
    if (message->msg_iovlen > RIN_SOCKET_MAX_IOV) {
        errno = EINVAL;
        return -1;
    }
    if (message->msg_iovlen != 0u && !message->msg_iov) {
        errno = EFAULT;
        return -1;
    }
    for (index = 0; index < message->msg_iovlen; ++index) {
        if (message->msg_iov[index].iov_len > SIZE_MAX - total) {
            errno = EOVERFLOW;
            return -1;
        }
        total += message->msg_iov[index].iov_len;
    }
    *length_out = total;
    return 0;
}

#define SCM_RIGHTS 0x01

#define _RIN_CMSG_ALIGN(len) (((len) + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1))
#define CMSG_ALIGN(len) _RIN_CMSG_ALIGN(len)
#define CMSG_SPACE(len) (_RIN_CMSG_ALIGN(sizeof(struct cmsghdr)) + _RIN_CMSG_ALIGN(len))
#define CMSG_LEN(len) (_RIN_CMSG_ALIGN(sizeof(struct cmsghdr)) + (len))
#define CMSG_DATA(cmsg) ((unsigned char*)(cmsg) + _RIN_CMSG_ALIGN(sizeof(struct cmsghdr)))
#define CMSG_FIRSTHDR(msg) \
    (((msg) && (msg)->msg_controllen >= sizeof(struct cmsghdr)) ? (struct cmsghdr*)((msg)->msg_control) : (struct cmsghdr*)0)
#define CMSG_NXTHDR(msg, cmsg) \
    ((((unsigned char*)(cmsg) + _RIN_CMSG_ALIGN((cmsg)->cmsg_len) + _RIN_CMSG_ALIGN(sizeof(struct cmsghdr))) > ((unsigned char*)(msg)->msg_control + (msg)->msg_controllen)) \
        ? (struct cmsghdr*)0 \
        : (struct cmsghdr*)((unsigned char*)(cmsg) + _RIN_CMSG_ALIGN((cmsg)->cmsg_len)))

/* ═══════════════════════════════════════════════════════════════
 * ソケット関数
 * ═══════════════════════════════════════════════════════════════*/

static inline int socket(int domain, int type, int protocol) {
    return __rin_socket_fd_result(
        _RIN_SOCKET_SYSCALL3(SYS_SOCKET, domain, type, protocol));
}

static inline int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    return __rin_socket_status_result(
        _RIN_SOCKET_SYSCALL3(SYS_BIND, sockfd, addr, addrlen));
}

static inline int listen(int sockfd, int backlog) {
    return __rin_socket_status_result(
        _RIN_SOCKET_SYSCALL2(SYS_LISTEN, sockfd, backlog));
}

static inline int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    return __rin_socket_fd_result(
        _RIN_SOCKET_SYSCALL3(SYS_ACCEPT, sockfd, addr, addrlen));
}

static inline int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    return __rin_socket_status_result(
        _RIN_SOCKET_SYSCALL3(SYS_CONNECT, sockfd, addr, addrlen));
}

static inline ssize_t send(int sockfd, const void* buf, size_t len, int flags) {
    return __rin_socket_count_result(
        _RIN_SOCKET_SYSCALL4(SYS_SEND, sockfd, buf, len, flags), len);
}

static inline ssize_t recv(int sockfd, void* buf, size_t len, int flags) {
    return __rin_socket_count_result(
        _RIN_SOCKET_SYSCALL4(SYS_RECV, sockfd, buf, len, flags), len);
}

static inline ssize_t sendto(int sockfd, const void* buf, size_t len, int flags,
                             const struct sockaddr* dest_addr, socklen_t addrlen) {
    return __rin_socket_count_result(
        _RIN_SOCKET_SYSCALL6(SYS_SENDTO, sockfd, buf, len, flags,
                             dest_addr, addrlen), len);
}

static inline ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags,
                               struct sockaddr* src_addr, socklen_t* addrlen) {
    return __rin_socket_count_result(
        _RIN_SOCKET_SYSCALL6(SYS_RECVFROM, sockfd, buf, len, flags,
                             src_addr, addrlen), len);
}

static inline int shutdown(int sockfd, int how) {
    return __rin_socket_status_result(
        _RIN_SOCKET_SYSCALL2(SYS_SOCK_SHUTDOWN, sockfd, how));
}

static inline int getsockopt(int sockfd, int level, int optname,
                             void* optval, socklen_t* optlen) {
    return __rin_socket_status_result(
        _RIN_SOCKET_SYSCALL5(SYS_GETSOCKOPT, sockfd, level, optname,
                             optval, optlen));
}

static inline int setsockopt(int sockfd, int level, int optname,
                             const void* optval, socklen_t optlen) {
    return __rin_socket_status_result(
        _RIN_SOCKET_SYSCALL5(SYS_SETSOCKOPT, sockfd, level, optname,
                             optval, optlen));
}

static inline int getsockname(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    return __rin_socket_status_result(
        _RIN_SOCKET_SYSCALL3(SYS_GETSOCKNAME, sockfd, addr, addrlen));
}

static inline int getpeername(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    return __rin_socket_status_result(
        _RIN_SOCKET_SYSCALL3(SYS_GETPEERNAME, sockfd, addr, addrlen));
}

static inline int socketpair(int domain, int type, int protocol, int sv[2]) {
    return __rin_socket_status_result(
        _RIN_SOCKET_SYSCALL4(SYS_SOCKETPAIR, domain, type, protocol, sv));
}

static inline ssize_t sendmsg(int sockfd, const struct msghdr* msg, int flags) {
    size_t requested;
    if (__rin_socket_message_length(msg, &requested) != 0)
        return (ssize_t)-1;
    return __rin_socket_count_result(
        _RIN_SOCKET_SYSCALL3(SYS_SENDMSG, sockfd, msg, flags), requested);
}

static inline ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags) {
    size_t requested;
    if (__rin_socket_message_length(msg, &requested) != 0)
        return (ssize_t)-1;
    return __rin_socket_count_result(
        _RIN_SOCKET_SYSCALL3(SYS_RECVMSG, sockfd, msg, flags), requested);
}

/* ═══════════════════════════════════════════════════════════════
 * accept4 - フラグ付きaccept
 * ═══════════════════════════════════════════════════════════════*/

static inline int accept4(int sockfd, struct sockaddr* addr,
                          socklen_t* addrlen, int flags) {
    int accepted;
    int saved_errno;
    if ((flags & ~(SOCK_NONBLOCK | SOCK_CLOEXEC)) != 0) {
        errno = EINVAL;
        return -1;
    }
    accepted = accept(sockfd, addr, addrlen);
    if (accepted < 0 || flags == 0) return accepted;
    if ((flags & SOCK_NONBLOCK) != 0) {
        int enabled = 1;
        if (setsockopt(accepted, SOL_SOCKET, SO_NONBLOCK, &enabled,
                       (socklen_t)sizeof(enabled)) != 0)
            goto fail;
    }
    if ((flags & SOCK_CLOEXEC) != 0) {
        intptr_t descriptor_flags = _RIN_SOCKET_SYSCALL3(
            SYS_FCNTL, accepted, 1 /* F_GETFD */, 0u);
        if (__rin_socket_raw_error(descriptor_flags) != 0)
            goto fail;
        if ((uintptr_t)descriptor_flags > (uintptr_t)INT_MAX)
            goto overflow;
        descriptor_flags |= 1 /* FD_CLOEXEC */;
        if (__rin_socket_status_result(_RIN_SOCKET_SYSCALL3(
                SYS_FCNTL, accepted, 2 /* F_SETFD */, descriptor_flags)) != 0)
            goto fail;
    }
    return accepted;
overflow:
    errno = EOVERFLOW;
fail:
    saved_errno = errno;
    (void)_RIN_SOCKET_SYSCALL1(SYS_CLOSE, accepted);
    errno = saved_errno;
    return -1;
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_SOCKET_H */
