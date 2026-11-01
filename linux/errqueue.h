/* SPDX-License-Identifier: MIT */
#ifndef _RIN_LINUX_ERRQUEUE_H
#define _RIN_LINUX_ERRQUEUE_H

#include <stdint.h>

/* Linux互換ソケット拡張エラーABIの発生元値。 */
#ifndef SO_EE_ORIGIN_NONE
#define SO_EE_ORIGIN_NONE       0
#define SO_EE_ORIGIN_LOCAL      1
#define SO_EE_ORIGIN_ICMP       2
#define SO_EE_ORIGIN_ICMP6      3
#define SO_EE_ORIGIN_TXSTATUS   4
#define SO_EE_ORIGIN_ZEROCOPY   5
#endif

#ifndef SO_EE_CODE_ZEROCOPY_COPIED
#define SO_EE_CODE_ZEROCOPY_COPIED 1
#endif

struct sock_extended_err {
    uint32_t ee_errno;
    uint8_t ee_origin;
    uint8_t ee_type;
    uint8_t ee_code;
    uint8_t ee_pad;
    uint32_t ee_info;
    uint32_t ee_data;
};

#if defined(__cplusplus)
static_assert(sizeof(sock_extended_err) == 16u,
              "sock_extended_err ABI drift");
#else
_Static_assert(sizeof(struct sock_extended_err) == 16u,
               "sock_extended_err ABI drift");
#endif

#define SO_EE_OFFENDER(error) \
    ((struct sockaddr*)(((uint8_t*)(error)) + sizeof(struct sock_extended_err)))

#endif /* _RIN_LINUX_ERRQUEUE_H */
