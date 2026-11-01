/* SPDX-License-Identifier: MIT */
#ifndef _RIN_SYS_ERRQUEUE_H
#define _RIN_SYS_ERRQUEUE_H 1

#include <stdint.h>
#include <sys/socket.h>

#define SO_EE_ORIGIN_NONE 0u
#define SO_EE_ORIGIN_LOCAL 1u
#define SO_EE_ORIGIN_ICMP 2u
#define SO_EE_ORIGIN_ICMP6 3u
#define SO_EE_ORIGIN_TXSTATUS 4u
#define SO_EE_ORIGIN_ZEROCOPY 5u
#define SO_EE_ORIGIN_TXTIME 6u

#define SO_EE_CODE_ZEROCOPY_COPIED 1u
#define SO_EE_CODE_TXTIME_INVALID_PARAM 1u
#define SO_EE_CODE_TXTIME_MISSED 2u

#ifndef _RIN_SOCK_EXTENDED_ERR_DEFINED
#define _RIN_SOCK_EXTENDED_ERR_DEFINED 1
struct sock_extended_err {
    uint32_t ee_errno;
    uint8_t ee_origin;
    uint8_t ee_type;
    uint8_t ee_code;
    uint8_t ee_pad;
    uint32_t ee_info;
    uint32_t ee_data;
};
#endif

#endif /* _RIN_SYS_ERRQUEUE_H */
