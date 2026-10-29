/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - sys/socket.h
 * ソケットAPI
 */

#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

#include "types.h"
#include <sys/types.h>
#include "syscall.h"
#include "../stdint.h"
#include "../time.h"
#include "../poll.h"
#include "../internal/pthread_cancel_point.h"
#include "../limits.h"
#define RIN_SOCKET_ABI_TYPES_PROVIDED 1
#include <rin/net/socket_abi.h>
#undef RIN_SOCKET_ABI_TYPES_PROVIDED

#ifndef MIDL_PASS
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

/* Link-layer address used by AF_PACKET / PF_PACKET. Protocol is in network
 * byte order, and sll_ifindex is the namespace-visible interface index. */
struct sockaddr_ll {
    sa_family_t sll_family;
    uint16_t sll_protocol;
    int32_t sll_ifindex;
    uint16_t sll_hatype;
    uint8_t sll_pkttype;
    uint8_t sll_halen;
    uint8_t sll_addr[8];
};

struct packet_mreq {
    int32_t mr_ifindex;
    uint16_t mr_type;
    uint16_t mr_alen;
    uint8_t mr_address[8];
};

#if defined(__cplusplus)
static_assert(sizeof(struct sockaddr_ll) == 20u, "sockaddr_ll ABI drift");
static_assert(offsetof(struct sockaddr_ll, sll_addr) == 12u,
              "sockaddr_ll address offset drift");
static_assert(sizeof(struct packet_mreq) == 16u, "packet_mreq ABI drift");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(struct sockaddr_ll) == 20u, "sockaddr_ll ABI drift");
_Static_assert(offsetof(struct sockaddr_ll, sll_addr) == 12u,
               "sockaddr_ll address offset drift");
_Static_assert(sizeof(struct packet_mreq) == 16u, "packet_mreq ABI drift");
#endif

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
#define SOL_PACKET  263

#define PACKET_ADD_MEMBERSHIP 1
#define PACKET_DROP_MEMBERSHIP 2
#define PACKET_MR_PROMISC 1
#define PACKET_HOST 0
#define PACKET_BROADCAST 1
#define PACKET_MULTICAST 2
#define PACKET_OTHERHOST 3
#define PACKET_OUTGOING 4

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
#define SO_PASSCRED     16  /* UNIX socketで送信者資格情報を受信 */
#define SO_PEERCRED     17  /* UNIX socketの接続時peer資格情報 */
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
#define MSG_EOR         0x80    /* レコード境界の終端 */
#define MSG_TRUNC       0x20    /* 切り詰めたdatagramの元の長さを返す */
#define MSG_DONTWAIT    0x40    /* ノンブロッキング操作 */
#define MSG_WAITALL     0x100   /* 全データを待つ */
#define MSG_NOSIGNAL    0x4000  /* SIGPIPEを送らない */
#define MSG_CTRUNC      0x08    /* 補助データが制御バッファで切り詰められた */
#define MSG_CMSG_CLOEXEC 0x40000000 /* SCM_RIGHTS descriptors use FD_CLOEXEC */
#define MSG_WAITFORONE  0x10000 /* 最初のmessage受信後は待たない */

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

#ifndef RIN_LIBC_IOVEC_DEFINED
#define RIN_LIBC_IOVEC_DEFINED 1
struct iovec {
    void*  iov_base;  /* バッファ開始 */
    size_t iov_len;   /* バッファ長 */
};
#endif

struct msghdr {
    void*         msg_name;       /* オプションアドレス */
    socklen_t     msg_namelen;    /* アドレス長 */
    struct iovec* msg_iov;        /* scatter/gather配列 */
    size_t        msg_iovlen;     /* 配列要素数 */
    void*         msg_control;    /* 補助データ */
    size_t        msg_controllen; /* 補助データ長 */
    int           msg_flags;      /* フラグ */
};

/* Linux互換のdatagram batch record。32-bitでは32 byte、native 64-bitでは
 * 64 byteとなり、msg_lenの位置は対応するmsghdr直後に固定する。 */
#ifndef _MMSGHDR_DEFINED
#define _MMSGHDR_DEFINED 1
struct mmsghdr {
    struct msghdr msg_hdr;
    unsigned int msg_len;
};
#endif

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
#define SCM_CREDENTIALS 0x02

