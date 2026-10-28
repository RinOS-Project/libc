/* SPDX-License-Identifier: MIT */

#include "aio.h"

#include "errno.h"
#include "fcntl.h"
#include "limits.h"
#include "pthread.h"
#include "signal.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"

#define RIN_AIO_CAPACITY AIO_MAX
#define RIN_AIO_WORKERS 4u
#define RIN_AIO_WORKER_STACK (1024u * 1024u)

enum RinAioState {
    RIN_AIO_FREE = 0,
    RIN_AIO_QUEUED,
    RIN_AIO_RUNNING,
    RIN_AIO_DONE
};

enum RinAioOperation {
    RIN_AIO_OP_READ = 1,
    RIN_AIO_OP_WRITE,
    RIN_AIO_OP_FSYNC
};

typedef struct RinAioNotification {
    int kind;
    int signo;
    union sigval value;
    void (*function)(union sigval);
    pthread_attr_t attributes;
    int has_attributes;
} RinAioNotification;

typedef struct RinAioGroup {
    uint32_t remaining;
    RinAioNotification notification;
} RinAioGroup;

typedef struct RinAioRequest {
    struct aiocb* control;
    int state;
    int operation;
    int held_fd;
    int completion_error;
    ssize_t result;
    uint64_t sequence;
    RinAioNotification notification;
    RinAioGroup* group;
} RinAioRequest;

typedef struct RinAioCallback {
    void (*function)(union sigval);
    union sigval value;
} RinAioCallback;

static pthread_mutex_t g_rin_aio_mutex = PTHREAD_MUTEX_INITIALIZER;
/* aio_suspend() accepts a relative timeout. Pair its monotonic deadline with
 * a condition variable configured for the same clock. */
static pthread_cond_t g_rin_aio_changed = {
    0u, 0, CLOCK_MONOTONIC, PTHREAD_PROCESS_PRIVATE
};
static RinAioRequest g_rin_aio_requests[RIN_AIO_CAPACITY];
static uint32_t g_rin_aio_worker_count;
static uint32_t g_rin_aio_idle_worker_count;
static uint64_t g_rin_aio_next_sequence = 1u;

static void* rin_aio_worker(void* unused);

static int rin_aio_notification_snapshot(const struct sigevent* source,
                                         RinAioNotification* output)
{
    if (!output) return EINVAL;
    memset(output, 0, sizeof(*output));
    output->kind = SIGEV_NONE;
    if (!source) return 0;

    if (source->sigev_notify == SIGEV_NONE) return 0;
    if (source->sigev_notify == SIGEV_SIGNAL) {
        /* ゼロ初期化されたsigeventは通知なしとして扱う。 */
        if (source->sigev_signo == 0) return 0;
        if (source->sigev_signo < 0 || source->sigev_signo >= NSIG ||
            source->sigev_signo == __RIN_SIGCANCEL ||
            source->sigev_signo == SIGKILL ||
            source->sigev_signo == SIGSTOP)
            return EINVAL;
        output->kind = SIGEV_SIGNAL;
        output->signo = source->sigev_signo;
        output->value = source->sigev_value;
        return 0;
    }
    if (source->sigev_notify == SIGEV_THREAD) {
        int detach_state;
        int scope;
        if (!source->sigev_notify_function) return EINVAL;
        output->kind = SIGEV_THREAD;
        output->function = source->sigev_notify_function;
        output->value = source->sigev_value;
        if (source->sigev_notify_attributes) {
            output->attributes =
                *(const pthread_attr_t*)source->sigev_notify_attributes;
            if (pthread_attr_getdetachstate(&output->attributes,
                                            &detach_state) != 0 ||
                pthread_attr_getscope(&output->attributes, &scope) != 0 ||
                (detach_state != PTHREAD_CREATE_JOINABLE &&
                 detach_state != PTHREAD_CREATE_DETACHED) ||
                scope != PTHREAD_SCOPE_SYSTEM ||
                output->attributes.stacksize < PTHREAD_STACK_MIN)
                return EINVAL;
            output->has_attributes = 1;
            /* 通知スレッドの寿命はAIO実装が管理する。 */
            output->attributes.detachstate = PTHREAD_CREATE_DETACHED;
        }
        return 0;
    }
    return EINVAL;
}

