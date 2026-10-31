/* SPDX-License-Identifier: MIT */
/* C++ standard-library bridges to the libc-owned conversion and process APIs. */

#define RIN_STDLIB_NO_INLINE_GETSUBOPT 1
#define RIN_STDLIB_ALIGNED_ALLOC_RUNTIME_IMPLEMENTATION 1
/* signal.h first: see the include-order contract in unistd.h.  This unit
 * must compile standalone for the libc build as well as host tests. */
#include "signal.h"
#include "stdlib.h"
#include "rin_stdlib_runtime.h"

#include "fcntl.h"
#include "unistd.h"

int getsubopt(char** optionp, char* const* keylistp, char** valuep) {
    return _rin_stdlib_getsubopt_parse(optionp, keylistp, valuep);
}

void* aligned_alloc(size_t alignment, size_t size) {
    return _rin_aligned_alloc_internal(alignment, size, 1);
}

int posix_memalign(void** memptr, size_t alignment, size_t size) {
    void* ptr;
    int saved_errno;
    if (!memptr) return EINVAL;
    if (alignment < sizeof(void*) || !_rin_is_power_of_two_size(alignment))
        return EINVAL;
    saved_errno = errno;
    ptr = _rin_aligned_alloc_internal(alignment, size, 0);
    errno = saved_errno;
    if (!ptr) return ENOMEM;
    *memptr = ptr;
    return 0;
}

double rin_stdlib_strtod(const char* text, char** end) {
    return strtod(text, end);
}

float rin_stdlib_strtof(const char* text, char** end) {
    return strtof(text, end);
}

long double rin_stdlib_strtold(const char* text, char** end) {
    return strtold(text, end);
}

char* rin_stdlib_getenv(const char* name) {
    return getenv(name);
}

int rin_stdlib_system(const char* command) {
    return system(command);
}
