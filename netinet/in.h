/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - netinet/in.h
 * インターネットアドレス
 */

#ifndef _NETINET_IN_H
#define _NETINET_IN_H

#include "../sys/socket.h"
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/* sys/socket.hで定義済みの型と構造体を再エクスポート */

/* ═══════════════════════════════════════════════════════════════
 * IPプロトコル番号
 * ═══════════════════════════════════════════════════════════════*/

#ifndef IPPROTO_IP
#define IPPROTO_IP       0    /* ダミー */
#endif
#ifndef IPPROTO_ICMP
#define IPPROTO_ICMP     1    /* ICMP */
#endif
#ifndef IPPROTO_IGMP
#define IPPROTO_IGMP     2    /* IGMP */
#endif
#ifndef IPPROTO_IPIP
#define IPPROTO_IPIP     4    /* IP in IP */
#endif
#ifndef IPPROTO_TCP
#define IPPROTO_TCP      6    /* TCP */
#endif
#ifndef IPPROTO_EGP
#define IPPROTO_EGP      8    /* EGP */
#endif
#ifndef IPPROTO_UDP
#define IPPROTO_UDP      17   /* UDP */
#endif
#ifndef IPPROTO_IDP
#define IPPROTO_IDP      22   /* XNS IDP */
#endif
#ifndef IPPROTO_TP
#define IPPROTO_TP       29   /* TP-4 */
#endif
#ifndef IPPROTO_DCCP
#define IPPROTO_DCCP     33   /* DCCP */
#endif
#ifndef IPPROTO_IPV6
#define IPPROTO_IPV6     41   /* IPv6 */
#endif
#ifndef IPPROTO_RSVP
#define IPPROTO_RSVP     46   /* RSVP */
#endif
#ifndef IPPROTO_GRE
#define IPPROTO_GRE      47   /* GRE */
#endif
#ifndef IPPROTO_ESP
#define IPPROTO_ESP      50   /* ESP */
#endif
#ifndef IPPROTO_AH
#define IPPROTO_AH       51   /* AH */
#endif
#ifndef IPPROTO_ICMPV6
#define IPPROTO_ICMPV6   58   /* ICMPv6 */
#endif
#ifndef IPPROTO_NONE
#define IPPROTO_NONE     59   /* No next header */
#endif
#ifndef IPPROTO_DSTOPTS
#define IPPROTO_DSTOPTS  60   /* Destination options */
#endif
#ifndef IPPROTO_MTP
#define IPPROTO_MTP      92   /* MTP */
#endif
#ifndef IPPROTO_ENCAP
#define IPPROTO_ENCAP    98   /* Encapsulation */
#endif
#ifndef IPPROTO_PIM
#define IPPROTO_PIM      103  /* PIM */
#endif
#ifndef IPPROTO_COMP
#define IPPROTO_COMP     108  /* Compression */
#endif
#ifndef IPPROTO_SCTP
#define IPPROTO_SCTP     132  /* SCTP */
#endif
#ifndef IPPROTO_UDPLITE
#define IPPROTO_UDPLITE  136  /* UDP-Lite */
#endif
#ifndef IPPROTO_RAW
#define IPPROTO_RAW      255  /* Raw IP */
#endif

/* ═══════════════════════════════════════════════════════════════
 * IPオプション
 * ═══════════════════════════════════════════════════════════════*/

#define IP_TOS             1   /* Type of Service */
#define IP_TTL             2   /* Time to Live */
#define IP_HDRINCL         3   /* Header is included */
#define IP_OPTIONS         4   /* IP options */
#define IP_ROUTER_ALERT    5   /* Router alert */
#define IP_RECVOPTS        6   /* Receive options */
#define IP_RETOPTS         7   /* Return options */
#define IP_PKTINFO         8   /* Packet info */
#define IP_MTU_DISCOVER    10  /* MTU discovery */
#define IP_RECVERR         11  /* Receive error */
#define IP_RECVTTL         12  /* Receive TTL */
#define IP_RECVTOS         13  /* Receive TOS */
#define IP_MTU             14  /* MTU */
#define IP_FREEBIND        15  /* Free bind */
#define IP_TRANSPARENT     19  /* Transparent proxy */
#define IP_MULTICAST_IF    32  /* Multicast interface */
#define IP_MULTICAST_TTL   33  /* Multicast TTL */
#define IP_MULTICAST_LOOP  34  /* Multicast loopback */
#define IP_ADD_MEMBERSHIP  35  /* Add membership */
#define IP_DROP_MEMBERSHIP 36  /* Drop membership */

