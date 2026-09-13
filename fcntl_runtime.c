/* SPDX-License-Identifier: MIT */
#include "fcntl.h"

int _fcntl_fd_flags[_FCNTL_MAX_FDS];
int _fcntl_fl_flags[_FCNTL_MAX_FDS];
int _fcntl_initialized = 0;
