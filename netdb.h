/*
 * RinOS libc - netdb.h
 * ネットワークデータベース関数
 */

#ifndef _NETDB_H
#define _NETDB_H

#include "stddef.h"
#include "stdint.h"
#include "errno.h"
#include "sys/socket.h"
#include "sys/un.h"
#include "netinet/in.h"
#include "arpa/inet.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"
#include "unistd.h"
#include "fcntl.h"
#include "limits.h"
#include "poll.h"
#include "time.h"
#include <rin/net/resolved_protocol.h>
#include <rin/net/ipv6_text.h>

/* libc transport endpoint; the wire ABI intentionally does not carry this
 * deployment-specific socket path. */
#define RIN_LIBC_RESOLVED_SOCKET_PATH "/run/rin/resolved.sock"

#ifndef RIN_NETDB_ALLOCATE
#define RIN_NETDB_ALLOCATE(size) malloc(size)
#endif
#ifndef RIN_NETDB_RELEASE
#define RIN_NETDB_RELEASE(pointer) free(pointer)
#endif
#ifndef RIN_NETDB_IO_TIMEOUT_NS
#define RIN_NETDB_IO_TIMEOUT_NS UINT64_C(30000000000)
#endif
#define RIN_NETDB_TRANSACT_SYSTEM_ERROR (-1)
#define RIN_NETDB_TRANSACT_PROTOCOL_ERROR (-2)
#define RIN_NETDB_TRANSACT_TIMEOUT_ERROR (-3)

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * hostent構造体 - ホスト情報
 * ═══════════════════════════════════════════════════════════════*/

struct hostent {
    char*  h_name;       /* 公式ホスト名 */
    char** h_aliases;    /* エイリアス一覧 */
    int    h_addrtype;   /* アドレスタイプ (AF_INET) */
    int    h_length;     /* アドレス長 */
    char** h_addr_list;  /* アドレス一覧 */
};

#define h_addr h_addr_list[0]  /* 後方互換性 */

/* ═══════════════════════════════════════════════════════════════
 * servent構造体 - サービス情報
 * ═══════════════════════════════════════════════════════════════*/

struct servent {
    char*  s_name;       /* サービス名 */
    char** s_aliases;    /* エイリアス一覧 */
    int    s_port;       /* ポート番号 (ネットワークバイトオーダー) */
    char*  s_proto;      /* プロトコル */
};

/* ═══════════════════════════════════════════════════════════════
 * protoent構造体 - プロトコル情報
 * ═══════════════════════════════════════════════════════════════*/

struct protoent {
    char*  p_name;       /* プロトコル名 */
    char** p_aliases;    /* エイリアス一覧 */
    int    p_proto;      /* プロトコル番号 */
};

/* ═══════════════════════════════════════════════════════════════
 * netent構造体 - ネットワーク情報
 * ═══════════════════════════════════════════════════════════════*/

struct netent {
    char*    n_name;         /* ネットワーク名 */
    char**   n_aliases;      /* エイリアス一覧 */
    int      n_addrtype;     /* アドレスタイプ (AF_INET) */
    uint32_t n_net;          /* ネットワーク番号 (ホストバイトオーダー) */
};

/* ═══════════════════════════════════════════════════════════════
 * addrinfo構造体 - アドレス情報
 * ═══════════════════════════════════════════════════════════════*/

struct addrinfo {
    int              ai_flags;     /* フラグ */
    int              ai_family;    /* アドレスファミリ */
    int              ai_socktype;  /* ソケットタイプ */
    int              ai_protocol;  /* プロトコル */
    socklen_t        ai_addrlen;   /* アドレス長 */
    struct sockaddr* ai_addr;      /* アドレス */
    char*            ai_canonname; /* 正式名 */
    struct addrinfo* ai_next;      /* 次のエントリ */
};

/* ai_flags */
#define AI_PASSIVE     0x0001  /* bind用 */
#define AI_CANONNAME   0x0002  /* 正式名を要求 */
#define AI_NUMERICHOST 0x0004  /* 数値ホスト名のみ */
#define AI_NUMERICSERV 0x0008  /* 数値サービス名のみ */
#define AI_V4MAPPED    0x0010  /* IPv4マップドアドレス */
#define AI_ALL         0x0020  /* すべてのアドレス */
#define AI_ADDRCONFIG  0x0040  /* 設定されたアドレスのみ */

#define RIN_NETDB_GAI_SUPPORTED_FLAGS \
    (RIN_RESOLVED_GAI_SUPPORTED_FLAGS | AI_V4MAPPED | AI_ALL | \
     AI_ADDRCONFIG)

#ifndef RIN_NETDB_ADDRCONFIG_SNAPSHOT
extern int rin_net_addrconfig_snapshot(int* ipv4_configured,
                                       int* ipv6_configured);
#define RIN_NETDB_ADDRCONFIG_SNAPSHOT rin_net_addrconfig_snapshot
#endif

/* ═══════════════════════════════════════════════════════════════
 * エラーコード
 * ═══════════════════════════════════════════════════════════════*/

#define EAI_AGAIN      -3   /* 一時的な失敗 */
#define EAI_BADFLAGS   -1   /* 無効なフラグ */
#define EAI_FAIL       -4   /* 永続的な失敗 */
#define EAI_FAMILY     -6   /* 未サポートのアドレスファミリ */
#define EAI_MEMORY     -10  /* メモリ不足 */
#define EAI_NONAME     -2   /* 名前解決失敗 */
#define EAI_NODATA     -5   /* 名前は存在するが要求データがない */
#define EAI_SERVICE    -8   /* 未サポートのサービス */
#define EAI_SOCKTYPE   -7   /* 未サポートのソケットタイプ */
#define EAI_SYSTEM     -11  /* システムエラー */
#define EAI_OVERFLOW   -12  /* バッファオーバーフロー */

/* h_errno値 */
#ifdef __cplusplus
#define RIN_NETDB_THREAD_LOCAL thread_local
#else
#define RIN_NETDB_THREAD_LOCAL _Thread_local
#endif

#define RIN_NETDB_DATABASE_NAME_MAX 128u
#define RIN_NETDB_DATABASE_MAX_ALIASES 16u
#define RIN_NETDB_DATABASE_LINE_MAX 4096u

typedef struct __rin_netdb_runtime_state_v1 {
    int h_errno_value;
    struct in_addr addr;
    char* addr_list[2];
    char* aliases[1];
    struct hostent hostent;
    char hostname[256];
    struct servent service_result;
    char service_name[RIN_NETDB_DATABASE_NAME_MAX];
    char service_protocol[RIN_NETDB_DATABASE_NAME_MAX];
    char service_alias_storage[RIN_NETDB_DATABASE_MAX_ALIASES]
                              [RIN_NETDB_DATABASE_NAME_MAX];
    char* service_aliases[RIN_NETDB_DATABASE_MAX_ALIASES + 1u];
    FILE* service_file;
    long service_file_offset;
    int service_stayopen;
    struct protoent protocol_result;
    char protocol_name[RIN_NETDB_DATABASE_NAME_MAX];
    char protocol_alias_storage[RIN_NETDB_DATABASE_MAX_ALIASES]
                               [RIN_NETDB_DATABASE_NAME_MAX];
    char* protocol_aliases[RIN_NETDB_DATABASE_MAX_ALIASES + 1u];
    FILE* protocol_file;
    long protocol_file_offset;
    int protocol_stayopen;
    FILE* host_file;
    long host_file_offset;
    int host_stayopen;
    struct in6_addr host_addr6;
    char network_name[RIN_NETDB_DATABASE_NAME_MAX];
    char network_alias_storage[RIN_NETDB_DATABASE_MAX_ALIASES]
                             [RIN_NETDB_DATABASE_NAME_MAX];
    char* network_aliases[RIN_NETDB_DATABASE_MAX_ALIASES + 1u];
    struct netent network_result;
    FILE* network_file;
    long network_file_offset;
    int network_stayopen;
} __rin_netdb_runtime_state_v1;

extern RIN_NETDB_THREAD_LOCAL __rin_netdb_runtime_state_v1
    __rin_netdb_runtime_state;

static inline int* _netdb_h_errno_location(void) {
    return &__rin_netdb_runtime_state.h_errno_value;
}

#define h_errno (*_netdb_h_errno_location())

#define HOST_NOT_FOUND 1
#define TRY_AGAIN      2
#define NO_RECOVERY    3
#define NO_DATA        4
#define NO_ADDRESS     NO_DATA

static inline int _netdb_h_errno_from_gai(int gai_error) {
    switch (gai_error) {
        case EAI_AGAIN: return TRY_AGAIN;
        case EAI_NONAME: return HOST_NOT_FOUND;
        case EAI_NODATA: return NO_DATA;
        default: return NO_RECOVERY;
    }
}

#define NI_MAXHOST 1025
#define NI_MAXSERV 32

#define NI_NUMERICHOST 1
#define NI_NUMERICSERV 2
#define NI_NOFQDN      4
#define NI_NAMEREQD    8
#define NI_DGRAM       16

/* ═══════════════════════════════════════════════════════════════
 * 静的データ (簡易実装用)
 * ═══════════════════════════════════════════════════════════════*/

#define _netdb_addr (__rin_netdb_runtime_state.addr)
#define _netdb_addr_list (__rin_netdb_runtime_state.addr_list)
#define _netdb_aliases (__rin_netdb_runtime_state.aliases)
#define _netdb_hostent (__rin_netdb_runtime_state.hostent)
#define _netdb_hostname (__rin_netdb_runtime_state.hostname)

static inline void _netdb_prepare_hostent_storage(void) {
    _netdb_addr_list[0] = (char*)&_netdb_addr;
    _netdb_addr_list[1] = NULL;
    _netdb_aliases[0] = NULL;
}

static inline int _netdb_parse_ipv4(const char* name, struct in_addr* out) {
    if (!name || !out) return -1;

    unsigned int octets[4] = {0, 0, 0, 0};
    int part = 0;
    unsigned int value = 0;
    int has_digit = 0;

    for (const char* p = name; ; p++) {
        char c = *p;
        if (c >= '0' && c <= '9') {
            unsigned int digit = (unsigned int)(c - '0');
            has_digit = 1;
            if (value > (255U - digit) / 10U) return -1;
            value = value * 10U + digit;
            continue;
        }
        if (c == '.' || c == '\0') {
            if (!has_digit || part >= 4) return -1;
            octets[part++] = value;
            value = 0;
            has_digit = 0;
            if (c == '\0') break;
            continue;
        }
        return -1;
    }

    if (part != 4) return -1;
    out->s_addr = htonl((octets[0] << 24) | (octets[1] << 16) | (octets[2] << 8) | octets[3]);
    return 0;
}

