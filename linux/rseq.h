/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _LINUX_RSEQ_H
#define _LINUX_RSEQ_H

#include "../stdint.h"

#define RSEQ_CPU_ID_UNINITIALIZED       (-1)
#define RSEQ_CPU_ID_REGISTRATION_FAILED (-2)
#define RSEQ_FLAG_UNREGISTER            (1u << 0)
#define RSEQ_ORIG_SIZE                  32u
#define RSEQ_ALIGN                      32u

/* x86 abort-handler marker consumed by the kernel at abort_ip - 4. */
#define RSEQ_SIG UINT32_C(0x53053053)

enum rseq_cs_flags_bit {
    RSEQ_CS_FLAG_NO_RESTART_ON_PREEMPT_BIT = 0,
    RSEQ_CS_FLAG_NO_RESTART_ON_SIGNAL_BIT = 1,
    RSEQ_CS_FLAG_NO_RESTART_ON_MIGRATE_BIT = 2
};

enum rseq_cs_flags {
    RSEQ_CS_FLAG_NO_RESTART_ON_PREEMPT =
        (1u << RSEQ_CS_FLAG_NO_RESTART_ON_PREEMPT_BIT),
    RSEQ_CS_FLAG_NO_RESTART_ON_SIGNAL =
        (1u << RSEQ_CS_FLAG_NO_RESTART_ON_SIGNAL_BIT),
    RSEQ_CS_FLAG_NO_RESTART_ON_MIGRATE =
        (1u << RSEQ_CS_FLAG_NO_RESTART_ON_MIGRATE_BIT)
};

struct rseq_cs {
    uint32_t version;
    uint32_t flags;
    uint64_t start_ip;
    uint64_t post_commit_offset;
    uint64_t abort_ip;
} __attribute__((aligned(4u * sizeof(uint64_t))));

struct rseq_slice_ctrl {
    union {
        uint32_t all;
        struct {
            uint8_t request;
            uint8_t granted;
            uint16_t reserved;
        };
    };
};

struct rseq {
    uint32_t cpu_id_start;
    uint32_t cpu_id;
    uint64_t rseq_cs;
    uint32_t flags;
    uint32_t node_id;
    uint32_t mm_cid;
    struct rseq_slice_ctrl slice_ctrl;
    uint8_t __reserved;
    char end[];
} __attribute__((aligned(RSEQ_ALIGN)));

#endif /* _LINUX_RSEQ_H */
