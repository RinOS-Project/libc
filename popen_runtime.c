/* SPDX-License-Identifier: MIT */
/* POSIX popen/pclose implementation using the process spawn owner. */

#include "stdio.h"

#if defined(RIN_USERSPACE)

#include "fcntl.h"
#include "signal.h"
#include "spawn.h"
#include "stdlib.h"
#include "sys/wait.h"
#include "unistd.h"

#ifndef RIN_POPEN_PIPE2
#define RIN_POPEN_PIPE2 pipe2
#endif
#ifndef RIN_POPEN_FCNTL
#define RIN_POPEN_FCNTL fcntl
#endif
#ifndef RIN_POPEN_CLOSE
#define RIN_POPEN_CLOSE close
#endif
#ifndef RIN_POPEN_SPAWN
#define RIN_POPEN_SPAWN posix_spawn
#endif
#ifndef RIN_POPEN_FDOPEN
#define RIN_POPEN_FDOPEN fdopen
#endif
#ifndef RIN_POPEN_FCLOSE
#define RIN_POPEN_FCLOSE fclose
#endif
#ifndef RIN_POPEN_WAITPID
#define RIN_POPEN_WAITPID waitpid
#endif
#ifndef RIN_POPEN_KILL
#define RIN_POPEN_KILL kill
#endif
#ifndef RIN_POPEN_ALLOC
#define RIN_POPEN_ALLOC malloc
#endif
#ifndef RIN_POPEN_FREE
#define RIN_POPEN_FREE free
#endif

typedef struct RinPopenChild {
    struct RinPopenChild* next;
    FILE* stream;
    pid_t pid;
} RinPopenChild;

static RinPopenChild* g_popen_children;
static volatile uint32_t g_popen_children_lock;

static void rin_popen_lock(void) {
    while (__atomic_exchange_n(&g_popen_children_lock, 1u,
                               __ATOMIC_ACQUIRE) != 0u) {
        while (__atomic_load_n(&g_popen_children_lock,
                               __ATOMIC_RELAXED) != 0u) {
        }
    }
}

static void rin_popen_unlock(void) {
    __atomic_store_n(&g_popen_children_lock, 0u, __ATOMIC_RELEASE);
}

static int rin_popen_close_fd(int* fd) {
    int value;
    if (!fd || *fd < 0) return 0;
    value = *fd;
    *fd = -1;
    return RIN_POPEN_CLOSE(value);
}

static void rin_popen_close_pipe(int descriptors[2], int extra_fd) {
    (void)rin_popen_close_fd(&descriptors[0]);
    (void)rin_popen_close_fd(&descriptors[1]);
    if (extra_fd >= 0) (void)RIN_POPEN_CLOSE(extra_fd);
}

static void rin_popen_reap_failed_child(pid_t pid) {
    int status = 0;
    (void)RIN_POPEN_KILL(pid, SIGKILL);
    while (RIN_POPEN_WAITPID(pid, &status, 0) < 0 && errno == EINTR) {
    }
}

static int rin_popen_move_above_stdio(int descriptors[2], int* extra_fd) {
    int index;
    for (index = 0; index < 2; ++index) {
        int old_fd = descriptors[index];
        int new_fd;
        if (old_fd >= 3) continue;
        new_fd = RIN_POPEN_FCNTL(old_fd, F_DUPFD_CLOEXEC, 3);
        if (new_fd < 0) return -1;
        *extra_fd = new_fd;
        if (RIN_POPEN_CLOSE(old_fd) != 0) return -1;
        descriptors[index] = new_fd;
        *extra_fd = -1;
    }
    return 0;
}

