/* SPDX-License-Identifier: MIT */

#define RIN_TIME_INTERNAL 1
#include "mqueue.h"

#include "pthread.h"
#include "internal/pthread_cancel_point.h"
#include "stdarg.h"
#include "string.h"
#include "sys/syscall.h"
#include "time.h"
#include "unistd.h"
#include <rin/ipc.h>

#define RIN_LIBC_MQ_NOTIFY_SLOTS 64u
#define RIN_LIBC_MQ_NOTIFY_JOBS 16u
#define RIN_LIBC_MQ_WAIT_RECHECK_NS UINT64_C(50000000)

enum {
    RIN_LIBC_SDK_LIBRARY_BASE = 1u,
    RIN_LIBC_SDK_LIBRARY_IPC = 2u,
    RIN_LIBC_SDK_BASE_OBJECT_CLOSE = 1u,
    RIN_LIBC_SDK_IPC_EVENT_CREATE = 4u,
    RIN_LIBC_SDK_IPC_EVENT_SIGNAL = 5u,
    RIN_LIBC_SDK_IPC_WAIT_MANY = 6u,
    RIN_LIBC_SDK_IPC_POSIX_MQ_OPEN = 15u,
    RIN_LIBC_SDK_IPC_POSIX_MQ_UNLINK = 16u,
    RIN_LIBC_SDK_IPC_POSIX_MQ_SEND = 17u,
    RIN_LIBC_SDK_IPC_POSIX_MQ_RECEIVE = 18u,
    RIN_LIBC_SDK_IPC_POSIX_MQ_GETATTR = 19u,
    RIN_LIBC_SDK_IPC_POSIX_MQ_SETATTR = 20u,
    RIN_LIBC_SDK_IPC_POSIX_MQ_NOTIFY = 21u,
    RIN_LIBC_SDK_IPC_POSIX_MQ_WAIT_RECEIVE_BEGIN = 22u,
    RIN_LIBC_SDK_IPC_POSIX_MQ_WAIT_RECEIVE_END = 23u,
    RIN_LIBC_SDK_IPC_EVENT_QUERY = 24u,
    RIN_LIBC_SDK_IPC_POSIX_MQ_TIMEOUT_SNAPSHOT = 25u
};

typedef struct RinLibcMqNotifySlot {
    volatile uint32_t state;
    RinEvent event;
    void (*function)(union sigval);
    union sigval value;
    uint32_t has_thread_attributes;
    pthread_attr_t thread_attributes;
} RinLibcMqNotifySlot;

typedef struct RinLibcMqNotifyJob {
    volatile uint32_t state;
    uint32_t generation;
    void (*function)(union sigval);
    union sigval value;
} RinLibcMqNotifyJob;

enum {
    RIN_LIBC_MQ_NOTIFY_SLOT_FREE = 0u,
    RIN_LIBC_MQ_NOTIFY_SLOT_RESERVING = 1u,
    RIN_LIBC_MQ_NOTIFY_SLOT_REGISTERED = 2u,
    RIN_LIBC_MQ_NOTIFY_SLOT_FIRED = 3u
};

static RinLibcMqNotifySlot g_mq_notify_slots[RIN_LIBC_MQ_NOTIFY_SLOTS];
static RinLibcMqNotifyJob g_mq_notify_jobs[RIN_LIBC_MQ_NOTIFY_JOBS];
static volatile uint32_t g_mq_notify_dispatcher_state;
static volatile uint32_t g_mq_notify_process_id;
static volatile uint32_t g_mq_notify_process_sync_lock;

static int32_t mq_raw_sdk_call(uint32_t library_id, uint32_t operation,
                              const RinSdkArgsV1* request, void* response,
                              uint32_t response_size)
{
    intptr_t result;
    if (!request || (response_size != 0u && !response))
        return RIN_ERROR_INVALID_ARGUMENT;
    result = __RIN_SYSCALL_7(
        SYS_SDK_CALL, (uintptr_t)request, (uintptr_t)sizeof(*request),
        (uintptr_t)library_id, (uintptr_t)operation,
        (uintptr_t)response, (uintptr_t)response_size);
    if (result > INT32_MAX || result < INT32_MIN) return RIN_ERROR_IO;
    return (int32_t)result;
}

static void mq_sdk_request_init(RinSdkArgsV1* request)
{
    size_t index;
    uint8_t* bytes = (uint8_t*)request;
    for (index = 0u; index < sizeof(*request); ++index) bytes[index] = 0u;
    request->struct_size = sizeof(*request);
    request->version = RIN_SDK_STRUCT_VERSION_1;
}

static int mq_error_from_result(int32_t result, uint32_t operation,
                                int open_flags)
{
    if (result >= 0) return 0;
    switch (result) {
    case RIN_ERROR_INVALID_ARGUMENT: errno = EINVAL; break;
    case RIN_ERROR_NOT_SUPPORTED: errno = ENOSYS; break;
    case RIN_ERROR_NOT_FOUND: errno = ENOENT; break;
    case RIN_ERROR_ACCESS_DENIED: errno = EACCES; break;
    case RIN_ERROR_NO_MEMORY: errno = ENOMEM; break;
    case RIN_ERROR_BUSY: errno = EBUSY; break;
    case RIN_ERROR_TIMEOUT: errno = ETIMEDOUT; break;
    case RIN_ERROR_CANCELLED: errno = ECANCELED; break;
    case RIN_ERROR_IO: errno = EIO; break;
    case RIN_ERROR_WOULD_BLOCK: errno = EAGAIN; break;
    case RIN_ERROR_ABI_MISMATCH: errno = EPROTO; break;
    case RIN_ERROR_STALE_HANDLE:
    case RIN_ERROR_WRONG_TYPE: errno = EBADF; break;
    case RIN_ERROR_INTEGRITY: errno = EIO; break;
    case RIN_ERROR_BUFFER_TOO_SMALL: errno = EMSGSIZE; break;
    case RIN_ERROR_TABLE_FULL: errno = EMFILE; break;
    case RIN_ERROR_SYSTEM_TABLE_FULL: errno = ENFILE; break;
    case RIN_ERROR_OVERFLOW: errno = EOVERFLOW; break;
    case RIN_ERROR_LIMIT_EXCEEDED:
        errno = operation == RIN_LIBC_SDK_IPC_POSIX_MQ_OPEN
                    ? ENOSPC
                    : operation == RIN_SDK_IPC_POSIX_MQ_USER_SNAPSHOT
                          ? ENAMETOOLONG : EOVERFLOW;
        break;
    case RIN_ERROR_INVALID_ADDRESS: errno = EFAULT; break;
    case RIN_ERROR_EXISTS: errno = EEXIST; break;
    default: errno = EIO; break;
    }
    if (operation == RIN_LIBC_SDK_IPC_POSIX_MQ_OPEN &&
        (open_flags & O_EXCL) != 0 && result == RIN_ERROR_BUSY)
        errno = EEXIST;
    return -1;
}