struct ucred {
    pid_t pid;
    uid_t uid;
    gid_t gid;
};

#ifdef __cplusplus
#define __RIN_CMSG_CONSTEXPR constexpr
#else
#define __RIN_CMSG_CONSTEXPR
#endif

static inline __RIN_CMSG_CONSTEXPR size_t __rin_cmsg_align_size(size_t length) {
    const size_t mask = sizeof(size_t) - 1u;
    if (length > SIZE_MAX - mask) return SIZE_MAX;
    return (length + mask) & ~mask;
}

static inline __RIN_CMSG_CONSTEXPR size_t __rin_cmsg_space_size(size_t payload_length) {
    const size_t header_length =
        __rin_cmsg_align_size(sizeof(struct cmsghdr));
    const size_t aligned_payload = __rin_cmsg_align_size(payload_length);
    if (aligned_payload > SIZE_MAX - header_length) return SIZE_MAX;
    return header_length + aligned_payload;
}

static inline __RIN_CMSG_CONSTEXPR size_t __rin_cmsg_len_size(size_t payload_length) {
    const size_t header_length =
        __rin_cmsg_align_size(sizeof(struct cmsghdr));
    if (payload_length > SIZE_MAX - header_length) return SIZE_MAX;
    return header_length + payload_length;
}

#undef __RIN_CMSG_CONSTEXPR

static inline struct cmsghdr* __rin_cmsg_nxthdr(
    const struct msghdr* message, const struct cmsghdr* current) {
    const uintptr_t base = message ? (uintptr_t)message->msg_control : 0u;
    const uintptr_t cursor = (uintptr_t)current;
    const size_t total = message ? message->msg_controllen : 0u;
    size_t offset;
    size_t remaining;
    size_t step;
    size_t current_length;

    if (!message || !current || !message->msg_control || base > UINTPTR_MAX - total ||
        cursor < base || cursor - base > total)
        return (struct cmsghdr*)0;
    offset = (size_t)(cursor - base);
    remaining = total - offset;
    if (remaining < sizeof(struct cmsghdr)) return (struct cmsghdr*)0;
    current_length = current->cmsg_len;
    if (current_length < sizeof(struct cmsghdr) || current_length > remaining)
        return (struct cmsghdr*)0;
    step = __rin_cmsg_align_size(current_length);
    if (step == SIZE_MAX || step > remaining) return (struct cmsghdr*)0;
    offset += step;
    if (total - offset < sizeof(struct cmsghdr)) return (struct cmsghdr*)0;
    return (struct cmsghdr*)((unsigned char*)message->msg_control + offset);
}

#define _RIN_CMSG_ALIGN(len) __rin_cmsg_align_size((size_t)(len))
#define CMSG_ALIGN(len) _RIN_CMSG_ALIGN(len)
#define CMSG_SPACE(len) __rin_cmsg_space_size((size_t)(len))
#define CMSG_LEN(len) __rin_cmsg_len_size((size_t)(len))
#define CMSG_DATA(cmsg) ((unsigned char*)(cmsg) + _RIN_CMSG_ALIGN(sizeof(struct cmsghdr)))
#define CMSG_FIRSTHDR(msg) \
    (((msg) && (msg)->msg_control && \
      (msg)->msg_controllen >= sizeof(struct cmsghdr)) \
         ? (struct cmsghdr*)((msg)->msg_control) : (struct cmsghdr*)0)
#define CMSG_NXTHDR(msg, cmsg) __rin_cmsg_nxthdr((msg), (cmsg))

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

static inline int getsockopt(int sockfd, int level, int optname,
                             void* optval, socklen_t* optlen);

/* The kernel32 socket dispatcher has no restartable blocking wait boundary.
 * Keep its Unix accept retry contract sleep-based at the libc boundary, while
 * preserving immediate EAGAIN for nonblocking or non-listening sockets. */
typedef struct __RinSocketAcceptWait {
    uint64_t deadline_ms;
    uint32_t delay_ms;
    int initialized;
    int timed;
} __RinSocketAcceptWait;

