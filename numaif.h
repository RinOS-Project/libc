/* SPDX-License-Identifier: MIT */
#ifndef _NUMAIF_H
#define _NUMAIF_H

#include <errno.h>
#include <limits.h>
#include <sched.h>
#include <stdint.h>
#include <rin/memory/numa_policy_abi.h>

#ifndef MPOL_DEFAULT
#define MPOL_DEFAULT RIN_NUMA_POLICY_DEFAULT
#define MPOL_PREFERRED RIN_NUMA_POLICY_PREFERRED
#define MPOL_BIND RIN_NUMA_POLICY_BIND
#define MPOL_INTERLEAVE RIN_NUMA_POLICY_INTERLEAVE
#define MPOL_LOCAL RIN_NUMA_POLICY_LOCAL
#endif
#ifndef MPOL_F_ADDR
#define MPOL_F_ADDR 2
#endif
#ifndef MPOL_MF_MOVE
#define MPOL_MF_MOVE 1
#endif
#ifndef MPOL_MF_MOVE_ALL
#define MPOL_MF_MOVE_ALL 4
#endif
#ifndef MPOL_MF_STRICT
#define MPOL_MF_STRICT 1
#endif

static inline int _rin_numa_policy_build(
    int mode, const unsigned long* nodemask, unsigned long maxnode,
    RinNumaMemoryPolicyV1* policy) {
    if (!policy || mode < MPOL_DEFAULT || mode > MPOL_LOCAL ||
        maxnode > 64ul ||
        ((mode == MPOL_BIND || mode == MPOL_INTERLEAVE ||
          mode == MPOL_PREFERRED) && maxnode != 0ul && nodemask == NULL)) {
        errno = EINVAL;
        return -1;
    }
    policy->struct_size = sizeof(*policy);
    policy->version = RIN_NUMA_POLICY_ABI_VERSION;
    policy->policy = (uint16_t)mode;
    policy->preferred_node = RIN_NUMA_POLICY_PREFERRED_NODE_ANY;
    policy->reserved0 = 0u;
    policy->flags = RIN_NUMA_POLICY_FLAG_NONE;
    policy->node_mask = 0u;
    if (mode == MPOL_BIND || mode == MPOL_INTERLEAVE ||
        mode == MPOL_PREFERRED) {
        const unsigned long word_bits = sizeof(unsigned long) * CHAR_BIT;
        for (unsigned long node = 0ul; node < maxnode; ++node)
            if ((nodemask[node / word_bits] &
                 (1ul << (node % word_bits))) != 0ul)
                policy->node_mask |= UINT64_C(1) << node;
        if (mode == MPOL_PREFERRED) {
            uint32_t preferred_node = 0u;
            if (policy->node_mask == 0u && maxnode == 0ul) {
                policy->policy = RIN_NUMA_POLICY_LOCAL;
                return 0;
            }
            if (policy->node_mask == 0u ||
                (policy->node_mask & (policy->node_mask - 1u)) != 0u) {
                errno = EINVAL;
                return -1;
            }
            while ((policy->node_mask &
                    (UINT64_C(1) << preferred_node)) == 0u)
                ++preferred_node;
            policy->preferred_node = (uint16_t)preferred_node;
        }
    }
    return 0;
}

static inline int set_mempolicy(int mode, const unsigned long* nodemask,
                                unsigned long maxnode) {
    RinNumaMemoryPolicyV1 policy = {0};
    if (_rin_numa_policy_build(mode, nodemask, maxnode, &policy) < 0)
        return -1;
    return rin_sched_set_thread_numa_policy(&policy);
}

static inline int mbind(void* address, unsigned long length, int mode,
                        const unsigned long* nodemask,
                        unsigned long maxnode, unsigned flags) {
    RinNumaMemoryPolicyRangeV1 range = {0};
    if (!address || length == 0ul ||
        (flags & ~(MPOL_MF_STRICT | MPOL_MF_MOVE | MPOL_MF_MOVE_ALL)) != 0u) {
        errno = EINVAL;
        return -1;
    }
    if (_rin_numa_policy_build(mode, nodemask, maxnode, &range.policy) < 0)
        return -1;
    range.struct_size = sizeof(range);
    range.version = RIN_NUMA_POLICY_ABI_VERSION;
    range.flags = (uint16_t)flags;
    range.address = (uint64_t)(uintptr_t)address;
    range.length = (uint64_t)length;
    return rin_sched_set_numa_policy_range(&range);
}

