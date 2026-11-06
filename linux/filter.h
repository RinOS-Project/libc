/* SPDX-License-Identifier: MIT */
/* Classic BPF instruction ABI used by seccomp and socket filters. */
#ifndef RIN_LIBC_LINUX_FILTER_H
#define RIN_LIBC_LINUX_FILTER_H

#include "../stdint.h"

#define BPF_CLASS(code) ((code) & 0x07)
#define BPF_SIZE(code)  ((code) & 0x18)
#define BPF_MODE(code)  ((code) & 0xe0)
#define BPF_RVAL(code)  ((code) & 0x18)
#define BPF_SRC(code)   ((code) & 0x08)
#define BPF_OP(code)    ((code) & 0xf0)

#define BPF_LD   0x00
#define BPF_LDX  0x01
#define BPF_ST   0x02
#define BPF_STX  0x03
#define BPF_ALU  0x04
#define BPF_JMP  0x05
#define BPF_RET  0x06
#define BPF_MISC 0x07

#define BPF_W    0x00
#define BPF_H    0x08
#define BPF_B    0x10
#define BPF_IMM  0x00
#define BPF_ABS  0x20
#define BPF_IND  0x40
#define BPF_MEM  0x60
#define BPF_LEN  0x80
#define BPF_MSH  0xa0
#define BPF_K    0x00
#define BPF_X    0x08
#define BPF_A    0x10

#define BPF_ADD  0x00
#define BPF_SUB  0x10
#define BPF_MUL  0x20
#define BPF_DIV  0x30
#define BPF_OR   0x40
#define BPF_AND  0x50
#define BPF_LSH  0x60
#define BPF_RSH  0x70
#define BPF_NEG  0x80
#define BPF_MOD  0x90
#define BPF_XOR  0xa0
#define BPF_JA   0x00
#define BPF_JEQ  0x10
#define BPF_JGT  0x20
#define BPF_JGE  0x30
#define BPF_JSET 0x40
#define BPF_TAX  0x00
#define BPF_TXA  0x80

/* Socket-filter ancillary load offsets (BPF_ABS + BPF_W). */
#define SKF_AD_OFF             (-0x1000)
#define SKF_AD_PROTOCOL        0
#define SKF_AD_PKTTYPE         4
#define SKF_AD_IFINDEX         8
#define SKF_AD_NLATTR          12
#define SKF_AD_NLATTR_NEST     16
#define SKF_AD_MARK            20
#define SKF_AD_QUEUE           24
#define SKF_AD_HATYPE          28
#define SKF_AD_RXHASH          32
#define SKF_AD_CPU             36
#define SKF_AD_ALU_XOR_X       40
#define SKF_AD_VLAN_TAG        44
#define SKF_AD_VLAN_TAG_PRESENT 48
#define SKF_AD_PAY_OFFSET      52
#define SKF_AD_RANDOM          56
#define SKF_AD_VLAN_TPID       60
#define SKF_AD_MAX             64
#define SKF_NET_OFF            (-0x100000)
#define SKF_LL_OFF             (-0x200000)
#define BPF_NET_OFF             SKF_NET_OFF
#define BPF_LL_OFF              SKF_LL_OFF

#define SKF_AD_PROTOCOL_LOAD \
    ((uint32_t)(SKF_AD_OFF + SKF_AD_PROTOCOL))
#define SKF_AD_PKTTYPE_LOAD \
    ((uint32_t)(SKF_AD_OFF + SKF_AD_PKTTYPE))
#define SKF_AD_IFINDEX_LOAD \
    ((uint32_t)(SKF_AD_OFF + SKF_AD_IFINDEX))
#define SKF_AD_HATYPE_LOAD \
    ((uint32_t)(SKF_AD_OFF + SKF_AD_HATYPE))
#define SKF_AD_CPU_LOAD \
    ((uint32_t)(SKF_AD_OFF + SKF_AD_CPU))
#define SKF_AD_ALU_XOR_X_LOAD \
    ((uint32_t)(SKF_AD_OFF + SKF_AD_ALU_XOR_X))

#define BPF_STMT(code, k) { (uint16_t)(code), 0u, 0u, (uint32_t)(k) }
#define BPF_JUMP(code, k, jt, jf) \
    { (uint16_t)(code), (uint8_t)(jt), (uint8_t)(jf), (uint32_t)(k) }

struct sock_filter {
    uint16_t code;
    uint8_t jt;
    uint8_t jf;
    uint32_t k;
};

struct sock_fprog {
    uint16_t len;
    struct sock_filter* filter;
};

#endif /* RIN_LIBC_LINUX_FILTER_H */