static int mq_user_snapshot(const void* source, size_t size, int string_mode,
                            void* destination)
{
    RinSdkArgsV1 request;
    if (!source || !destination || size == 0u || size > UINT32_MAX) {
        errno = source && destination ? EINVAL : EFAULT;
        return -1;
    }
    mq_sdk_request_init(&request);
    request.value[0] = (uint64_t)(uintptr_t)source;
    request.value[1] = (uint64_t)size;
    request.value[2] = string_mode ? 1u : 0u;
    return mq_error_from_result(
        mq_raw_sdk_call(RIN_LIBC_SDK_LIBRARY_IPC,
                        RIN_SDK_IPC_POSIX_MQ_USER_SNAPSHOT,
                        &request, destination, (uint32_t)size),
        RIN_SDK_IPC_POSIX_MQ_USER_SNAPSHOT, 0);
}

static int mq_name_snapshot(const char* name,
                            char buffer[RIN_POSIX_MQ_NAME_MAX + 1u],
                            RinStringV1* slice)
{
    size_t length;
    if (!name || !buffer || !slice) {
        errno = EFAULT;
        return -1;
    }
    if (mq_user_snapshot(name, RIN_POSIX_MQ_NAME_MAX + 1u, 1,
                         buffer) != 0)
        return -1;
    for (length = 0u; length <= RIN_POSIX_MQ_NAME_MAX; ++length) {
        if (buffer[length] == '\0') break;
    }
    if (length < 2u || length > RIN_POSIX_MQ_NAME_MAX) {
        errno = length > RIN_POSIX_MQ_NAME_MAX ? ENAMETOOLONG : EINVAL;
        return -1;
    }
    slice->address = (uint64_t)(uintptr_t)buffer;
    slice->size = (uint64_t)length;
    return 0;
}

static void mq_attributes_to_posix(const RinPosixMqAttrV1* source,
                                   struct mq_attr* destination)
{
    destination->mq_flags =
        (source->mq_flags & RIN_POSIX_MQ_ATTR_NONBLOCK) ? O_NONBLOCK : 0;
    destination->mq_maxmsg = (long)source->mq_maxmsg;
    destination->mq_msgsize = (long)source->mq_msgsize;
    destination->mq_curmsgs = (long)source->mq_curmsgs;
    destination->__reserved[0] = 0;
    destination->__reserved[1] = 0;
    destination->__reserved[2] = 0;
    destination->__reserved[3] = 0;
}

static int mq_getattr_raw(mqd_t queue, RinPosixMqAttrV1* attributes)
{
    RinSdkArgsV1 request;
    int32_t result;
    if (!attributes) return RIN_ERROR_INVALID_ARGUMENT;
    mq_sdk_request_init(&request);
    request.value[0] = queue;
    result = mq_raw_sdk_call(RIN_LIBC_SDK_LIBRARY_IPC,
                             RIN_LIBC_SDK_IPC_POSIX_MQ_GETATTR,
                             &request, attributes, sizeof(*attributes));
    if (result == RIN_SUCCESS &&
        (attributes->struct_size != sizeof(*attributes) ||
         attributes->version != RIN_POSIX_MQ_ABI_VERSION ||
         attributes->reserved[0] != 0u || attributes->reserved[1] != 0u))
        return RIN_ERROR_ABI_MISMATCH;
    return result;
}

static int mq_setattr_raw(mqd_t queue, const RinPosixMqAttrV1* attributes,
                          struct mq_attr* old_attributes)
{
    RinSdkArgsV1 request;
    if (!attributes) return RIN_ERROR_INVALID_ARGUMENT;
    mq_sdk_request_init(&request);
    request.value[0] = queue;
    request.value[1] = (uint64_t)(uintptr_t)attributes;
    if (old_attributes) {
        request.value[2] = (uint64_t)(uintptr_t)old_attributes;
        request.value[3] = sizeof(long);
        request.value[4] = O_NONBLOCK;
    }
    return mq_raw_sdk_call(RIN_LIBC_SDK_LIBRARY_IPC,
                           RIN_LIBC_SDK_IPC_POSIX_MQ_SETATTR,
                           &request, NULL, 0u);
}

static int32_t mq_send_once(mqd_t queue, const char* message, size_t length,
                            unsigned int priority)
{
    RinSdkArgsV1 request;
    mq_sdk_request_init(&request);
    request.value[0] = queue;
    request.value[1] = (uint64_t)(uintptr_t)message;
    request.value[2] = (uint64_t)length;
    request.value[3] = priority;
    return mq_raw_sdk_call(RIN_LIBC_SDK_LIBRARY_IPC,
                           RIN_LIBC_SDK_IPC_POSIX_MQ_SEND,
                           &request, NULL, 0u);
}

static int32_t mq_receive_once(mqd_t queue, char* message, size_t capacity,
                               uint64_t waiter_token,
                               unsigned int* priority_out,
                               RinPosixMqReceiveResultV1* result_out)
{
    RinSdkArgsV1 request;
    int32_t result;
    mq_sdk_request_init(&request);
    request.value[0] = queue;
    request.value[1] = (uint64_t)(uintptr_t)message;
    request.value[2] = (uint64_t)capacity;
    request.value[3] = waiter_token;
    request.value[RIN_POSIX_MQ_RECEIVE_PRIORITY_OUTPUT_SLOT] =
        (uint64_t)(uintptr_t)priority_out;
    result = mq_raw_sdk_call(RIN_LIBC_SDK_LIBRARY_IPC,
                             RIN_LIBC_SDK_IPC_POSIX_MQ_RECEIVE,
                             &request, result_out, sizeof(*result_out));
    if (result == RIN_SUCCESS &&
        (result_out->struct_size != sizeof(*result_out) ||
         result_out->version != RIN_POSIX_MQ_ABI_VERSION ||
         result_out->reserved0 != 0u || result_out->reserved[0] != 0u ||
         result_out->reserved[1] != 0u || result_out->length > capacity))
        return RIN_ERROR_ABI_MISMATCH;
    return result;
}

