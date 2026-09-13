/* SPDX-License-Identifier: MIT */
#include "netdb.h"

#if defined(__cplusplus)
thread_local __rin_netdb_runtime_state_v1 __rin_netdb_runtime_state = {};
#else
_Thread_local __rin_netdb_runtime_state_v1 __rin_netdb_runtime_state = {0};
#endif