static void* rin_aio_callback_start(void* argument)
{
    RinAioCallback* callback = (RinAioCallback*)argument;
    void (*function)(union sigval) = callback->function;
    union sigval value = callback->value;
    free(callback);
    function(value);
    return NULL;
}

static void rin_aio_notify(const RinAioNotification* notification)
{
    if (!notification || notification->kind == SIGEV_NONE) return;
    if (notification->kind == SIGEV_SIGNAL) {
        (void)sigqueue(getpid(), notification->signo, notification->value);
        return;
    }
    if (notification->kind == SIGEV_THREAD && notification->function) {
        RinAioCallback* callback =
            (RinAioCallback*)malloc(sizeof(*callback));
        pthread_t thread;
        pthread_attr_t attributes;
        const pthread_attr_t* attributes_ptr = NULL;
        int result;
        if (!callback) {
            notification->function(notification->value);
            return;
        }
        callback->function = notification->function;
        callback->value = notification->value;
        if (notification->has_attributes) {
            attributes = notification->attributes;
            attributes_ptr = &attributes;
        }
        result = pthread_create(&thread, attributes_ptr,
                                rin_aio_callback_start, callback);
        if (result != 0) {
            free(callback);
            notification->function(notification->value);
        }
    }
}

static int rin_aio_find_locked(const struct aiocb* control)
{
    uint32_t index;
    for (index = 0u; index < RIN_AIO_CAPACITY; ++index) {
        if (g_rin_aio_requests[index].state != RIN_AIO_FREE &&
            g_rin_aio_requests[index].control == control)
            return (int)index;
    }
    return -1;
}

static int rin_aio_free_slot_locked(void)
{
    uint32_t index;
    for (index = 0u; index < RIN_AIO_CAPACITY; ++index) {
        if (g_rin_aio_requests[index].state == RIN_AIO_FREE)
            return (int)index;
    }
    return -1;
}

static uint32_t rin_aio_queued_count_locked(void)
{
    uint32_t count = 0u;
    uint32_t index;
    for (index = 0u; index < RIN_AIO_CAPACITY; ++index)
        if (g_rin_aio_requests[index].state == RIN_AIO_QUEUED)
            ++count;
    return count;
}

static int rin_aio_start_workers_locked(uint32_t incoming_requests)
{
    const uint32_t queued = rin_aio_queued_count_locked() +
                            incoming_requests;
    uint32_t active = g_rin_aio_worker_count;
    uint32_t target;
    if (g_rin_aio_idle_worker_count < active)
        active -= g_rin_aio_idle_worker_count;
    else
        active = 0u;
    if (active > RIN_AIO_WORKERS) active = RIN_AIO_WORKERS;
    target = active + queued;
    if (target > RIN_AIO_WORKERS) target = RIN_AIO_WORKERS;
    while (g_rin_aio_worker_count < target) {
        pthread_attr_t attributes;
        pthread_t thread;
        int attributes_initialized = 0;
        int result = pthread_attr_init(&attributes);
        if (result == 0) {
            attributes_initialized = 1;
            result = pthread_attr_setstacksize(&attributes,
                                               RIN_AIO_WORKER_STACK);
        }
        if (result == 0)
            result = pthread_attr_setdetachstate(&attributes,
                                                 PTHREAD_CREATE_DETACHED);
        if (result == 0)
            result = pthread_create(&thread, &attributes, rin_aio_worker,
                                    NULL);
        if (attributes_initialized)
            (void)pthread_attr_destroy(&attributes);
        if (result != 0) break;
        ++g_rin_aio_worker_count;
    }
    return g_rin_aio_worker_count != 0u ? 0 : EAGAIN;
}