static int mq_realtime_ns(const struct timespec* absolute, uint64_t* ns_out)
{
    struct timespec deadline_copy;
    if (!absolute || !ns_out) return -1;
    if (mq_user_snapshot(absolute, sizeof(deadline_copy), 0,
                         &deadline_copy) != 0)
        return -1;
    if (deadline_copy.tv_nsec < 0 || deadline_copy.tv_nsec >= 1000000000L) {
        errno = EINVAL;
        return -1;
    }
    if (deadline_copy.tv_sec < 0) {
        *ns_out = 0u;
        return 0;
    }
    if ((uint64_t)deadline_copy.tv_sec >
        (UINT64_MAX - (uint64_t)deadline_copy.tv_nsec) /
            UINT64_C(1000000000)) {
        errno = EOVERFLOW;
        return -1;
    }
    *ns_out = (uint64_t)deadline_copy.tv_sec * UINT64_C(1000000000) +
              (uint64_t)deadline_copy.tv_nsec;
    return 0;
}

static int mq_current_realtime_ns(uint64_t* ns_out)
{
    struct timespec now;
    if (!ns_out || clock_gettime(CLOCK_REALTIME, &now) != 0) return -1;
    if (now.tv_sec < 0 || now.tv_nsec < 0 || now.tv_nsec >= 1000000000L ||
        (uint64_t)now.tv_sec >
            (UINT64_MAX - (uint64_t)now.tv_nsec) / UINT64_C(1000000000)) {
        errno = EOVERFLOW;
        return -1;
    }
    *ns_out = (uint64_t)now.tv_sec * UINT64_C(1000000000) +
              (uint64_t)now.tv_nsec;
    return 0;
}

static int mq_wait_ready(mqd_t queue, uint32_t events,
                         const struct timespec* absolute)
{
    for (;;) {
        RinSdkArgsV1 request;
        RinWaitItemV1 item;
        uint32_t index = UINT32_MAX;
        uint64_t timeout_ns = RIN_LIBC_MQ_WAIT_RECHECK_NS;
        int32_t result;
        if (absolute) {
            uint64_t deadline_ns;
            uint64_t now_ns;
            if (mq_realtime_ns(absolute, &deadline_ns) != 0 ||
                mq_current_realtime_ns(&now_ns) != 0)
                return -1;
            if (now_ns >= deadline_ns) {
                errno = ETIMEDOUT;
                return -1;
            }
            timeout_ns = deadline_ns - now_ns;
            if (timeout_ns > RIN_LIBC_MQ_WAIT_RECHECK_NS)
                timeout_ns = RIN_LIBC_MQ_WAIT_RECHECK_NS;
        }
        item.handle = queue;
        item.events = events;
        item.observed = 0u;
        item.user_tag = 0u;
        mq_sdk_request_init(&request);
        request.value[0] = (uint64_t)(uintptr_t)&item;
        request.value[1] = sizeof(item);
        request.value[2] = timeout_ns;
        result = mq_raw_sdk_call(RIN_LIBC_SDK_LIBRARY_IPC,
                                 RIN_LIBC_SDK_IPC_WAIT_MANY, &request,
                                 &index, sizeof(index));
        if (result == RIN_SUCCESS) {
            if (index != 0u) {
                errno = EIO;
                return -1;
            }
            return 0;
        }
        if (result != RIN_ERROR_TIMEOUT) {
            if (result == RIN_ERROR_CANCELLED) {
                errno = EINTR;
                return -1;
            }
            (void)mq_error_from_result(result,
                                       RIN_LIBC_SDK_IPC_WAIT_MANY, 0);
            return -1;
        }
        /* Return after each bounded wait so deferred cancellation is checked
         * and absolute CLOCK_REALTIME deadlines are recomputed by the caller. */
        return 1;
    }
}

static int32_t mq_wait_receive_begin(mqd_t queue, uint64_t* token_out)
{
    RinSdkArgsV1 request;
    if (!token_out) return RIN_ERROR_INVALID_ARGUMENT;
    *token_out = RIN_POSIX_MQ_WAIT_TOKEN_INVALID;
    mq_sdk_request_init(&request);
    request.value[0] = queue;
    return mq_raw_sdk_call(RIN_LIBC_SDK_LIBRARY_IPC,
                           RIN_LIBC_SDK_IPC_POSIX_MQ_WAIT_RECEIVE_BEGIN,
                           &request, token_out, sizeof(*token_out));
}

static int32_t mq_wait_receive_end(uint64_t token)
{
    RinSdkArgsV1 request;
    if (token == RIN_POSIX_MQ_WAIT_TOKEN_INVALID) return RIN_SUCCESS;
    mq_sdk_request_init(&request);
    request.value[0] = token;
    return mq_raw_sdk_call(RIN_LIBC_SDK_LIBRARY_IPC,
                           RIN_LIBC_SDK_IPC_POSIX_MQ_WAIT_RECEIVE_END,
                           &request, NULL, 0u);
}

static int mq_timeout_snapshot(const struct timespec* absolute,
                               struct timespec* snapshot)
{
    RinSdkArgsV1 request;
    int32_t result;
    if (!absolute || !snapshot) {
        errno = EFAULT;
        return -1;
    }
    mq_sdk_request_init(&request);
    request.value[0] = (uint64_t)(uintptr_t)absolute;
    request.value[1] = sizeof(*snapshot);
    result = mq_raw_sdk_call(RIN_LIBC_SDK_LIBRARY_IPC,
                             RIN_LIBC_SDK_IPC_POSIX_MQ_TIMEOUT_SNAPSHOT,
                             &request, snapshot, sizeof(*snapshot));
    if (result != RIN_SUCCESS)
        return mq_error_from_result(
            result, RIN_LIBC_SDK_IPC_POSIX_MQ_TIMEOUT_SNAPSHOT, 0);
    return 0;
}

static int mq_timed_transfer_validate(const struct timespec* absolute)
{
    uint64_t ignored;
    if (!absolute) {
        errno = EFAULT;
        return -1;
    }
    return mq_realtime_ns(absolute, &ignored);
}

static int mq_send_common(mqd_t queue, const char* message, size_t length,
                          unsigned int priority, int timed,
                          const struct timespec* absolute)
{
    struct timespec timeout_snapshot;
    const struct timespec* wait_timeout = NULL;
    int timeout_validated = 0;
    for (;;) {
        RinPosixMqAttrV1 attributes;
        int wait_result;
        __rin_pthread_cancel_point_if_linked();
        int32_t result = mq_send_once(queue, message, length, priority);
        if (result == RIN_SUCCESS) return 0;
        if (result != RIN_ERROR_WOULD_BLOCK)
            return mq_error_from_result(result,
                                        RIN_LIBC_SDK_IPC_POSIX_MQ_SEND, 0);
        result = mq_getattr_raw(queue, &attributes);
        if (result != RIN_SUCCESS)
            return mq_error_from_result(result,
                                        RIN_LIBC_SDK_IPC_POSIX_MQ_GETATTR, 0);
        if ((attributes.mq_flags & RIN_POSIX_MQ_ATTR_NONBLOCK) != 0u) {
            errno = EAGAIN;
            return -1;
        }
        if (timed && !timeout_validated) {
            if (!absolute) {
                errno = EFAULT;
                return -1;
            }
            if (mq_timeout_snapshot(absolute, &timeout_snapshot) != 0)
                return -1;
            if (mq_timed_transfer_validate(&timeout_snapshot) != 0)
                return -1;
            wait_timeout = &timeout_snapshot;
            timeout_validated = 1;
        }
        wait_result = mq_wait_ready(queue, RIN_WAIT_EVENT_WRITABLE,
                                    wait_timeout);
        __rin_pthread_cancel_point_if_linked();
        if (wait_result < 0) return -1;
    }
}

