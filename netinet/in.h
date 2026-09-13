/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - netinet/in.h
 * インターネットアドレス
 */

#ifndef _NETINET_IN_H
#define _NETINET_IN_H

#include "../sys/socket.h"

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

/* ═══════════════════════════════════════════════════════════════
 * IPv6オプション
 * ═══════════════════════════════════════════════════════════════*/

#define IPV6_UNICAST_HOPS   16
#define IPV6_MULTICAST_IF   17
#define IPV6_MULTICAST_HOPS 18
#define IPV6_MULTICAST_LOOP 19
#define IPV6_JOIN_GROUP     20
#define IPV6_LEAVE_GROUP    21
#define IPV6_V6ONLY         26
#define IPV6_RECVPKTINFO    49
#define IPV6_PKTINFO        50
#define IPV6_RECVHOPLIMIT   51
#define IPV6_HOPLIMIT       52

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
