/* SPDX-License-Identifier: MIT */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 202405L
#endif
#include "netdb.h"

#if defined(__cplusplus)
thread_local __rin_netdb_runtime_state_v1 __rin_netdb_runtime_state = {};
#else
_Thread_local __rin_netdb_runtime_state_v1 __rin_netdb_runtime_state = {0};
#endif