static int rin_aio_prepare(struct aiocb* control, int operation,
                           int* held_fd_out,
                           RinAioNotification* notification_out)
{
    int flags;
    int held_fd;
    int error;
    if (!control || !held_fd_out || !notification_out) return EINVAL;
    if (control->aio_fildes < 0 ||
        ((operation == RIN_AIO_OP_READ || operation == RIN_AIO_OP_WRITE) &&
         control->aio_reqprio != 0) ||
        ((operation == RIN_AIO_OP_READ || operation == RIN_AIO_OP_WRITE) &&
         control->aio_nbytes > (size_t)SSIZE_MAX))
        return EINVAL;
    if ((operation == RIN_AIO_OP_READ || operation == RIN_AIO_OP_WRITE) &&
        (control->aio_offset < 0 ||
         (control->aio_nbytes != 0u && !control->aio_buf)))
        return EINVAL;

    error = rin_aio_notification_snapshot(&control->aio_sigevent,
                                          notification_out);
    if (error != 0) return error;
    /* 先に複製し、別スレッドが元FDを閉じて再利用しても、送信時に選ばれた
     * open file descriptionを要求完了まで保持する。 */
    held_fd = fcntl(control->aio_fildes, F_DUPFD_CLOEXEC, 0);
    if (held_fd < 0) return errno;
    flags = fcntl(held_fd, F_GETFL);
    if (flags < 0) {
        error = errno;
        (void)close(held_fd);
        return error;
    }
    if ((operation == RIN_AIO_OP_FSYNC &&
         (flags & O_ACCMODE) == O_RDONLY) ||
        (operation == RIN_AIO_OP_READ && (flags & O_ACCMODE) == O_WRONLY) ||
        (operation == RIN_AIO_OP_WRITE &&
         (flags & O_ACCMODE) == O_RDONLY)) {
        (void)close(held_fd);
        return EBADF;
    }
    *held_fd_out = held_fd;
    return 0;
}

static RinAioGroup* rin_aio_group_complete_locked(RinAioGroup* group,
                                                  RinAioNotification* output)
{
    if (!group || !output || group->remaining == 0u) return NULL;
    --group->remaining;
    if (group->remaining != 0u) return NULL;
    *output = group->notification;
    return group;
}

static int rin_aio_submit_prepared(struct aiocb* control, int operation,
                                   int held_fd,
                                   const RinAioNotification* notification,
                                   RinAioGroup* group)
{
    int slot;
    int error = 0;
    (void)pthread_mutex_lock(&g_rin_aio_mutex);
    if (rin_aio_find_locked(control) >= 0) {
        error = EINVAL;
    } else if ((slot = rin_aio_free_slot_locked()) < 0) {
        error = EAGAIN;
    } else if ((error = rin_aio_start_workers_locked(1u)) == 0) {
        RinAioRequest* request = &g_rin_aio_requests[slot];
        memset(request, 0, sizeof(*request));
        request->control = control;
        request->state = RIN_AIO_QUEUED;
        request->operation = operation;
        request->held_fd = held_fd;
        request->sequence = g_rin_aio_next_sequence++;
        if (g_rin_aio_next_sequence == 0u) g_rin_aio_next_sequence = 1u;
        if (notification) request->notification = *notification;
        request->group = group;
        (void)pthread_cond_broadcast(&g_rin_aio_changed);
    }
    (void)pthread_mutex_unlock(&g_rin_aio_mutex);
    if (error != 0) {
        (void)close(held_fd);
        errno = error;
        return -1;
    }
    return 0;
}

static int rin_aio_submit(struct aiocb* control, int operation)
{
    RinAioNotification notification;
    int held_fd = -1;
    int error = rin_aio_prepare(control, operation, &held_fd,
                                &notification);
    if (error != 0) {
        errno = error;
        return -1;
    }
    return rin_aio_submit_prepared(control, operation, held_fd,
                                   &notification, NULL);
}

static int rin_aio_find_queued_locked(void)
{
    uint32_t index;
    int selected = -1;
    uint64_t sequence = UINT64_MAX;
    for (index = 0u; index < RIN_AIO_CAPACITY; ++index) {
        RinAioRequest* request = &g_rin_aio_requests[index];
        if (request->state == RIN_AIO_QUEUED &&
            (selected < 0 || request->sequence < sequence)) {
            selected = (int)index;
            sequence = request->sequence;
        }
    }
    return selected;
}

