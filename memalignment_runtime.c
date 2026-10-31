/* SPDX-License-Identifier: MIT */
#include "stddef.h"
#include "stdint.h"

size_t memalignment(const void* ptr) {
    uintptr_t address = (uintptr_t)ptr;
    return (size_t)(address & (~address + (uintptr_t)1u));
}