static inline int __rin_socket_accept_wait_init(
    int sockfd, __RinSocketAcceptWait* wait) {
    int nonblocking = 0;
    socklen_t option_size = (socklen_t)sizeof(nonblocking);
    struct timeval timeout = {0, 0};
    struct timespec now;
    uint64_t timeout_ms;

    if (!wait) {
        errno = EINVAL;
        return 0;
    }
    if (getsockopt(sockfd, SOL_SOCKET, SO_NONBLOCK, &nonblocking,
                   &option_size) != 0)
        return 0;
    if (option_size != (socklen_t)sizeof(nonblocking)) {
        errno = EIO;
        return 0;
    }
    if (nonblocking != 0) return 0;

    int accepting = 0;
    option_size = (socklen_t)sizeof(accepting);
    if (getsockopt(sockfd, SOL_SOCKET, SO_ACCEPTCONN, &accepting,
                   &option_size) != 0)
        return 0;
    if (option_size != (socklen_t)sizeof(accepting)) {
        errno = EIO;
        return 0;
    }
    if (accepting != 1) {
        errno = EINVAL;
        return 0;
    }

    option_size = (socklen_t)sizeof(timeout);
    if (getsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                   &option_size) != 0)
        return 0;
    if (option_size != (socklen_t)sizeof(timeout) ||
        timeout.tv_sec < 0 || timeout.tv_usec < 0 ||
        timeout.tv_usec >= 1000000L) {
        errno = EIO;
        return 0;
    }

    wait->timed = timeout.tv_sec != 0 || timeout.tv_usec != 0;
    wait->deadline_ms = 0u;
    if (wait->timed) {
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0;
        if (now.tv_sec < 0 || now.tv_nsec < 0 ||
            now.tv_nsec >= 1000000000L ||
            (uint64_t)now.tv_sec >
                (UINT64_MAX - (uint64_t)now.tv_nsec / 1000000u) / 1000u) {
            errno = EOVERFLOW;
            return 0;
        }
        timeout_ms = (uint64_t)timeout.tv_sec > UINT64_MAX / 1000u
            ? UINT64_MAX
            : (uint64_t)timeout.tv_sec * 1000u;
        if (timeout_ms != UINT64_MAX) {
            uint64_t fractional_ms =
                ((uint64_t)timeout.tv_usec + 999u) / 1000u;
            timeout_ms = timeout_ms > UINT64_MAX - fractional_ms
                ? UINT64_MAX : timeout_ms + fractional_ms;
        }
        wait->deadline_ms = (uint64_t)now.tv_sec * 1000u +
                            (uint64_t)now.tv_nsec / 1000000u;
        wait->deadline_ms = wait->deadline_ms > UINT64_MAX - timeout_ms
            ? UINT64_MAX : wait->deadline_ms + timeout_ms;
    }
    wait->delay_ms = 1u;
    wait->initialized = 1;
    return 1;
}

static inline int __rin_socket_accept_backoff(
    int sockfd, __RinSocketAcceptWait* wait) {
    uint32_t delay_ms;
    if (!wait) {
        errno = EINVAL;
        return 0;
    }
    if (!wait->initialized && !__rin_socket_accept_wait_init(sockfd, wait))
        return 0;
    delay_ms = wait->delay_ms;
    if (wait->timed) {
        struct timespec now;
        uint64_t now_ms;
        uint64_t fractional_ms;
        uint64_t remaining_ms;
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0;
        if (now.tv_sec < 0 || now.tv_nsec < 0 ||
            now.tv_nsec >= 1000000000L) {
            errno = EOVERFLOW;
            return 0;
        }
        fractional_ms = (uint64_t)now.tv_nsec / 1000000u;
        if ((uint64_t)now.tv_sec >
            (UINT64_MAX - fractional_ms) / 1000u) {
            errno = EOVERFLOW;
            return 0;
        }
        now_ms = (uint64_t)now.tv_sec * 1000u +
                 fractional_ms;
        if (now_ms >= wait->deadline_ms) {
            errno = EAGAIN;
            return 0;
        }
        remaining_ms = wait->deadline_ms - now_ms;
        if (remaining_ms < delay_ms)
            delay_ms = (uint32_t)remaining_ms;
        if (delay_ms == 0u) delay_ms = 1u;
    }

    struct timespec delay = {0, (long)delay_ms * 1000000L};
    if (nanosleep(&delay, 0) != 0) return 0;
    if (wait->delay_ms < 256u) {
        wait->delay_ms = wait->delay_ms > 128u
            ? 256u : wait->delay_ms * 2u;
    }
    return 1;
}