static void* rin_aio_worker(void* unused)
{
    (void)unused;
    for (;;) {
        RinAioRequest snapshot;
        RinAioNotification completed_notifications[2];
        RinAioGroup* completed_group = NULL;
        uint32_t notification_count = 0u;
        int slot;
        int saved_errno = 0;
        ssize_t operation_result = -1;

        (void)pthread_mutex_lock(&g_rin_aio_mutex);
        while ((slot = rin_aio_find_queued_locked()) < 0) {
            ++g_rin_aio_idle_worker_count;
            (void)pthread_cond_wait(&g_rin_aio_changed, &g_rin_aio_mutex);
            if (g_rin_aio_idle_worker_count != 0u)
                --g_rin_aio_idle_worker_count;
        }
        g_rin_aio_requests[slot].state = RIN_AIO_RUNNING;
        snapshot = g_rin_aio_requests[slot];
        (void)pthread_mutex_unlock(&g_rin_aio_mutex);

        errno = 0;
        if (snapshot.operation == RIN_AIO_OP_READ) {
            operation_result = pread(snapshot.held_fd,
                                     (void*)snapshot.control->aio_buf,
                                     snapshot.control->aio_nbytes,
                                     snapshot.control->aio_offset);
        } else if (snapshot.operation == RIN_AIO_OP_WRITE) {
            operation_result = pwrite(snapshot.held_fd,
                                      (const void*)snapshot.control->aio_buf,
                                      snapshot.control->aio_nbytes,
                                      snapshot.control->aio_offset);
        } else {
            operation_result = fsync(snapshot.held_fd);
        }
        if (operation_result < 0) saved_errno = errno ? errno : EIO;
        (void)close(snapshot.held_fd);

        memset(completed_notifications, 0, sizeof(completed_notifications));
        (void)pthread_mutex_lock(&g_rin_aio_mutex);
        if (g_rin_aio_requests[slot].state == RIN_AIO_RUNNING) {
            RinAioRequest* request = &g_rin_aio_requests[slot];
            request->state = RIN_AIO_DONE;
            request->completion_error = saved_errno;
            request->result = operation_result;
            if (request->notification.kind != SIGEV_NONE)
                completed_notifications[notification_count++] =
                    request->notification;
            if (request->group)
                completed_group = rin_aio_group_complete_locked(
                    request->group,
                    &completed_notifications[notification_count]);
            if (completed_group) ++notification_count;
            (void)pthread_cond_broadcast(&g_rin_aio_changed);
        }
        (void)pthread_mutex_unlock(&g_rin_aio_mutex);
        if (completed_group) free(completed_group);
        {
            uint32_t index;
            for (index = 0u; index < notification_count; ++index)
                rin_aio_notify(&completed_notifications[index]);
        }
    }
    return NULL;
}

int aio_read(struct aiocb* request)
{
    return rin_aio_submit(request, RIN_AIO_OP_READ);
}

int aio_write(struct aiocb* request)
{
    return rin_aio_submit(request, RIN_AIO_OP_WRITE);
}

int aio_fsync(int operation, struct aiocb* request)
{
    if (operation != O_SYNC && operation != O_DSYNC) {
        errno = EINVAL;
        return -1;
    }
    return rin_aio_submit(request, RIN_AIO_OP_FSYNC);
}

int aio_error(const struct aiocb* request)
{
    int slot;
    int result;
    if (!request) {
        errno = EINVAL;
        return -1;
    }
    (void)pthread_mutex_lock(&g_rin_aio_mutex);
    slot = rin_aio_find_locked(request);
    if (slot < 0) {
        (void)pthread_mutex_unlock(&g_rin_aio_mutex);
        errno = EINVAL;
        return -1;
    }
    if (g_rin_aio_requests[slot].state != RIN_AIO_DONE)
        result = EINPROGRESS;
    else
        result = g_rin_aio_requests[slot].completion_error;
    (void)pthread_mutex_unlock(&g_rin_aio_mutex);
    return result;
}

