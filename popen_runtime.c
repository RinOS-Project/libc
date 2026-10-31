/* SPDX-License-Identifier: MIT */
/* POSIX popen/pclose implementation using the process spawn owner. */

#ifndef RINSDK_SPLIT_BUILD
#define RINSDK_SPLIT_BUILD 1
#endif
#include "stdio.h"

#if defined(RIN_USERSPACE)

#include "fcntl.h"
#include "pthread.h"
#include "spawn.h"
#include "stdlib.h"
#include "sys/wait.h"
#include "unistd.h"
#include "internal/rin_child_pidfd.h"

#ifndef RIN_POPEN_PIPE2
#define RIN_POPEN_PIPE2 pipe2
#endif
#ifndef RIN_POPEN_FCNTL
#define RIN_POPEN_FCNTL fcntl
#endif
#ifndef RIN_POPEN_CLOSE
#define RIN_POPEN_CLOSE close
#endif
#ifndef RIN_POPEN_SPAWN_PIDFD
#define RIN_POPEN_SPAWN_PIDFD rin_posix_spawn_pidfd
#endif
#ifndef RIN_POPEN_FDOPEN
#define RIN_POPEN_FDOPEN fdopen
#endif
#ifndef RIN_POPEN_FCLOSE
#define RIN_POPEN_FCLOSE fclose
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
    int pidfd;
    int registered;
} RinPopenChild;

static RinPopenChild* g_popen_children;
static pthread_mutex_t g_popen_children_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t g_popen_children_once = PTHREAD_ONCE_INIT;
static int g_popen_children_atfork_error;

static void rin_popen_atfork_prepare(void) {
    (void)pthread_mutex_lock(&g_popen_children_lock);
}

static void rin_popen_atfork_unlock(void) {
    (void)pthread_mutex_unlock(&g_popen_children_lock);
}

static void rin_popen_initialize_atfork(void) {
    g_popen_children_atfork_error = pthread_atfork(
        rin_popen_atfork_prepare, rin_popen_atfork_unlock,
        rin_popen_atfork_unlock);
}

/* Cancellation stays disabled until the caller releases the registry lock.
 * A fork runs the prepare handler before cloning the list and the parent/child
 * handler releases the mutex in each resulting process. */
static int rin_popen_lock(int* old_cancel_state) {
    int error;
    if (!old_cancel_state) {
        errno = EINVAL;
        return -1;
    }
    *old_cancel_state = -1;
    error = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,
                                   old_cancel_state);
    if (error != 0) {
        errno = error;
        return -1;
    }
    error = pthread_once(&g_popen_children_once,
                         rin_popen_initialize_atfork);
    if (error != 0) {
        errno = error;
        return -1;
    }
    if (g_popen_children_atfork_error != 0) {
        errno = g_popen_children_atfork_error;
        return -1;
    }
    error = pthread_mutex_lock(&g_popen_children_lock);
    if (error != 0) {
        errno = error;
        return -1;
    }
    return 0;
}

static void rin_popen_unlock(void) {
    (void)pthread_mutex_unlock(&g_popen_children_lock);
}

