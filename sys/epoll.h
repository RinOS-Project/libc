/* SPDX-License-Identifier: MIT */
#ifndef _SYS_EPOLL_H
#define _SYS_EPOLL_H

#include "../signal.h"
#include "../time.h"
#include "../stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef union epoll_data {
    void* ptr;
    int fd;
    uint32_t u32;
    uint64_t u64;
} epoll_data_t;

struct epoll_event {
    uint32_t events;
    epoll_data_t data;
} __attribute__((packed));

#if defined(__cplusplus)
static_assert(sizeof(struct epoll_event) == 12u,
              "epoll_event ABI must be 12 bytes");
#else
_Static_assert(sizeof(struct epoll_event) == 12u,
               "epoll_event ABI must be 12 bytes");
#endif

#define EPOLL_CLOEXEC 0x00080000

#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

#define EPOLLIN       0x00000001u
#define EPOLLPRI      0x00000002u
#define EPOLLOUT      0x00000004u
#define EPOLLERR      0x00000008u
#define EPOLLHUP      0x00000010u
#define EPOLLRDHUP    0x00002000u
#define EPOLLONESHOT  0x40000000u
#define EPOLLET       0x80000000u

int epoll_create(int size);
int epoll_create1(int flags);
int epoll_ctl(int epoll_fd, int operation, int fd,
              struct epoll_event* event);
int epoll_wait(int epoll_fd, struct epoll_event* events,
               int max_events, int timeout_ms);
int epoll_pwait(int epoll_fd, struct epoll_event* events,
                int max_events, int timeout_ms, const sigset_t* signal_mask);
int epoll_pwait2(int epoll_fd, struct epoll_event* events,
                 int max_events, const struct timespec* timeout,
                 const sigset_t* signal_mask);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_EPOLL_H */