static inline long move_pages(int process_id, unsigned long count,
                              void* const pages[], const int nodes[],
                              int status[], int flags) {
    RinNumaMovePagesV1 request = {0};
    uint64_t addresses[RIN_NUMA_MOVE_PAGES_MAX_COUNT];
    unsigned long failed = 0ul;
    if (count > RIN_NUMA_MOVE_PAGES_MAX_COUNT ||
        (count != 0ul && (!pages || !status)) || process_id < 0 ||
        (flags & ~(MPOL_MF_MOVE | MPOL_MF_MOVE_ALL)) != 0) {
        errno = EINVAL;
        return -1;
    }
    for (unsigned long i = 0ul; i < count; ++i)
        addresses[i] = (uint64_t)(uintptr_t)pages[i];
    request.struct_size = sizeof(request);
    request.version = RIN_NUMA_POLICY_ABI_VERSION;
    request.process_id = process_id;
    request.flags = (uint32_t)flags;
    request.count = (uint32_t)count;
    request.addresses = (uint64_t)(uintptr_t)addresses;
    request.nodes = (uint64_t)(uintptr_t)nodes;
    request.status = (uint64_t)(uintptr_t)status;
    if (rin_sched_move_numa_pages(&request) < 0) return -1;
    for (unsigned long i = 0ul; i < count; ++i)
        if (status[i] < 0) ++failed;
    return (long)failed;
}

static inline int migrate_pages(int process_id, unsigned long maxnode,
                                const unsigned long* old_nodes,
                                const unsigned long* new_nodes) {
    RinNumaMigratePagesV1 request = {0};
    const unsigned long word_bits = sizeof(unsigned long) * CHAR_BIT;
    if (process_id < 0 || maxnode == 0ul || maxnode > 64ul) {
        errno = EINVAL;
        return -1;
    }
    request.struct_size = sizeof(request);
    request.version = RIN_NUMA_POLICY_ABI_VERSION;
    request.process_id = process_id;
    request.maxnode = (uint32_t)maxnode;
    request.node_word_bits = (uint32_t)word_bits;
    request.old_nodes = (uint64_t)(uintptr_t)old_nodes;
    request.new_nodes = (uint64_t)(uintptr_t)new_nodes;
    return (int)rin_sched_migrate_numa_pages(&request);
}
static inline int get_mempolicy(int* mode, unsigned long* nodemask,
                                unsigned long maxnode, void* address,
                                unsigned long flags) {
    RinNumaMemoryPolicyV1 policy = {0};
    if (!mode || maxnode > 64ul || (nodemask && maxnode == 0ul) ||
        (address == NULL && flags != 0ul) ||
        (address != NULL && flags != MPOL_F_ADDR)) {
        errno = EINVAL;
        return -1;
    }
    if (address != NULL) {
        RinNumaMemoryPolicyRangeV1 range = {0};
        range.struct_size = sizeof(range);
        range.version = RIN_NUMA_POLICY_ABI_VERSION;
        range.address = (uint64_t)(uintptr_t)address;
        range.length = 4096u;
        if (rin_sched_get_numa_policy_range(&range) < 0) return -1;
        policy = range.policy;
    } else if (rin_sched_get_thread_numa_policy(&policy) < 0) {
        return -1;
    }
    *mode = policy.policy;
    if (nodemask) {
        const unsigned long word_bits = sizeof(unsigned long) * CHAR_BIT;
        const unsigned long words = (maxnode + word_bits - 1ul) / word_bits;
        for (unsigned long word = 0ul; word < words; ++word)
            ((unsigned long*)nodemask)[word] = 0ul;
        for (unsigned long node = 0ul; node < maxnode; ++node)
            if ((policy.node_mask & (UINT64_C(1) << node)) != 0u)
                ((unsigned long*)nodemask)[node / word_bits] |=
                    1ul << (node % word_bits);
    }
    return 0;
}

#endif /* _NUMAIF_H */