ssize_t aio_return(struct aiocb* request)
{
    int slot;
    int error;
    ssize_t result;
    if (!request) {
        errno = EINVAL;
        return -1;
    }
    (void)pthread_mutex_lock(&g_rin_aio_mutex);
    slot = rin_aio_find_locked(request);
    if (slot < 0 || g_rin_aio_requests[slot].state != RIN_AIO_DONE) {
        (void)pthread_mutex_unlock(&g_rin_aio_mutex);
        errno = EINVAL;
        return -1;
    }
    error = g_rin_aio_requests[slot].completion_error;
    result = g_rin_aio_requests[slot].result;
    memset(&g_rin_aio_requests[slot], 0, sizeof(g_rin_aio_requests[slot]));
    (void)pthread_mutex_unlock(&g_rin_aio_mutex);
    if (error != 0) {
        errno = error;
        return -1;
    }
    return result;
}

int aio_cancel(int fd, struct aiocb* request)
{
    int original_flags;
    int result = AIO_ALLDONE;
    int saw_running = 0;
    int saw_canceled = 0;
    int held_fds[RIN_AIO_CAPACITY];
    RinAioNotification notifications[RIN_AIO_CAPACITY * 2u];
    uint32_t held_count = 0u;
    uint32_t notification_count = 0u;
    uint32_t index;

    if (fd < 0 || (original_flags = fcntl(fd, F_GETFL)) < 0) {
        if (fd < 0) errno = EBADF;
        return -1;
    }
    (void)original_flags;
    (void)pthread_mutex_lock(&g_rin_aio_mutex);
    for (index = 0u; index < RIN_AIO_CAPACITY; ++index) {
        RinAioRequest* current = &g_rin_aio_requests[index];
        RinAioNotification completed;
        RinAioGroup* completed_group;
        if (current->state == RIN_AIO_FREE || current->control == NULL ||
            current->control->aio_fildes != fd ||
            (request && current->control != request))
            continue;
        if (current->state == RIN_AIO_QUEUED) {
            if (held_count < RIN_AIO_CAPACITY)
                held_fds[held_count++] = current->held_fd;
            current->held_fd = -1;
            current->state = RIN_AIO_DONE;
            current->completion_error = ECANCELED;
            current->result = -1;
            memset(&completed, 0, sizeof(completed));
            completed_group = current->group
                                  ? rin_aio_group_complete_locked(
                                        current->group, &completed)
                                  : NULL;
            if (completed_group) free(completed_group);
            if (current->notification.kind != SIGEV_NONE &&
                notification_count < RIN_AIO_CAPACITY * 2u)
                notifications[notification_count++] = current->notification;
            if (completed_group && completed.kind != SIGEV_NONE &&
                notification_count < RIN_AIO_CAPACITY * 2u)
                notifications[notification_count++] = completed;
            saw_canceled = 1;
        } else if (current->state == RIN_AIO_RUNNING) {
            saw_running = 1;
        }
    }
    (void)pthread_cond_broadcast(&g_rin_aio_changed);
    (void)pthread_mutex_unlock(&g_rin_aio_mutex);

    for (index = 0u; index < held_count; ++index)
        (void)close(held_fds[index]);
    for (index = 0u; index < notification_count; ++index)
        rin_aio_notify(&notifications[index]);
    if (saw_running) result = AIO_NOTCANCELED;
    else if (saw_canceled) result = AIO_CANCELED;
    return result;
}

static int rin_aio_timeout_deadline(const struct timespec* timeout,
                                   struct timespec* deadline)
{
    struct timespec now;
    uint64_t now_seconds;
    uint64_t delta_seconds;
    uint64_t max_seconds;
    uint64_t sum;
    if (!timeout || !deadline || timeout->tv_sec < 0 || timeout->tv_nsec < 0 ||
        timeout->tv_nsec >= 1000000000L)
        return EINVAL;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return errno ? errno : EIO;
    now_seconds = (uint64_t)now.tv_sec;
    delta_seconds = (uint64_t)timeout->tv_sec;
    max_seconds = sizeof(time_t) >= sizeof(int64_t)
                      ? (uint64_t)INT64_MAX
                      : (uint64_t)INT32_MAX;
    if (delta_seconds > max_seconds - now_seconds)
        sum = max_seconds;
    else
        sum = now_seconds + delta_seconds;
    deadline->tv_sec = (time_t)sum;
    deadline->tv_nsec = now.tv_nsec + timeout->tv_nsec;
    if (deadline->tv_nsec >= 1000000000L) {
        deadline->tv_nsec -= 1000000000L;
        if (sum < max_seconds) ++deadline->tv_sec;
    }
    return 0;
}