/* MTU discovery */
#define IP_PMTUDISC_DONT    0
#define IP_PMTUDISC_WANT    1
#define IP_PMTUDISC_DO      2
#define IP_PMTUDISC_PROBE   3

/* ═══════════════════════════════════════════════════════════════
 * マルチキャスト構造体
 * ═══════════════════════════════════════════════════════════════*/

struct ip_mreq {
    struct in_addr imr_multiaddr; /* マルチキャストグループ */
    struct in_addr imr_interface; /* インターフェース */
};

struct ip_mreqn {
    struct in_addr imr_multiaddr;
    struct in_addr imr_address;
    int            imr_ifindex;
};

#ifndef MCAST_BLOCK_SOURCE
#define MCAST_BLOCK_SOURCE 43
#endif
#ifndef MCAST_JOIN_GROUP
#define MCAST_JOIN_GROUP 42
#endif
#ifndef MCAST_LEAVE_GROUP
#define MCAST_LEAVE_GROUP 45
#endif
#ifndef MCAST_UNBLOCK_SOURCE
#define MCAST_UNBLOCK_SOURCE 44
#endif
#ifndef MCAST_JOIN_SOURCE_GROUP
#define MCAST_JOIN_SOURCE_GROUP 46
#endif
#ifndef MCAST_LEAVE_SOURCE_GROUP
#define MCAST_LEAVE_SOURCE_GROUP 47
#endif

#define RIN_MCAST_SOURCE_FILTER_V1 0x7f50
#define RIN_MCAST_SOURCE_FILTER_ABI_VERSION 1u
#define RIN_MCAST_SOURCE_FILTER_MAX_SOURCES 64u
#define RIN_MCAST_FILTER_EXCLUDE 0u
#define RIN_MCAST_FILTER_INCLUDE 1u
#ifndef MCAST_EXCLUDE
#define MCAST_EXCLUDE RIN_MCAST_FILTER_EXCLUDE
#endif
#ifndef MCAST_INCLUDE
#define MCAST_INCLUDE RIN_MCAST_FILTER_INCLUDE
#endif

struct group_req {
    uint32_t gr_interface;
    struct sockaddr_storage gr_group;
};

struct group_source_req {
    uint32_t gsr_interface;
    struct sockaddr_storage gsr_group;
    struct sockaddr_storage gsr_source;
};

/* Versioned fixed-size socket option used by setsourcefilter/getsourcefilter.
 * The fixed layout keeps native64 and compat32 callers on the same ABI. */
struct rin_mcast_source_filter_v1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t address_family;
    uint32_t interface_index;
    uint32_t filter_mode;
    uint32_t source_count;
    uint32_t reserved[2];
    uint8_t group[16];
    uint8_t sources[RIN_MCAST_SOURCE_FILTER_MAX_SOURCES][16];
};

static inline int __rin_mcast_source_filter_address(
    uint32_t family, const struct sockaddr* address, socklen_t address_length,
    uint8_t output[16]) {
    uint32_t index;
    for (index = 0u; index < 16u; ++index) output[index] = 0u;
    if (address == NULL) { errno = EFAULT; return -1; }
    if (family == AF_INET) {
        struct sockaddr_in value;
        if (address_length < sizeof(value) || address->sa_family != AF_INET) {
            errno = EINVAL; return -1;
        }
        for (index = 0u; index < sizeof(value); ++index)
            ((uint8_t*)&value)[index] = ((const uint8_t*)address)[index];
        for (index = 0u; index < 4u; ++index)
            output[index] = ((const uint8_t*)&value.sin_addr.s_addr)[index];
        return 0;
    }
    if (family == AF_INET6) {
        struct sockaddr_in6 value;
        if (address_length < sizeof(value) || address->sa_family != AF_INET6) {
            errno = EINVAL; return -1;
        }
        for (index = 0u; index < sizeof(value); ++index)
            ((uint8_t*)&value)[index] = ((const uint8_t*)address)[index];
        for (index = 0u; index < 16u; ++index)
            output[index] = value.sin6_addr.s6_addr[index];
        return 0;
    }
    errno = EAFNOSUPPORT;
    return -1;
}

