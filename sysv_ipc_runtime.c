/* SPDX-License-Identifier: MIT */

#include "sys/msg.h"
#include "sys/sem.h"
#include "sys/shm.h"

#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include "sys/syscall.h"
#include <rin/sysv_ipc.h>

static uint32_t sysv_ipc_operation(uint32_t domain, uint32_t command)
{
    return (domain << 16) | command;
}

static void sysv_perm_to_wire(const struct ipc_perm* source,
                              RinSysvIpcPerm* target)
{
    memset(target, 0, sizeof(*target));
    target->key = source->__key;
    target->uid = source->uid;
    target->gid = source->gid;
    target->cuid = source->cuid;
    target->cgid = source->cgid;
    target->mode = source->mode;
    target->sequence = source->__seq;
    target->reserved0 = 0u;
    target->reserved[0] = 0u;
    target->reserved[1] = 0u;
}

static void sysv_perm_from_wire(const RinSysvIpcPerm* source,
                                struct ipc_perm* target)
{
    target->__key = source->key;
    target->uid = source->uid;
    target->gid = source->gid;
    target->cuid = source->cuid;
    target->cgid = source->cgid;
    target->mode = source->mode;
    target->__seq = source->sequence;
    target->__reserved0 = source->reserved0;
    target->__reserved[0] = source->reserved[0];
    target->__reserved[1] = source->reserved[1];
}

static int sysv_message_call(uint32_t command, uintptr_t arg1,
                             uintptr_t arg2, uintptr_t arg3,
                             uintptr_t arg4, uintptr_t arg5)
{
    return (int)syscall(SYS_SYSV_IPC,
        sysv_ipc_operation(RIN_SYSV_IPC_COMMAND_MESSAGE, command),
        arg1, arg2, arg3, arg4, arg5);
}

static int sysv_semaphore_call(uint32_t command, uintptr_t arg1,
                               uintptr_t arg2, uintptr_t arg3,
                               uintptr_t arg4, uintptr_t arg5)
{
    return (int)syscall(SYS_SYSV_IPC,
        sysv_ipc_operation(RIN_SYSV_IPC_COMMAND_SEMAPHORE, command),
        arg1, arg2, arg3, arg4, arg5);
}

static int64_t sysv_shared_memory_call(uint32_t command, uintptr_t arg1,
                                      uintptr_t arg2, uintptr_t arg3)
{
    return syscall(SYS_SYSV_IPC,
        sysv_ipc_operation(RIN_SYSV_IPC_COMMAND_SHARED_MEMORY, command),
        arg1, arg2, arg3, 0u, 0u);
}

int msgget(key_t key, int msgflg)
{
    return sysv_message_call(RIN_SYSV_MSGGET, (uintptr_t)(intptr_t)key,
                             (uintptr_t)(unsigned int)msgflg, 0u, 0u, 0u);
}

int msgsnd(int msqid, const void* msgp, size_t msgsz, int msgflg)
{
    return sysv_message_call(RIN_SYSV_MSGSND, (uintptr_t)(unsigned int)msqid,
        (uintptr_t)msgp, (uintptr_t)msgsz, (uintptr_t)(unsigned int)msgflg,
        0u);
}

ssize_t msgrcv(int msqid, void* msgp, size_t msgsz, long msgtyp, int msgflg)
{
    return (ssize_t)sysv_message_call(
        RIN_SYSV_MSGRCV, (uintptr_t)(unsigned int)msqid, (uintptr_t)msgp,
        (uintptr_t)msgsz, (uintptr_t)(intptr_t)msgtyp,
        (uintptr_t)(unsigned int)msgflg);
}

int msgctl(int msqid, int cmd, struct msqid_ds* buffer)
{
    RinSysvMsqidDs wire;
    int result;
    cmd &= ~IPC_64;
    if ((cmd == IPC_SET || cmd == IPC_STAT) && buffer == NULL) {
        errno = EFAULT;
        return -1;
    }
    if (cmd == IPC_SET) {
        memset(&wire, 0, sizeof(wire));
        sysv_perm_to_wire(&buffer->msg_perm, &wire.perm);
        wire.byte_limit = buffer->msg_qbytes;
    }
    result = sysv_message_call(RIN_SYSV_MSGCTL,
        (uintptr_t)(unsigned int)msqid, (uintptr_t)(unsigned int)cmd,
        (uintptr_t)(cmd == IPC_SET || cmd == IPC_STAT
            ? (void*)&wire : (void*)buffer),
        0u, 0u);
    if (result == 0 && cmd == IPC_STAT) {
        buffer->msg_perm = (struct ipc_perm){0};
        sysv_perm_from_wire(&wire.perm, &buffer->msg_perm);
        buffer->msg_stime = (time_t)wire.send_time;
        buffer->msg_rtime = (time_t)wire.receive_time;
        buffer->msg_ctime = (time_t)wire.change_time;
        buffer->msg_cbytes = wire.current_bytes;
        buffer->msg_qnum = wire.message_count;
        buffer->msg_qbytes = wire.byte_limit;
        buffer->msg_lspid = wire.last_sender_pid;
        buffer->msg_lrpid = wire.last_receiver_pid;
        buffer->__reserved[0] = wire.reserved[0];
        buffer->__reserved[1] = wire.reserved[1];
    }
    return result;
}

int semget(key_t key, int nsems, int semflg)
{
    return sysv_semaphore_call(RIN_SYSV_SEMGET,
        (uintptr_t)(intptr_t)key, (uintptr_t)(unsigned int)nsems,
        (uintptr_t)(unsigned int)semflg, 0u, 0u);
}