static inline int _netdb_is_localhost(const char* name) {
    static const char localhost_name[] = "localhost";
    if (!name) return 0;
    for (size_t index = 0u; index < sizeof(localhost_name) - 1u; ++index) {
        unsigned char value = (unsigned char)name[index];
        if (value == '\0') return 0;
        if (value >= (unsigned char)'A' && value <= (unsigned char)'Z')
            value = (unsigned char)(value + ((unsigned char)'a' - (unsigned char)'A'));
        if (value != (unsigned char)localhost_name[index]) return 0;
    }
    return name[sizeof(localhost_name) - 1u] == '\0';
}

static inline int _netdb_resolved_now_ns(uint64_t* now_ns_out) {
    struct timespec now;
    uint64_t seconds;
    if (!now_ns_out || clock_gettime(CLOCK_MONOTONIC, &now) != 0) return -1;
    if (now.tv_sec < 0 || now.tv_nsec < 0 || now.tv_nsec >= 1000000000L) {
        errno = EIO;
        return -1;
    }
    seconds = (uint64_t)now.tv_sec;
    if (seconds >
        (UINT64_MAX - (uint64_t)now.tv_nsec) / UINT64_C(1000000000)) {
        errno = EOVERFLOW;
        return -1;
    }
    *now_ns_out = seconds * UINT64_C(1000000000) +
                  (uint64_t)now.tv_nsec;
    return 0;
}

/* Return 0 when ready, 1 when the shared transaction deadline expires, and
 * -1 for a clock or poll failure.  EINTR is retried against the same deadline. */
static inline int _netdb_resolved_wait(int fd, short events,
                                       uint64_t deadline_ns) {
    for (;;) {
        struct pollfd descriptor;
        uint64_t now_ns;
        uint64_t remaining_ns;
        uint64_t timeout_ms;
        int poll_timeout;
        int rc;
        if (_netdb_resolved_now_ns(&now_ns) != 0) return -1;
        if (now_ns >= deadline_ns) {
            errno = ETIMEDOUT;
            return 1;
        }
        remaining_ns = deadline_ns - now_ns;
        timeout_ms = remaining_ns / UINT64_C(1000000);
        if (remaining_ns % UINT64_C(1000000) != 0u) ++timeout_ms;
        poll_timeout = timeout_ms > (uint64_t)INT_MAX
            ? INT_MAX : (int)timeout_ms;
        descriptor.fd = fd;
        descriptor.events = events;
        descriptor.revents = 0;
        rc = poll(&descriptor, 1u, poll_timeout);
        if (rc > 0) return 0;
        if (rc == 0) continue;
        if (errno == EINTR) continue;
        return -1;
    }
}

static inline int _netdb_resolved_deadline_check(uint64_t deadline_ns) {
    uint64_t now_ns;
    if (_netdb_resolved_now_ns(&now_ns) != 0) return -1;
    if (now_ns >= deadline_ns) {
        errno = ETIMEDOUT;
        return 1;
    }
    return 0;
}

static inline int _netdb_resolved_send_all(int fd, const void* buf, size_t len,
                                           uint64_t deadline_ns) {
    const uint8_t* ptr = (const uint8_t*)buf;
    size_t off = 0;
    while (off < len) {
        int deadline_status = _netdb_resolved_deadline_check(deadline_ns);
        if (deadline_status != 0) return deadline_status;
        ssize_t rc = send(fd, ptr + off, len - off, 0);
        if (rc < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                int wait_status = _netdb_resolved_wait(
                    fd, POLLOUT, deadline_ns);
                if (wait_status != 0) return wait_status;
                continue;
            }
            return RIN_NETDB_TRANSACT_SYSTEM_ERROR;
        }
        if (rc == 0) {
            errno = EPIPE;
            return RIN_NETDB_TRANSACT_SYSTEM_ERROR;
        }
        off += (size_t)rc;
    }
    return 0;
}

static inline int _netdb_resolved_recv_all(int fd, void* buf, size_t len,
                                           uint64_t deadline_ns) {
    uint8_t* ptr = (uint8_t*)buf;
    size_t off = 0;
    while (off < len) {
        int deadline_status = _netdb_resolved_deadline_check(deadline_ns);
        if (deadline_status != 0) return deadline_status;
        ssize_t rc = recv(fd, ptr + off, len - off, 0);
        if (rc < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                int wait_status = _netdb_resolved_wait(
                    fd, POLLIN, deadline_ns);
                if (wait_status != 0) return wait_status;
                continue;
            }
            return RIN_NETDB_TRANSACT_SYSTEM_ERROR;
        }
        if (rc == 0) {
            errno = ECONNRESET;
            return RIN_NETDB_TRANSACT_SYSTEM_ERROR;
        }
        off += (size_t)rc;
    }
    return 0;
}

static inline int _netdb_resolved_connect(uint64_t deadline_ns) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int file_flags = fcntl(fd, F_GETFL);
    if (file_flags < 0 || fcntl(fd, F_SETFL, file_flags | O_NONBLOCK) < 0) {
        int saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, RIN_LIBC_RESOLVED_SOCKET_PATH,
            sizeof(addr.sun_path) - 1);
    if (connect(fd, (const struct sockaddr*)&addr, (socklen_t)sizeof(addr)) < 0) {
        int saved_errno = errno;
        if (saved_errno != EINPROGRESS && saved_errno != EALREADY &&
            saved_errno != EAGAIN && saved_errno != EWOULDBLOCK &&
            saved_errno != EINTR) {
            close(fd);
            errno = saved_errno;
            return -1;
        }
        {
            int wait_status = _netdb_resolved_wait(fd, POLLOUT, deadline_ns);
            int connect_error = 0;
            socklen_t error_size = (socklen_t)sizeof(connect_error);
            if (wait_status != 0) {
                saved_errno = errno;
                close(fd);
                errno = saved_errno;
                return wait_status == 1
                    ? RIN_NETDB_TRANSACT_TIMEOUT_ERROR : -1;
            }
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &connect_error,
                           &error_size) != 0) {
                saved_errno = errno;
                close(fd);
                errno = saved_errno;
                return -1;
            }
            if (error_size != (socklen_t)sizeof(connect_error)) {
                close(fd);
                errno = EIO;
                return -1;
            }
            if (connect_error != 0) {
                close(fd);
                errno = connect_error;
                return -1;
            }
        }
    }
    return fd;
}

static inline int _netdb_resolved_transact(uint32_t command,
                                           const void* payload,
                                           uint32_t payload_len,
                                           RinResolvedMsgHeader* out_header,
                                           void* out_payload,
                                           uint32_t out_payload_cap) {
    uint64_t now_ns;
    uint64_t deadline_ns;
    int fd;
    int io_status;
    if (_netdb_resolved_now_ns(&now_ns) != 0) return RIN_NETDB_TRANSACT_SYSTEM_ERROR;
    if (RIN_NETDB_IO_TIMEOUT_NS > UINT64_MAX - now_ns) {
        errno = EOVERFLOW;
        return RIN_NETDB_TRANSACT_SYSTEM_ERROR;
    }
    deadline_ns = now_ns + RIN_NETDB_IO_TIMEOUT_NS;
    fd = _netdb_resolved_connect(deadline_ns);
    if (fd < 0) return fd == RIN_NETDB_TRANSACT_TIMEOUT_ERROR
        ? fd : RIN_NETDB_TRANSACT_SYSTEM_ERROR;

    RinResolvedMsgHeader header;
    memset(&header, 0, sizeof(header));
    header.magic = RIN_RESOLVED_MAGIC;
    header.version = RIN_RESOLVED_VERSION;
    header.command = command;
    header.payload_len = payload_len;

    io_status = _netdb_resolved_send_all(fd, &header, sizeof(header),
                                         deadline_ns);
    if (io_status == 0 && payload_len > 0) {
        io_status = _netdb_resolved_send_all(fd, payload, payload_len,
                                             deadline_ns);
    }
    if (io_status != 0) {
        int saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return io_status == 1 ? RIN_NETDB_TRANSACT_TIMEOUT_ERROR
            : RIN_NETDB_TRANSACT_SYSTEM_ERROR;
    }

    io_status = _netdb_resolved_recv_all(fd, &header, sizeof(header),
                                         deadline_ns);
    if (io_status != 0) {
        int saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return io_status == 1 ? RIN_NETDB_TRANSACT_TIMEOUT_ERROR
            : RIN_NETDB_TRANSACT_SYSTEM_ERROR;
    }
    if (!rin_resolved_response_header_valid(
            &header, command, out_payload_cap)) {
        int saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return RIN_NETDB_TRANSACT_PROTOCOL_ERROR;
    }

    if (header.payload_len > 0) {
        if (!out_payload) {
            int saved_errno = errno;
            close(fd);
            errno = saved_errno;
            return RIN_NETDB_TRANSACT_PROTOCOL_ERROR;
        }
        io_status = _netdb_resolved_recv_all(
            fd, out_payload, header.payload_len, deadline_ns);
        if (io_status != 0) {
            int saved_errno = errno;
            close(fd);
            errno = saved_errno;
            return io_status == 1 ? RIN_NETDB_TRANSACT_TIMEOUT_ERROR
                : RIN_NETDB_TRANSACT_SYSTEM_ERROR;
        }
    }

    close(fd);
    if (out_header) *out_header = header;
    return 0;
}

#ifndef RIN_NETDB_RESOLVED_TRANSACT
#define RIN_NETDB_RESOLVED_TRANSACT _netdb_resolved_transact
#endif

static inline int _netdb_resolved_gai_error(int transact_status) {
    if (transact_status == RIN_NETDB_TRANSACT_SYSTEM_ERROR) return EAI_SYSTEM;
    if (transact_status == RIN_NETDB_TRANSACT_TIMEOUT_ERROR) return EAI_AGAIN;
    return EAI_FAIL;
}

