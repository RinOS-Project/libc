/* SPDX-License-Identifier: MIT */
#ifndef _SYS_SEM_H
#define _SYS_SEM_H

#include "ipc.h"
#include "../time.h"

#ifdef __cplusplus
extern "C" {
#endif

struct semid_ds {
    struct ipc_perm sem_perm;
    time_t sem_otime;
    time_t sem_ctime;
    unsigned long sem_nsems;
    unsigned long __reserved[2];
};

struct sembuf {
    unsigned short sem_num;
    short sem_op;
    short sem_flg;
};

union semun {
    int val;
    struct semid_ds* buf;
    unsigned short* array;
    void* __buf;
};

#define SEM_UNDO  010000
#define GETNCNT   3
#define GETPID    11
#define GETVAL    12
#define GETALL    13
#define GETZCNT   14
#define SETVAL    16
#define SETALL    17

int semget(key_t key, int nsems, int semflg);
int semop(int semid, struct sembuf* sops, size_t nsops);
int semtimedop(int semid, struct sembuf* sops, size_t nsops,
               const struct timespec* timeout);
int semctl(int semid, int semnum, int cmd, ...);

#ifdef __cplusplus
}
#endif
#endif