static ssize_t mq_receive_common(mqd_t queue, char* message, size_t capacity,
                                 unsigned int* priority_out,
                                 int timed,
                                 const struct timespec* absolute)
{
    uint64_t waiter_token = RIN_POSIX_MQ_WAIT_TOKEN_INVALID;
    struct timespec timeout_snapshot;
    const struct timespec* wait_timeout = NULL;
    int timeout_validated = 0;
    for (;;) {
        RinPosixMqAttrV1 attributes;
        RinPosixMqReceiveResultV1 received;
        int wait_result;
        __rin_pthread_cancel_point_if_linked();
        int32_t result = mq_receive_once(queue, message, capacity,
                                         waiter_token, priority_out,
                                         &received);
        if (result == RIN_SUCCESS) {
            /* The kernel consumes a registered receiver token atomically
             * with the message dequeue. */
            waiter_token = RIN_POSIX_MQ_WAIT_TOKEN_INVALID;
            return (ssize_t)received.length;
        }
        if (result != RIN_ERROR_WOULD_BLOCK) {
            (void)mq_wait_receive_end(waiter_token);
            return (ssize_t)mq_error_from_result(
                result, RIN_LIBC_SDK_IPC_POSIX_MQ_RECEIVE, 0);
        }
        result = mq_getattr_raw(queue, &attributes);
        if (result != RIN_SUCCESS) {
            (void)mq_wait_receive_end(waiter_token);
            (void)mq_error_from_result(
                result, RIN_LIBC_SDK_IPC_POSIX_MQ_GETATTR, 0);
            return -1;
        }
        if ((attributes.mq_flags & RIN_POSIX_MQ_ATTR_NONBLOCK) != 0u) {
            (void)mq_wait_receive_end(waiter_token);
            errno = EAGAIN;
            return -1;
        }
        if (timed && !timeout_validated) {
            if (!absolute) {
                (void)mq_wait_receive_end(waiter_token);
                errno = EFAULT;
                return -1;
            }
            if (mq_timeout_snapshot(absolute, &timeout_snapshot) != 0) {
                (void)mq_wait_receive_end(waiter_token);
                return -1;
            }
            if (mq_timed_transfer_validate(&timeout_snapshot) != 0) {
                (void)mq_wait_receive_end(waiter_token);
                return -1;
            }
            wait_timeout = &timeout_snapshot;
            timeout_validated = 1;
        }
        if (waiter_token == RIN_POSIX_MQ_WAIT_TOKEN_INVALID) {
            result = mq_wait_receive_begin(queue, &waiter_token);
            if (result != RIN_SUCCESS)
                return (ssize_t)mq_error_from_result(
                    result, RIN_LIBC_SDK_IPC_POSIX_MQ_WAIT_RECEIVE_BEGIN, 0);
            if (waiter_token == RIN_POSIX_MQ_WAIT_TOKEN_INVALID)
                continue;
        }
        wait_result = mq_wait_ready(queue, RIN_WAIT_EVENT_READABLE,
                                    wait_timeout);
        (void)mq_wait_receive_end(waiter_token);
        waiter_token = RIN_POSIX_MQ_WAIT_TOKEN_INVALID;
        __rin_pthread_cancel_point_if_linked();
        if (wait_result < 0) return -1;
    }
}

static int mq_close_handle(RinHandle handle)
{
    RinSdkArgsV1 request;
    mq_sdk_request_init(&request);
    request.value[0] = handle;
    return mq_raw_sdk_call(RIN_LIBC_SDK_LIBRARY_BASE,
                           RIN_LIBC_SDK_BASE_OBJECT_CLOSE,
                           &request, NULL, 0u);
}