FILE* popen(const char* command, const char* type) {
    int descriptors[2] = {-1, -1};
    int extra_fd = -1;
    int parent_index;
    int child_index;
    int child_target;
    const char* stream_mode;
    posix_spawn_file_actions_t actions;
    int actions_initialized = 0;
    int error;
    int saved_error;
    pid_t child = 0;
    char* arguments[4];
    RinPopenChild* record;
    FILE* stream;

    if (!command || !type ||
        !((type[0] == 'r' || type[0] == 'w') && type[1] == '\0')) {
        errno = EINVAL;
        return NULL;
    }
    record = (RinPopenChild*)RIN_POPEN_ALLOC(sizeof(*record));
    if (!record) {
        if (errno == 0) errno = ENOMEM;
        return NULL;
    }

    if (RIN_POPEN_PIPE2(descriptors, O_CLOEXEC) != 0) {
        saved_error = errno;
        RIN_POPEN_FREE(record);
        errno = saved_error;
        return NULL;
    }
    if (rin_popen_move_above_stdio(descriptors, &extra_fd) != 0) {
        saved_error = errno ? errno : EIO;
        goto fail_before_spawn;
    }

    if (type[0] == 'r') {
        parent_index = 0;
        child_index = 1;
        child_target = STDOUT_FILENO;
        stream_mode = "r";
    } else {
        parent_index = 1;
        child_index = 0;
        child_target = STDIN_FILENO;
        stream_mode = "w";
    }

    error = posix_spawn_file_actions_init(&actions);
    if (error != 0) {
        saved_error = error;
        goto fail_before_spawn;
    }
    actions_initialized = 1;
    error = posix_spawn_file_actions_adddup2(
        &actions, descriptors[child_index], child_target);
    if (error == 0)
        error = posix_spawn_file_actions_addclose(&actions, descriptors[0]);
    if (error == 0)
        error = posix_spawn_file_actions_addclose(&actions, descriptors[1]);
    if (error != 0) {
        saved_error = error;
        goto fail_before_spawn;
    }

    arguments[0] = (char*)"sh";
    arguments[1] = (char*)"-c";
    arguments[2] = (char*)command;
    arguments[3] = NULL;
    error = RIN_POPEN_SPAWN(&child, "/bin/sh", &actions, NULL,
                            arguments, NULL);
    if (error != 0) {
        saved_error = error;
        goto fail_before_spawn;
    }
    error = posix_spawn_file_actions_destroy(&actions);
    actions_initialized = 0;
    if (error != 0) {
        saved_error = error;
        goto fail_after_spawn;
    }
    if (rin_popen_close_fd(&descriptors[child_index]) != 0) {
        saved_error = errno ? errno : EIO;
        goto fail_after_spawn;
    }

    stream = RIN_POPEN_FDOPEN(descriptors[parent_index], stream_mode);
    if (!stream) {
        saved_error = errno ? errno : EIO;
        goto fail_after_spawn;
    }
    descriptors[parent_index] = -1;

    record->stream = stream;
    record->pid = child;
    rin_popen_lock();
    record->next = g_popen_children;
    g_popen_children = record;
    rin_popen_unlock();
    return stream;

fail_after_spawn:
    rin_popen_close_pipe(descriptors, extra_fd);
    rin_popen_reap_failed_child(child);
    RIN_POPEN_FREE(record);
    errno = saved_error;
    return NULL;

fail_before_spawn:
    if (actions_initialized)
        (void)posix_spawn_file_actions_destroy(&actions);
    rin_popen_close_pipe(descriptors, extra_fd);
    RIN_POPEN_FREE(record);
    errno = saved_error;
    return NULL;
}

int pclose(FILE* stream) {
    RinPopenChild** cursor;
    RinPopenChild* record;
    pid_t child;
    int close_result;
    int close_error;
    int status = 0;
    pid_t waited;

    if (!stream) {
        errno = EINVAL;
        return -1;
    }
    rin_popen_lock();
    cursor = &g_popen_children;
    while (*cursor && (*cursor)->stream != stream)
        cursor = &(*cursor)->next;
    record = *cursor;
    if (record) *cursor = record->next;
    rin_popen_unlock();
    if (!record) {
        errno = EINVAL;
        return -1;
    }

    child = record->pid;
    RIN_POPEN_FREE(record);
    close_result = RIN_POPEN_FCLOSE(stream);
    close_error = errno;
    do {
        waited = RIN_POPEN_WAITPID(child, &status, 0);
    } while (waited < 0 && errno == EINTR);
    if (close_result != 0) {
        errno = close_error ? close_error : EIO;
        return -1;
    }
    if (waited < 0) return -1;
    if (waited != child) {
        errno = ECHILD;
        return -1;
    }
    return status;
}

#endif /* RIN_USERSPACE */
