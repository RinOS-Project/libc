/* SPDX-License-Identifier: MIT */
/* C++ standard-library bridges to the libc-owned conversion and process APIs. */

#define _RIN_SIGNAL_RUNTIME_IMPLEMENTATION 1
#define RIN_STDLIB_ABORT_RUNTIME_IMPLEMENTATION 1
#define RIN_STDLIB_NO_INLINE_GETSUBOPT 1
#define RIN_STDLIB_ALIGNED_ALLOC_RUNTIME_IMPLEMENTATION 1
/* signal.h first: see the include-order contract in unistd.h.  This unit
 * must compile standalone for the libc build as well as host tests. */
#include "signal.h"
/* Compiler-generated C++ cleanup paths call the standard abort symbol when
 * destructor registration fails. Preserve the freestanding implementation
 * below while exporting that implementation from the runtime object too. */
#define abort rin_stdlib_abort_inline
#include "stdlib.h"
#undef abort
#include "rin_stdlib_runtime.h"

#include "fcntl.h"
#include "unistd.h"

__attribute__((noreturn)) void abort(void) {
    rin_stdlib_abort_inline();
}

int getsubopt(char** optionp, char* const* keylistp, char** valuep) {
    return _rin_stdlib_getsubopt_parse(optionp, keylistp, valuep);
}

void* aligned_alloc(size_t alignment, size_t size) {
    return _rin_aligned_alloc_internal(alignment, size);
}

int posix_memalign(void** memptr, size_t alignment, size_t size) {
    void* ptr;
    int saved_errno;
    if (!memptr) return EINVAL;
    if (alignment < sizeof(void*) || !_rin_is_power_of_two_size(alignment))
        return EINVAL;
    saved_errno = errno;
    ptr = _rin_aligned_alloc_internal(alignment, size);
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
