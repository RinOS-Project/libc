/* SPDX-License-Identifier: MIT */
#include "grp.h"

#if defined(__cplusplus)
thread_local __rin_group_runtime_state_v1 __rin_group_runtime_state = {};
#else
_Thread_local __rin_group_runtime_state_v1 __rin_group_runtime_state = {0};
#endif