static inline int setsourcefilter(
    int sockfd, uint32_t interface_index, const struct sockaddr* group,
    socklen_t group_length, uint32_t filter_mode, uint32_t source_count,
    const struct sockaddr_storage* source_list) {
    struct rin_mcast_source_filter_v1 request = {0};
    uint32_t family;
    uint32_t index;
    if (group == NULL) { errno = EFAULT; return -1; }
    family = group->sa_family;
    if (source_count > RIN_MCAST_SOURCE_FILTER_MAX_SOURCES) {
        errno = ENOBUFS; return -1;
    }
    if ((filter_mode != RIN_MCAST_FILTER_INCLUDE &&
         filter_mode != RIN_MCAST_FILTER_EXCLUDE) ||
        (source_count != 0u && source_list == NULL)) {
        errno = EINVAL; return -1;
    }
    if (__rin_mcast_source_filter_address(
            family, group, group_length, request.group) != 0)
        return -1;
    if (family == AF_INET6) {
        struct sockaddr_in6 group_address = {0};
        uint32_t byte;
        for (byte = 0u; byte < sizeof(group_address); ++byte)
            ((uint8_t*)&group_address)[byte] =
                ((const uint8_t*)group)[byte];
        if (group_address.sin6_scope_id != 0u) {
            if (interface_index != 0u &&
                interface_index != group_address.sin6_scope_id) {
                errno = EINVAL; return -1;
            }
            interface_index = group_address.sin6_scope_id;
        }
    }
    request.struct_size = sizeof(request);
    request.version = RIN_MCAST_SOURCE_FILTER_ABI_VERSION;
    request.address_family = family;
    request.interface_index = interface_index;
    request.filter_mode = filter_mode;
    request.source_count = source_count;
    for (index = 0u; index < source_count; ++index) {
        if (source_list[index].ss_family != family) {
            errno = EINVAL; return -1;
        }
        if (family == AF_INET6) {
            struct sockaddr_in6 source_address = {0};
            uint32_t byte;
            for (byte = 0u; byte < sizeof(source_address); ++byte)
                ((uint8_t*)&source_address)[byte] =
                    ((const uint8_t*)&source_list[index])[byte];
            if (source_address.sin6_scope_id != 0u &&
                source_address.sin6_scope_id != interface_index) {
                errno = EINVAL; return -1;
            }
        }
        if (__rin_mcast_source_filter_address(
                family, (const struct sockaddr*)&source_list[index],
                (socklen_t)sizeof(source_list[index]),
                request.sources[index]) != 0)
            return -1;
    }
    return setsockopt(sockfd, family == AF_INET ? IPPROTO_IP : IPPROTO_IPV6,
                      RIN_MCAST_SOURCE_FILTER_V1, &request,
                      (socklen_t)sizeof(request));
}

