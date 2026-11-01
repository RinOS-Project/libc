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

static inline int set_mempolicy(int mode, const unsigned long* nodemask,
                                unsigned long maxnode) {
    RinNumaMemoryPolicyV1 policy = {0};
    if (mode < MPOL_DEFAULT || mode > MPOL_LOCAL || maxnode > 64ul ||
        ((mode == MPOL_BIND || mode == MPOL_INTERLEAVE ||
          mode == MPOL_PREFERRED) && maxnode != 0ul && nodemask == NULL)) {
        errno = EINVAL;
        return -1;
    }
    policy.struct_size = sizeof(policy);
    policy.version = RIN_NUMA_POLICY_ABI_VERSION;
    policy.policy = (uint16_t)mode;
    policy.preferred_node = RIN_NUMA_POLICY_PREFERRED_NODE_ANY;
    if (mode == MPOL_BIND || mode == MPOL_INTERLEAVE ||
        mode == MPOL_PREFERRED) {
        const unsigned long word_bits = sizeof(unsigned long) * CHAR_BIT;
        for (unsigned long node = 0ul; node < maxnode; ++node)
            if ((nodemask[node / word_bits] &
                 (1ul << (node % word_bits))) != 0ul)
                policy.node_mask |= UINT64_C(1) << node;
        if (mode == MPOL_PREFERRED) {
            uint32_t preferred_node = 0u;
            if (policy.node_mask == 0u && maxnode == 0ul) {
                policy.policy = RIN_NUMA_POLICY_LOCAL;
                return rin_sched_set_thread_numa_policy(&policy);
            }
            if (policy.node_mask == 0u ||
                (policy.node_mask & (policy.node_mask - 1u)) != 0u) {
                errno = EINVAL;
                return -1;
            }
            while ((policy.node_mask & (UINT64_C(1) << preferred_node)) == 0u)
                ++preferred_node;
            policy.preferred_node = (uint16_t)preferred_node;
        }
    }
    return rin_sched_set_thread_numa_policy(&policy);
}

static inline int get_mempolicy(int* mode, unsigned long* nodemask,
                                unsigned long maxnode, void* address,
                                unsigned long flags) {
    RinNumaMemoryPolicyV1 policy = {0};
    if (!mode || maxnode > 64ul || (nodemask && maxnode == 0ul) ||
        address != NULL || flags != 0ul) {
        errno = EINVAL;
        return -1;
    }
    if (rin_sched_get_thread_numa_policy(&policy) < 0) return -1;
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