static RinLibcMqNotifySlot* mq_notify_reserve(void)
{
    uint32_t index;
    for (index = 0u; index < RIN_LIBC_MQ_NOTIFY_SLOTS; ++index) {
        uint32_t expected = RIN_LIBC_MQ_NOTIFY_SLOT_FREE;
        if (__atomic_compare_exchange_n(&g_mq_notify_slots[index].state,
                                        &expected,
                                        RIN_LIBC_MQ_NOTIFY_SLOT_RESERVING, 0,
                                        __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
            return &g_mq_notify_slots[index];
    }
    return NULL;
}

static int32_t mq_notify_call(mqd_t queue, RinEvent event, uint32_t action,
                              uint64_t argument3)
{
    RinSdkArgsV1 request;
    mq_sdk_request_init(&request);
    request.value[0] = queue;
    request.value[1] = event;
    request.value[2] = action;
    request.value[3] = argument3;
    return mq_raw_sdk_call(RIN_LIBC_SDK_LIBRARY_IPC,
                           RIN_LIBC_SDK_IPC_POSIX_MQ_NOTIFY,
                           &request, NULL, 0u);
}

static int32_t mq_event_create(RinEvent* event_out)
{
    RinSdkArgsV1 request;
    if (!event_out) return RIN_ERROR_INVALID_ARGUMENT;
    mq_sdk_request_init(&request);
    request.value[0] = RIN_EVENT_CREATE_FLAG_CLOSE_ON_EXEC;
    *event_out = RIN_OBJECT_HANDLE_INVALID;
    return mq_raw_sdk_call(RIN_LIBC_SDK_LIBRARY_IPC,
                           RIN_LIBC_SDK_IPC_EVENT_CREATE,
                           &request, event_out, sizeof(*event_out));
}

static int32_t mq_event_query(RinEvent event, uint64_t* value_out)
{
    RinSdkArgsV1 request;
    if (!value_out) return RIN_ERROR_INVALID_ARGUMENT;
    *value_out = 0u;
    mq_sdk_request_init(&request);
    request.value[0] = event;
    return mq_raw_sdk_call(RIN_LIBC_SDK_LIBRARY_IPC,
                           RIN_LIBC_SDK_IPC_EVENT_QUERY, &request,
                           value_out, sizeof(*value_out));
}

static void mq_notify_slot_release(RinLibcMqNotifySlot* slot)
{
    RinEvent event;
    if (!slot) return;
    event = slot->event;
    slot->event = RIN_OBJECT_HANDLE_INVALID;
    slot->function = NULL;
    slot->value.sival_ptr = NULL;
    slot->has_thread_attributes = 0u;
    memset(&slot->thread_attributes, 0, sizeof(slot->thread_attributes));
    if (event != RIN_OBJECT_HANDLE_INVALID) (void)mq_close_handle(event);
    __atomic_store_n(&slot->state, RIN_LIBC_MQ_NOTIFY_SLOT_FREE,
                     __ATOMIC_RELEASE);
}

typedef struct RinLibcMqNotifyJobCleanup {
    RinLibcMqNotifyJob* job;
    uint32_t generation;
} RinLibcMqNotifyJobCleanup;

static void mq_notify_job_release(RinLibcMqNotifyJob* job,
                                  uint32_t generation)
{
    if (!job || __atomic_load_n(&job->generation, __ATOMIC_ACQUIRE) !=
                    generation)
        return;
    job->function = NULL;
    job->value.sival_ptr = NULL;
    __atomic_store_n(&job->state, 0u, __ATOMIC_RELEASE);
}

static void mq_notify_job_cleanup(void* argument)
{
    RinLibcMqNotifyJobCleanup* cleanup =
        (RinLibcMqNotifyJobCleanup*)argument;
    if (cleanup)
        mq_notify_job_release(cleanup->job, cleanup->generation);
}

static void* mq_notify_callback_worker(void* argument)
{
    RinLibcMqNotifyJob* job = (RinLibcMqNotifyJob*)argument;
    RinLibcMqNotifyJobCleanup cleanup;
    void (*callback)(union sigval);
    union sigval callback_value;
    if (!job) return NULL;
    cleanup.job = job;
    cleanup.generation = __atomic_load_n(&job->generation, __ATOMIC_ACQUIRE);
    callback = job->function;
    callback_value = job->value;
    pthread_cleanup_push(mq_notify_job_cleanup, &cleanup);
    if (callback) callback(callback_value);
    pthread_cleanup_pop(1);
    return NULL;
}

static RinLibcMqNotifyJob* mq_notify_job_reserve(void)
{
    uint32_t index;
    for (index = 0u; index < RIN_LIBC_MQ_NOTIFY_JOBS; ++index) {
        uint32_t expected = 0u;
        RinLibcMqNotifyJob* job = &g_mq_notify_jobs[index];
        if (__atomic_compare_exchange_n(&job->state, &expected, 1u, 0,
                                        __ATOMIC_ACQ_REL,
                                        __ATOMIC_ACQUIRE)) {
            uint32_t generation =
                __atomic_load_n(&job->generation, __ATOMIC_RELAXED) + 1u;
            if (generation == 0u) generation = 1u;
            __atomic_store_n(&job->generation, generation, __ATOMIC_RELEASE);
            return job;
        }
    }
    return NULL;
}

static int mq_notify_launch_callback(RinLibcMqNotifySlot* slot)
{
    RinLibcMqNotifyJob* job;
    pthread_attr_t attributes;
    pthread_t thread;
    uint64_t event_value = 0u;
    uint32_t generation;
    int attributes_initialized = 0;
    int thread_result;
    int32_t result;
    if (!slot) return -1;
    result = mq_event_query(slot->event, &event_value);
    if (result != RIN_SUCCESS) return -1;
    if (event_value == 0u) {
        mq_notify_slot_release(slot);
        return 0;
    }
    job = mq_notify_job_reserve();
    if (!job) return 1;
    generation = __atomic_load_n(&job->generation, __ATOMIC_ACQUIRE);
    job->function = slot->function;
    job->value = slot->value;

    if (slot->has_thread_attributes != 0u) {
        attributes = slot->thread_attributes;
        thread_result = 0;
    } else {
        thread_result = pthread_attr_init(&attributes);
        attributes_initialized = thread_result == 0;
    }
    if (thread_result == 0)
        thread_result = pthread_attr_setdetachstate(
            &attributes, PTHREAD_CREATE_DETACHED);
    if (thread_result != 0) {
        if (attributes_initialized)
            (void)pthread_attr_destroy(&attributes);
        mq_notify_job_release(job, generation);
        return 1;
    }
    thread_result = pthread_create(&thread, &attributes,
                                   mq_notify_callback_worker, job);
    if (attributes_initialized)
        (void)pthread_attr_destroy(&attributes);
    if (thread_result != 0) {
        mq_notify_job_release(job, generation);
        return 1;
    }
    mq_notify_slot_release(slot);
    return 0;
}

static int mq_notify_wait_many(RinWaitItemV1* items, uint32_t count,
                               uint32_t* index_out)
{
    RinSdkArgsV1 request;
    int32_t result;
    if (!items || count == 0u || count > RIN_LIBC_MQ_NOTIFY_SLOTS ||
        !index_out)
        return RIN_ERROR_INVALID_ARGUMENT;
    *index_out = UINT32_MAX;
    mq_sdk_request_init(&request);
    request.value[0] = (uint64_t)(uintptr_t)items;
    request.value[1] = (uint64_t)count * sizeof(*items);
    request.value[2] = RIN_LIBC_MQ_WAIT_RECHECK_NS;
    result = mq_raw_sdk_call(RIN_LIBC_SDK_LIBRARY_IPC,
                             RIN_LIBC_SDK_IPC_WAIT_MANY, &request,
                             index_out, sizeof(*index_out));
    if (result == RIN_SUCCESS)
        return *index_out < count ? 0 : RIN_ERROR_IO;
    if (result == RIN_ERROR_TIMEOUT || result == RIN_ERROR_CANCELLED)
        return 1;
    return result;
}

static void mq_notify_pause(void)
{
    struct timespec delay;
    delay.tv_sec = 0;
    delay.tv_nsec = (long)RIN_LIBC_MQ_WAIT_RECHECK_NS;
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) { }
}

static void* mq_notify_dispatcher(void* argument)
{
    (void)argument;
    for (;;) {
        RinWaitItemV1 items[RIN_LIBC_MQ_NOTIFY_SLOTS];
        uint32_t count = 0u;
        uint32_t index;
        int wait_result;
        for (index = 0u; index < RIN_LIBC_MQ_NOTIFY_SLOTS; ++index) {
            RinLibcMqNotifySlot* slot = &g_mq_notify_slots[index];
            if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) ==
                RIN_LIBC_MQ_NOTIFY_SLOT_FIRED)
                (void)mq_notify_launch_callback(slot);
        }
        for (index = 0u; index < RIN_LIBC_MQ_NOTIFY_SLOTS; ++index) {
            RinLibcMqNotifySlot* slot = &g_mq_notify_slots[index];
            if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) !=
                RIN_LIBC_MQ_NOTIFY_SLOT_REGISTERED)
                continue;
            items[count].handle = slot->event;
            items[count].events = RIN_WAIT_EVENT_READABLE;
            items[count].observed = 0u;
            items[count].user_tag = index;
            ++count;
        }
        if (count == 0u) {
            mq_notify_pause();
            continue;
        }
        wait_result = mq_notify_wait_many(items, count, &index);
        if (wait_result == 1) continue;
        if (wait_result != 0) {
            mq_notify_pause();
            continue;
        }
        if (items[index].user_tag >= RIN_LIBC_MQ_NOTIFY_SLOTS) {
            mq_notify_pause();
            continue;
        }
        {
            RinLibcMqNotifySlot* slot =
                &g_mq_notify_slots[(uint32_t)items[index].user_tag];
            uint64_t event_value = 0u;
            if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) !=
                RIN_LIBC_MQ_NOTIFY_SLOT_REGISTERED)
                continue;
            if (mq_event_query(slot->event, &event_value) != RIN_SUCCESS) {
                mq_notify_pause();
                continue;
            }
            if (event_value == 0u) {
                mq_notify_slot_release(slot);
            } else {
                uint32_t expected = RIN_LIBC_MQ_NOTIFY_SLOT_REGISTERED;
                (void)__atomic_compare_exchange_n(
                    &slot->state, &expected, RIN_LIBC_MQ_NOTIFY_SLOT_FIRED,
                    0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
            }
        }
    }
}