static inline int _netdb_resolved_system_error_status(
    const RinResolvedMsgHeader* header, const void* payload) {
    if (!header || header->status != EAI_SYSTEM) return EAI_FAIL;
    if (header->version == RIN_RESOLVED_VERSION_V1 &&
        header->payload_len == 0u)
        return EAI_SYSTEM;
    if (header->version == RIN_RESOLVED_VERSION_V2 && payload != NULL &&
        rin_resolved_system_error_v2_valid(
            (const RinResolvedSystemErrorV2*)payload,
            header->payload_len, header->status)) {
        errno = ((const RinResolvedSystemErrorV2*)payload)->error_number;
        return EAI_SYSTEM;
    }
    return EAI_FAIL;
}

#include "internal/netdb_database.h"

static inline int _netdb_parse_service_port(const char* service,
                                             int socket_type,
                                             uint16_t* port_out) {
    _NetdbServiceRecord record = { 0 };
    uint32_t port = 0u;
    const char* cursor;
    int lookup_status;
    if (!port_out) return -1;
    if (!service) {
        *port_out = 0u;
        return 0;
    }
    if (!*service) return -1;
    cursor = service;
    while (*cursor >= '0' && *cursor <= '9') {
        uint32_t digit = (uint32_t)(*cursor - '0');
        if (port > (65535u - digit) / 10u) return -1;
        port = port * 10u + digit;
        ++cursor;
    }
    if (*cursor == '\0') {
        *port_out = (uint16_t)port;
        return 0;
    }
    lookup_status = _netdb_find_service_by_name(
        service, NULL, socket_type, &record);
    if (lookup_status != 1) return -1;
    *port_out = record.port;
    return 0;
}
static inline uint32_t _netdb_format_service_port(uint16_t port,
                                                   char output[6]) {
    char reversed[5];
    uint32_t count = 0u;
    uint32_t index;
    do {
        reversed[count++] = (char)('0' + (port % 10u));
        port = (uint16_t)(port / 10u);
    } while (port != 0u);
    for (index = 0u; index < count; ++index) {
        output[index] = reversed[count - index - 1u];
    }
    output[count] = '\0';
    return count;
}

static inline int _netdb_bounded_length(const char* text,
                                         size_t maximum,
                                         size_t* length_out) {
    size_t length = 0u;
    if (!length_out) return -1;
    if (!text) {
        *length_out = 0u;
        return 0;
    }
    while (length <= maximum && text[length] != '\0') ++length;
    if (length > maximum) return -1;
    *length_out = length;
    return 0;
}

static inline int _netdb_service_is_numeric(const char* service,
                                             size_t length) {
    size_t index;
    if (!service) return 1;
    if (length == 0u) return 0;
    for (index = 0u; index < length; ++index) {
        if (service[index] < '0' || service[index] > '9') return 0;
    }
    return 1;
}

typedef struct _NetdbSocketCandidate {
    int socket_type;
    int protocol;
    uint16_t port;
} _NetdbSocketCandidate;

