/* SPDX-License-Identifier: MIT */
#ifndef _SYS_SHM_H
#define _SYS_SHM_H

#include "ipc.h"
#include "../time.h"

#ifdef __cplusplus
extern "C" {
#endif

struct shmid_ds {
    struct ipc_perm shm_perm;
    size_t shm_segsz;
    time_t shm_atime;
    time_t shm_dtime;
    time_t shm_ctime;
    pid_t shm_cpid;
    pid_t shm_lpid;
    unsigned long shm_nattch;
    uint64_t __reserved[2];
};

#define SHM_RDONLY 010000
#define SHM_RND    020000
#define SHM_REMAP  040000
#define SHM_EXEC   0100000

int shmget(key_t key, size_t size, int shmflg);
void* shmat(int shmid, const void* shmaddr, int shmflg);
int shmdt(const void* shmaddr);
int shmctl(int shmid, int command, struct shmid_ds* buffer);

#ifdef __cplusplus
}
#endif
#endif