static int mq_notify_ensure_dispatcher(void)
{
    for (;;) {
        uint32_t state =
            __atomic_load_n(&g_mq_notify_dispatcher_state, __ATOMIC_ACQUIRE);
        uint32_t expected = 0u;
        if (state == 2u) return 0;
        if (state == 1u) {
            struct timespec delay = {0, 1000000L};
            (void)nanosleep(&delay, NULL);
            continue;
        }
        if (!__atomic_compare_exchange_n(&g_mq_notify_dispatcher_state,
                                         &expected, 1u, 0,
                                         __ATOMIC_ACQ_REL,
                                         __ATOMIC_ACQUIRE))
            continue;
        {
            pthread_attr_t attributes;
            pthread_t thread;
            int result = pthread_attr_init(&attributes);
            int attributes_initialized = result == 0;
            if (result == 0)
                result = pthread_attr_setdetachstate(
                    &attributes, PTHREAD_CREATE_DETACHED);
            if (result == 0)
                result = pthread_create(&thread, &attributes,
                                        mq_notify_dispatcher, NULL);
            if (attributes_initialized)
                (void)pthread_attr_destroy(&attributes);
            if (result != 0) {
                __atomic_store_n(&g_mq_notify_dispatcher_state, 0u,
                                 __ATOMIC_RELEASE);
                errno = result;
                return -1;
            }
            __atomic_store_n(&g_mq_notify_dispatcher_state, 2u,
                             __ATOMIC_RELEASE);
            return 0;
        }
    }
}

static void* mq_notify_attribute_probe(void* argument)
{
    (void)argument;
    return NULL;
}

static int mq_notify_validate_attributes(const pthread_attr_t* attributes)
{
    pthread_attr_t probe_attributes;
    pthread_t probe_thread;
    int result;
    if (!attributes) return 0;
    if (attributes->detachstate != PTHREAD_CREATE_JOINABLE &&
        attributes->detachstate != PTHREAD_CREATE_DETACHED)
        return EINVAL;
    probe_attributes = *attributes;
    result = pthread_attr_setdetachstate(&probe_attributes,
                                        PTHREAD_CREATE_JOINABLE);
    if (result != 0) return result;
    result = pthread_create(&probe_thread, &probe_attributes,
                            mq_notify_attribute_probe, NULL);
    if (result != 0) return result;
    return pthread_join(probe_thread, NULL);
}

static void mq_notify_process_reset(void)
{
    uint32_t index;
    for (index = 0u; index < RIN_LIBC_MQ_NOTIFY_SLOTS; ++index)
        mq_notify_slot_release(&g_mq_notify_slots[index]);
    for (index = 0u; index < RIN_LIBC_MQ_NOTIFY_JOBS; ++index) {
        RinLibcMqNotifyJob* job = &g_mq_notify_jobs[index];
        uint32_t generation =
            __atomic_load_n(&job->generation, __ATOMIC_RELAXED) + 1u;
        if (generation == 0u) generation = 1u;
        job->function = NULL;
        job->value.sival_ptr = NULL;
        __atomic_store_n(&job->generation, generation, __ATOMIC_RELEASE);
        __atomic_store_n(&job->state, 0u, __ATOMIC_RELEASE);
    }
    __atomic_store_n(&g_mq_notify_dispatcher_state, 0u, __ATOMIC_RELEASE);
}

