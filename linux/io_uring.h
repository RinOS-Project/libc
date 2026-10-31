/* SPDX-License-Identifier: MIT */
/* RinOS Linux-compatible io_uring UAPI subset. */
#ifndef _LINUX_IO_URING_H
#define _LINUX_IO_URING_H

#include <stdint.h>

#ifndef RIN_LINUX_IO_URING_TYPES_DEFINED
#define RIN_LINUX_IO_URING_TYPES_DEFINED 1
typedef uint8_t __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint64_t __u64;
typedef int32_t __s32;
typedef int64_t __s64;
#endif

struct io_uring_sqe {
    __u8 opcode;
    __u8 flags;
    __u16 ioprio;
    __s32 fd;
    __u64 off;
    __u64 addr;
    __u32 len;
    union {
        __u32 rw_flags;
        __u32 poll32_events;
        __u16 poll_events;
    };
    __u64 user_data;
    union {
        __u16 buf_index;
        __u16 buf_group;
    } __attribute__((packed));
    __u16 personality;
    union {
        __s32 splice_fd_in;
        __u32 file_index;
    };
    __u64 addr3;
    __u64 __pad2;
};

struct io_uring_cqe {
    __u64 user_data;
    __s32 res;
    __u32 flags;
};

struct io_sqring_offsets {
    __u32 head;
    __u32 tail;
    __u32 ring_mask;
    __u32 ring_entries;
    __u32 flags;
    __u32 dropped;
    __u32 array;
    __u32 resv1;
    __u64 user_addr;
};

struct io_cqring_offsets {
    __u32 head;
    __u32 tail;
    __u32 ring_mask;
    __u32 ring_entries;
    __u32 overflow;
    __u32 cqes;
    __u32 flags;
    __u32 resv1;
    __u64 user_addr;
};

struct io_uring_params {
    __u32 sq_entries;
    __u32 cq_entries;
    __u32 flags;
    __u32 sq_thread_cpu;
    __u32 sq_thread_idle;
    __u32 features;
    __u32 wq_fd;
    __u32 resv[3];
    struct io_sqring_offsets sq_off;
    struct io_cqring_offsets cq_off;
};

struct io_uring_getevents_arg {
    __u64 sigmask;
    __u32 sigmask_sz;
    __u32 pad;
    __u64 ts;
};

struct io_uring_files_update {
    __u32 offset;
    __u32 resv;
    __u64 fds;
};

struct io_uring_probe_op {
    __u8 op;
    __u8 resv;
    __u16 flags;
    __u32 resv2;
};

struct io_uring_probe {
    __u8 last_op;
    __u8 ops_len;
    __u16 resv;
    __u32 resv2[3];
    struct io_uring_probe_op ops[];
};

#define IORING_SETUP_IOPOLL              (1U << 0)
#define IORING_SETUP_SQPOLL              (1U << 1)
#define IORING_SETUP_SQ_AFF              (1U << 2)
#define IORING_SETUP_CQSIZE              (1U << 3)
#define IORING_SETUP_CLAMP               (1U << 4)
#define IORING_SETUP_ATTACH_WQ            (1U << 5)
#define IORING_SETUP_R_DISABLED           (1U << 6)
#define IORING_SETUP_SUBMIT_ALL           (1U << 7)
#define IORING_SETUP_COOP_TASKRUN         (1U << 8)
#define IORING_SETUP_TASKRUN_FLAG         (1U << 9)
#define IORING_SETUP_SQE128               (1U << 10)
#define IORING_SETUP_CQE32                (1U << 11)
#define IORING_SETUP_SINGLE_ISSUER        (1U << 12)
#define IORING_SETUP_DEFER_TASKRUN        (1U << 13)
#define IORING_SETUP_NO_MMAP              (1U << 14)
#define IORING_SETUP_REGISTERED_FD_ONLY   (1U << 15)
#define IORING_SETUP_NO_SQARRAY           (1U << 16)

#define IORING_FEAT_SINGLE_MMAP           (1U << 0)
#define IORING_FEAT_NODROP                (1U << 1)
#define IORING_FEAT_SUBMIT_STABLE         (1U << 2)
#define IORING_FEAT_RW_CUR_POS            (1U << 3)
#define IORING_FEAT_FAST_POLL             (1U << 5)
#define IORING_FEAT_POLL_32BITS           (1U << 6)