static inline int __rin_socket_accept_wait_expired(
    const __RinSocketAcceptWait* wait) {
    struct timespec now;
    uint64_t now_ms;
    uint64_t fractional_ms;
    if (!wait || !wait->initialized || !wait->timed) return 0;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return -1;
    if (now.tv_sec < 0 || now.tv_nsec < 0 ||
        now.tv_nsec >= 1000000000L) {
        errno = EOVERFLOW;
        return -1;
    }
    fractional_ms = (uint64_t)now.tv_nsec / 1000000u;
    if ((uint64_t)now.tv_sec >
        (UINT64_MAX - fractional_ms) / 1000u) {
        errno = EOVERFLOW;
        return -1;
    }
    now_ms = (uint64_t)now.tv_sec * 1000u +
             fractional_ms;
    if (now_ms < wait->deadline_ms) return 0;
    errno = EAGAIN;
    return 1;
}

static inline void __rin_socket_cancel_close_fd(void* argument) {
    const int fd = argument ? *(const int*)argument : -1;
    if (fd >= 0)
        (void)_RIN_SOCKET_SYSCALL1(SYS_CLOSE, (uintptr_t)(intptr_t)fd);
}

static inline int accept(int sockfd, struct sockaddr* addr,
                         socklen_t* addrlen) {
    __RinSocketAcceptWait wait = {0u, 1u, 0, 0};
    __rin_pthread_cancel_point_if_linked();
    for (;;) {
        if (__rin_socket_accept_wait_expired(&wait) != 0) return -1;
        const intptr_t result =
            _RIN_SOCKET_SYSCALL3(SYS_ACCEPT, sockfd, addr, addrlen);
        if (result >= 0) {
            int accepted = __rin_socket_fd_result(result);
            if (accepted >= 0)
                __rin_pthread_cancel_point_cleanup_if_linked(
                    __rin_socket_cancel_close_fd, &accepted);
            return accepted;
        }
        __rin_pthread_cancel_point_if_linked();
        (void)__rin_socket_raw_error(result);
        if (errno != EAGAIN ||
            !__rin_socket_accept_backoff(sockfd, &wait))
            return -1;
    }
}

static inline int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    (void)addr;
    (void)addrlen;
    uint32_t retry_count = 0u;
    uint32_t delay_ms = 1u;
    __rin_pthread_cancel_point_if_linked();

    for (;;) {
        const intptr_t result =
            _RIN_SOCKET_SYSCALL3(SYS_CONNECT, sockfd, addr, addrlen);
        __rin_pthread_cancel_point_if_linked();
        if (result == 0) return 0;
        if (result > 0) {
            errno = EIO;
            return -1;
        }
        (void)__rin_socket_raw_error(result);
        if (errno != EAGAIN || retry_count >= 9u)
            return -1;

        int nonblocking = 0;
        socklen_t nonblocking_size = (socklen_t)sizeof(nonblocking);
        if (getsockopt(sockfd, SOL_SOCKET, SO_NONBLOCK, &nonblocking,
                       &nonblocking_size) != 0)
            return -1;
        if (nonblocking_size != (socklen_t)sizeof(nonblocking)) {
            errno = EIO;
            return -1;
        }
        if (nonblocking != 0) return -1;

        /* Connect EAGAIN can reflect transient admission pressure. Sleep with
         * exponential backoff for at most 511 ms; the kernel syscall never
         * spins or parks a worker while waiting for capacity. Nonblocking
         * sockets and other errors return immediately. */
        struct timespec delay = {0, (long)delay_ms * 1000000L};
        if (nanosleep(&delay, 0) != 0) return -1;
        ++retry_count;
        delay_ms <<= 1;
    }
}

static inline ssize_t send(int sockfd, const void* buf, size_t len, int flags) {
    __rin_pthread_cancel_point_if_linked();
    ssize_t result = __rin_socket_count_result(
        _RIN_SOCKET_SYSCALL4(SYS_SEND, sockfd, buf, len, flags), len);
    __rin_pthread_cancel_point_if_linked();
    return result;
}

