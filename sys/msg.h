/* SPDX-License-Identifier: MIT */
#ifndef _SYS_MSG_H
#define _SYS_MSG_H

#include "ipc.h"

#ifdef __cplusplus
extern "C" {
#endif

struct msqid_ds {
    struct ipc_perm msg_perm;
    time_t msg_stime;
    time_t msg_rtime;
    time_t msg_ctime;
    unsigned long msg_cbytes;
    unsigned long msg_qnum;
    unsigned long msg_qbytes;
    pid_t msg_lspid;
    pid_t msg_lrpid;
    unsigned long __reserved[2];
};

struct msginfo {
    int msgpool;
    int msgmap;
    int msgmax;
    int msgmnb;
    int msgmni;
    int msgssz;
    int msgtql;
    unsigned short msgseg;
};

#define MSG_NOERROR 010000
#define MSG_EXCEPT  020000

int msgget(key_t key, int msgflg);
int msgsnd(int msqid, const void* msgp, size_t msgsz, int msgflg);
ssize_t msgrcv(int msqid, void* msgp, size_t msgsz, long msgtyp,
               int msgflg);
int msgctl(int msqid, int cmd, struct msqid_ds* buf);

#ifdef __cplusplus
}
#endif
#endif