int semop(int semid, struct sembuf* operations, size_t operation_count)
{
    return sysv_semaphore_call(RIN_SYSV_SEMOP,
        (uintptr_t)(unsigned int)semid, (uintptr_t)operations,
        (uintptr_t)operation_count, 0u, 0u);
}

int semtimedop(int semid, struct sembuf* operations, size_t operation_count,
               const struct timespec* timeout)
{
    RinSysvTimespec wire_timeout;
    const RinSysvTimespec* wire_timeout_pointer = NULL;
    if (timeout != NULL) {
        wire_timeout.seconds = (int64_t)timeout->tv_sec;
        wire_timeout.nanoseconds = (int64_t)timeout->tv_nsec;
        wire_timeout_pointer = &wire_timeout;
    }
    return sysv_semaphore_call(RIN_SYSV_SEMTIMEDOP,
        (uintptr_t)(unsigned int)semid, (uintptr_t)operations,
        (uintptr_t)operation_count, (uintptr_t)wire_timeout_pointer, 0u);
}

int semctl(int semid, int semnum, int command, ...)
{
    va_list arguments;
    uintptr_t argument = 0u;
    int result;
    command &= ~IPC_64;
    if (command == IPC_SET || command == IPC_STAT) {
        union semun value;
        va_start(arguments, command);
        value = va_arg(arguments, union semun);
        va_end(arguments);
        if (value.buf == NULL) {
            errno = EFAULT;
            return -1;
        }
        if (command == IPC_SET) {
            RinSysvSemidDs wire;
            memset(&wire, 0, sizeof(wire));
            sysv_perm_to_wire(&value.buf->sem_perm, &wire.perm);
            argument = (uintptr_t)&wire;
            result = sysv_semaphore_call(RIN_SYSV_SEMCTL,
                (uintptr_t)(unsigned int)semid,
                (uintptr_t)(unsigned int)semnum,
                (uintptr_t)(unsigned int)command, argument, 0u);
            return result;
        }
        {
            RinSysvSemidDs wire;
            memset(&wire, 0, sizeof(wire));
            result = sysv_semaphore_call(RIN_SYSV_SEMCTL,
                (uintptr_t)(unsigned int)semid,
                (uintptr_t)(unsigned int)semnum,
                (uintptr_t)(unsigned int)command,
                (uintptr_t)&wire, 0u);
            if (result == 0) {
                sysv_perm_from_wire(&wire.perm, &value.buf->sem_perm);
                value.buf->sem_otime = (time_t)wire.operation_time;
                value.buf->sem_ctime = (time_t)wire.change_time;
                value.buf->sem_nsems = wire.semaphore_count;
                value.buf->__reserved[0] = wire.reserved[0];
                value.buf->__reserved[1] = wire.reserved[1];
            }
            return result;
        }
    }
    if (command == SETVAL || command == SETALL || command == GETALL) {
        union semun value;
        va_start(arguments, command);
        value = va_arg(arguments, union semun);
        va_end(arguments);
        argument = command == SETVAL ? (uintptr_t)(unsigned int)value.val
                                     : (uintptr_t)value.array;
    }
    result = sysv_semaphore_call(RIN_SYSV_SEMCTL,
        (uintptr_t)(unsigned int)semid, (uintptr_t)(unsigned int)semnum,
        (uintptr_t)(unsigned int)command, argument, 0u);
    return result;
}

int shmget(key_t key, size_t size, int shmflg)
{
    return (int)sysv_shared_memory_call(RIN_SYSV_SHMGET,
        (uintptr_t)(intptr_t)key, (uintptr_t)size,
        (uintptr_t)(unsigned int)shmflg);
}

void* shmat(int shmid, const void* shmaddr, int shmflg)
{
    int64_t result = sysv_shared_memory_call(RIN_SYSV_SHMAT,
        (uintptr_t)(unsigned int)shmid, (uintptr_t)shmaddr,
        (uintptr_t)(unsigned int)shmflg);
    return result < 0 ? (void*)(intptr_t)-1 : (void*)(uintptr_t)result;
}

int shmdt(const void* shmaddr)
{
    return (int)sysv_shared_memory_call(RIN_SYSV_SHMDT,
        (uintptr_t)shmaddr, 0u, 0u);
}

int shmctl(int shmid, int command, struct shmid_ds* buffer)
{
    RinSysvShmidDs wire;
    int result;
    command &= ~IPC_64;
    if ((command == IPC_SET || command == IPC_STAT) && buffer == NULL) {
        errno = EFAULT;
        return -1;
    }
    if (command == IPC_SET) {
        memset(&wire, 0, sizeof(wire));
        sysv_perm_to_wire(&buffer->shm_perm, &wire.perm);
    }
    result = (int)sysv_shared_memory_call(RIN_SYSV_SHMCTL,
        (uintptr_t)(unsigned int)shmid, (uintptr_t)(unsigned int)command,
        (uintptr_t)(command == IPC_SET || command == IPC_STAT
            ? (void*)&wire : (void*)buffer));
    if (result == 0 && command == IPC_STAT) {
        memset(buffer, 0, sizeof(*buffer));
        sysv_perm_from_wire(&wire.perm, &buffer->shm_perm);
        buffer->shm_segsz = (size_t)wire.size;
        buffer->shm_atime = (time_t)wire.attach_time;
        buffer->shm_dtime = (time_t)wire.detach_time;
        buffer->shm_ctime = (time_t)wire.change_time;
        buffer->shm_nattch = (unsigned long)wire.attach_count;
        buffer->shm_cpid = wire.creator_pid;
        buffer->shm_lpid = wire.last_pid;
        buffer->__reserved[0] = wire.reserved[0];
        buffer->__reserved[1] = wire.reserved[1];
    }
    return result;
}