static inline ssize_t recv(int sockfd, void* buf, size_t len, int flags) {
    __rin_pthread_cancel_point_if_linked();
    ssize_t result = __rin_socket_count_result(
        _RIN_SOCKET_SYSCALL4(SYS_RECV, sockfd, buf, len, flags), len);
    __rin_pthread_cancel_point_if_linked();
    return result;
}

static inline ssize_t sendto(int sockfd, const void* buf, size_t len, int flags,
                             const struct sockaddr* dest_addr, socklen_t addrlen) {
    __rin_pthread_cancel_point_if_linked();
    ssize_t result = __rin_socket_count_result(
        _RIN_SOCKET_SYSCALL6(SYS_SENDTO, sockfd, buf, len, flags,
                             dest_addr, addrlen), len);
    __rin_pthread_cancel_point_if_linked();
    return result;
}

static inline ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags,
                               struct sockaddr* src_addr, socklen_t* addrlen) {
    __rin_pthread_cancel_point_if_linked();
    ssize_t result = __rin_socket_count_result(
        _RIN_SOCKET_SYSCALL6(SYS_RECVFROM, sockfd, buf, len, flags,
                             src_addr, addrlen), len);
    __rin_pthread_cancel_point_if_linked();
    return result;
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
    __rin_pthread_cancel_point_if_linked();
    if (__rin_socket_message_length(msg, &requested) != 0)
        return (ssize_t)-1;
    ssize_t result = __rin_socket_count_result(
        _RIN_SOCKET_SYSCALL3(SYS_SENDMSG, sockfd, msg, flags), requested);
    __rin_pthread_cancel_point_if_linked();
    return result;
}

static inline ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags) {
    size_t requested;
    __rin_pthread_cancel_point_if_linked();
    if (__rin_socket_message_length(msg, &requested) != 0)
        return (ssize_t)-1;
    ssize_t result = __rin_socket_count_result(
        _RIN_SOCKET_SYSCALL3(SYS_RECVMSG, sockfd, msg, flags), requested);
    __rin_pthread_cancel_point_if_linked();
    return result;
}

#define RIN_SOCKET_MMSG_MAX_MESSAGES 64u
#define RIN_SOCKET_MMSG_MAX_MESSAGE_BYTES 16777216u
#define RIN_SOCKET_MMSG_MAX_TOTAL_BYTES \
    ((uint64_t)RIN_SOCKET_MMSG_MAX_MESSAGES * \
     (uint64_t)RIN_SOCKET_MMSG_MAX_MESSAGE_BYTES)

static inline intptr_t __rin_socket_mmsg_call(
    int receive, int sockfd, struct mmsghdr* messages,
    unsigned int count, unsigned int flags) {
    return _RIN_SOCKET_SYSCALL4(
        receive ? SYS_RECVMMSG : SYS_SENDMMSG,
        sockfd, messages, count, flags);
}

static inline int __rin_socket_mmsg_nonblocking(int sockfd) {
    /* F_GETFL/O_NONBLOCK values are part of the public fcntl ABI. */
    intptr_t result = _syscall3((uintptr_t)SYS_FCNTL,
                                (uintptr_t)sockfd, 3u, 0u);
    if (__rin_socket_raw_error(result) != 0) return -1;
    return (((unsigned int)result & 0x0040u) != 0u);
}

static inline int __rin_socket_mmsg_now_ns(uint64_t* value_out) {
    struct timespec now;
    uint64_t seconds;
    if (!value_out || clock_gettime(CLOCK_MONOTONIC, &now) != 0) return -1;
    if (now.tv_sec < 0 || now.tv_nsec < 0 || now.tv_nsec >= 1000000000L) {
        errno = EIO;
        return -1;
    }
    seconds = (uint64_t)now.tv_sec;
    if (seconds > (UINT64_MAX - (uint64_t)now.tv_nsec) / UINT64_C(1000000000)) {
        errno = EOVERFLOW;
        return -1;
    }
    *value_out = seconds * UINT64_C(1000000000) +
                 (uint64_t)now.tv_nsec;
    return 0;
}

