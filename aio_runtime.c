/* SPDX-License-Identifier: MIT */

#include "aio.h"

#include "errno.h"
#include "fcntl.h"
#include "limits.h"
#include "pthread.h"
#include "internal/pthread_cond_interrupt.h"
#include "signal.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"

#define RIN_AIO_CAPACITY AIO_MAX
#define RIN_AIO_WORKERS 4u
#define RIN_AIO_WORKER_STACK (1024u * 1024u)
#define RIN_AIO_CALLBACK_CAPACITY RIN_AIO_CAPACITY

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

typedef struct RinAioRequest RinAioRequest;
typedef struct RinAioSuspendWaiter RinAioSuspendWaiter;

typedef struct RinAioSubscription {
    RinAioSuspendWaiter* waiter;
    RinAioRequest* request;
    struct RinAioSubscription* waiter_next;
    struct RinAioSubscription* request_previous;
    struct RinAioSubscription* request_next;
} RinAioSubscription;

typedef struct RinAioNotification {
    int kind;
    int signo;
    union sigval value;
    void (*function)(union sigval);
    pthread_attr_t attributes;
    int has_attributes;
    int callback_reserved;
} RinAioNotification;

typedef struct RinAioGroup {
    uint32_t remaining;
    RinAioNotification notification;
} RinAioGroup;

struct RinAioRequest {
    struct aiocb* control;
    int state;
    int operation;
    int original_fd;
    int held_fd;
    int completion_error;
    ssize_t result;
    uint64_t sequence;
    uint64_t barrier_sequence;
    RinAioNotification notification;
    RinAioGroup* group;
    RinAioSubscription* suspend_waiters;
};

struct RinAioSuspendWaiter {
    pthread_cond_t changed;
    int completed;
    RinAioSubscription* subscriptions;
};

typedef struct RinAioCallback {
    void (*function)(union sigval);
    union sigval value;
} RinAioCallback;

typedef struct RinAioCallbackJob {
    void (*function)(union sigval);
    union sigval value;
    pthread_attr_t attributes;
    int has_attributes;
} RinAioCallbackJob;

static pthread_mutex_t g_rin_aio_mutex = PTHREAD_MUTEX_INITIALIZER;
/* aio_suspend() accepts a relative timeout. Pair its monotonic deadline with
 * a condition variable configured for the same clock. */
static pthread_cond_t g_rin_aio_changed = {
    0u, 0, CLOCK_MONOTONIC, PTHREAD_PROCESS_PRIVATE
};
/* Serialize submit-side worker admission while allowing the AIO state mutex
 * to be dropped around pthread_create().  Workers never acquire this mutex. */
static pthread_mutex_t g_rin_aio_worker_start_mutex = PTHREAD_MUTEX_INITIALIZER;
static RinAioRequest g_rin_aio_requests[RIN_AIO_CAPACITY];
static uint32_t g_rin_aio_worker_count;
static uint32_t g_rin_aio_idle_worker_count;
static uint64_t g_rin_aio_next_sequence = 1u;
/* A dedicated dispatcher keeps callback work out of the I/O worker pool.
 * Reservations bound queued plus running callback work before an AIO request
 * is accepted, so completion never has to run a callback inline or drop it. */
static pthread_mutex_t g_rin_aio_callback_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_rin_aio_callback_changed = {
    0u, 0, CLOCK_MONOTONIC, PTHREAD_PROCESS_PRIVATE
};
static RinAioCallbackJob
    g_rin_aio_callback_jobs[RIN_AIO_CALLBACK_CAPACITY];
static uint32_t g_rin_aio_callback_head;
static uint32_t g_rin_aio_callback_count;
static uint32_t g_rin_aio_callback_reservations;
static uint32_t g_rin_aio_callback_dispatcher_started;

static void* rin_aio_worker(void* unused);
static void* rin_aio_callback_dispatcher(void* unused);
static void rin_aio_suspend_notify_locked(RinAioRequest* request);
static int rin_aio_callback_start_dispatcher_locked(void);
static int rin_aio_callback_reserve(void);
static void rin_aio_callback_release_reservation(void);