static inline int _netdb_select_socket_candidates(
    const char* service, int socket_type, int protocol,
    _NetdbSocketCandidate candidates[2], size_t* candidate_count) {
    int effective_socket_type;
    int effective_protocol;
    uint16_t port;
    size_t count = 0u;
    static const int socket_types[2] = {SOCK_STREAM, SOCK_DGRAM};
    static const int protocols[2] = {IPPROTO_TCP, IPPROTO_UDP};
    size_t index;

    if (!candidates || !candidate_count) return EAI_FAIL;
    *candidate_count = 0u;
    if (socket_type != 0 || protocol != 0) {
        if (!rin_resolved_getaddrinfo_socket_pair(
                socket_type, protocol, &effective_socket_type,
                &effective_protocol)) {
            return EAI_SOCKTYPE;
        }
        if (_netdb_parse_service_port(
                service, effective_socket_type, &port) != 0) {
            return EAI_SERVICE;
        }
        candidates[0].socket_type = effective_socket_type;
        candidates[0].protocol = effective_protocol;
        candidates[0].port = port;
        *candidate_count = 1u;
        return 0;
    }

    /* An unconstrained request accepts each socket/protocol pair for which
     * the service is defined.  Numeric or absent services are valid for both
     * supported transports; named services retain their database-specific
     * port and protocol. */
    for (index = 0u; index < 2u; ++index) {
        if (_netdb_parse_service_port(service, socket_types[index], &port) !=
            0) {
            continue;
        }
        candidates[count].socket_type = socket_types[index];
        candidates[count].protocol = protocols[index];
        candidates[count].port = port;
        ++count;
    }
    if (count == 0u) return EAI_SERVICE;
    *candidate_count = count;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * ホスト名解決関数
 * ═══════════════════════════════════════════════════════════════*/

static inline int getaddrinfo(const char* node, const char* service,
                              const struct addrinfo* hints,
                              struct addrinfo** res);
static inline void freeaddrinfo(struct addrinfo* res);
static inline int getnameinfo(const struct sockaddr* sa, socklen_t salen,
                              char* host, socklen_t hostlen,
                              char* serv, socklen_t servlen,
                              int flags);

/* gethostbyname - ホスト名からアドレスを取得 */
static inline struct hostent* gethostbyname(const char* name) {
    int lookup_result;
    if (!name) {
        h_errno = NO_RECOVERY;
        return NULL;
    }

    _netdb_prepare_hostent_storage();

    struct addrinfo hints;
    struct addrinfo* ai = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_CANONNAME;
    lookup_result = getaddrinfo(name, NULL, &hints, &ai);
    if (lookup_result != 0 || !ai || !ai->ai_addr) {
        if (ai) freeaddrinfo(ai);
        h_errno = lookup_result != 0
            ? _netdb_h_errno_from_gai(lookup_result) : NO_DATA;
        return NULL;
    }

    memcpy(&_netdb_addr, &((struct sockaddr_in*)ai->ai_addr)->sin_addr, sizeof(_netdb_addr));
    const char* host_name = (ai->ai_canonname && ai->ai_canonname[0]) ? ai->ai_canonname : name;
    int i = 0;
    while (host_name[i] && i < 255) { _netdb_hostname[i] = host_name[i]; i++; }
    _netdb_hostname[i] = '\0';
    freeaddrinfo(ai);

    _netdb_hostent.h_name = _netdb_hostname;
    _netdb_hostent.h_aliases = _netdb_aliases;
    _netdb_hostent.h_addrtype = AF_INET;
    _netdb_hostent.h_length = 4;
    _netdb_hostent.h_addr_list = _netdb_addr_list;
    h_errno = 0;
    return &_netdb_hostent;
}

/* gethostbyaddr - アドレスからホスト名を取得 */
static inline struct hostent* gethostbyaddr(const void* addr, socklen_t len, int type) {
    int lookup_result;
    if (type != AF_INET || !addr || len != (socklen_t)sizeof(struct in_addr)) {
        h_errno = NO_RECOVERY;
        return NULL;
    }

    _netdb_prepare_hostent_storage();

    struct sockaddr_in sa;
    char host[256];
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    memcpy(&sa.sin_addr, addr, sizeof(sa.sin_addr));
    _netdb_addr = sa.sin_addr;

    lookup_result = getnameinfo((const struct sockaddr*)&sa,
                                (socklen_t)sizeof(sa), host,
                                (socklen_t)sizeof(host), NULL, 0,
                                NI_NAMEREQD);
    if (lookup_result != 0) {
        h_errno = lookup_result == EAI_NONAME ||
                  lookup_result == EAI_NODATA
            ? NO_DATA : _netdb_h_errno_from_gai(lookup_result);
        return NULL;
    }
    strncpy(_netdb_hostname, host, sizeof(_netdb_hostname) - 1);
    _netdb_hostname[sizeof(_netdb_hostname) - 1] = '\0';

    _netdb_hostent.h_name = _netdb_hostname;
    _netdb_hostent.h_aliases = _netdb_aliases;
    _netdb_hostent.h_addrtype = AF_INET;
    _netdb_hostent.h_length = 4;
    _netdb_hostent.h_addr_list = _netdb_addr_list;
    h_errno = 0;

    return &_netdb_hostent;
}

/* ═══════════════════════════════════════════════════════════════
 * getaddrinfo/freeaddrinfo
 * ═══════════════════════════════════════════════════════════════*/

static inline void _netdb_map_ipv4_address(
    uint8_t output[RIN_RESOLVED_IPV6_ADDRESS_BYTES],
    const uint8_t input[RIN_RESOLVED_IPV4_ADDRESS_BYTES]) {
    memset(output, 0, RIN_RESOLVED_IPV6_ADDRESS_BYTES);
    output[10] = 0xffu;
    output[11] = 0xffu;
    memcpy(output + 12u, input, RIN_RESOLVED_IPV4_ADDRESS_BYTES);
}

static inline int _netdb_addrconfig_snapshot(int* ipv4_configured,
                                             int* ipv6_configured) {
    if (!ipv4_configured || !ipv6_configured) return -1;
    return RIN_NETDB_ADDRCONFIG_SNAPSHOT(ipv4_configured,
                                         ipv6_configured);
}

static inline int _netdb_resolved_lookup_one(
    const char* node, size_t node_length, uint16_t port, int has_service,
    int family, int socktype, int protocol, int flags,
    uint8_t address[RIN_RESOLVED_IPV6_ADDRESS_BYTES], int* result_family,
    char* canonical_name, size_t canonical_capacity) {
    RinResolvedGetAddrInfoRequest req;
    RinResolvedGetAddrInfoResponse resp;
    RinResolvedMsgHeader header;
    char service_text[6];
    uint8_t local_address[RIN_RESOLVED_IPV6_ADDRESS_BYTES];
    char local_canonical[RIN_RESOLVED_HOST_NAME_MAX + 1u];
    uint32_t node_len = (uint32_t)node_length;
    uint32_t service_len = has_service
        ? _netdb_format_service_port(port, service_text) : 0u;
    size_t payload_len = sizeof(req) + node_len + service_len;
    uint8_t* payload;
    uint8_t* ptr;
    uint8_t outbuf[sizeof(resp) + RIN_RESOLVED_HOST_NAME_MAX];
    uint32_t address_bytes;
    int request_status;
    int rc;

    if (!node || node_length == 0u || !address || !result_family ||
        !canonical_name || canonical_capacity == 0u ||
        ((uint32_t)flags & ~RIN_RESOLVED_GAI_SUPPORTED_FLAGS) != 0u) {
        return EAI_FAIL;
    }
    payload = (uint8_t*)RIN_NETDB_ALLOCATE(payload_len);
    if (!payload) return EAI_MEMORY;
    memset(&req, 0, sizeof(req));
    req.ai_flags = flags;
    req.ai_family = family;
    req.ai_socktype = socktype;
    req.ai_protocol = protocol;
    req.node_len = node_len;
    req.service_len = service_len;
    memcpy(payload, &req, sizeof(req));
    ptr = payload + sizeof(req);
    memcpy(ptr, node, node_len);
    if (service_len > 0u) {
        memcpy(ptr + node_len, service_text, service_len);
    }
    request_status = rin_resolved_getaddrinfo_request_status(
        &req, payload + sizeof(req), (uint32_t)payload_len);
    if (request_status != 0) {
        RIN_NETDB_RELEASE(payload);
        return request_status;
    }

    memset(&header, 0, sizeof(header));
    memset(outbuf, 0, sizeof(outbuf));
    rc = RIN_NETDB_RESOLVED_TRANSACT(
        RIN_RESOLVED_CMD_GETADDRINFO_V1, payload, (uint32_t)payload_len,
        &header, outbuf, (uint32_t)sizeof(outbuf));
    RIN_NETDB_RELEASE(payload);
    if (rc != 0) {
        return _netdb_resolved_gai_error(rc);
    }
    if (!rin_resolved_getaddrinfo_status_valid(header.status)) {
        return EAI_FAIL;
    }
    if (header.status == EAI_SYSTEM)
        return _netdb_resolved_system_error_status(&header, outbuf);
    if (header.status != 0) {
        return header.payload_len == 0u ? header.status : EAI_FAIL;
    }
    if (header.payload_len < sizeof(resp) ||
        header.payload_len > sizeof(outbuf)) {
        return EAI_FAIL;
    }
    memcpy(&resp, outbuf, sizeof(resp));
    if (!rin_resolved_getaddrinfo_response_valid(
            &resp, outbuf + sizeof(resp), header.payload_len,
            &req, htons(port))) {
        return EAI_FAIL;
    }
    address_bytes = rin_resolved_address_bytes_for_family(resp.ai_family);
    if (address_bytes == 0u || resp.canon_len >= canonical_capacity) {
        return EAI_FAIL;
    }
    memset(local_address, 0, sizeof(local_address));
    memcpy(local_address, resp.addr, address_bytes);
    local_canonical[0] = '\0';
    if (resp.canon_len > 0u) {
        memcpy(local_canonical, outbuf + sizeof(resp), resp.canon_len);
        local_canonical[resp.canon_len] = '\0';
    }

    memcpy(address, local_address, sizeof(local_address));
    memcpy(canonical_name, local_canonical, resp.canon_len + 1u);
    *result_family = resp.ai_family;
    return 0;
}

/* The service owns the two DNS transports concurrently.  libc still blocks
 * for its single request, but it no longer opens an AAAA transaction and then
 * serially opens A: the reply is one validated, generation-consistent result
 * carrying every admitted address from each family. */
static inline int _netdb_resolved_lookup_dual(
    const char* node, size_t node_length, uint16_t port, int has_service,
    int socktype, int protocol, int flags,
    uint8_t ipv6_addresses[RIN_RESOLVED_GAI_MAX_RESULTS]
                          [RIN_RESOLVED_IPV6_ADDRESS_BYTES],
    uint32_t* ipv6_count_out,
    uint8_t ipv4_addresses[RIN_RESOLVED_GAI_MAX_RESULTS]
                          [RIN_RESOLVED_IPV4_ADDRESS_BYTES],
    uint32_t* ipv4_count_out, char* canonical_name, size_t canonical_capacity) {
    RinResolvedGetAddrInfoRequest req;
    RinResolvedGetAddrInfoDualResponse resp;
    RinResolvedMsgHeader header;
    char service_text[6];
    uint32_t node_len = (uint32_t)node_length;
    uint32_t service_len = has_service
        ? _netdb_format_service_port(port, service_text) : 0u;
    size_t payload_len = sizeof(req) + node_len + service_len;
    uint8_t* payload;
    uint8_t* ptr;
    uint8_t outbuf[sizeof(resp) +
                   RIN_RESOLVED_GAI_MAX_RESULTS *
                       (RIN_RESOLVED_IPV6_ADDRESS_BYTES +
                        RIN_RESOLVED_IPV4_ADDRESS_BYTES) +
                   RIN_RESOLVED_HOST_NAME_MAX];
    const uint8_t* ipv6_data;
    const uint8_t* ipv4_data;
    const uint8_t* canonical_data;
    uint64_t ipv6_bytes;
    uint64_t ipv4_bytes;
    int request_status;
    int rc;

    if (ipv6_count_out) *ipv6_count_out = 0u;
    if (ipv4_count_out) *ipv4_count_out = 0u;
    if (canonical_name && canonical_capacity > 0u) canonical_name[0] = '\0';
    if (!node || node_length == 0u || !ipv6_addresses || !ipv6_count_out ||
        !ipv4_addresses || !ipv4_count_out || !canonical_name ||
        canonical_capacity == 0u ||
        ((uint32_t)flags & ~RIN_RESOLVED_GAI_SUPPORTED_FLAGS) != 0u) {
        return EAI_FAIL;
    }
    payload = (uint8_t*)RIN_NETDB_ALLOCATE(payload_len);
    if (!payload) return EAI_MEMORY;
    memset(&req, 0, sizeof(req));
    req.ai_flags = flags;
    req.ai_family = AF_UNSPEC;
    req.ai_socktype = socktype;
    req.ai_protocol = protocol;
    req.node_len = node_len;
    req.service_len = service_len;
    memcpy(payload, &req, sizeof(req));
    ptr = payload + sizeof(req);
    memcpy(ptr, node, node_len);
    if (service_len > 0u) memcpy(ptr + node_len, service_text, service_len);
    request_status = rin_resolved_getaddrinfo_request_status(
        &req, payload + sizeof(req), (uint32_t)payload_len);
    if (request_status != 0) {
        RIN_NETDB_RELEASE(payload);
        return request_status;
    }
    memset(&header, 0, sizeof(header));
    memset(outbuf, 0, sizeof(outbuf));
    rc = RIN_NETDB_RESOLVED_TRANSACT(
        RIN_RESOLVED_CMD_GETADDRINFO_DUAL_V3, payload, (uint32_t)payload_len,
        &header, outbuf, (uint32_t)sizeof(outbuf));
    RIN_NETDB_RELEASE(payload);
    if (rc != 0) {
        return _netdb_resolved_gai_error(rc);
    }
    if (!rin_resolved_getaddrinfo_status_valid(header.status)) {
        return EAI_FAIL;
    }
    if (header.status == EAI_SYSTEM)
        return _netdb_resolved_system_error_status(&header, outbuf);
    if (header.status != 0) {
        return header.payload_len == 0u ? header.status : EAI_FAIL;
    }
    if (header.payload_len < sizeof(resp) || header.payload_len > sizeof(outbuf))
        return EAI_FAIL;
    memcpy(&resp, outbuf, sizeof(resp));
    if (resp.ipv6_count > RIN_RESOLVED_GAI_MAX_RESULTS ||
        resp.ipv4_count > RIN_RESOLVED_GAI_MAX_RESULTS) {
        return EAI_FAIL;
    }
    ipv6_data = outbuf + sizeof(resp);
    ipv6_bytes = (uint64_t)resp.ipv6_count *
                 RIN_RESOLVED_IPV6_ADDRESS_BYTES;
    ipv4_data = ipv6_data + ipv6_bytes;
    ipv4_bytes = (uint64_t)resp.ipv4_count *
                 RIN_RESOLVED_IPV4_ADDRESS_BYTES;
    canonical_data = ipv4_data + ipv4_bytes;
    if (!rin_resolved_getaddrinfo_dual_response_valid(
            &resp, ipv6_data, ipv4_data, canonical_data, header.payload_len,
            &req, htons(port)) || resp.canon_len >= canonical_capacity) {
        return EAI_FAIL;
    }
    if (resp.ipv6_count > 0u) {
        memcpy(ipv6_addresses, ipv6_data, (size_t)ipv6_bytes);
    }
    if (resp.ipv4_count > 0u) {
        memcpy(ipv4_addresses, ipv4_data, (size_t)ipv4_bytes);
    }
    if (resp.canon_len > 0u) {
        memcpy(canonical_name, canonical_data, resp.canon_len);
    }
    canonical_name[resp.canon_len] = '\0';
    *ipv6_count_out = resp.ipv6_count;
    *ipv4_count_out = resp.ipv4_count;
    return 0;
}

static inline struct addrinfo* _netdb_allocate_addrinfo_one(
    int flags, int family, int socktype, int protocol, uint16_t port,
    const uint8_t address[RIN_RESOLVED_IPV6_ADDRESS_BYTES],
    const char* canonical_name) {
    struct addrinfo* ai;
    struct sockaddr* socket_address;
    size_t socket_address_size = family == AF_INET6
        ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);

    if (!address || (family != AF_INET && family != AF_INET6)) return NULL;
    ai = (struct addrinfo*)RIN_NETDB_ALLOCATE(sizeof(*ai));
    if (!ai) return NULL;
    memset(ai, 0, sizeof(*ai));
    socket_address = (struct sockaddr*)RIN_NETDB_ALLOCATE(socket_address_size);
    if (!socket_address) {
        RIN_NETDB_RELEASE(ai);
        return NULL;
    }
    memset(socket_address, 0, socket_address_size);
    if (family == AF_INET6) {
        struct sockaddr_in6* address6 = (struct sockaddr_in6*)socket_address;
        address6->sin6_family = AF_INET6;
        address6->sin6_port = htons(port);
        memcpy(address6->sin6_addr.s6_addr, address,
               RIN_RESOLVED_IPV6_ADDRESS_BYTES);
    } else {
        struct sockaddr_in* address4 = (struct sockaddr_in*)socket_address;
        address4->sin_family = AF_INET;
        address4->sin_port = htons(port);
        memcpy(&address4->sin_addr.s_addr, address,
               RIN_RESOLVED_IPV4_ADDRESS_BYTES);
    }

    ai->ai_flags = flags;
    ai->ai_family = family;
    ai->ai_socktype = socktype;
    ai->ai_protocol = protocol;
    ai->ai_addrlen = (socklen_t)socket_address_size;
    ai->ai_addr = socket_address;
    if (canonical_name) {
        size_t length = strlen(canonical_name);
        ai->ai_canonname = (char*)RIN_NETDB_ALLOCATE(length + 1u);
        if (!ai->ai_canonname) {
            RIN_NETDB_RELEASE(socket_address);
            RIN_NETDB_RELEASE(ai);
            return NULL;
        }
        memcpy(ai->ai_canonname, canonical_name, length + 1u);
    }
    return ai;
}

