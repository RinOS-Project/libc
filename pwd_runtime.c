/* SPDX-License-Identifier: MIT */
#include "pwd.h"

#if defined(__cplusplus)
thread_local __rin_passwd_runtime_state_v1 __rin_passwd_runtime_state = {};
#else
_Thread_local __rin_passwd_runtime_state_v1 __rin_passwd_runtime_state = {0};
#endif
