/* SPDX-License-Identifier: MIT */
#include "linux/bpf.h"

int bpf(int command, union rin_bpf_attr* attribute, unsigned int size)
{
    return (int)syscall(SYS_BPF, (unsigned int)command, attribute, size);
}