static inline int getaddrinfo(const char* node, const char* service,
                              const struct addrinfo* hints,
                              struct addrinfo** res) {
    size_t node_length;
    size_t service_length;
    int effective_socktype;
    int effective_protocol;
    int node_missing;
    int ipv4_configured = 1;
    int ipv6_configured = 1;
    _NetdbSocketCandidate socket_candidates[2];
    size_t socket_candidate_count = 0u;
    if (!res) return EAI_FAIL;
    *res = NULL;

    int family = hints ? hints->ai_family : AF_UNSPEC;
    int socktype = hints ? hints->ai_socktype : 0;
    int protocol = hints ? hints->ai_protocol : 0;
    int flags = hints ? hints->ai_flags : 0;

    if (((uint32_t)flags & ~RIN_NETDB_GAI_SUPPORTED_FLAGS) != 0u) {
        return EAI_BADFLAGS;
    }
    if (family != AF_UNSPEC && family != AF_INET && family != AF_INET6) {
        return EAI_FAMILY;
    }
    if ((flags & AI_V4MAPPED) != 0 && family != AF_INET6) {
        return EAI_BADFLAGS;
    }
    if ((flags & AI_ALL) != 0 &&
        (family != AF_INET6 || (flags & AI_V4MAPPED) == 0)) {
        return EAI_BADFLAGS;
    }
    if (hints && (hints->ai_addrlen != 0u || hints->ai_addr != NULL ||
                  hints->ai_canonname != NULL || hints->ai_next != NULL)) {
        return EAI_FAIL;
    }
    if (!rin_resolved_getaddrinfo_socket_pair(
            socktype, protocol, &effective_socktype, &effective_protocol)) {
        return EAI_SOCKTYPE;
    }
    if (_netdb_bounded_length(node, RIN_RESOLVED_HOST_NAME_MAX,
                              &node_length) != 0) {
        return EAI_NONAME;
    }
    if (_netdb_bounded_length(service, RIN_RESOLVED_HOST_NAME_MAX,
                              &service_length) != 0) {
        return EAI_SERVICE;
    }
    node_missing = node == NULL;
    if (node != NULL && node_length == 0u) return EAI_NONAME;
    if (node_missing && service_length == 0u) return EAI_NONAME;
    if ((flags & AI_CANONNAME) != 0 && node_missing) return EAI_BADFLAGS;
    if ((flags & AI_NUMERICSERV) != 0 &&
        !_netdb_service_is_numeric(service, service_length)) {
        return EAI_NONAME;
    }

    /* アドレス/サービス解決 */
    uint8_t address[RIN_RESOLVED_GAI_MAX_RESULTS *
                    RIN_RESOLVED_IPV6_ADDRESS_BYTES];
    uint8_t secondary_address[RIN_RESOLVED_GAI_MAX_RESULTS *
                              RIN_RESOLVED_IPV6_ADDRESS_BYTES];
    int result_family;
    int secondary_family = AF_UNSPEC;
    int have_secondary = 0;
    uint32_t primary_address_count = 1u;
    uint32_t secondary_address_count = 0u;
    int unspec_dual = 0;
    int mapped_ipv4_primary = 0;
    char canon_buf[256];
    memset(address, 0, sizeof(address));
    memset(secondary_address, 0, sizeof(secondary_address));
    canon_buf[0] = '\0';
    uint16_t port;
    int resolved_locally = 0;
    {
        int candidate_status = _netdb_select_socket_candidates(
            service, socktype, protocol, socket_candidates,
            &socket_candidate_count);
        if (candidate_status != 0) return candidate_status;
    }
    effective_socktype = socket_candidates[0].socket_type;
    effective_protocol = socket_candidates[0].protocol;
    port = socket_candidates[0].port;

    if ((flags & AI_ADDRCONFIG) != 0) {
        if (_netdb_addrconfig_snapshot(&ipv4_configured,
                                       &ipv6_configured) != 0) {
            return EAI_FAIL;
        }
        if (!ipv4_configured && !ipv6_configured) return EAI_NONAME;
        if (family == AF_INET && !ipv4_configured) return EAI_NONAME;
        if (family == AF_INET6 && !ipv6_configured) {
            if ((flags & AI_V4MAPPED) == 0 || !ipv4_configured)
                return EAI_NONAME;
            mapped_ipv4_primary = 1;
        }
    }
    /* AF_UNSPEC must request both configured families.  Without
     * AI_ADDRCONFIG the platform contract treats both families as eligible;
     * the V3 resolver request starts the two wire lookups together. */
    unspec_dual = family == AF_UNSPEC && ipv4_configured && ipv6_configured;
    if (family == AF_INET6 ||
        (family == AF_UNSPEC &&
         (((flags & AI_ADDRCONFIG) != 0 && ipv6_configured) ||
          unspec_dual))) {
        result_family = AF_INET6;
    } else {
        result_family = AF_INET;
    }

    if (node_missing) {
        if (result_family == AF_INET6) {
            if (mapped_ipv4_primary) {
                uint8_t ipv4_loopback[RIN_RESOLVED_IPV4_ADDRESS_BYTES] = {
                    0u, 0u, 0u, 0u
                };
                if ((flags & AI_PASSIVE) == 0) ipv4_loopback[0] = 127u;
                if ((flags & AI_PASSIVE) == 0) ipv4_loopback[3] = 1u;
                _netdb_map_ipv4_address(address, ipv4_loopback);
            } else {
                if ((flags & AI_PASSIVE) == 0) address[15] = 1u;
            }
            if (unspec_dual) {
                if ((flags & AI_PASSIVE) == 0) {
                    secondary_address[0] = 127u;
                    secondary_address[3] = 1u;
                }
                secondary_family = AF_INET;
                have_secondary = 1;
            } else if ((flags & AI_ALL) != 0 && ipv4_configured &&
                       ipv6_configured) {
                uint8_t ipv4_loopback[RIN_RESOLVED_IPV4_ADDRESS_BYTES] = {
                    0u, 0u, 0u, 0u
                };
                if ((flags & AI_PASSIVE) == 0) ipv4_loopback[0] = 127u;
                if ((flags & AI_PASSIVE) == 0) ipv4_loopback[3] = 1u;
                _netdb_map_ipv4_address(secondary_address, ipv4_loopback);
                secondary_family = AF_INET6;
                have_secondary = 1;
            }
        } else if ((flags & AI_PASSIVE) == 0) {
            address[0] = 127u;
            address[3] = 1u;
        }
        resolved_locally = 1;
    } else {
        struct in_addr parsed_ipv4;
        if ((family != AF_INET6 || (flags & AI_V4MAPPED) != 0) &&
            _netdb_parse_ipv4(node, &parsed_ipv4) == 0) {
            if (!ipv4_configured) return EAI_NONAME;
            if (family == AF_INET6) {
                result_family = AF_INET6;
                _netdb_map_ipv4_address(
                    address, (const uint8_t*)&parsed_ipv4.s_addr);
            } else {
                result_family = AF_INET;
                memcpy(address, &parsed_ipv4.s_addr,
                       RIN_RESOLVED_IPV4_ADDRESS_BYTES);
            }
            resolved_locally = 1;
        } else if (family != AF_INET &&
                   rin_ipv6_text_parse(node, address) == 0) {
            if (!ipv6_configured) return EAI_NONAME;
            result_family = AF_INET6;
            resolved_locally = 1;
        } else if ((flags & AI_NUMERICHOST) != 0) {
            return EAI_NONAME;
        } else if (_netdb_is_localhost(node)) {
            if (result_family == AF_INET6) {
                if (mapped_ipv4_primary) {
                    const uint8_t ipv4_loopback[
                        RIN_RESOLVED_IPV4_ADDRESS_BYTES] = {127u, 0u, 0u, 1u};
                    _netdb_map_ipv4_address(address, ipv4_loopback);
                } else {
                    address[15] = 1u;
                }
                if (unspec_dual) {
                    secondary_address[0] = 127u;
                    secondary_address[3] = 1u;
                    secondary_family = AF_INET;
                    have_secondary = 1;
                } else if ((flags & AI_ALL) != 0 && ipv4_configured &&
                           ipv6_configured) {
                    const uint8_t ipv4_loopback[
                        RIN_RESOLVED_IPV4_ADDRESS_BYTES] = {
                            127u, 0u, 0u, 1u
                        };
                    _netdb_map_ipv4_address(
                        secondary_address, ipv4_loopback);
                    secondary_family = AF_INET6;
                    have_secondary = 1;
                }
            } else {
                address[0] = 127u;
                address[3] = 1u;
            }
            strncpy(canon_buf, "localhost", sizeof(canon_buf) - 1);
            canon_buf[sizeof(canon_buf) - 1] = '\0';
            resolved_locally = 1;
        }
    }

    if (!resolved_locally) {
        int lookup_status;
        int wire_flags = flags & ~(AI_V4MAPPED | AI_ALL | AI_ADDRCONFIG);
        int dual_lookup = unspec_dual || (flags & AI_ALL) != 0;
        if (dual_lookup) {
            if (unspec_dual) {
                uint8_t ipv6_addresses[RIN_RESOLVED_GAI_MAX_RESULTS]
                                      [RIN_RESOLVED_IPV6_ADDRESS_BYTES];
                uint8_t ipv4_addresses[RIN_RESOLVED_GAI_MAX_RESULTS]
                                      [RIN_RESOLVED_IPV4_ADDRESS_BYTES];
                uint32_t ipv6_count = 0u;
                uint32_t ipv4_count = 0u;
                memset(ipv6_addresses, 0, sizeof(ipv6_addresses));
                memset(ipv4_addresses, 0, sizeof(ipv4_addresses));
                lookup_status = _netdb_resolved_lookup_dual(
                    node, node_length, port, service && *service,
                    effective_socktype, effective_protocol, wire_flags,
                    ipv6_addresses, &ipv6_count, ipv4_addresses, &ipv4_count,
                    canon_buf, sizeof(canon_buf));
                if (lookup_status != 0) return lookup_status;
                if (ipv6_count > 0u) {
                    memcpy(address, ipv6_addresses,
                           (size_t)ipv6_count *
                               RIN_RESOLVED_IPV6_ADDRESS_BYTES);
                    result_family = AF_INET6;
                    primary_address_count = ipv6_count;
                    if (ipv4_count > 0u) {
                        memcpy(secondary_address, ipv4_addresses,
                               (size_t)ipv4_count *
                                   RIN_RESOLVED_IPV4_ADDRESS_BYTES);
                        secondary_family = AF_INET;
                        secondary_address_count = ipv4_count;
                        have_secondary = 1;
                    }
                } else if (ipv4_count > 0u) {
                    memcpy(address, ipv4_addresses,
                           (size_t)ipv4_count *
                               RIN_RESOLVED_IPV4_ADDRESS_BYTES);
                    result_family = AF_INET;
                    primary_address_count = ipv4_count;
                } else {
                    return EAI_FAIL;
                }
            } else {
                uint8_t ipv6_addresses[RIN_RESOLVED_GAI_MAX_RESULTS]
                                      [RIN_RESOLVED_IPV6_ADDRESS_BYTES];
                uint8_t ipv4_addresses[RIN_RESOLVED_GAI_MAX_RESULTS]
                                      [RIN_RESOLVED_IPV4_ADDRESS_BYTES];
                uint32_t ipv6_count = 0u;
                uint32_t ipv4_count = 0u;
                uint32_t index;

                memset(ipv6_addresses, 0, sizeof(ipv6_addresses));
                memset(ipv4_addresses, 0, sizeof(ipv4_addresses));
                lookup_status = _netdb_resolved_lookup_dual(
                    node, node_length, port, service && *service,
                    effective_socktype, effective_protocol, wire_flags,
                    ipv6_addresses, &ipv6_count, ipv4_addresses, &ipv4_count,
                    canon_buf, sizeof(canon_buf));
                if (lookup_status != 0) return lookup_status;

                /* AI_ALL is an AF_INET6 contract.  Keep every admitted AAAA
                 * result and append every admitted A result as an IPv4-mapped
                 * IPv6 address.  The resolver may return both families even
                 * with AI_ADDRCONFIG, so filter each family against the same
                 * configuration snapshot used before the query. */
                if (!ipv6_configured) ipv6_count = 0u;
                if (!ipv4_configured) ipv4_count = 0u;
                if (ipv6_count > 0u) {
                    memcpy(address, ipv6_addresses,
                           (size_t)ipv6_count *
                               RIN_RESOLVED_IPV6_ADDRESS_BYTES);
                    primary_address_count = ipv6_count;
                }
                if (ipv4_count > 0u) {
                    for (index = 0u; index < ipv4_count; ++index) {
                        _netdb_map_ipv4_address(
                            secondary_address +
                                (size_t)index *
                                    RIN_RESOLVED_IPV6_ADDRESS_BYTES,
                            ipv4_addresses[index]);
                    }
                    secondary_address_count = ipv4_count;
                    secondary_family = AF_INET6;
                    have_secondary = ipv6_count > 0u;
                }
                if (ipv6_count == 0u && ipv4_count > 0u) {
                    memcpy(address, secondary_address,
                           (size_t)ipv4_count *
                               RIN_RESOLVED_IPV6_ADDRESS_BYTES);
                    primary_address_count = ipv4_count;
                    have_secondary = 0;
                } else if (ipv6_count == 0u && ipv4_count == 0u) {
                    return EAI_NONAME;
                }
                result_family = AF_INET6;
            }
        } else {
            int lookup_family = family;
            int map_lookup_result = 0;
            if ((flags & AI_ADDRCONFIG) != 0) {
                if (family == AF_UNSPEC) {
                    lookup_family = ipv6_configured ? AF_INET6 : AF_INET;
                } else if (mapped_ipv4_primary) {
                    lookup_family = AF_INET;
                    map_lookup_result = 1;
                }
            }
            if ((flags & AI_ALL) != 0) {
                lookup_family = ipv6_configured ? AF_INET6 : AF_INET;
                map_lookup_result = lookup_family == AF_INET;
            }
            lookup_status = _netdb_resolved_lookup_one(
                node, node_length, port, service && *service,
                lookup_family, effective_socktype, effective_protocol,
                wire_flags,
                address, &result_family, canon_buf, sizeof(canon_buf));
            if (lookup_status == 0 && map_lookup_result) {
                uint8_t ipv4_address[RIN_RESOLVED_IPV4_ADDRESS_BYTES];
                if (result_family != AF_INET) return EAI_FAIL;
                memcpy(ipv4_address, address, sizeof(ipv4_address));
                _netdb_map_ipv4_address(address, ipv4_address);
                result_family = AF_INET6;
            } else if ((lookup_status == EAI_NONAME ||
                        lookup_status == EAI_NODATA) &&
                       lookup_family == AF_INET6 &&
                       (flags & AI_V4MAPPED) != 0 && ipv4_configured) {
                uint8_t ipv4_address[RIN_RESOLVED_IPV6_ADDRESS_BYTES];
                int fallback_family = AF_UNSPEC;
                memset(ipv4_address, 0, sizeof(ipv4_address));
                canon_buf[0] = '\0';
                lookup_status = _netdb_resolved_lookup_one(
                    node, node_length, port, service && *service,
                    AF_INET, effective_socktype, effective_protocol,
                    wire_flags, ipv4_address, &fallback_family, canon_buf,
                    sizeof(canon_buf));
                if (lookup_status == 0) {
                    if (fallback_family != AF_INET) return EAI_FAIL;
                    _netdb_map_ipv4_address(address, ipv4_address);
                    result_family = AF_INET6;
                }
            }
        }
        if (lookup_status != 0) return lookup_status;
    }

    const char* primary_canonical = NULL;
    struct addrinfo* ai = NULL;
    struct addrinfo** next_link = &ai;
    size_t address_index;
    if ((flags & AI_CANONNAME) != 0 && !node_missing) {
        primary_canonical = canon_buf[0] ? canon_buf : node;
    }
    if (have_secondary && secondary_address_count == 0u) {
        /* Local/narrow V1 paths contribute exactly one secondary address.
         * V3 sets the real count above and is never silently truncated. */
        secondary_address_count = 1u;
    }
    for (address_index = 0u; address_index < (have_secondary ? 2u : 1u);
         ++address_index) {
        const uint8_t* address_set = address_index == 0u
            ? address : secondary_address;
        int candidate_family = address_index == 0u
            ? result_family : secondary_family;
        uint32_t address_count = address_index == 0u
            ? primary_address_count : secondary_address_count;
        uint32_t address_bytes = candidate_family == AF_INET6
            ? RIN_RESOLVED_IPV6_ADDRESS_BYTES : RIN_RESOLVED_IPV4_ADDRESS_BYTES;
        uint32_t address_offset;
        if (address_count == 0u ||
            address_count > RIN_RESOLVED_GAI_MAX_RESULTS) {
            freeaddrinfo(ai);
            return EAI_FAIL;
        }
        for (address_offset = 0u; address_offset < address_count;
             ++address_offset) {
            const uint8_t* candidate_address =
                address_set + (size_t)address_offset * address_bytes;
            size_t socket_index;
            for (socket_index = 0u; socket_index < socket_candidate_count;
                 ++socket_index) {
                const char* candidate_canonical =
                    address_index == 0u && address_offset == 0u &&
                    socket_index == 0u ? primary_canonical : NULL;
                struct addrinfo* candidate = _netdb_allocate_addrinfo_one(
                    flags, candidate_family,
                    socket_candidates[socket_index].socket_type,
                    socket_candidates[socket_index].protocol,
                    socket_candidates[socket_index].port, candidate_address,
                    candidate_canonical);
                if (!candidate) {
                    freeaddrinfo(ai);
                    return EAI_MEMORY;
                }
                *next_link = candidate;
                next_link = &candidate->ai_next;
            }
        }
    }

    *res = ai;
    return 0;
}