static int rin_aio_list_has_completion_locked(
    const struct aiocb* const requests[], int count)
{
    int index;
    for (index = 0; index < count; ++index) {
        int slot;
        if (!requests[index]) continue;
        slot = rin_aio_find_locked(requests[index]);
        if (slot < 0 ||
            g_rin_aio_requests[slot].state == RIN_AIO_DONE)
            return 1;
    }
    return 0;
}

static void rin_aio_unlock_mutex_cleanup(void* argument)
{
    pthread_mutex_t* mutex = (pthread_mutex_t*)argument;
    if (mutex) (void)pthread_mutex_unlock(mutex);
}

int aio_suspend(const struct aiocb* const requests[], int count,
                const struct timespec* timeout)
{
    struct timespec deadline;
    int index;
    int has_request = 0;
    int wait_error;
    if (!requests || count <= 0 || count > AIO_LISTIO_MAX) {
        errno = EINVAL;
        return -1;
    }
    for (index = 0; index < count; ++index)
        if (requests[index]) has_request = 1;
    if (!has_request) {
        errno = EINVAL;
        return -1;
    }
    if (timeout) {
        wait_error = rin_aio_timeout_deadline(timeout, &deadline);
        if (wait_error != 0) {
            errno = wait_error;
            return -1;
        }
    }

    wait_error = pthread_mutex_lock(&g_rin_aio_mutex);
    if (wait_error != 0) {
        errno = wait_error;
        return -1;
    }
    pthread_cleanup_push(rin_aio_unlock_mutex_cleanup, &g_rin_aio_mutex);
    for (;;) {
        if (rin_aio_list_has_completion_locked(requests, count)) {
            wait_error = 0;
            break;
        }
        if (timeout)
            wait_error = pthread_cond_timedwait(&g_rin_aio_changed,
                                                &g_rin_aio_mutex, &deadline);
        else
            wait_error = pthread_cond_wait(&g_rin_aio_changed,
                                           &g_rin_aio_mutex);
        if (wait_error != 0 && wait_error != ETIMEDOUT) {
            errno = wait_error;
            break;
        }
        if (wait_error == ETIMEDOUT) {
            if (rin_aio_list_has_completion_locked(requests, count)) {
                wait_error = 0;
                break;
            }
            errno = EAGAIN;
            wait_error = EAGAIN;
            break;
        }
    }
    pthread_cleanup_pop(1);
    return wait_error == 0 ? 0 : -1;
}

static void rin_aio_release_prepared(struct aiocb* const requests[],
                                     const int held_fds[], int count)
{
    int index;
    for (index = 0; index < count; ++index)
        if (held_fds[index] >= 0) (void)close(held_fds[index]);
    (void)requests;
}

