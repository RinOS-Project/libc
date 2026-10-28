/* SPDX-License-Identifier: MIT */
#ifndef _MQUEUE_H
#define _MQUEUE_H

#include "errno.h"
#include "fcntl.h"
#include "signal.h"
#include "stddef.h"
#include "stdint.h"
#include "limits.h"
#include "sys/types.h"
#include "time.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t mqd_t;

struct mq_attr {
    long mq_flags;
    long mq_maxmsg;
    long mq_msgsize;
    long mq_curmsgs;
    long __reserved[4];
};

mqd_t mq_open(const char* name, int oflag, ...);
int mq_close(mqd_t mqdes);
int mq_unlink(const char* name);
int mq_getattr(mqd_t mqdes, struct mq_attr* attr);
int mq_setattr(mqd_t mqdes, const struct mq_attr* newattr,
               struct mq_attr* oldattr);
int mq_send(mqd_t mqdes, const char* msg_ptr, size_t msg_len,
            unsigned int msg_prio);
ssize_t mq_receive(mqd_t mqdes, char* msg_ptr, size_t msg_len,
                   unsigned int* msg_prio);
int mq_timedsend(mqd_t mqdes, const char* msg_ptr, size_t msg_len,
                 unsigned int msg_prio,
                 const struct timespec* abs_timeout);
ssize_t mq_timedreceive(mqd_t mqdes, char* msg_ptr, size_t msg_len,
                        unsigned int* msg_prio,
                        const struct timespec* abs_timeout);
int mq_notify(mqd_t mqdes, const struct sigevent* notification);

#ifdef __cplusplus
}
#endif

#endif /* _MQUEUE_H */