static int mq_notify_process_sync(void)
{
    pid_t process_id = getpid();
    uint32_t process_id_word;
    if (process_id <= 0) return -1;
    process_id_word = (uint32_t)process_id;
    if (__atomic_load_n(&g_mq_notify_process_id, __ATOMIC_ACQUIRE) ==
        process_id_word)
        return 0;
    for (;;) {
        uint32_t lock =
            __atomic_load_n(&g_mq_notify_process_sync_lock, __ATOMIC_ACQUIRE);
        uint32_t expected;
        if (lock != 0u && lock != process_id_word) {
            expected = lock;
            (void)__atomic_compare_exchange_n(
                &g_mq_notify_process_sync_lock, &expected, 0u, 0,
                __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
            continue;
        }
        if (lock == process_id_word) continue;
        expected = 0u;
        if (!__atomic_compare_exchange_n(&g_mq_notify_process_sync_lock,
                                         &expected, process_id_word, 0,
                                         __ATOMIC_ACQ_REL,
                                         __ATOMIC_ACQUIRE))
            continue;
        if (__atomic_load_n(&g_mq_notify_process_id, __ATOMIC_ACQUIRE) !=
            process_id_word) {
            mq_notify_process_reset();
            __atomic_store_n(&g_mq_notify_process_id, process_id_word,
                             __ATOMIC_RELEASE);
        }
        __atomic_store_n(&g_mq_notify_process_sync_lock, 0u,
                         __ATOMIC_RELEASE);
        return 0;
    }
}

mqd_t mq_open(const char* name, int oflag, ...)
{
    RinStringV1 name_string;
    char name_copy[RIN_POSIX_MQ_NAME_MAX + 1u];
    RinPosixMqAttrV1 wire_attributes;
    RinObjectHandle queue = RIN_OBJECT_HANDLE_INVALID;
    RinSdkArgsV1 request;
    const struct mq_attr* attributes = NULL;
    mode_t mode = 0u;
    uint32_t flags;
    int allowed_flags = O_ACCMODE | O_CREAT | O_EXCL | O_NONBLOCK | O_CLOEXEC;
    int32_t result;
    if (mq_name_snapshot(name, name_copy, &name_string) != 0)
        return (mqd_t)-1;
    if ((oflag & ~allowed_flags) != 0) {
        errno = EINVAL;
        return (mqd_t)-1;
    }
    if ((oflag & O_ACCMODE) != O_RDONLY &&
        (oflag & O_ACCMODE) != O_WRONLY &&
        (oflag & O_ACCMODE) != O_RDWR) {
        errno = EINVAL;
        return (mqd_t)-1;
    }
    if ((oflag & O_CREAT) != 0) {
        va_list arguments;
        va_start(arguments, oflag);
        mode = va_arg(arguments, mode_t);
        attributes = va_arg(arguments, const struct mq_attr*);
        va_end(arguments);
    }
    flags = (oflag & O_ACCMODE) == O_RDONLY
                ? RIN_POSIX_MQ_OPEN_READ
                : (oflag & O_ACCMODE) == O_WRONLY
                      ? RIN_POSIX_MQ_OPEN_WRITE
                      : RIN_POSIX_MQ_OPEN_READ_WRITE;
    if ((oflag & O_CREAT) != 0) flags |= RIN_POSIX_MQ_OPEN_CREATE;
    if ((oflag & O_EXCL) != 0) flags |= RIN_POSIX_MQ_OPEN_EXCLUSIVE;
    if ((oflag & O_NONBLOCK) != 0) flags |= RIN_POSIX_MQ_OPEN_NONBLOCK;
    if ((oflag & O_CLOEXEC) != 0) flags |= RIN_POSIX_MQ_OPEN_CLOEXEC;

    mq_sdk_request_init(&request);
    request.value[0] = name_string.address;
    request.value[1] = name_string.size;
    request.value[2] = flags;
    request.value[3] = mode;
    if (attributes && (oflag & O_CREAT) != 0) {
        struct mq_attr attr_copy;
        if (mq_user_snapshot(attributes, sizeof(attr_copy), 0,
                             &attr_copy) != 0)
            return (mqd_t)-1;
        wire_attributes.struct_size = sizeof(wire_attributes);
        wire_attributes.version = RIN_POSIX_MQ_ABI_VERSION;
        wire_attributes.mq_flags = 0u;
        wire_attributes.mq_maxmsg = (uint64_t)attr_copy.mq_maxmsg;
        wire_attributes.mq_msgsize = (uint64_t)attr_copy.mq_msgsize;
        wire_attributes.mq_curmsgs = 0u;
        wire_attributes.reserved[0] = 0u;
        wire_attributes.reserved[1] = 0u;
        request.value[4] = (uint64_t)(uintptr_t)&wire_attributes;
    }
    result = mq_raw_sdk_call(RIN_LIBC_SDK_LIBRARY_IPC,
                             RIN_LIBC_SDK_IPC_POSIX_MQ_OPEN,
                             &request, &queue, sizeof(queue));
    if (result != RIN_SUCCESS) {
        (void)mq_error_from_result(result,
                                   RIN_LIBC_SDK_IPC_POSIX_MQ_OPEN, oflag);
        return (mqd_t)-1;
    }
    if (queue == RIN_OBJECT_HANDLE_INVALID) {
        errno = EIO;
        return (mqd_t)-1;
    }
    return (mqd_t)queue;
}

int mq_close(mqd_t mqdes)
{
    return mq_error_from_result(
        mq_close_handle((RinHandle)mqdes),
        RIN_LIBC_SDK_BASE_OBJECT_CLOSE, 0);
}

int mq_unlink(const char* name)
{
    RinStringV1 name_string;
    char name_copy[RIN_POSIX_MQ_NAME_MAX + 1u];
    RinSdkArgsV1 request;
    int32_t result;
    if (mq_name_snapshot(name, name_copy, &name_string) != 0) return -1;
    mq_sdk_request_init(&request);
    request.value[0] = name_string.address;
    request.value[1] = name_string.size;
    result = mq_raw_sdk_call(RIN_LIBC_SDK_LIBRARY_IPC,
                             RIN_LIBC_SDK_IPC_POSIX_MQ_UNLINK,
                             &request, NULL, 0u);
    return mq_error_from_result(result,
                                RIN_LIBC_SDK_IPC_POSIX_MQ_UNLINK, 0);
}

int mq_getattr(mqd_t mqdes, struct mq_attr* attr)
{
    RinPosixMqAttrV1 wire_attributes;
    struct mq_attr attr_copy;
    int32_t result;
    if (!attr) {
        errno = EFAULT;
        return -1;
    }
    result = mq_getattr_raw(mqdes, &wire_attributes);
    if (mq_error_from_result(result,
                             RIN_LIBC_SDK_IPC_POSIX_MQ_GETATTR, 0) != 0)
        return -1;
    mq_attributes_to_posix(&wire_attributes, &attr_copy);
    return mq_user_snapshot(&attr_copy, sizeof(attr_copy), 0, attr);
}

int mq_setattr(mqd_t mqdes, const struct mq_attr* newattr,
               struct mq_attr* oldattr)
{
    RinPosixMqAttrV1 requested;
    struct mq_attr attr_copy;
    int32_t result;
    if (!newattr) {
        errno = EFAULT;
        return -1;
    }
    if (mq_user_snapshot(newattr, sizeof(attr_copy), 0, &attr_copy) != 0)
        return -1;
    if ((attr_copy.mq_flags & ~O_NONBLOCK) != 0) {
        errno = EINVAL;
        return -1;
    }
    requested.struct_size = sizeof(requested);
    requested.version = RIN_POSIX_MQ_ABI_VERSION;
    requested.mq_flags = (attr_copy.mq_flags & O_NONBLOCK)
                             ? RIN_POSIX_MQ_ATTR_NONBLOCK : 0u;
    requested.mq_maxmsg = 0u;
    requested.mq_msgsize = 0u;
    requested.mq_curmsgs = 0u;
    requested.reserved[0] = 0u;
    requested.reserved[1] = 0u;
    result = mq_setattr_raw(mqdes, &requested, oldattr);
    if (mq_error_from_result(result,
                             RIN_LIBC_SDK_IPC_POSIX_MQ_SETATTR, 0) != 0)
        return -1;
    return 0;
}

int mq_send(mqd_t mqdes, const char* msg_ptr, size_t msg_len,
            unsigned int msg_prio)
{
    if (msg_len != 0u && !msg_ptr) {
        errno = EFAULT;
        return -1;
    }
    return mq_send_common(mqdes, msg_ptr, msg_len, msg_prio, 0, NULL);
}

int mq_timedsend(mqd_t mqdes, const char* msg_ptr, size_t msg_len,
                 unsigned int msg_prio,
                 const struct timespec* abs_timeout)
{
    if (msg_len != 0u && !msg_ptr) {
        errno = EFAULT;
        return -1;
    }
    return mq_send_common(mqdes, msg_ptr, msg_len, msg_prio, 1, abs_timeout);
}

ssize_t mq_receive(mqd_t mqdes, char* msg_ptr, size_t msg_len,
                   unsigned int* msg_prio)
{
    if (!msg_ptr) {
        errno = EFAULT;
        return -1;
    }
    return mq_receive_common(mqdes, msg_ptr, msg_len, msg_prio, 0, NULL);
}

ssize_t mq_timedreceive(mqd_t mqdes, char* msg_ptr, size_t msg_len,
                        unsigned int* msg_prio,
                        const struct timespec* abs_timeout)
{
    if (!msg_ptr) {
        errno = EFAULT;
        return -1;
    }
    return mq_receive_common(mqdes, msg_ptr, msg_len, msg_prio, 1,
                             abs_timeout);
}

int mq_notify(mqd_t mqdes, const struct sigevent* notification)
{
    RinLibcMqNotifySlot* slot = NULL;
    RinEvent event = RIN_OBJECT_HANDLE_INVALID;
    struct sigevent event_copy;
    pthread_attr_t attr_copy;
    int has_thread_attributes = 0;
    uint32_t action;
    int32_t result;
    if (mq_notify_process_sync() != 0) return -1;
    if (!notification) {
        result = mq_notify_call(
            mqdes, RIN_OBJECT_HANDLE_INVALID,
            RIN_POSIX_MQ_NOTIFY_CANCEL, 0u);
        if (result == RIN_ERROR_NOT_FOUND) return 0;
        if (mq_error_from_result(result,
                                 RIN_LIBC_SDK_IPC_POSIX_MQ_NOTIFY, 0) != 0)
            return -1;
        return 0;
    }
    /* Single snapshot: every field below comes from the copy, so a
     * concurrent caller-side modification cannot mix generations. */
    if (mq_user_snapshot(notification, sizeof(event_copy), 0,
                         &event_copy) != 0)
        return -1;
    if (event_copy.sigev_notify == SIGEV_THREAD &&
        event_copy.sigev_notify_attributes) {
        if (mq_user_snapshot(event_copy.sigev_notify_attributes,
                             sizeof(attr_copy), 0, &attr_copy) != 0)
            return -1;
        has_thread_attributes = 1;
    }

    if (event_copy.sigev_notify == SIGEV_NONE)
        action = RIN_POSIX_MQ_NOTIFY_NONE;
    else if (event_copy.sigev_notify == SIGEV_SIGNAL) {
        if (event_copy.sigev_signo <= 0 ||
            event_copy.sigev_signo >= NSIG ||
            event_copy.sigev_signo == __RIN_SIGCANCEL ||
            event_copy.sigev_signo == SIGKILL ||
            event_copy.sigev_signo == SIGSTOP) {
            errno = EINVAL;
            return -1;
        }
        {
            uint64_t signal_value = 0u;
            const size_t value_size = sizeof(event_copy.sigev_value) <
                                              sizeof(signal_value)
                                          ? sizeof(event_copy.sigev_value)
                                          : sizeof(signal_value);
            memcpy(&signal_value, &event_copy.sigev_value, value_size);
            return mq_error_from_result(
                mq_notify_call(mqdes,
                               (RinEvent)(uint32_t)event_copy.sigev_signo,
                               RIN_POSIX_MQ_NOTIFY_SIGNAL, signal_value),
                RIN_LIBC_SDK_IPC_POSIX_MQ_NOTIFY, 0);
        }
    } else if (event_copy.sigev_notify == SIGEV_THREAD) {
        if (!event_copy.sigev_notify_function) {
            errno = EINVAL;
            return -1;
        }
        if (has_thread_attributes) {
            int attribute_result =
                mq_notify_validate_attributes(&attr_copy);
            if (attribute_result != 0) {
                errno = attribute_result;
                return -1;
            }
            if (pthread_attr_setdetachstate(
                    &attr_copy, PTHREAD_CREATE_DETACHED) != 0) {
                errno = EINVAL;
                return -1;
            }
        }
        action = RIN_POSIX_MQ_NOTIFY_EVENT;
    } else {
        errno = EINVAL;
        return -1;
    }
    if (action == RIN_POSIX_MQ_NOTIFY_NONE)
        return mq_error_from_result(
            mq_notify_call(mqdes, RIN_OBJECT_HANDLE_INVALID, action, 0u),
            RIN_LIBC_SDK_IPC_POSIX_MQ_NOTIFY, 0);

    if (mq_notify_ensure_dispatcher() != 0) return -1;
    slot = mq_notify_reserve();
    if (!slot) {
        errno = EAGAIN;
        return -1;
    }
    slot->event = RIN_OBJECT_HANDLE_INVALID;
    result = mq_event_create(&event);
    if (result != RIN_SUCCESS) {
        mq_notify_slot_release(slot);
        return mq_error_from_result(result,
                                    RIN_LIBC_SDK_IPC_EVENT_CREATE, 0);
    }
    slot->event = event;
    slot->function = event_copy.sigev_notify_function;
    slot->value = event_copy.sigev_value;
    slot->has_thread_attributes = has_thread_attributes ? 1u : 0u;
    if (has_thread_attributes) slot->thread_attributes = attr_copy;
    result = mq_notify_call(mqdes, event, RIN_POSIX_MQ_NOTIFY_EVENT, 0u);
    if (result != RIN_SUCCESS) {
        mq_notify_slot_release(slot);
        return mq_error_from_result(result,
                                    RIN_LIBC_SDK_IPC_POSIX_MQ_NOTIFY, 0);
    }
    /* Event signals are persistent, so publishing the slot after the kernel
     * call cannot lose a notification that arrives before this return. */
    __atomic_store_n(&slot->state, RIN_LIBC_MQ_NOTIFY_SLOT_REGISTERED,
                     __ATOMIC_RELEASE);
    return 0;
}