int lio_listio(int mode, struct aiocb* const requests[], int count,
               const struct sigevent* notification)
{
    struct aiocb* controls[AIO_LISTIO_MAX];
    int operations[AIO_LISTIO_MAX];
    int held_fds[AIO_LISTIO_MAX];
    RinAioNotification request_notifications[AIO_LISTIO_MAX];
    RinAioNotification list_notification;
    RinAioGroup* group = NULL;
    int active_count = 0;
    int index;
    int error = 0;
    int result;

    if ((mode != LIO_WAIT && mode != LIO_NOWAIT) || !requests || count <= 0 ||
        count > AIO_LISTIO_MAX) {
        errno = EINVAL;
        return -1;
    }
    memset(held_fds, 0xff, sizeof(held_fds));
    memset(&list_notification, 0, sizeof(list_notification));
    if (mode == LIO_NOWAIT && notification) {
        error = rin_aio_notification_snapshot(notification,
                                              &list_notification);
        if (error != 0) {
            errno = error;
            return -1;
        }
        if (list_notification.kind != SIGEV_NONE) {
            group = (RinAioGroup*)malloc(sizeof(*group));
            if (!group) {
                errno = EAGAIN;
                return -1;
            }
            group->remaining = 0u;
            group->notification = list_notification;
        }
    }

    for (index = 0; index < count; ++index) {
        struct aiocb* control = requests[index];
        int operation;
        int held_fd = -1;
        if (!control || control->aio_lio_opcode == LIO_NOP) continue;
        if (control->aio_lio_opcode == LIO_READ)
            operation = RIN_AIO_OP_READ;
        else if (control->aio_lio_opcode == LIO_WRITE)
            operation = RIN_AIO_OP_WRITE;
        else {
            error = EINVAL;
            break;
        }
        error = rin_aio_prepare(control, operation, &held_fd,
                                &request_notifications[active_count]);
        if (error != 0) break;
        controls[active_count] = control;
        operations[active_count] = operation;
        held_fds[active_count] = held_fd;
        ++active_count;
    }
    if (error == 0) {
        for (index = 0; index < active_count; ++index) {
            int other;
            for (other = index + 1; other < active_count; ++other) {
                if (controls[index] == controls[other]) {
                    error = EINVAL;
                    break;
                }
            }
            if (error != 0) break;
        }
    }

    if (error == 0) {
        uint32_t available_slots = 0u;
        uint32_t slot_index;
        (void)pthread_mutex_lock(&g_rin_aio_mutex);
        for (index = 0; index < active_count; ++index) {
            if (rin_aio_find_locked(controls[index]) >= 0) {
                error = EINVAL;
                break;
            }
        }
        for (slot_index = 0u; slot_index < RIN_AIO_CAPACITY; ++slot_index)
            if (g_rin_aio_requests[slot_index].state == RIN_AIO_FREE)
                ++available_slots;
        if (error == 0 && active_count > (int)available_slots)
            error = EAGAIN;
        if (error == 0 && active_count != 0)
            error = rin_aio_start_workers_locked((uint32_t)active_count);
        if (error == 0) {
            if (group) group->remaining = (uint32_t)active_count;
            for (index = 0; index < active_count; ++index) {
                const int slot = rin_aio_free_slot_locked();
                RinAioRequest* request = &g_rin_aio_requests[slot];
                memset(request, 0, sizeof(*request));
                request->control = controls[index];
                request->state = RIN_AIO_QUEUED;
                request->operation = operations[index];
                request->held_fd = held_fds[index];
                request->notification = request_notifications[index];
                request->sequence = g_rin_aio_next_sequence++;
                if (g_rin_aio_next_sequence == 0u)
                    g_rin_aio_next_sequence = 1u;
                request->group = group;
            }
            (void)pthread_cond_broadcast(&g_rin_aio_changed);
        }
        (void)pthread_mutex_unlock(&g_rin_aio_mutex);
    }

    if (error != 0) {
        rin_aio_release_prepared(controls, held_fds, active_count);
        free(group);
        errno = error;
        return -1;
    }
    if (active_count == 0 && group) {
        free(group);
        rin_aio_notify(&list_notification);
    }
    if (mode == LIO_NOWAIT) return 0;
    if (active_count == 0) return 0;
    {
        int has_error = 0;
        for (;;) {
            const struct aiocb* pending[AIO_LISTIO_MAX];
            int pending_count = 0;
            for (index = 0; index < active_count; ++index) {
                const int request_error = aio_error(controls[index]);
                if (request_error == EINPROGRESS) {
                    pending[pending_count++] = controls[index];
                } else if (request_error != 0) {
                    has_error = 1;
                }
            }
            if (pending_count == 0) break;
            result = aio_suspend(pending, pending_count, NULL);
            if (result != 0) return -1;
        }
        if (has_error) {
            errno = EIO;
            return -1;
        }
    }
    return 0;
}