static void rin_popen_restore_cancel_state(int state) {
    if (state >= 0) (void)pthread_setcancelstate(state, NULL);
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

static void rin_popen_reap_failed_child(int* pidfd) {
    (void)rin_child_pidfd_terminate_and_wait(pidfd);
}

static void rin_popen_cancel_child(void* argument) {
    RinPopenChild* record = (RinPopenChild*)argument;
    RinPopenChild** cursor;

    if (!record) return;
    (void)pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    if (record->registered) {
        if (pthread_mutex_lock(&g_popen_children_lock) != 0) return;
        cursor = &g_popen_children;
        while (*cursor && *cursor != record) cursor = &(*cursor)->next;
        if (*cursor == record) *cursor = record->next;
        record->registered = 0;
        rin_popen_unlock();
    }
    if (record->stream) {
        (void)RIN_POPEN_FCLOSE(record->stream);
        record->stream = NULL;
    }
    if (record->pidfd >= 0) {
        rin_popen_reap_failed_child(&record->pidfd);
    }
    RIN_POPEN_FREE(record);
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
    int cancel_state = -1;
    int lock_cancel_state = -1;
    pid_t child = 0;
    int child_pidfd = -1;
    char* arguments[4];
    RinPopenChild* record;
    FILE* stream;

    if (!command || !type ||
        !((type[0] == 'r' || type[0] == 'w') && type[1] == '\0')) {
        errno = EINVAL;
        return NULL;
    }
    error = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancel_state);
    if (error != 0) {
        errno = error;
        return NULL;
    }
    record = (RinPopenChild*)RIN_POPEN_ALLOC(sizeof(*record));
    if (!record) {
        saved_error = errno ? errno : ENOMEM;
        rin_popen_restore_cancel_state(cancel_state);
        errno = saved_error;
        return NULL;
    }

    if (RIN_POPEN_PIPE2(descriptors, O_CLOEXEC) != 0) {
        saved_error = errno ? errno : EIO;
        RIN_POPEN_FREE(record);
        rin_popen_restore_cancel_state(cancel_state);
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
    error = RIN_POPEN_SPAWN_PIDFD(&child, &child_pidfd, "/bin/sh",
                                  &actions, NULL, arguments, NULL);
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
    record->pidfd = child_pidfd;
    child_pidfd = -1;
    record->registered = 0;
    if (rin_popen_lock(&lock_cancel_state) != 0) {
        saved_error = errno ? errno : EIO;
        (void)RIN_POPEN_FCLOSE(stream);
        rin_popen_reap_failed_child(&record->pidfd);
        RIN_POPEN_FREE(record);
        rin_popen_restore_cancel_state(cancel_state);
        errno = saved_error;
        return NULL;
    }
    record->next = g_popen_children;
    g_popen_children = record;
    record->registered = 1;
    pthread_cleanup_push(rin_popen_cancel_child, record);
    rin_popen_unlock();
    error = pthread_setcancelstate(cancel_state, NULL);
    pthread_cleanup_pop(error != 0);
    if (error != 0) {
        errno = error;
        return NULL;
    }
    return stream;

fail_after_spawn:
    rin_popen_close_pipe(descriptors, extra_fd);
    rin_popen_reap_failed_child(&child_pidfd);
    RIN_POPEN_FREE(record);
    rin_popen_restore_cancel_state(cancel_state);
    errno = saved_error;
    return NULL;

fail_before_spawn:
    if (actions_initialized)
        (void)posix_spawn_file_actions_destroy(&actions);
    rin_popen_close_pipe(descriptors, extra_fd);
    RIN_POPEN_FREE(record);
    rin_popen_restore_cancel_state(cancel_state);
    errno = saved_error;
    return NULL;
}

int pclose(FILE* stream) {
    RinPopenChild** cursor;
    RinPopenChild* record;
    int close_result;
    int close_error;
    int status = 0;
    int cancel_state = -1;
    int wait_error = 0;
    int result = -1;

    if (!stream) {
        errno = EINVAL;
        return -1;
    }
    if (rin_popen_lock(&cancel_state) != 0) {
        int lock_error = errno ? errno : EIO;
        rin_popen_restore_cancel_state(cancel_state);
        errno = lock_error;
        return -1;
    }
    cursor = &g_popen_children;
    while (*cursor && (*cursor)->stream != stream)
        cursor = &(*cursor)->next;
    record = *cursor;
    if (record) {
        *cursor = record->next;
        record->registered = 0;
    }
    rin_popen_unlock();
    if (!record) {
        rin_popen_restore_cancel_state(cancel_state);
        errno = EINVAL;
        return -1;
    }

    pthread_cleanup_push(rin_popen_cancel_child, record);
    close_result = RIN_POPEN_FCLOSE(stream);
    record->stream = NULL;
    close_error = errno;
    (void)pthread_setcancelstate(cancel_state, NULL);
    if (rin_child_pidfd_wait_status(record->pidfd, &status) == 0) {
        (void)RIN_POPEN_CLOSE(record->pidfd);
        record->pidfd = -1;
        result = status;
    } else {
        wait_error = errno ? errno : EIO;
        if (wait_error == ECHILD) {
            (void)RIN_POPEN_CLOSE(record->pidfd);
            record->pidfd = -1;
        } else {
            rin_popen_reap_failed_child(&record->pidfd);
        }
    }
    (void)pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_cleanup_pop(0);
    RIN_POPEN_FREE(record);
    rin_popen_restore_cancel_state(cancel_state);
    if (close_result != 0) {
        errno = close_error ? close_error : EIO;
        return -1;
    }
    if (wait_error != 0) {
        errno = wait_error;
        return -1;
    }
    return result;
}

#endif /* RIN_USERSPACE */