static int rin_aio_callback_start_dispatcher_locked(void)
{
    pthread_attr_t attributes;
    pthread_t thread;
    int attributes_initialized = 0;
    int result = pthread_attr_init(&attributes);
    if (result == 0) {
        attributes_initialized = 1;
        result = pthread_attr_setdetachstate(
            &attributes, PTHREAD_CREATE_DETACHED);
    }
    if (result == 0)
        result = pthread_create(&thread, &attributes,
                                rin_aio_callback_dispatcher, NULL);
    if (attributes_initialized)
        (void)pthread_attr_destroy(&attributes);
    if (result == 0) g_rin_aio_callback_dispatcher_started = 1u;
    return result == 0 ? 0 : EAGAIN;
}

static int rin_aio_callback_reserve(void)
{
    int result = 0;
    (void)pthread_mutex_lock(&g_rin_aio_callback_mutex);
    if (g_rin_aio_callback_dispatcher_started == 0u) {
        result = rin_aio_callback_start_dispatcher_locked();
        if (result != 0) {
            (void)pthread_mutex_unlock(&g_rin_aio_callback_mutex);
            return EAGAIN;
        }
    }
    if (g_rin_aio_callback_reservations >=
        RIN_AIO_CALLBACK_CAPACITY) {
        result = EAGAIN;
    } else {
        ++g_rin_aio_callback_reservations;
    }
    (void)pthread_mutex_unlock(&g_rin_aio_callback_mutex);
    return result;
}

static void rin_aio_callback_release_reservation(void)
{
    (void)pthread_mutex_lock(&g_rin_aio_callback_mutex);
    if (g_rin_aio_callback_reservations != 0u)
        --g_rin_aio_callback_reservations;
    if (g_rin_aio_callback_dispatcher_started == 0u &&
        g_rin_aio_callback_count != 0u)
        (void)rin_aio_callback_start_dispatcher_locked();
    (void)pthread_cond_broadcast(&g_rin_aio_callback_changed);
    (void)pthread_mutex_unlock(&g_rin_aio_callback_mutex);
}

static void rin_aio_callback_release_cleanup(void* unused)
{
    (void)unused;
    rin_aio_callback_release_reservation();
}

static void rin_aio_callback_dispatcher_recover(void* unused)
{
    (void)unused;
    (void)pthread_mutex_lock(&g_rin_aio_callback_mutex);
    g_rin_aio_callback_dispatcher_started = 0u;
    if (g_rin_aio_callback_count != 0u)
        (void)rin_aio_callback_start_dispatcher_locked();
    (void)pthread_cond_broadcast(&g_rin_aio_callback_changed);
    (void)pthread_mutex_unlock(&g_rin_aio_callback_mutex);
}

static void rin_aio_notification_release(
    const RinAioNotification* notification)
{
    if (notification && notification->callback_reserved)
        rin_aio_callback_release_reservation();
}

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
        } else {
            if (pthread_attr_init(&output->attributes) != 0)
                return EAGAIN;
            output->has_attributes = 1;
        }
        /* POSIX owns callback thread lifetime; never leave a joinable thread
         * whose identifier is not returned to the caller. */
        if (pthread_attr_setdetachstate(
                &output->attributes, PTHREAD_CREATE_DETACHED) != 0)
            return EAGAIN;
        if (rin_aio_callback_reserve() != 0) return EAGAIN;
        output->callback_reserved = 1;
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
    pthread_cleanup_push(rin_aio_callback_release_cleanup, NULL);
    function(value);
    pthread_cleanup_pop(1);
    return NULL;
}

