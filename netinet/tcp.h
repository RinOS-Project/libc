/* SPDX-License-Identifier: MIT */
/* RinOS libc - netinet/tcp.h */
#ifndef _NETINET_TCP_H
#define _NETINET_TCP_H

/* The product socket contract owns the TCP level and option constants. */
#include "../sys/socket.h"
#include <stdint.h>

#ifndef IPPROTO_TCP
#define IPPROTO_TCP 6
#endif
#ifndef TCP_NODELAY
#define TCP_NODELAY 1
#endif
#ifndef TCP_MAXSEG
#define TCP_MAXSEG 2
#endif
#ifndef TCP_KEEPIDLE
#define TCP_KEEPIDLE 4
#endif
#ifndef TCP_KEEPINTVL
#define TCP_KEEPINTVL 5
#endif
#ifndef TCP_KEEPCNT
#define TCP_KEEPCNT 6
#endif
#ifndef TCP_CONGESTION
#define TCP_CONGESTION 13
#endif
#ifndef TCP_INFO
#define TCP_INFO 11
#endif
#ifndef TCP_NOTSENT_LOWAT
#define TCP_NOTSENT_LOWAT 25
#endif

#ifndef TCPI_OPT_SACK
#define TCPI_OPT_SACK 2
#endif
#ifndef TCPI_OPT_WSCALE
#define TCPI_OPT_WSCALE 4
#endif

/* The first 104 bytes follow the Linux TCP_INFO stable prefix. Fields that
 * RinOS does not track remain zero; rin_valid_mask identifies populated
 * metrics in the extension. optlen may request or receive any prefix length. */
#define RIN_TCP_INFO_VERSION 1u
#define RIN_TCP_INFO_PREFIX_SIZE 104u
#define RIN_TCP_INFO_VALID_STATE          (UINT64_C(1) << 0)
#define RIN_TCP_INFO_VALID_CA_STATE       (UINT64_C(1) << 1)
#define RIN_TCP_INFO_VALID_RETRANSMITS    (UINT64_C(1) << 2)
#define RIN_TCP_INFO_VALID_PROBES         (UINT64_C(1) << 3)
#define RIN_TCP_INFO_VALID_OPTIONS        (UINT64_C(1) << 4)
#define RIN_TCP_INFO_VALID_WSCALE         (UINT64_C(1) << 5)
#define RIN_TCP_INFO_VALID_RTO            (UINT64_C(1) << 6)
#define RIN_TCP_INFO_VALID_SND_MSS        (UINT64_C(1) << 7)
#define RIN_TCP_INFO_VALID_UNACKED        (UINT64_C(1) << 8)
#define RIN_TCP_INFO_VALID_RTT            (UINT64_C(1) << 9)
#define RIN_TCP_INFO_VALID_RTTVAR         (UINT64_C(1) << 10)
#define RIN_TCP_INFO_VALID_SND_SSTHRESH   (UINT64_C(1) << 11)
#define RIN_TCP_INFO_VALID_SND_CWND       (UINT64_C(1) << 12)
#define RIN_TCP_INFO_VALID_RCV_SPACE      (UINT64_C(1) << 13)
#define RIN_TCP_INFO_VALID_TOTAL_RETRANS  (UINT64_C(1) << 14)
#define RIN_TCP_INFO_VALID_UNACKED_BYTES  (UINT64_C(1) << 15)

struct tcp_info {
    uint8_t tcpi_state;
    uint8_t tcpi_ca_state;
    uint8_t tcpi_retransmits;
    uint8_t tcpi_probes;
    uint8_t tcpi_backoff;
    uint8_t tcpi_options;
    uint8_t tcpi_snd_wscale : 4;
    uint8_t tcpi_rcv_wscale : 4;
    uint8_t tcpi_delivery_rate_app_limited : 1;
    uint8_t tcpi_fastopen_client_fail : 2;
    uint8_t tcpi_reserved_flags : 5;
    uint32_t tcpi_rto;
    uint32_t tcpi_ato;
    uint32_t tcpi_snd_mss;
    uint32_t tcpi_rcv_mss;
    uint32_t tcpi_unacked;
    uint32_t tcpi_sacked;
    uint32_t tcpi_lost;
    uint32_t tcpi_retrans;
    uint32_t tcpi_fackets;
    uint32_t tcpi_last_data_sent;
    uint32_t tcpi_last_ack_sent;
    uint32_t tcpi_last_data_recv;
    uint32_t tcpi_last_ack_recv;
    uint32_t tcpi_pmtu;
    uint32_t tcpi_rcv_ssthresh;
    uint32_t tcpi_rtt;
    uint32_t tcpi_rttvar;
    uint32_t tcpi_snd_ssthresh;
    uint32_t tcpi_snd_cwnd;
    uint32_t tcpi_advmss;
    uint32_t tcpi_reordering;
    uint32_t tcpi_rcv_rtt;
    uint32_t tcpi_rcv_space;
    uint32_t tcpi_total_retrans;
    uint64_t rin_valid_mask;
    uint32_t rin_unacked_bytes;
    uint32_t rin_reserved0;
    uint64_t rin_reserved[7];
};

#endif /* _NETINET_TCP_H */