static inline int __rin_socket_mmsg_timeout_update(
    struct timespec* timeout, uint64_t deadline_ns, int* poll_ms_out) {
    uint64_t now_ns;
    uint64_t remaining_ns;
    uint64_t rounded_ms;
    if (!timeout || !poll_ms_out ||
        __rin_socket_mmsg_now_ns(&now_ns) != 0)
        return -1;
    remaining_ns = now_ns >= deadline_ns ? 0u : deadline_ns - now_ns;
    timeout->tv_sec = (time_t)(remaining_ns / UINT64_C(1000000000));
    timeout->tv_nsec = (long)(remaining_ns % UINT64_C(1000000000));
    rounded_ms = remaining_ns / UINT64_C(1000000);
    if ((remaining_ns % UINT64_C(1000000)) != 0u) rounded_ms++;
    *poll_ms_out = rounded_ms > (uint64_t)INT_MAX
                       ? INT_MAX : (int)rounded_ms;
    return 0;
}

static inline int sendmmsg(int sockfd, struct mmsghdr* messages,
                           unsigned int count, int flags) {
    intptr_t result;
    __rin_pthread_cancel_point_if_linked();
    if (count > RIN_SOCKET_MMSG_MAX_MESSAGES ||
        (count != 0u && messages == NULL)) {
        errno = count != 0u && messages == NULL ? EFAULT : EINVAL;
        return -1;
    }
    if (count == 0u) return 0;
    if (((unsigned int)flags & MSG_WAITFORONE) != 0u) {
        errno = EINVAL;
        return -1;
    }
    result = __rin_socket_mmsg_call(0, sockfd, messages, count, flags);
    if (__rin_socket_raw_error(result) != 0) {
        result = -1;
    } else if ((uintptr_t)result > (uintptr_t)count) {
        errno = EIO;
        result = -1;
    }
    __rin_pthread_cancel_point_if_linked();
    return (int)result;
}