static inline void freeaddrinfo(struct addrinfo* res) {
    while (res) {
        struct addrinfo* next = res->ai_next;
        if (res->ai_addr) RIN_NETDB_RELEASE(res->ai_addr);
        if (res->ai_canonname) RIN_NETDB_RELEASE(res->ai_canonname);
        RIN_NETDB_RELEASE(res);
        res = next;
    }
}

/* gai_strerror - エラーメッセージを取得 */
static inline const char* gai_strerror(int errcode) {
    switch (errcode) {
        case 0: return "Success";
        case EAI_AGAIN: return "Temporary failure in name resolution";
        case EAI_BADFLAGS: return "Invalid flags";
        case EAI_FAIL: return "Non-recoverable failure in name resolution";
        case EAI_FAMILY: return "Address family not supported";
        case EAI_MEMORY: return "Memory allocation failure";
        case EAI_NONAME: return "Name does not resolve";
        case EAI_NODATA: return "No address data available";
        case EAI_OVERFLOW: return "Argument buffer overflow";
        case EAI_SERVICE: return "Service not supported";
        case EAI_SOCKTYPE: return "Socket type not supported";
        case EAI_SYSTEM: return "System error";
        default: return "Unknown error";
    }
}

/* ═══════════════════════════════════════════════════════════════
 * getnameinfo
 * ═══════════════════════════════════════════════════════════════*/