static inline int getsourcefilter(
    int sockfd, uint32_t interface_index, const struct sockaddr* group,
    socklen_t group_length, uint32_t* filter_mode, uint32_t* source_count,
    struct sockaddr_storage* source_list) {
    struct rin_mcast_source_filter_v1 request = {0};
    uint32_t family;
    uint32_t capacity;
    uint32_t index;
    socklen_t option_length;
    if (filter_mode == NULL || source_count == NULL) {
        errno = EFAULT; return -1;
    }
    if (group == NULL) { errno = EFAULT; return -1; }
    family = group->sa_family;
    capacity = *source_count;
    if (capacity != 0u && source_list == NULL) {
        errno = EFAULT; return -1;
    }
    if (capacity > RIN_MCAST_SOURCE_FILTER_MAX_SOURCES)
        capacity = RIN_MCAST_SOURCE_FILTER_MAX_SOURCES;
    if (__rin_mcast_source_filter_address(
            family, group, group_length, request.group) != 0)
        return -1;
    if (family == AF_INET6) {
        struct sockaddr_in6 group_address = {0};
        uint32_t byte;
        for (byte = 0u; byte < sizeof(group_address); ++byte)
            ((uint8_t*)&group_address)[byte] =
                ((const uint8_t*)group)[byte];
        if (group_address.sin6_scope_id != 0u) {
            if (interface_index != 0u &&
                interface_index != group_address.sin6_scope_id) {
                errno = EINVAL; return -1;
            }
            interface_index = group_address.sin6_scope_id;
        }
    }
    request.struct_size = sizeof(request);
    request.version = RIN_MCAST_SOURCE_FILTER_ABI_VERSION;
    request.address_family = family;
    request.interface_index = interface_index;
    request.filter_mode = RIN_MCAST_FILTER_EXCLUDE;
    request.source_count = RIN_MCAST_SOURCE_FILTER_MAX_SOURCES;
    option_length = (socklen_t)sizeof(request);
    if (getsockopt(sockfd, family == AF_INET ? IPPROTO_IP : IPPROTO_IPV6,
                   RIN_MCAST_SOURCE_FILTER_V1, &request,
                   &option_length) != 0)
        return -1;
    if (option_length != sizeof(request) ||
        request.struct_size != sizeof(request) ||
        request.version != RIN_MCAST_SOURCE_FILTER_ABI_VERSION ||
        request.address_family != family ||
        (request.filter_mode != RIN_MCAST_FILTER_INCLUDE &&
         request.filter_mode != RIN_MCAST_FILTER_EXCLUDE) ||
        request.reserved[0] != 0u || request.reserved[1] != 0u ||
        request.source_count > RIN_MCAST_SOURCE_FILTER_MAX_SOURCES) {
        errno = EINVAL; return -1;
    }
    *filter_mode = request.filter_mode;
    *source_count = request.source_count;
    for (index = 0u; index < capacity && index < request.source_count; ++index) {
        uint32_t byte;
        for (byte = 0u; byte < sizeof(source_list[index]); ++byte)
            ((uint8_t*)&source_list[index])[byte] = 0u;
        source_list[index].ss_family = (sa_family_t)family;
        if (family == AF_INET) {
            struct sockaddr_in value = {0};
            value.sin_family = AF_INET;
            for (byte = 0u; byte < 4u; ++byte)
                ((uint8_t*)&value.sin_addr.s_addr)[byte] =
                    request.sources[index][byte];
            for (byte = 0u; byte < sizeof(value); ++byte)
                ((uint8_t*)&source_list[index])[byte] =
                    ((const uint8_t*)&value)[byte];
        } else {
            struct sockaddr_in6 value = {0};
            value.sin6_family = AF_INET6;
            for (byte = 0u; byte < 16u; ++byte)
                value.sin6_addr.s6_addr[byte] = request.sources[index][byte];
            if (value.sin6_addr.s6_addr[0] == 0xfeu &&
                (value.sin6_addr.s6_addr[1] & 0xc0u) == 0x80u)
                value.sin6_scope_id = request.interface_index;
            for (byte = 0u; byte < sizeof(value); ++byte)
                ((uint8_t*)&source_list[index])[byte] =
                    ((const uint8_t*)&value)[byte];
        }
    }
    return 0;
}

/* Linux-compatible packet metadata payloads used by recvmsg(2).  RinOS
 * exposes the same fixed-width control payload even though the network stack
 * currently publishes one interface per namespace. */
struct in_pktinfo {
    struct in_addr ipi_addr;
    struct in_addr ipi_spec_dst;
    int ipi_ifindex;
};

/* ═══════════════════════════════════════════════════════════════
 * IPv6オプション
 * ═══════════════════════════════════════════════════════════════*/

#define IPV6_CHECKSUM        7
#define IPV6_UNICAST_HOPS   16
#define IPV6_MULTICAST_IF   17
#define IPV6_MULTICAST_HOPS 18
#define IPV6_MULTICAST_LOOP 19
#define IPV6_JOIN_GROUP     20
#define IPV6_LEAVE_GROUP    21
#define IPV6_V6ONLY         26
#define IPV6_RECVPKTINFO    49
#define IPV6_RECVERR        25
#define IPV6_PKTINFO        50
#define IPV6_RECVHOPLIMIT   51
#define IPV6_HOPLIMIT       52
#define IPV6_TCLASS         67
#define IPV6_DONTFRAG       62

struct in6_pktinfo {
    struct in6_addr ipi6_addr;
    uint32_t ipi6_ifindex;
};

/* ═══════════════════════════════════════════════════════════════
 * ポート定数
 * ═══════════════════════════════════════════════════════════════*/

#define IPPORT_ECHO        7
#define IPPORT_DISCARD     9
#define IPPORT_SYSTAT      11
#define IPPORT_DAYTIME     13
#define IPPORT_NETSTAT     15
#define IPPORT_FTP         21
#define IPPORT_TELNET      23
#define IPPORT_SMTP        25
#define IPPORT_TIMESERVER  37
#define IPPORT_NAMESERVER  42
#define IPPORT_WHOIS       43
#define IPPORT_MTP         57
#define IPPORT_TFTP        69
#define IPPORT_RJE         77
#define IPPORT_FINGER      79
#define IPPORT_HTTP        80
#define IPPORT_TTYLINK     87
#define IPPORT_SUPDUP      95
#define IPPORT_POP3        110
#define IPPORT_SUNRPC      111
#define IPPORT_NNTP        119
#define IPPORT_NTP         123
#define IPPORT_IMAP        143
#define IPPORT_SNMP        161
#define IPPORT_SNMPTRAP    162
#define IPPORT_BGP         179
#define IPPORT_IRC         194
#define IPPORT_HTTPS       443
#define IPPORT_LDAPS       636
#define IPPORT_SOCKS       1080

