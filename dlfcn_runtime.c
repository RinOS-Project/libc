/* SPDX-License-Identifier: MIT */
/* One dynamic-link error slot per thread, shared across translation units. */

#include "internal/rin_dlerror_runtime.h"
#include "dlfcn.h"

#if defined(__cplusplus)
static thread_local RinDlerrorState g_dlerror_state;
#else
static _Thread_local RinDlerrorState g_dlerror_state;
#endif

RinDlerrorState* __rin_dlerror_state(void) {
    return &g_dlerror_state;
}

int __rin_run_startup_dependency_initializers(void) {
#if defined(RIN_FREESTANDING) && RIN_FREESTANDING && \
    (defined(__x86_64__) || defined(__i386__))
    return _rin_dl_lifecycle_run(0u, RIN_DL_LIFECYCLE_INIT_NEXT);
#else
    return -1;
#endif
}

int __rin_run_process_dependency_finalizers(void) {
#if defined(RIN_FREESTANDING) && RIN_FREESTANDING && \
    (defined(__x86_64__) || defined(__i386__))
    return _rin_dl_lifecycle_run(
        0u, RIN_DL_LIFECYCLE_PROCESS_FINI_NEXT);
#else
    return -1;
#endif
}