static inline int recvmmsg(int sockfd, struct mmsghdr* messages,
                           unsigned int count, int flags,
                           struct timespec* timeout) {
    struct timespec timeout_snapshot = {0, 0};
    uint64_t deadline_ns = 0u;
    int has_timeout = timeout != NULL;
    int nonblocking;
    int completed = 0;
    int wait_for_one = ((unsigned int)flags & MSG_WAITFORONE) != 0u;
    int caller_dontwait = ((unsigned int)flags & MSG_DONTWAIT) != 0u;
    unsigned int batch_flags = (unsigned int)flags &
                               ~(unsigned int)MSG_WAITFORONE;
    __rin_pthread_cancel_point_if_linked();
    if (count > RIN_SOCKET_MMSG_MAX_MESSAGES ||
        (count != 0u && messages == NULL)) {
        errno = count != 0u && messages == NULL ? EFAULT : EINVAL;
        return -1;
    }
    if (count == 0u) return 0;
    if (has_timeout) {
        uint64_t duration_ns;
        uint64_t now_ns;
        timeout_snapshot = *timeout;
        if (timeout_snapshot.tv_sec < 0 || timeout_snapshot.tv_nsec < 0 ||
            timeout_snapshot.tv_nsec >= 1000000000L) {
            errno = EINVAL;
            return -1;
        }
        if ((uint64_t)timeout_snapshot.tv_sec >
            (UINT64_MAX - (uint64_t)timeout_snapshot.tv_nsec) /
                UINT64_C(1000000000)) {
            errno = EOVERFLOW;
            return -1;
        }
        duration_ns = (uint64_t)timeout_snapshot.tv_sec *
                          UINT64_C(1000000000) +
                      (uint64_t)timeout_snapshot.tv_nsec;
        if (__rin_socket_mmsg_now_ns(&now_ns) != 0) return -1;
        if (duration_ns > UINT64_MAX - now_ns) {
            errno = EOVERFLOW;
            return -1;
        }
        deadline_ns = now_ns + duration_ns;
    }
    nonblocking = caller_dontwait ? 1 : __rin_socket_mmsg_nonblocking(sockfd);
    if (nonblocking < 0) return -1;
    batch_flags |= MSG_DONTWAIT;

    for (;;) {
        intptr_t raw;
        int wait_result;
        short poll_events = 0;
        if (!caller_dontwait && !nonblocking) {
            struct pollfd descriptor;
            int poll_timeout = -1;
            descriptor.fd = sockfd;
            descriptor.events = POLLIN;
            descriptor.revents = 0;
            if (has_timeout &&
                __rin_socket_mmsg_timeout_update(
                    &timeout_snapshot, deadline_ns, &poll_timeout) != 0)
                goto failed;
            wait_result = poll(&descriptor, 1u, poll_timeout);
            if (wait_result < 0) {
                int saved_errno = errno;
                if (has_timeout &&
                    __rin_socket_mmsg_timeout_update(
                        &timeout_snapshot, deadline_ns, &poll_timeout) == 0)
                    *timeout = timeout_snapshot;
                errno = saved_errno;
                if (completed != 0) return completed;
                return -1;
            }
            if (wait_result == 0) {
                if (has_timeout &&
                    __rin_socket_mmsg_timeout_update(
                        &timeout_snapshot, deadline_ns, &poll_timeout) != 0)
                    goto failed;
                if (has_timeout) *timeout = timeout_snapshot;
                if (completed != 0) return completed;
                errno = EAGAIN;
                return -1;
            }
            poll_events = descriptor.revents;
        }

        raw = __rin_socket_mmsg_call(
            1, sockfd, messages + completed,
            count - (unsigned int)completed, batch_flags);
        if (raw >= 0 &&
            (uintptr_t)raw > (uintptr_t)(count - (unsigned int)completed))
            raw = -EIO;
        if (raw < 0) {
            int error_number = (int)-raw;
            if (error_number == EAGAIN && !caller_dontwait && !nonblocking &&
                (poll_events & (POLLERR | POLLHUP | POLLNVAL)) == 0)
                continue;
            if (has_timeout &&
                __rin_socket_mmsg_timeout_update(
                    &timeout_snapshot, deadline_ns, &wait_result) != 0)
                goto failed;
            if (has_timeout) *timeout = timeout_snapshot;
            if (completed != 0) return completed;
            errno = error_number;
            return -1;
        }
        if (raw == 0) {
            if (has_timeout &&
                __rin_socket_mmsg_timeout_update(
                    &timeout_snapshot, deadline_ns, &wait_result) != 0)
                goto failed;
            if (has_timeout) *timeout = timeout_snapshot;
            return completed;
        }
        completed += (int)raw;
        if (completed >= (int)count || wait_for_one || caller_dontwait ||
            nonblocking) {
            if (has_timeout &&
                __rin_socket_mmsg_timeout_update(
                    &timeout_snapshot, deadline_ns, &wait_result) != 0)
                goto failed;
            if (has_timeout) *timeout = timeout_snapshot;
            return completed;
        }
        /* Blocking mode without MSG_WAITFORONE waits until the vector fills
         * or its deadline expires; poll prevents recvmsg from consuming a
         * message before a later output-copy fault is reported. */
    }

failed:
    if (has_timeout) *timeout = timeout_snapshot;
    return completed != 0 ? completed : -1;
}

/* ═══════════════════════════════════════════════════════════════
 * accept4 - フラグ付きaccept
 * ═══════════════════════════════════════════════════════════════*/

static inline int accept4(int sockfd, struct sockaddr* addr,
                          socklen_t* addrlen, int flags) {
    __RinSocketAcceptWait wait = {0u, 1u, 0, 0};
    __rin_pthread_cancel_point_if_linked();
    for (;;) {
        if (__rin_socket_accept_wait_expired(&wait) != 0) return -1;
        const intptr_t result = _RIN_SOCKET_SYSCALL5(
            SYS_ACCEPT4, sockfd, addr, addrlen, flags, 0u);
        if (result >= 0) {
            int accepted = __rin_socket_fd_result(result);
            if (accepted >= 0)
                __rin_pthread_cancel_point_cleanup_if_linked(
                    __rin_socket_cancel_close_fd, &accepted);
            return accepted;
        }
        __rin_pthread_cancel_point_if_linked();
        (void)__rin_socket_raw_error(result);
        if (errno != EAGAIN ||
            !__rin_socket_accept_backoff(sockfd, &wait))
            return -1;
    }
}

#ifdef __cplusplus
}
#endif
#endif /* !MIDL_PASS */

#endif /* _SYS_SOCKET_H */