/* ═══════════════════════════════════════════════════════════════
 * アドレス変換マクロ
 * ═══════════════════════════════════════════════════════════════*/

#define IN_CLASSA(a)        ((((in_addr_t)(a)) & 0x80000000) == 0)
#define IN_CLASSA_NET       0xff000000
#define IN_CLASSA_NSHIFT    24
#define IN_CLASSA_HOST      (0xffffffff & ~IN_CLASSA_NET)
#define IN_CLASSA_MAX       128

#define IN_CLASSB(a)        ((((in_addr_t)(a)) & 0xc0000000) == 0x80000000)
#define IN_CLASSB_NET       0xffff0000
#define IN_CLASSB_NSHIFT    16
#define IN_CLASSB_HOST      (0xffffffff & ~IN_CLASSB_NET)
#define IN_CLASSB_MAX       65536

#define IN_CLASSC(a)        ((((in_addr_t)(a)) & 0xe0000000) == 0xc0000000)
#define IN_CLASSC_NET       0xffffff00
#define IN_CLASSC_NSHIFT    8
#define IN_CLASSC_HOST      (0xffffffff & ~IN_CLASSC_NET)

#define IN_CLASSD(a)        ((((in_addr_t)(a)) & 0xf0000000) == 0xe0000000)
#define IN_MULTICAST(a)     IN_CLASSD(a)

#define IN_EXPERIMENTAL(a)  ((((in_addr_t)(a)) & 0xf0000000) == 0xf0000000)
#define IN_BADCLASS(a)      IN_EXPERIMENTAL(a)

#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK     ((in_addr_t)0x7f000001) /* 127.0.0.1 */
#endif
#define INADDR_UNSPEC_GROUP ((in_addr_t)0xe0000000) /* 224.0.0.0 */
#define INADDR_ALLHOSTS_GROUP ((in_addr_t)0xe0000001) /* 224.0.0.1 */
#define INADDR_ALLRTRS_GROUP ((in_addr_t)0xe0000002) /* 224.0.0.2 */
#define INADDR_MAX_LOCAL_GROUP ((in_addr_t)0xe00000ff) /* 224.0.0.255 */

/* ═══════════════════════════════════════════════════════════════
 * IPv6マクロ
 * ═══════════════════════════════════════════════════════════════*/

#define IN6_IS_ADDR_UNSPECIFIED(a) \
    (((a)->s6_addr32[0] | (a)->s6_addr32[1] | \
      (a)->s6_addr32[2] | (a)->s6_addr32[3]) == 0)

#define IN6_IS_ADDR_LOOPBACK(a) \
    ((a)->s6_addr32[0] == 0 && (a)->s6_addr32[1] == 0 && \
     (a)->s6_addr32[2] == 0 && (a)->s6_addr32[3] == htonl(1))

#define IN6_IS_ADDR_MULTICAST(a) ((a)->s6_addr[0] == 0xff)

#define IN6_IS_ADDR_LINKLOCAL(a) \
    (((a)->s6_addr[0] == 0xfe) && (((a)->s6_addr[1] & 0xc0) == 0x80))

#define IN6_IS_ADDR_SITELOCAL(a) \
    (((a)->s6_addr[0] == 0xfe) && (((a)->s6_addr[1] & 0xc0) == 0xc0))

#define IN6_IS_ADDR_V4MAPPED(a) \
    ((a)->s6_addr32[0] == 0 && (a)->s6_addr32[1] == 0 && \
     (a)->s6_addr32[2] == htonl(0xffff))

#define IN6_IS_ADDR_V4COMPAT(a) \
    ((a)->s6_addr32[0] == 0 && (a)->s6_addr32[1] == 0 && \
     (a)->s6_addr32[2] == 0 && ntohl((a)->s6_addr32[3]) > 1)

#ifdef __cplusplus
}
#endif

#endif /* _NETINET_IN_H */