static inline int _netdb_ipv6_extract_ipv4(
    const uint8_t address[RIN_RESOLVED_IPV6_ADDRESS_BYTES],
    uint8_t output[RIN_RESOLVED_IPV4_ADDRESS_BYTES]) {
    size_t index;
    int mapped = 1;
    int compatible = 1;
    int all_zero = 1;
    int loopback = 1;
    if (!address || !output) return 0;
    for (index = 0u; index < 10u; ++index) {
        if (address[index] != 0u) mapped = 0;
    }
    if (mapped && address[10] == 0xffu && address[11] == 0xffu) {
        memcpy(output, address + 12u, RIN_RESOLVED_IPV4_ADDRESS_BYTES);
        return 1;
    }
    for (index = 0u; index < 12u; ++index) {
        if (address[index] != 0u) compatible = 0;
    }
    for (index = 12u; index < RIN_RESOLVED_IPV6_ADDRESS_BYTES; ++index) {
        if (address[index] != 0u) all_zero = 0;
    }
    if (address[12] != 0u || address[13] != 0u || address[14] != 0u ||
        address[15] != 1u) {
        loopback = 0;
    }
    if (compatible && !all_zero && !loopback) {
        memcpy(output, address + 12u, RIN_RESOLVED_IPV4_ADDRESS_BYTES);
        return 1;
    }
    return 0;
}

