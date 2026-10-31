/* SPDX-License-Identifier: MIT */
#ifndef RIN_LIBC_INTERNAL_PTHREAD_COND_INTERRUPT_H
#define RIN_LIBC_INTERNAL_PTHREAD_COND_INTERRUPT_H

#include "../pthread.h"

int __rin_pthread_cond_wait_interruptible(pthread_cond_t* condition,
                                          pthread_mutex_t* mutex);
int __rin_pthread_cond_timedwait_interruptible(
    pthread_cond_t* condition, pthread_mutex_t* mutex,
    const struct timespec* deadline);

#endif
