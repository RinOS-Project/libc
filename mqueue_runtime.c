/* SPDX-License-Identifier: MIT */

#include "mqueue.h"

#include "pthread.h"
#include "stdarg.h"
#include "string.h"
#include "sys/syscall.h"
#include "unistd.h"
#include <rin/ipc.h>

#define RIN_LIBC_MQ_NOTIFY_SLOTS 64u
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
    RIN_LIBC_SDK_IPC_EVENT_QUERY = 24u
};

typedef struct RinLibcMqNotifySlot {
    volatile uint32_t state;
    uint32_t notification;
    uint64_t sequence;
    mqd_t queue;
    RinEvent event;
    void (*function)(union sigval);
    union sigval value;
} RinLibcMqNotifySlot;

static RinLibcMqNotifySlot g_mq_notify_slots[RIN_LIBC_MQ_NOTIFY_SLOTS];
static volatile uint64_t g_mq_notify_sequence;

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
    case RIN_ERROR_LIMIT_EXCEEDED: errno = EOVERFLOW; break;
    case RIN_ERROR_INVALID_ADDRESS: errno = EFAULT; break;
    case RIN_ERROR_EXISTS: errno = EEXIST; break;
    default: errno = EIO; break;
    }
    if (operation == RIN_LIBC_SDK_IPC_POSIX_MQ_OPEN &&
        (open_flags & O_EXCL) != 0 && result == RIN_ERROR_BUSY)
        errno = EEXIST;
    return -1;
}

static int mq_name_slice(const char* name, RinStringV1* slice)
{
    size_t length;
    if (!name || !slice) {
        errno = EFAULT;
        return -1;
    }
    for (length = 0u; length <= RIN_POSIX_MQ_NAME_MAX; ++length) {
        if (name[length] == '\0') break;
    }
    if (length < 2u || length > RIN_POSIX_MQ_NAME_MAX) {
        errno = length > RIN_POSIX_MQ_NAME_MAX ? ENAMETOOLONG : EINVAL;
        return -1;
    }
    slice->address = (uint64_t)(uintptr_t)name;
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
                          RinPosixMqAttrV1* old_attributes)
{
    RinSdkArgsV1 request;
    int32_t result;
    if (!attributes || !old_attributes) return RIN_ERROR_INVALID_ARGUMENT;
    mq_sdk_request_init(&request);
    request.value[0] = queue;
    request.value[1] = (uint64_t)(uintptr_t)attributes;
    result = mq_raw_sdk_call(RIN_LIBC_SDK_LIBRARY_IPC,
                             RIN_LIBC_SDK_IPC_POSIX_MQ_SETATTR,
                             &request, old_attributes,
                             sizeof(*old_attributes));
    if (result == RIN_SUCCESS &&
        (old_attributes->struct_size != sizeof(*old_attributes) ||
         old_attributes->version != RIN_POSIX_MQ_ABI_VERSION ||
         old_attributes->reserved[0] != 0u ||
         old_attributes->reserved[1] != 0u))
        return RIN_ERROR_ABI_MISMATCH;
    return result;
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
                               RinPosixMqReceiveResultV1* result_out)
{
    RinSdkArgsV1 request;
    int32_t result;
    mq_sdk_request_init(&request);
    request.value[0] = queue;
    request.value[1] = (uint64_t)(uintptr_t)message;
    request.value[2] = (uint64_t)capacity;
    request.value[3] = waiter_token;
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
    if (!absolute || !ns_out) return -1;
    if (absolute->tv_nsec < 0 || absolute->tv_nsec >= 1000000000L) {
        errno = EINVAL;
        return -1;
    }
    if (absolute->tv_sec < 0) {
        *ns_out = 0u;
        return 0;
    }
    if ((uint64_t)absolute->tv_sec >
        (UINT64_MAX - (uint64_t)absolute->tv_nsec) / UINT64_C(1000000000)) {
        errno = EOVERFLOW;
        return -1;
    }
    *ns_out = (uint64_t)absolute->tv_sec * UINT64_C(1000000000) +
              (uint64_t)absolute->tv_nsec;
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
        uint64_t timeout_ns = RIN_SDK_INFINITE;
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
            (void)mq_error_from_result(result,
                                       RIN_LIBC_SDK_IPC_WAIT_MANY, 0);
            return -1;
        }
        if (!absolute) continue;
        /* Recompute CLOCK_REALTIME after each bounded wait so clock changes
         * are observed while retaining the caller's absolute deadline. */
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
                          unsigned int priority,
                          const struct timespec* absolute)
{
    for (;;) {
        RinPosixMqAttrV1 attributes;
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
        if (mq_wait_ready(queue, RIN_WAIT_EVENT_WRITABLE, absolute) != 0)
            return -1;
    }
}

