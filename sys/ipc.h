/* SPDX-License-Identifier: MIT */
#ifndef _SYS_IPC_H
#define _SYS_IPC_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ipc_perm {
    key_t __key;
    uid_t uid;
    gid_t gid;
    uid_t cuid;
    gid_t cgid;
    mode_t mode;
    uint32_t __seq;
    uint32_t __reserved0;
    uint64_t __reserved[2];
};

#define IPC_PRIVATE 0
#define IPC_CREAT   01000
#define IPC_EXCL    02000
#define IPC_NOWAIT  04000
#define IPC_RMID    0
#define IPC_SET     1
#define IPC_STAT    2
#define IPC_64      0100

#ifdef __cplusplus
}
#endif
#endif