#define IORING_ENTER_GETEVENTS            (1U << 0)
#define IORING_ENTER_SQ_WAKEUP            (1U << 1)
#define IORING_ENTER_SQ_WAIT              (1U << 2)
#define IORING_ENTER_EXT_ARG              (1U << 3)

#define IORING_OFF_SQ_RING                0ULL
#define IORING_OFF_CQ_RING                0x8000000ULL
#define IORING_OFF_SQES                   0x10000000ULL

#define IORING_OP_NOP                     0
#define IORING_OP_READV                   1
#define IORING_OP_WRITEV                  2
#define IORING_OP_FSYNC                   3
#define IORING_OP_READ_FIXED              4
#define IORING_OP_WRITE_FIXED             5
#define IORING_OP_POLL_ADD                6
#define IORING_OP_POLL_REMOVE             7
#define IORING_OP_SYNC_FILE_RANGE         8
#define IORING_OP_SENDMSG                 9
#define IORING_OP_RECVMSG                 10
#define IORING_OP_TIMEOUT                 11
#define IORING_OP_TIMEOUT_REMOVE          12
#define IORING_OP_ACCEPT                  13
#define IORING_OP_ASYNC_CANCEL            14
#define IORING_OP_LINK_TIMEOUT            15
#define IORING_OP_CONNECT                 16
#define IORING_OP_FALLOCATE               17
#define IORING_OP_OPENAT                  18
#define IORING_OP_CLOSE                   19
#define IORING_OP_FILES_UPDATE            20
#define IORING_OP_STATX                   21
#define IORING_OP_READ                    22
#define IORING_OP_WRITE                   23
#define IORING_OP_FADVISE                 24
#define IORING_OP_MADVISE                 25
#define IORING_OP_SEND                    26
#define IORING_OP_RECV                    27
#define IORING_OP_OPENAT2                 28
#define IORING_OP_EPOLL_CTL               29
#define IORING_OP_SPLICE                  30
#define IORING_OP_PROVIDE_BUFFERS         31
#define IORING_OP_REMOVE_BUFFERS          32
#define IORING_OP_TEE                     33
#define IORING_OP_SHUTDOWN                34
#define IORING_OP_RENAMEAT                35
#define IORING_OP_UNLINKAT                36
#define IORING_OP_MKDIRAT                 37
#define IORING_OP_SYMLINKAT               38
#define IORING_OP_LINKAT                  39
#define IORING_OP_MSG_RING                40
#define IORING_OP_LAST                    41

#define IOSQE_FIXED_FILE                  (1U << 0)
#define IOSQE_IO_DRAIN                    (1U << 1)
#define IOSQE_IO_LINK                     (1U << 2)
#define IOSQE_IO_HARDLINK                 (1U << 3)
#define IOSQE_ASYNC                       (1U << 4)
#define IOSQE_BUFFER_SELECT               (1U << 5)
#define IOSQE_CQE_SKIP_SUCCESS            (1U << 6)

#define IORING_REGISTER_BUFFERS           0
#define IORING_UNREGISTER_BUFFERS         1
#define IORING_REGISTER_FILES             2
#define IORING_UNREGISTER_FILES           3
#define IORING_REGISTER_EVENTFD           4
#define IORING_UNREGISTER_EVENTFD         5
#define IORING_REGISTER_FILES_UPDATE      6
#define IORING_REGISTER_PROBE             8
#define IORING_REGISTER_RESTRICTIONS      11
#define IORING_REGISTER_ENABLE_RINGS      12

#define IO_URING_OP_SUPPORTED             (1U << 0)
#define IORING_ASYNC_CANCEL_ALL           (1U << 0)
#define IORING_ASYNC_CANCEL_FD            (1U << 1)
#define IORING_ASYNC_CANCEL_ANY           (1U << 2)
#define IORING_FSYNC_DATASYNC             (1U << 0)
#define IORING_CQE_F_MORE                 (1U << 1)
#define IORING_CQE_F_BUFFER               (1U << 0)

#endif /* _LINUX_IO_URING_H */