static void* rin_aio_callback_dispatcher(void* unused)
{
    (void)unused;
    for (;;) {
        RinAioCallbackJob job;
        (void)pthread_mutex_lock(&g_rin_aio_callback_mutex);
        while (g_rin_aio_callback_count == 0u)
            (void)pthread_cond_wait(&g_rin_aio_callback_changed,
                                    &g_rin_aio_callback_mutex);
        job = g_rin_aio_callback_jobs[g_rin_aio_callback_head];
        memset(&g_rin_aio_callback_jobs[g_rin_aio_callback_head], 0,
               sizeof(g_rin_aio_callback_jobs[0]));
        g_rin_aio_callback_head =
            (g_rin_aio_callback_head + 1u) % RIN_AIO_CALLBACK_CAPACITY;
        --g_rin_aio_callback_count;
        (void)pthread_cond_broadcast(&g_rin_aio_callback_changed);
        (void)pthread_mutex_unlock(&g_rin_aio_callback_mutex);

        {
            RinAioCallback* callback =
                (RinAioCallback*)malloc(sizeof(*callback));
            pthread_t thread;
            int result = EAGAIN;
            if (callback) {
                callback->function = job.function;
                callback->value = job.value;
                result = pthread_create(
                    &thread, job.has_attributes ? &job.attributes : NULL,
                    rin_aio_callback_start, callback);
                if (result == 0) continue;
                free(callback);
            }
        }

        /* Resource exhaustion falls back to this dedicated dispatcher, never
         * an I/O worker. A callback that waits for AIO therefore leaves the
         * independent I/O worker pool available to make progress. */
        pthread_cleanup_push(rin_aio_callback_dispatcher_recover, NULL);
        pthread_cleanup_push(rin_aio_callback_release_cleanup, NULL);
        job.function(job.value);
        pthread_cleanup_pop(1);
        pthread_cleanup_pop(0);
    }
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
        uint32_t tail;
        if (!notification->callback_reserved) return;
        (void)pthread_mutex_lock(&g_rin_aio_callback_mutex);
        while (g_rin_aio_callback_count >= RIN_AIO_CALLBACK_CAPACITY)
            (void)pthread_cond_wait(&g_rin_aio_callback_changed,
                                    &g_rin_aio_callback_mutex);
        tail = (g_rin_aio_callback_head + g_rin_aio_callback_count) %
               RIN_AIO_CALLBACK_CAPACITY;
        g_rin_aio_callback_jobs[tail].function = notification->function;
        g_rin_aio_callback_jobs[tail].value = notification->value;
        g_rin_aio_callback_jobs[tail].has_attributes =
            notification->has_attributes;
        if (notification->has_attributes)
            g_rin_aio_callback_jobs[tail].attributes =
                notification->attributes;
        ++g_rin_aio_callback_count;
        (void)pthread_cond_signal(&g_rin_aio_callback_changed);
        (void)pthread_mutex_unlock(&g_rin_aio_callback_mutex);
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

/* Caller holds both g_rin_aio_worker_start_mutex and g_rin_aio_mutex.  The
 * state mutex is released while thread creation allocates stacks, then
 * reacquired before returning.  Requests are published only after this
 * helper succeeds, so failed first-worker creation cannot strand queued AIO. */
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
    {
        uint32_t needed = target > g_rin_aio_worker_count
                              ? target - g_rin_aio_worker_count : 0u;
        uint32_t started = 0u;
        (void)pthread_mutex_unlock(&g_rin_aio_mutex);
        while (started < needed) {
            pthread_attr_t attributes;
            pthread_t thread;
            int attributes_initialized = 0;
            int result = pthread_attr_init(&attributes);
            if (result == 0) {
                attributes_initialized = 1;
                result = pthread_attr_setstacksize(
                    &attributes, RIN_AIO_WORKER_STACK);
            }
            if (result == 0)
                result = pthread_attr_setdetachstate(
                    &attributes, PTHREAD_CREATE_DETACHED);
            if (result == 0)
                result = pthread_create(&thread, &attributes,
                                        rin_aio_worker, NULL);
            if (attributes_initialized)
                (void)pthread_attr_destroy(&attributes);
            if (result != 0) break;
            ++started;
        }
        (void)pthread_mutex_lock(&g_rin_aio_mutex);
        g_rin_aio_worker_count += started;
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
    if (held_fd < 0) {
        error = errno;
        rin_aio_notification_release(notification_out);
        return error;
    }
    flags = fcntl(held_fd, F_GETFL);
    if (flags < 0) {
        error = errno;
        (void)close(held_fd);
        rin_aio_notification_release(notification_out);
        return error;
    }
    if ((operation == RIN_AIO_OP_FSYNC &&
         (flags & O_ACCMODE) == O_RDONLY) ||
        (operation == RIN_AIO_OP_READ && (flags & O_ACCMODE) == O_WRONLY) ||
        (operation == RIN_AIO_OP_WRITE &&
         (flags & O_ACCMODE) == O_RDONLY)) {
        (void)close(held_fd);
        rin_aio_notification_release(notification_out);
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
    error = pthread_mutex_lock(&g_rin_aio_worker_start_mutex);
    if (error != 0) {
        (void)close(held_fd);
        rin_aio_notification_release(notification);
        errno = error;
        return -1;
    }
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
        request->original_fd = control->aio_fildes;
        request->held_fd = held_fd;
        if (operation == RIN_AIO_OP_FSYNC)
            request->barrier_sequence = g_rin_aio_next_sequence - 1u;
        request->sequence = g_rin_aio_next_sequence++;
        if (g_rin_aio_next_sequence == 0u) g_rin_aio_next_sequence = 1u;
        if (notification) request->notification = *notification;
        request->group = group;
        (void)pthread_cond_broadcast(&g_rin_aio_changed);
    }
    (void)pthread_mutex_unlock(&g_rin_aio_mutex);
    (void)pthread_mutex_unlock(&g_rin_aio_worker_start_mutex);
    if (error != 0) {
        (void)close(held_fd);
        rin_aio_notification_release(notification);
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

static int rin_aio_fsync_barrier_ready_locked(
    const RinAioRequest* barrier)
{
    uint32_t index;
    if (!barrier || barrier->operation != RIN_AIO_OP_FSYNC) return 1;
    for (index = 0u; index < RIN_AIO_CAPACITY; ++index) {
        const RinAioRequest* candidate = &g_rin_aio_requests[index];
        if (candidate == barrier || candidate->state == RIN_AIO_FREE ||
            candidate->state == RIN_AIO_DONE ||
            candidate->original_fd != barrier->original_fd ||
            candidate->sequence > barrier->barrier_sequence)
            continue;
        return 0;
    }
    return 1;
}

static int rin_aio_find_queued_locked(void)
{
    uint32_t index;
    int selected = -1;
    uint64_t sequence = UINT64_MAX;
    for (index = 0u; index < RIN_AIO_CAPACITY; ++index) {
        RinAioRequest* request = &g_rin_aio_requests[index];
        if (request->state == RIN_AIO_QUEUED &&
            rin_aio_fsync_barrier_ready_locked(request) &&
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
            rin_aio_suspend_notify_locked(request);
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
            rin_aio_suspend_notify_locked(current);
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

static void rin_aio_suspend_subscribe_locked(
    RinAioSuspendWaiter* waiter, RinAioSubscription* subscription,
    RinAioRequest* request)
{
    subscription->waiter = waiter;
    subscription->request = request;
    subscription->waiter_next = waiter->subscriptions;
    waiter->subscriptions = subscription;
    subscription->request_previous = NULL;
    subscription->request_next = request->suspend_waiters;
    if (request->suspend_waiters)
        request->suspend_waiters->request_previous = subscription;
    request->suspend_waiters = subscription;
}

/* Completion unlinks the stack-owned subscription before waking its waiter.
 * aio_return() may then recycle the request slot without retaining a pointer
 * into a thread that has not run yet. */
static void rin_aio_suspend_notify_locked(RinAioRequest* request)
{
    RinAioSubscription* subscription;
    if (!request) return;
    subscription = request->suspend_waiters;
    request->suspend_waiters = NULL;
    while (subscription) {
        RinAioSubscription* next = subscription->request_next;
        RinAioSuspendWaiter* waiter = subscription->waiter;
        subscription->request = NULL;
        subscription->request_previous = NULL;
        subscription->request_next = NULL;
        if (waiter) {
            waiter->completed = 1;
            (void)pthread_cond_signal(&waiter->changed);
        }
        subscription = next;
    }
}

/* Cancellation and normal return both run with g_rin_aio_mutex held. */
static void rin_aio_suspend_waiter_cleanup(void* argument)
{
    RinAioSuspendWaiter* waiter = (RinAioSuspendWaiter*)argument;
    RinAioSubscription* subscription;
    if (!waiter) return;
    subscription = waiter->subscriptions;
    while (subscription) {
        RinAioSubscription* next = subscription->waiter_next;
        RinAioRequest* request = subscription->request;
        if (request) {
            if (subscription->request_previous)
                subscription->request_previous->request_next =
                    subscription->request_next;
            else if (request->suspend_waiters == subscription)
                request->suspend_waiters = subscription->request_next;
            if (subscription->request_next)
                subscription->request_next->request_previous =
                    subscription->request_previous;
            subscription->request = NULL;
        }
        subscription->waiter = NULL;
        subscription->waiter_next = NULL;
        subscription->request_previous = NULL;
        subscription->request_next = NULL;
        subscription = next;
    }
    waiter->subscriptions = NULL;
    (void)pthread_mutex_unlock(&g_rin_aio_mutex);
    (void)pthread_cond_destroy(&waiter->changed);
}

int aio_suspend(const struct aiocb* const requests[], int count,
                const struct timespec* timeout)
{
    struct timespec deadline;
    pthread_condattr_t condition_attributes;
    RinAioSuspendWaiter waiter;
    RinAioSubscription subscriptions[AIO_LISTIO_MAX];
    int subscription_count = 0;
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

    memset(&waiter, 0, sizeof(waiter));
    wait_error = pthread_condattr_init(&condition_attributes);
    if (wait_error == 0) {
        wait_error = pthread_condattr_setclock(&condition_attributes,
                                              CLOCK_MONOTONIC);
        if (wait_error == 0)
            wait_error = pthread_cond_init(&waiter.changed,
                                           &condition_attributes);
        (void)pthread_condattr_destroy(&condition_attributes);
    }
    if (wait_error != 0) {
        errno = wait_error;
        return -1;
    }

    wait_error = pthread_mutex_lock(&g_rin_aio_mutex);
    if (wait_error != 0) {
        (void)pthread_cond_destroy(&waiter.changed);
        errno = wait_error;
        return -1;
    }
    pthread_cleanup_push(rin_aio_suspend_waiter_cleanup, &waiter);
    if (rin_aio_list_has_completion_locked(requests, count)) {
        wait_error = 0;
    } else {
        for (index = 0; index < count; ++index) {
            int earlier;
            int slot;
            if (!requests[index]) continue;
            for (earlier = 0; earlier < index; ++earlier)
                if (requests[earlier] == requests[index]) break;
            if (earlier != index) continue;
            slot = rin_aio_find_locked(requests[index]);
            if (slot < 0 ||
                g_rin_aio_requests[slot].state == RIN_AIO_DONE) {
                waiter.completed = 1;
                break;
            }
            rin_aio_suspend_subscribe_locked(
                &waiter, &subscriptions[subscription_count++],
                &g_rin_aio_requests[slot]);
        }
        while (wait_error == 0 && !waiter.completed) {
            if (timeout)
                wait_error = __rin_pthread_cond_timedwait_interruptible(
                    &waiter.changed, &g_rin_aio_mutex, &deadline);
            else
                wait_error = __rin_pthread_cond_wait_interruptible(
                    &waiter.changed, &g_rin_aio_mutex);
            if (wait_error == EINTR) {
                /* Completion and signal delivery may race as the wait
                 * reacquires the mutex; completion wins that race. */
                if (waiter.completed) {
                    wait_error = 0;
                    break;
                }
                errno = EINTR;
                break;
            }
            if (wait_error != 0 && wait_error != ETIMEDOUT) {
                errno = wait_error;
                break;
            }
            if (wait_error == ETIMEDOUT) {
                if (waiter.completed) {
                    wait_error = 0;
                    break;
                }
                errno = EAGAIN;
                wait_error = EAGAIN;
                break;
            }
            if (wait_error == 0 && waiter.completed)
                break;
        }
        if (waiter.completed &&
            (wait_error == 0 || wait_error == EINTR ||
             wait_error == ETIMEDOUT)) {
            wait_error = 0;
        }
    }
    (void)subscription_count;
    pthread_cleanup_pop(1);
    return wait_error == 0 ? 0 : -1;
}

static void rin_aio_release_prepared(struct aiocb* const requests[],
                                     const int held_fds[],
                                     const RinAioNotification notifications[],
                                     int count)
{
    int index;
    for (index = 0; index < count; ++index) {
        if (held_fds[index] >= 0) (void)close(held_fds[index]);
        rin_aio_notification_release(&notifications[index]);
    }
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
                rin_aio_notification_release(&list_notification);
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
        error = pthread_mutex_lock(&g_rin_aio_worker_start_mutex);
        if (error == 0) {
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
                    request->original_fd = controls[index]->aio_fildes;
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
            (void)pthread_mutex_unlock(&g_rin_aio_worker_start_mutex);
        }
    }

    if (error != 0) {
        rin_aio_release_prepared(controls, held_fds, request_notifications,
                                 active_count);
        if (group)
            rin_aio_notification_release(&group->notification);
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