static ssize_t mq_receive_common(mqd_t queue, char* message, size_t capacity,
                                 unsigned int* priority_out,
                                 const struct timespec* absolute)
{
    uint64_t waiter_token = RIN_POSIX_MQ_WAIT_TOKEN_INVALID;
    for (;;) {
        RinPosixMqAttrV1 attributes;
        RinPosixMqReceiveResultV1 received;
        int32_t result = mq_receive_once(queue, message, capacity,
                                         waiter_token, &received);
        if (result == RIN_SUCCESS) {
            /* The kernel consumes a registered receiver token atomically
             * with the message dequeue. */
            waiter_token = RIN_POSIX_MQ_WAIT_TOKEN_INVALID;
            if (priority_out) *priority_out = received.priority;
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
        if (waiter_token == RIN_POSIX_MQ_WAIT_TOKEN_INVALID) {
            result = mq_wait_receive_begin(queue, &waiter_token);
            if (result != RIN_SUCCESS)
                return (ssize_t)mq_error_from_result(
                    result, RIN_LIBC_SDK_IPC_POSIX_MQ_WAIT_RECEIVE_BEGIN, 0);
            if (waiter_token == RIN_POSIX_MQ_WAIT_TOKEN_INVALID)
                continue;
        }
        if (mq_wait_ready(queue, RIN_WAIT_EVENT_READABLE, absolute) != 0) {
            (void)mq_wait_receive_end(waiter_token);
            return -1;
        }
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
        uint32_t expected = 0u;
        if (__atomic_compare_exchange_n(&g_mq_notify_slots[index].state,
                                        &expected, 1u, 0,
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

static int32_t mq_event_signal(RinEvent event, uint64_t value)
{
    RinSdkArgsV1 request;
    mq_sdk_request_init(&request);
    request.value[0] = event;
    request.value[1] = value;
    return mq_raw_sdk_call(RIN_LIBC_SDK_LIBRARY_IPC,
                           RIN_LIBC_SDK_IPC_EVENT_SIGNAL,
                           &request, NULL, 0u);
}

static int32_t mq_event_wait(RinEvent event, uint64_t* value_out)
{
    RinSdkArgsV1 request;
    RinWaitItemV1 item;
    uint32_t index = UINT32_MAX;
    int32_t result;
    if (!value_out) return RIN_ERROR_INVALID_ARGUMENT;
    *value_out = 0u;
    item.handle = event;
    item.events = RIN_WAIT_EVENT_READABLE;
    item.observed = 0u;
    item.user_tag = 0u;
    mq_sdk_request_init(&request);
    request.value[0] = (uint64_t)(uintptr_t)&item;
    request.value[1] = sizeof(item);
    request.value[2] = RIN_SDK_INFINITE;
    result = mq_raw_sdk_call(RIN_LIBC_SDK_LIBRARY_IPC,
                             RIN_LIBC_SDK_IPC_WAIT_MANY, &request,
                             &index, sizeof(index));
    if (result != RIN_SUCCESS || index != 0u) return -1;
    mq_sdk_request_init(&request);
    request.value[0] = event;
    result = mq_raw_sdk_call(RIN_LIBC_SDK_LIBRARY_IPC,
                             RIN_LIBC_SDK_IPC_EVENT_QUERY, &request,
                             value_out, sizeof(*value_out));
    return result == RIN_SUCCESS ? 0 : -1;
}

static void* mq_notify_worker(void* argument)
{
    RinLibcMqNotifySlot* slot = (RinLibcMqNotifySlot*)argument;
    int wait_result;
    uint64_t event_value = 0u;
    if (!slot) return NULL;
    wait_result = mq_event_wait(slot->event, &event_value);
    if (wait_result == 0 && event_value != 0u &&
        __atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) == 2u) {
        if (slot->notification == SIGEV_THREAD) {
            if (slot->function) slot->function(slot->value);
        }
    }
    (void)mq_close_handle(slot->event);
    slot->event = RIN_OBJECT_HANDLE_INVALID;
    slot->queue = RIN_OBJECT_HANDLE_INVALID;
    slot->function = NULL;
    slot->value.sival_ptr = NULL;
    __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
    return NULL;
}

mqd_t mq_open(const char* name, int oflag, ...)
{
    RinStringV1 name_string;
    RinPosixMqAttrV1 wire_attributes;
    RinObjectHandle queue = RIN_OBJECT_HANDLE_INVALID;
    RinSdkArgsV1 request;
    const struct mq_attr* attributes = NULL;
    mode_t mode = 0u;
    uint32_t flags;
    int allowed_flags = O_ACCMODE | O_CREAT | O_EXCL | O_NONBLOCK | O_CLOEXEC;
    int32_t result;
    if (mq_name_slice(name, &name_string) != 0) return (mqd_t)-1;
    if ((oflag & ~allowed_flags) != 0) {
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
        wire_attributes.struct_size = sizeof(wire_attributes);
        wire_attributes.version = RIN_POSIX_MQ_ABI_VERSION;
        wire_attributes.mq_flags = 0u;
        wire_attributes.mq_maxmsg = (uint64_t)attributes->mq_maxmsg;
        wire_attributes.mq_msgsize = (uint64_t)attributes->mq_msgsize;
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
    RinSdkArgsV1 request;
    int32_t result;
    if (mq_name_slice(name, &name_string) != 0) return -1;
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
    int32_t result;
    if (!attr) {
        errno = EFAULT;
        return -1;
    }
    result = mq_getattr_raw(mqdes, &wire_attributes);
    if (mq_error_from_result(result,
                             RIN_LIBC_SDK_IPC_POSIX_MQ_GETATTR, 0) != 0)
        return -1;
    mq_attributes_to_posix(&wire_attributes, attr);
    return 0;
}

int mq_setattr(mqd_t mqdes, const struct mq_attr* newattr,
               struct mq_attr* oldattr)
{
    RinPosixMqAttrV1 requested;
    RinPosixMqAttrV1 previous;
    int32_t result;
    if (!newattr) {
        errno = EFAULT;
        return -1;
    }
    if ((newattr->mq_flags & ~O_NONBLOCK) != 0) {
        errno = EINVAL;
        return -1;
    }
    requested.struct_size = sizeof(requested);
    requested.version = RIN_POSIX_MQ_ABI_VERSION;
    requested.mq_flags = (newattr->mq_flags & O_NONBLOCK)
                             ? RIN_POSIX_MQ_ATTR_NONBLOCK : 0u;
    requested.mq_maxmsg = 0u;
    requested.mq_msgsize = 0u;
    requested.mq_curmsgs = 0u;
    requested.reserved[0] = 0u;
    requested.reserved[1] = 0u;
    result = mq_setattr_raw(mqdes, &requested, &previous);
    if (mq_error_from_result(result,
                             RIN_LIBC_SDK_IPC_POSIX_MQ_SETATTR, 0) != 0)
        return -1;
    if (oldattr) mq_attributes_to_posix(&previous, oldattr);
    return 0;
}

int mq_send(mqd_t mqdes, const char* msg_ptr, size_t msg_len,
            unsigned int msg_prio)
{
    if (msg_len != 0u && !msg_ptr) {
        errno = EFAULT;
        return -1;
    }
    return mq_send_common(mqdes, msg_ptr, msg_len, msg_prio, NULL);
}

int mq_timedsend(mqd_t mqdes, const char* msg_ptr, size_t msg_len,
                 unsigned int msg_prio,
                 const struct timespec* abs_timeout)
{
    if (msg_len != 0u && !msg_ptr) {
        errno = EFAULT;
        return -1;
    }
    if (mq_timed_transfer_validate(abs_timeout) != 0) return -1;
    return mq_send_common(mqdes, msg_ptr, msg_len, msg_prio, abs_timeout);
}

ssize_t mq_receive(mqd_t mqdes, char* msg_ptr, size_t msg_len,
                   unsigned int* msg_prio)
{
    if (!msg_ptr) {
        errno = EFAULT;
        return -1;
    }
    return mq_receive_common(mqdes, msg_ptr, msg_len, msg_prio, NULL);
}

ssize_t mq_timedreceive(mqd_t mqdes, char* msg_ptr, size_t msg_len,
                        unsigned int* msg_prio,
                        const struct timespec* abs_timeout)
{
    if (!msg_ptr) {
        errno = EFAULT;
        return -1;
    }
    if (mq_timed_transfer_validate(abs_timeout) != 0) return -1;
    return mq_receive_common(mqdes, msg_ptr, msg_len, msg_prio, abs_timeout);
}

int mq_notify(mqd_t mqdes, const struct sigevent* notification)
{
    RinLibcMqNotifySlot* slot = NULL;
    RinEvent event = RIN_OBJECT_HANDLE_INVALID;
    uint32_t action;
    int32_t result;
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

    if (notification->sigev_notify == SIGEV_NONE)
        action = RIN_POSIX_MQ_NOTIFY_NONE;
    else if (notification->sigev_notify == SIGEV_SIGNAL) {
        if (notification->sigev_signo <= 0 ||
            notification->sigev_signo >= NSIG ||
            notification->sigev_signo == SIGKILL ||
            notification->sigev_signo == SIGSTOP) {
            errno = EINVAL;
            return -1;
        }
        {
            uint64_t signal_value = 0u;
            const size_t value_size = sizeof(notification->sigev_value) <
                                              sizeof(signal_value)
                                          ? sizeof(notification->sigev_value)
                                          : sizeof(signal_value);
            memcpy(&signal_value, &notification->sigev_value, value_size);
            return mq_error_from_result(
                mq_notify_call(mqdes,
                               (RinEvent)(uint32_t)notification->sigev_signo,
                               RIN_POSIX_MQ_NOTIFY_SIGNAL, signal_value),
                RIN_LIBC_SDK_IPC_POSIX_MQ_NOTIFY, 0);
        }
    } else if (notification->sigev_notify == SIGEV_THREAD) {
        if (!notification->sigev_notify_function) {
            errno = EINVAL;
            return -1;
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

    slot = mq_notify_reserve();
    if (!slot) {
        errno = EAGAIN;
        return -1;
    }
    result = mq_event_create(&event);
    if (result != RIN_SUCCESS) {
        __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
        return mq_error_from_result(result,
                                    RIN_LIBC_SDK_IPC_EVENT_CREATE, 0);
    }
    slot->notification = (uint32_t)notification->sigev_notify;
    slot->queue = mqdes;
    slot->event = event;
    slot->function = notification->sigev_notify_function;
    slot->value = notification->sigev_value;
    slot->sequence = __atomic_add_fetch(&g_mq_notify_sequence, 1u,
                                        __ATOMIC_RELAXED);
    {
        pthread_t thread;
        const int thread_result = pthread_create(
            &thread,
            (const pthread_attr_t*)notification->sigev_notify_attributes,
            mq_notify_worker, slot);
        if (thread_result != 0) {
            (void)mq_close_handle(event);
            slot->event = RIN_OBJECT_HANDLE_INVALID;
            __atomic_store_n(&slot->state, 0u, __ATOMIC_RELEASE);
            errno = thread_result;
            return -1;
        }
        (void)pthread_detach(thread);
    }
    /* Start the worker before publishing the kernel registration.  The
     * kernel may signal the event before mq_notify returns, so a worker must
     * already be waiting when registration succeeds. */
    __atomic_store_n(&slot->state, 2u, __ATOMIC_RELEASE);
    result = mq_notify_call(mqdes, event, RIN_POSIX_MQ_NOTIFY_EVENT, 0u);
    if (result != RIN_SUCCESS) {
        __atomic_store_n(&slot->state, 3u, __ATOMIC_RELEASE);
        (void)mq_event_signal(event, 1u);
        return mq_error_from_result(result,
                                    RIN_LIBC_SDK_IPC_POSIX_MQ_NOTIFY, 0);
    }
    return 0;
}
