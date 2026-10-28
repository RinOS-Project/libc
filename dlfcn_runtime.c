/* SPDX-License-Identifier: MIT */
/* One dynamic-link error slot per thread, shared across translation units. */

#include "internal/rin_dlerror_runtime.h"

#if defined(__cplusplus)
static thread_local RinDlerrorState g_dlerror_state;
#else
static _Thread_local RinDlerrorState g_dlerror_state;
#endif

RinDlerrorState* __rin_dlerror_state(void) {
    return &g_dlerror_state;
}