static inline int getnameinfo(const struct sockaddr* sa, socklen_t salen,
                              char* host, socklen_t hostlen,
                              char* serv, socklen_t servlen,
                              int flags) {
    const struct sockaddr_in* sin;
    const struct sockaddr_in6* sin6;
    _NetdbServiceRecord service_record;
    char numeric_host[RIN_IPV6_TEXT_MAX];
    char numeric_service[NI_MAXSERV];
    char service_text[NI_MAXSERV];
    uint8_t embedded_ipv4[RIN_RESOLVED_IPV4_ADDRESS_BYTES];
    const uint8_t* lookup_address;
    size_t numeric_host_length;
    size_t numeric_service_length;
    size_t service_text_length;
    uint32_t lookup_address_length;
    int lookup_family;
    uint16_t port;
    const char* service_protocol;
    int service_lookup_status;
    int saved_errno;
    int formatted;
    int host_requested;
    int service_requested;
    if (!sa) return EAI_FAMILY;
    if (sa->sa_family == AF_INET) {
        if (salen < (socklen_t)sizeof(struct sockaddr_in)) return EAI_FAMILY;
        sin = (const struct sockaddr_in*)sa;
        sin6 = NULL;
    } else if (sa->sa_family == AF_INET6) {
        if (salen < (socklen_t)sizeof(struct sockaddr_in6)) return EAI_FAMILY;
        sin = NULL;
        sin6 = (const struct sockaddr_in6*)sa;
    } else {
        return EAI_FAMILY;
    }

    if (((uint32_t)flags & ~RIN_RESOLVED_NAMEINFO_SUPPORTED_FLAGS) != 0u) {
        return EAI_BADFLAGS;
    }
    if ((host != NULL && hostlen == 0u) ||
        (serv != NULL && servlen == 0u)) {
        return EAI_OVERFLOW;
    }
    host_requested = host != NULL;
    service_requested = serv != NULL;
    if (!host_requested && !service_requested) return EAI_NONAME;

    memset(numeric_host, 0, sizeof(numeric_host));
    memset(numeric_service, 0, sizeof(numeric_service));
    if (sin6 && _netdb_ipv6_extract_ipv4(
                    sin6->sin6_addr.s6_addr, embedded_ipv4)) {
        lookup_family = AF_INET;
        lookup_address = embedded_ipv4;
        lookup_address_length = RIN_RESOLVED_IPV4_ADDRESS_BYTES;
    } else if (sin6) {
        lookup_family = AF_INET6;
        lookup_address = sin6->sin6_addr.s6_addr;
        lookup_address_length = RIN_RESOLVED_IPV6_ADDRESS_BYTES;
    } else {
        lookup_family = AF_INET;
        lookup_address = (const uint8_t*)&sin->sin_addr.s_addr;
        lookup_address_length = RIN_RESOLVED_IPV4_ADDRESS_BYTES;
    }
    if (sin6 && (flags & NI_NUMERICHOST) != 0) {
        /* Reverse lookup treats IPv4-mapped/compatible IPv6 addresses as
         * IPv4, but NI_NUMERICHOST reports the supplied sockaddr's family. */
        if (rin_ipv6_text_format(sin6->sin6_addr.s6_addr, numeric_host,
                                 sizeof(numeric_host)) != 0)
            return EAI_FAIL;
    } else if (lookup_family == AF_INET6) {
        if (rin_ipv6_text_format(sin6->sin6_addr.s6_addr, numeric_host,
                                 sizeof(numeric_host)) != 0)
            return EAI_FAIL;
    } else {
        uint32_t address;
        memcpy(&address, lookup_address, sizeof(address));
        address = ntohl(address);
        formatted = snprintf(numeric_host, sizeof(numeric_host),
                             "%u.%u.%u.%u",
                             (unsigned)((address >> 24) & 0xFF),
                             (unsigned)((address >> 16) & 0xFF),
                             (unsigned)((address >> 8) & 0xFF),
                             (unsigned)(address & 0xFF));
        if (formatted <= 0 || (size_t)formatted >= sizeof(numeric_host))
            return EAI_FAIL;
    }
    formatted = snprintf(
        numeric_service, sizeof(numeric_service), "%u",
        (unsigned)ntohs(sin6 ? sin6->sin6_port : sin->sin_port));
    if (formatted <= 0 || (size_t)formatted >= sizeof(numeric_service))
        return EAI_FAIL;
    port = ntohs(sin6 ? sin6->sin6_port : sin->sin_port);
    service_text_length = (size_t)formatted;
    memcpy(service_text, numeric_service, service_text_length + 1u);
    if ((flags & NI_NUMERICSERV) == 0) {
        service_protocol = (flags & NI_DGRAM) != 0 ? "udp" : "tcp";
        saved_errno = errno;
        service_lookup_status = _netdb_find_service_by_port(
            port, service_protocol, &service_record);
        errno = saved_errno;
        if (service_lookup_status == 1) {
            service_text_length = strlen(service_record.name);
            if (service_text_length >= sizeof(service_text)) return EAI_FAIL;
            memcpy(service_text, service_record.name,
                   service_text_length + 1u);
        }
    }
    numeric_host_length = strlen(numeric_host);
    numeric_service_length = (size_t)formatted;

    if (!host_requested) {
        if (service_text_length + 1u > (size_t)servlen)
            return EAI_OVERFLOW;
        memcpy(serv, service_text, service_text_length + 1u);
        return 0;
    }

    if ((flags & NI_NUMERICHOST) != 0) {
        if (numeric_host_length + 1u > (size_t)hostlen ||
            (service_requested &&
             service_text_length + 1u > (size_t)servlen))
            return EAI_OVERFLOW;
        memcpy(host, numeric_host, numeric_host_length + 1u);
        if (service_requested)
            memcpy(serv, service_text, service_text_length + 1u);
        return 0;
    }

    RinResolvedGetNameInfoRequest req;
    RinResolvedGetNameInfoResponse resp;
    RinResolvedMsgHeader header;
    uint8_t outbuf[sizeof(resp) + NI_MAXHOST + NI_MAXSERV];
    memset(&req, 0, sizeof(req));
    req.flags = (uint32_t)flags;
    req.family = (uint32_t)lookup_family;
    req.addr_len = lookup_address_length;
    memcpy(req.addr, lookup_address, lookup_address_length);
    req.port = sin6 ? sin6->sin6_port : sin->sin_port;

    {
        int transact_status = RIN_NETDB_RESOLVED_TRANSACT(
            RIN_RESOLVED_CMD_GETNAMEINFO_V1, &req,
            (uint32_t)sizeof(req), &header, outbuf,
            (uint32_t)sizeof(outbuf));
        if (transact_status != 0) {
            return _netdb_resolved_gai_error(transact_status);
        }
    }
    if (!rin_resolved_getnameinfo_status_valid(header.status)) {
        return EAI_FAIL;
    }
    if (header.status == EAI_SYSTEM)
        return _netdb_resolved_system_error_status(&header, outbuf);
    if (header.status != 0 && header.status != EAI_NONAME)
        return header.status < 0 ? header.status : EAI_FAIL;
    if (header.status == EAI_NONAME && host_requested &&
        (flags & NI_NAMEREQD) != 0) {
        return header.status;
    }
    if (header.payload_len < sizeof(resp)) {
        return EAI_FAIL;
    }
    memcpy(&resp, outbuf, sizeof(resp));
    if (!rin_resolved_getnameinfo_payload_valid(
            &resp, outbuf, header.payload_len)) {
        return EAI_FAIL;
    }
    {
        const uint8_t* response_host = outbuf + sizeof(resp);
        const uint8_t* response_service = response_host + resp.host_len;
        if (resp.serv_len != numeric_service_length ||
            memcmp(response_service, numeric_service,
                   numeric_service_length) != 0)
            return EAI_FAIL;
        if (header.status == EAI_NONAME) {
            if (resp.host_len != numeric_host_length ||
                memcmp(response_host, numeric_host,
                       numeric_host_length) != 0)
                return EAI_FAIL;
        } else if (!rin_resolved_dns_name_valid(
                       (const char*)response_host, resp.host_len)) {
            return EAI_FAIL;
        }
    }
    if ((size_t)resp.host_len + 1u > (size_t)hostlen ||
        (service_requested &&
         service_text_length + 1u > (size_t)servlen)) {
        return EAI_OVERFLOW;
    }
    if (host_requested) {
        memcpy(host, outbuf + sizeof(resp), resp.host_len);
        host[resp.host_len] = '\0';
    }
    if (service_requested) {
        memcpy(serv, service_text, service_text_length + 1u);
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * サービス/プロトコル関数
 * ═══════════════════════════════════════════════════════════════*/

static inline struct servent* getservbyname(const char* name, const char* proto) {
    _NetdbServiceRecord record;
    int saved_errno = errno;
    int status;
    if (!name) {
        errno = EINVAL;
        return NULL;
    }
    status = _netdb_find_service_by_name(name, proto, 0, &record);
    if (status <= 0) {
        if (status == 0) errno = ENOENT;
        return NULL;
    }
    {
        struct servent* result = _netdb_publish_service(&record);
        errno = saved_errno;
        return result;
    }
}

static inline struct servent* getservbyport(int port, const char* proto) {
    _NetdbServiceRecord record;
    struct servent* result;
    int saved_errno = errno;
    int status;
    if (port < 0 || port > 65535) {
        errno = EINVAL;
        return NULL;
    }
    status = _netdb_find_service_by_port(ntohs((uint16_t)port), proto,
                                         &record);
    if (status <= 0) {
        if (status == 0) errno = ENOENT;
        return NULL;
    }
    result = _netdb_publish_service(&record);
    errno = saved_errno;
    return result;
}

static inline struct protoent* getprotobyname(const char* name) {
    _NetdbProtocolRecord record;
    struct protoent* result;
    int saved_errno = errno;
    int status;
    if (!name) {
        errno = EINVAL;
        return NULL;
    }
    status = _netdb_find_protocol_by_name(name, &record);
    if (status <= 0) {
        if (status == 0) errno = ENOENT;
        return NULL;
    }
    result = _netdb_publish_protocol(&record);
    errno = saved_errno;
    return result;
}

static inline struct protoent* getprotobynumber(int proto) {
    _NetdbProtocolRecord record;
    struct protoent* result;
    int saved_errno = errno;
    int status = _netdb_find_protocol_by_number(proto, &record);
    if (status <= 0) {
        if (status == 0) errno = ENOENT;
        return NULL;
    }
    result = _netdb_publish_protocol(&record);
    errno = saved_errno;
    return result;
}

static inline const char* hstrerror(int err) {
    switch (err) {
        case HOST_NOT_FOUND: return "Host not found";
        case TRY_AGAIN: return "Try again";
        case NO_RECOVERY: return "Non-recoverable error";
        case NO_DATA: return "No data";
        default: return "Unknown error";
    }
}

#ifndef RIN_NETDB_WRITE
#define RIN_NETDB_WRITE write
#endif

static inline int _netdb_herror_write_all(const char* text, size_t length) {
    size_t offset = 0u;
    while (offset < length) {
        ssize_t result = RIN_NETDB_WRITE(STDERR_FILENO, text + offset,
                                         length - offset);
        if (result < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (result == 0 || (size_t)result > length - offset) return -1;
        offset += (size_t)result;
    }
    return 0;
}

static inline void herror(const char* s) {
    const char* message = hstrerror(h_errno);
    int saved_errno = errno;
    if (s && *s) {
        if (_netdb_herror_write_all(s, strlen(s)) != 0 ||
            _netdb_herror_write_all(": ", 2u) != 0) {
            errno = saved_errno;
            return;
        }
    }
    if (_netdb_herror_write_all(message, strlen(message)) == 0) {
        (void)_netdb_herror_write_all("\n", 1u);
    }
    errno = saved_errno;
}

#ifdef __cplusplus
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * Re-entrant (_r) forms.  These share the static implementation,
 * then copy the published entry into the caller buffer, so they
 * must follow the non-re-entrant definitions above.
 * ══════════════════════════════════════════════════════════════*/

/* The re-entrant lookups share the static implementation, then copy the
 * published entry into the caller buffer.  Publication uses thread-local
 * state and the copy happens before returning, so no static storage
 * escapes. */
static inline int getservbyname_r(const char* name, const char* proto,
                                  struct servent* ret, char* buf,
                                  size_t buflen, struct servent** result)
{
    struct servent* found;
    if (result == NULL) return EINVAL;
    *result = NULL;
    if (name == NULL || ret == NULL) return EINVAL;
    found = getservbyname(name, proto);
    if (found == NULL) return errno != 0 ? errno : ENOENT;
    return _netdb_pack_servent(buf, buflen, found, ret, result);
}

static inline int getservbyport_r(int port, const char* proto,
                                  struct servent* ret, char* buf,
                                  size_t buflen, struct servent** result)
{
    struct servent* found;
    if (result == NULL) return EINVAL;
    *result = NULL;
    if (ret == NULL) return EINVAL;
    found = getservbyport(port, proto);
    if (found == NULL) return errno != 0 ? errno : ENOENT;
    return _netdb_pack_servent(buf, buflen, found, ret, result);
}

static inline int getservent_r(struct servent* ret, char* buf, size_t buflen,
                               struct servent** result)
{
    int saved_errno = errno;
    struct servent* found;
    if (result == NULL) return EINVAL;
    *result = NULL;
    if (ret == NULL) return EINVAL;
    found = getservent();
    if (found == NULL) return errno == saved_errno ? ENOENT : errno;
    return _netdb_pack_servent(buf, buflen, found, ret, result);
}

static inline int getprotobyname_r(const char* name, struct protoent* ret,
                                   char* buf, size_t buflen,
                                   struct protoent** result)
{
    struct protoent* found;
    if (result == NULL) return EINVAL;
    *result = NULL;
    if (name == NULL || ret == NULL) return EINVAL;
    found = getprotobyname(name);
    if (found == NULL) return errno != 0 ? errno : ENOENT;
    return _netdb_pack_protoent(buf, buflen, found, ret, result);
}

static inline int getprotobynumber_r(int proto, struct protoent* ret,
                                     char* buf, size_t buflen,
                                     struct protoent** result)
{
    struct protoent* found;
    if (result == NULL) return EINVAL;
    *result = NULL;
    if (ret == NULL) return EINVAL;
    found = getprotobynumber(proto);
    if (found == NULL) return errno != 0 ? errno : ENOENT;
    return _netdb_pack_protoent(buf, buflen, found, ret, result);
}

static inline int getprotoent_r(struct protoent* ret, char* buf,
                                size_t buflen, struct protoent** result)
{
    int saved_errno = errno;
    struct protoent* found;
    if (result == NULL) return EINVAL;
    *result = NULL;
    if (ret == NULL) return EINVAL;
    found = getprotoent();
    if (found == NULL) return errno == saved_errno ? ENOENT : errno;
    return _netdb_pack_protoent(buf, buflen, found, ret, result);
}

static inline int gethostbyname_r(const char* name, struct hostent* ret,
                                  char* buf, size_t buflen,
                                  struct hostent** result, int* h_errnop)
{
    struct hostent* found;
    int pack_status;
    if (result == NULL) return EINVAL;
    *result = NULL;
    if (name == NULL || ret == NULL || h_errnop == NULL) return EINVAL;
    found = gethostbyname(name);
    if (found == NULL) {
        *h_errnop = h_errno;
        return errno != 0 ? errno : ENOENT;
    }
    pack_status = _netdb_pack_hostent(buf, buflen, found, ret, result);
    if (pack_status != 0) {
        *h_errnop = NO_RECOVERY;
        return pack_status;
    }
    *h_errnop = 0;
    return 0;
}

static inline int gethostbyaddr_r(const void* addr, socklen_t len, int type,
                                  struct hostent* ret, char* buf,
                                  size_t buflen, struct hostent** result,
                                  int* h_errnop)
{
    struct hostent* found;
    int pack_status;
    if (result == NULL) return EINVAL;
    *result = NULL;
    if (addr == NULL || ret == NULL || h_errnop == NULL) return EINVAL;
    found = gethostbyaddr(addr, len, type);
    if (found == NULL) {
        *h_errnop = h_errno;
        return errno != 0 ? errno : ENOENT;
    }
    pack_status = _netdb_pack_hostent(buf, buflen, found, ret, result);
    if (pack_status != 0) {
        *h_errnop = NO_RECOVERY;
        return pack_status;
    }
    *h_errnop = 0;
    return 0;
}

static inline int gethostent_r(struct hostent* ret, char* buf, size_t buflen,
                               struct hostent** result, int* h_errnop)
{
    int saved_errno = errno;
    struct hostent* found;
    int pack_status;
    if (result == NULL) return EINVAL;
    *result = NULL;
    if (ret == NULL || h_errnop == NULL) return EINVAL;
    found = gethostent();
    if (found == NULL) {
        *h_errnop = h_errno;
        return errno == saved_errno ? ENOENT : errno;
    }
    pack_status = _netdb_pack_hostent(buf, buflen, found, ret, result);
    if (pack_status != 0) {
        *h_errnop = NO_RECOVERY;
        return pack_status;
    }
    *h_errnop = 0;
    return 0;
}

static inline int getnetbyname_r(const char* name, struct netent* ret,
                                 char* buf, size_t buflen,
                                 struct netent** result, int* h_errnop)
{
    struct netent* found;
    int pack_status;
    if (result == NULL) return EINVAL;
    *result = NULL;
    if (name == NULL || ret == NULL || h_errnop == NULL) return EINVAL;
    found = getnetbyname(name);
    if (found == NULL) {
        *h_errnop = h_errno;
        return errno != 0 ? errno : ENOENT;
    }
    pack_status = _netdb_pack_netent(buf, buflen, found, ret, result);
    if (pack_status != 0) {
        *h_errnop = NO_RECOVERY;
        return pack_status;
    }
    *h_errnop = 0;
    return 0;
}

static inline int getnetbyaddr_r(uint32_t net, int type, struct netent* ret,
                                 char* buf, size_t buflen,
                                 struct netent** result, int* h_errnop)
{
    struct netent* found;
    int pack_status;
    if (result == NULL) return EINVAL;
    *result = NULL;
    if (ret == NULL || h_errnop == NULL) return EINVAL;
    found = getnetbyaddr(net, type);
    if (found == NULL) {
        *h_errnop = h_errno;
        return errno != 0 ? errno : ENOENT;
    }
    pack_status = _netdb_pack_netent(buf, buflen, found, ret, result);
    if (pack_status != 0) {
        *h_errnop = NO_RECOVERY;
        return pack_status;
    }
    *h_errnop = 0;
    return 0;
}

static inline int getnetent_r(struct netent* ret, char* buf, size_t buflen,
                              struct netent** result, int* h_errnop)
{
    int saved_errno = errno;
    struct netent* found;
    int pack_status;
    if (result == NULL) return EINVAL;
    *result = NULL;
    if (ret == NULL || h_errnop == NULL) return EINVAL;
    found = getnetent();
    if (found == NULL) {
        *h_errnop = h_errno;
        return errno == saved_errno ? ENOENT : errno;
    }
    pack_status = _netdb_pack_netent(buf, buflen, found, ret, result);
    if (pack_status != 0) {
        *h_errnop = NO_RECOVERY;
        return pack_status;
    }
    *h_errnop = 0;
    return 0;
}


#undef _netdb_addr
#undef _netdb_addr_list
#undef _netdb_aliases
#undef _netdb_hostent
#undef _netdb_hostname

#endif /* _NETDB_H */
