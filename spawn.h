/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - spawn.h
 * Minimal POSIX spawn compatibility for Ladybird/LibCore.
 */

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ && \
    (!defined(RIN_FREESTANDING) || !(RIN_FREESTANDING)) && \
    defined(__linux__) && (defined(__GNUC__) || defined(__clang__)) && \
    !defined(_RIN_POSIX_SPAWN_SYSCALL6) && \
    !defined(_RIN_POSIX_SPAWN_ACTION_SYSCALL6)
#include_next <spawn.h>
#else

#ifndef _SPAWN_H
#define _SPAWN_H

#include "errno.h"
#include "fcntl.h"
#include "signal.h"
#include "sched.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include <rin/process/posix_spawn_abi.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    short flags;
    pid_t pgroup;
    sigset_t sigdefault;
    sigset_t sigmask;
    int sched_policy;
    int sched_priority;
} posix_spawnattr_t;

/* GNU/POSIX extensions implemented by the product spawn owner.  SETSID stays
 * in the scalar syscall flags word; the payload-bearing attributes are
 * serialized into a versioned reserved action record. */
#define POSIX_SPAWN_RESETIDS 0x0001
#define POSIX_SPAWN_SETPGROUP 0x0002
#define POSIX_SPAWN_SETSIGDEF 0x0004
#define POSIX_SPAWN_SETSIGMASK 0x0008
#define POSIX_SPAWN_SETSCHEDPARAM 0x0010
#define POSIX_SPAWN_SETSCHEDULER 0x0020
#define POSIX_SPAWN_SETSID 0x0080

static inline int _rin_posix_spawn_scheduler_policy_supported(int policy) {
    return policy == SCHED_OTHER || policy == SCHED_FIFO ||
           policy == SCHED_RR || policy == SCHED_BATCH ||
           policy == SCHED_IDLE;
}

enum {
    _RIN_POSIX_SPAWN_ACTION_NONE = 0,
    _RIN_POSIX_SPAWN_ACTION_OPEN = RIN_POSIX_SPAWN_ACTION_OPEN,
    _RIN_POSIX_SPAWN_ACTION_CLOSE = RIN_POSIX_SPAWN_ACTION_CLOSE,
    _RIN_POSIX_SPAWN_ACTION_DUP2 = RIN_POSIX_SPAWN_ACTION_DUP2,
    _RIN_POSIX_SPAWN_ACTION_ATTRIBUTES = RIN_POSIX_SPAWN_ACTION_ATTRIBUTES,
};

typedef struct {
    int type;
    int fd;
    int src_fd;
    int oflag;
    mode_t mode;
    char path[256];
} _rin_posix_spawn_file_action_t;

#define _RIN_POSIX_SPAWN_MAX_ACTIONS RIN_POSIX_SPAWN_ACTION_MAX
#define _RIN_POSIX_SPAWN_ACTION_PATH_CAPACITY RIN_POSIX_SPAWN_ACTION_PATH_MAX
#define _RIN_POSIX_SPAWN_EXEC_PATH_CAPACITY 512u
#define _RIN_POSIX_SPAWN_ENV_PATH_CAPACITY 4096u
#define _RIN_POSIX_SPAWN_ACTIONS_MAGIC UINT32_C(0x53504157)

typedef struct {
    int action_count;
    _rin_posix_spawn_file_action_t* actions;
    size_t action_capacity;
    uint32_t state_cookie;
} posix_spawn_file_actions_t;

#if defined(__cplusplus)
static_assert(sizeof(_rin_posix_spawn_file_action_t) ==
                  sizeof(RinPosixSpawnFileActionV1),
              "posix_spawn action ABI drift");
#else
_Static_assert(sizeof(_rin_posix_spawn_file_action_t) ==
                   sizeof(RinPosixSpawnFileActionV1),
               "posix_spawn action ABI drift");
#endif

static inline int posix_spawnattr_init(posix_spawnattr_t* attr) {
    if (!attr)
        return EINVAL;
    attr->flags = 0;
    attr->pgroup = 0;
    attr->sigdefault = 0;
    attr->sigmask = 0;
    attr->sched_policy = SCHED_OTHER;
    attr->sched_priority = 0;
    return 0;
}

static inline int posix_spawnattr_destroy(posix_spawnattr_t* attr) {
    if (!attr)
        return EINVAL;
    attr->flags = 0;
    attr->pgroup = 0;
    attr->sigdefault = 0;
    attr->sigmask = 0;
    attr->sched_policy = SCHED_OTHER;
    attr->sched_priority = 0;
    return 0;
}

static inline int posix_spawnattr_setflags(posix_spawnattr_t* attr,
                                           short flags)
{
    if (!attr) return EINVAL;
    if ((flags & (short)~(POSIX_SPAWN_RESETIDS |
                          POSIX_SPAWN_SETPGROUP |
                          POSIX_SPAWN_SETSIGDEF |
                          POSIX_SPAWN_SETSIGMASK |
                          POSIX_SPAWN_SETSCHEDPARAM |
                          POSIX_SPAWN_SETSCHEDULER |
                          POSIX_SPAWN_SETSID)) != 0) return EINVAL;
    attr->flags = flags;
    return 0;
}

static inline int posix_spawnattr_setpgroup(posix_spawnattr_t* attr,
                                            pid_t pgroup) {
    if (!attr || pgroup < 0) return EINVAL;
    attr->pgroup = pgroup;
    return 0;
}

static inline int posix_spawnattr_getpgroup(const posix_spawnattr_t* attr,
                                            pid_t* pgroup) {
    if (!attr || !pgroup) return EINVAL;
    *pgroup = attr->pgroup;
    return 0;
}

static inline int posix_spawnattr_setsigdefault(posix_spawnattr_t* attr,
                                                const sigset_t* set) {
    if (!attr || !set) return EINVAL;
    attr->sigdefault = *set;
    return 0;
}

static inline int posix_spawnattr_getsigdefault(const posix_spawnattr_t* attr,
                                                sigset_t* set) {
    if (!attr || !set) return EINVAL;
    *set = attr->sigdefault;
    return 0;
}

static inline int posix_spawnattr_setsigmask(posix_spawnattr_t* attr,
                                             const sigset_t* set) {
    if (!attr || !set) return EINVAL;
    attr->sigmask = *set;
    return 0;
}

static inline int posix_spawnattr_getsigmask(const posix_spawnattr_t* attr,
                                             sigset_t* set) {
    if (!attr || !set) return EINVAL;
    *set = attr->sigmask;
    return 0;
}

static inline int posix_spawnattr_setschedparam(
    posix_spawnattr_t* attr, const struct sched_param* param) {
    if (!attr || !param)
        return EINVAL;
    if (param->sched_priority < 0 || param->sched_priority > 99)
        return EINVAL;
    attr->sched_priority = param->sched_priority;
    return 0;
}

static inline int posix_spawnattr_getschedparam(
    const posix_spawnattr_t* attr, struct sched_param* param) {
    if (!attr || !param)
        return EINVAL;
    param->sched_priority = attr->sched_priority;
    return 0;
}

static inline int posix_spawnattr_setschedpolicy(
    posix_spawnattr_t* attr, int policy) {
    int minimum;
    int maximum;
    if (!attr || !_rin_posix_spawn_scheduler_policy_supported(policy))
        return EINVAL;
    minimum = sched_get_priority_min(policy);
    maximum = sched_get_priority_max(policy);
    if (minimum < 0 || maximum < 0)
        return EINVAL;
    attr->sched_policy = policy;
    return 0;
}

static inline int posix_spawnattr_getschedpolicy(
    const posix_spawnattr_t* attr, int* policy) {
    if (!attr || !policy)
        return EINVAL;
    *policy = attr->sched_policy;
    return 0;
}

static inline int posix_spawnattr_getflags(const posix_spawnattr_t* attr,
                                           short* flags)
{
    if (!attr || !flags) return EINVAL;
    *flags = attr->flags;
    return 0;
}

static _rin_posix_spawn_file_action_t _rin_posix_spawn_empty_action;

static inline int posix_spawn_file_actions_init(posix_spawn_file_actions_t* actions) {
    if (!actions)
        return EINVAL;
    actions->action_count = 0;
    actions->actions = NULL;
    actions->action_capacity = 0u;
    actions->state_cookie = _RIN_POSIX_SPAWN_ACTIONS_MAGIC;
    return 0;
}

static inline int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t* actions) {
    if (!actions)
        return EINVAL;
    if (actions->state_cookie != _RIN_POSIX_SPAWN_ACTIONS_MAGIC ||
        actions->action_capacity > _RIN_POSIX_SPAWN_MAX_ACTIONS ||
        (actions->action_capacity == 0u) != (actions->actions == NULL))
        return EINVAL;
    free(actions->actions);
    actions->action_count = 0;
    /* A zero sentinel keeps legacy code that inspects the now-invalid object
     * from dereferencing null while retaining no per-object storage. */
    actions->actions = (_rin_posix_spawn_file_action_t*)
        &_rin_posix_spawn_empty_action;
    actions->action_capacity = 0u;
    actions->state_cookie = 0u;
    return 0;
}

/* Never use strlen() for a caller-owned action path.  Apart from making the
 * admission boundary explicit, the bounded scan keeps malformed metadata
 * from walking into an adjacent object looking for a terminator. */
static inline int _rin_posix_spawn_bounded_string_length(
    const char* path, size_t capacity, size_t* output_length)
{
    size_t index;

    if (!path || !output_length)
        return EINVAL;
    for (index = 0; index < capacity; ++index) {
        if (path[index] == '\0') {
            *output_length = index;
            return 0;
        }
    }
    return ENAMETOOLONG;
}

static inline int _rin_posix_spawn_bounded_path_length(
    const char* path, size_t* output_length)
{
    return _rin_posix_spawn_bounded_string_length(
        path, _RIN_POSIX_SPAWN_ACTION_PATH_CAPACITY, output_length);
}

static inline int _rin_posix_spawn_action_tail_is_zero(
    const _rin_posix_spawn_file_action_t* action, size_t path_length)
{
    size_t index;

    if (!action || path_length >= sizeof(action->path))
        return 0;
    for (index = path_length + 1u; index < sizeof(action->path); ++index) {
        if (action->path[index] != '\0')
            return 0;
    }
    return 1;
}

static inline int _rin_posix_spawn_action_is_valid(
    const _rin_posix_spawn_file_action_t* action)
{
    size_t path_length;
    int error;

    if (!action || action->fd < 0)
        return EINVAL;
    switch (action->type) {
    case _RIN_POSIX_SPAWN_ACTION_OPEN:
        if (action->src_fd != -1)
            return EINVAL;
        error = _rin_posix_spawn_bounded_path_length(
            action->path, &path_length);
        if (error != 0 || path_length == 0u ||
            !_rin_posix_spawn_action_tail_is_zero(action, path_length))
            return EINVAL;
        return 0;
    case _RIN_POSIX_SPAWN_ACTION_CLOSE:
        if (action->src_fd != -1 || action->oflag != 0 || action->mode != 0)
            return EINVAL;
        return action->path[0] == '\0' &&
                       _rin_posix_spawn_action_tail_is_zero(action, 0u)
                   ? 0
                   : EINVAL;
    case _RIN_POSIX_SPAWN_ACTION_DUP2:
        if (action->src_fd < 0 || action->oflag != 0 || action->mode != 0)
            return EINVAL;
        return action->path[0] == '\0' &&
                       _rin_posix_spawn_action_tail_is_zero(action, 0u)
                   ? 0
                   : EINVAL;
    default:
        return EINVAL;
    }
}

static inline int _rin_posix_spawn_reserve_action(
    posix_spawn_file_actions_t* actions,
    _rin_posix_spawn_file_action_t** output)
{
    size_t new_capacity;
    _rin_posix_spawn_file_action_t* new_actions;
    if (!actions || !output)
        return EINVAL;
    if (actions->state_cookie != _RIN_POSIX_SPAWN_ACTIONS_MAGIC)
        return EINVAL;
    if (actions->action_count < 0 ||
        actions->action_count > (int)_RIN_POSIX_SPAWN_MAX_ACTIONS)
        return EINVAL;
    if (actions->action_count == _RIN_POSIX_SPAWN_MAX_ACTIONS)
        return ENOMEM;
    if (actions->action_capacity > _RIN_POSIX_SPAWN_MAX_ACTIONS ||
        (actions->action_capacity == 0u) != (actions->actions == NULL) ||
        (size_t)actions->action_count > actions->action_capacity)
        return EINVAL;
    if ((size_t)actions->action_count == actions->action_capacity) {
        new_capacity = actions->action_capacity == 0u
            ? 4u : actions->action_capacity * 2u;
        if (new_capacity > _RIN_POSIX_SPAWN_MAX_ACTIONS)
            new_capacity = _RIN_POSIX_SPAWN_MAX_ACTIONS;
        if (new_capacity > SIZE_MAX / sizeof(*actions->actions))
            return ENOMEM;
        new_actions = (_rin_posix_spawn_file_action_t*)realloc(
            actions->actions, new_capacity * sizeof(*actions->actions));
        if (!new_actions)
            return ENOMEM;
        memset(new_actions + actions->action_capacity, 0,
               (new_capacity - actions->action_capacity) *
                   sizeof(*actions->actions));
        actions->actions = new_actions;
        actions->action_capacity = new_capacity;
    }
    *output = &actions->actions[actions->action_count];
    memset(*output, 0, sizeof(**output));
    (*output)->src_fd = -1;
    ++actions->action_count;
    return 0;
}

static inline int posix_spawn_file_actions_addopen(posix_spawn_file_actions_t* actions, int fd, const char* path, int oflag, mode_t mode) {
    _rin_posix_spawn_file_action_t* action;
    size_t path_length;
    int error;
    if (!actions || !path || fd < 0)
        return EINVAL;
    error = _rin_posix_spawn_bounded_path_length(path, &path_length);
    if (error != 0)
        return error;
    if (path_length == 0u)
        return EINVAL;
    error = _rin_posix_spawn_reserve_action(actions, &action);
    if (error != 0)
        return error;
    action->type = _RIN_POSIX_SPAWN_ACTION_OPEN;
    action->fd = fd;
    action->src_fd = -1;
    action->oflag = oflag;
    action->mode = mode;
    memcpy(action->path, path, path_length + 1u);
    return 0;
}

static inline int posix_spawn_file_actions_addclose(posix_spawn_file_actions_t* actions, int fd) {
    _rin_posix_spawn_file_action_t* action;
    int error;
    if (fd < 0)
        return EINVAL;
    error = _rin_posix_spawn_reserve_action(actions, &action);
    if (error != 0)
        return error;
    action->type = _RIN_POSIX_SPAWN_ACTION_CLOSE;
    action->fd = fd;
    action->src_fd = -1;
    action->oflag = 0;
    action->mode = 0;
    return 0;
}

static inline int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t* actions, int oldfd, int newfd) {
    _rin_posix_spawn_file_action_t* action;
    int error;
    if (oldfd < 0 || newfd < 0)
        return EINVAL;
    error = _rin_posix_spawn_reserve_action(actions, &action);
    if (error != 0)
        return error;
    action->type = _RIN_POSIX_SPAWN_ACTION_DUP2;
    action->fd = newfd;
    action->src_fd = oldfd;
    action->oflag = 0;
    action->mode = 0;
    return 0;
}

#ifndef _RIN_POSIX_SPAWN_SYSCALL6
#define _RIN_POSIX_SPAWN_SYSCALL6(path, argv, envp, takeover_fd, takeover_name, flags) \
    _syscall6((uintptr_t)SYS_SPAWN_PROCESS, (uintptr_t)(path), (uintptr_t)(argv), \
              (uintptr_t)(envp), (uintptr_t)(takeover_fd), \
              (uintptr_t)(takeover_name), (uintptr_t)(flags))
#endif

#ifndef _RIN_POSIX_SPAWN_ACTION_SYSCALL6
#define _RIN_POSIX_SPAWN_ACTION_SYSCALL6(path, argv, envp, actions, count, flags) \
    _syscall6((uintptr_t)SYS_SPAWN_PROCESS_ACTIONS, (uintptr_t)(path), \
              (uintptr_t)(argv), (uintptr_t)(envp), (uintptr_t)(actions), \
              (uintptr_t)(count), (uintptr_t)(flags))
#endif

#ifndef _RIN_POSIX_SPAWN_GETENV
#define _RIN_POSIX_SPAWN_GETENV(name) secure_getenv(name)
#endif

static inline int _rin_posix_spawn_capture_search_path(
    char** storage_out, const char** path_out,
    char*** environment_out, unsigned int* epoch_out)
{
    char** snapshot;
    const char* path;
    char* storage;
    size_t path_length;
    int result;
    if (!storage_out || !path_out || !environment_out || !epoch_out)
        return EINVAL;
    *storage_out = NULL;
    *path_out = NULL;
    *environment_out = NULL;
    *epoch_out = 0u;
    snapshot = __rin_unistd_environment_snapshot(epoch_out);
    *environment_out = snapshot;
#if defined(RIN_FREESTANDING) && RIN_FREESTANDING && \
    defined(RIN_USERSPACE) && RIN_USERSPACE
    path = __rin_env_get_secure_from_snapshot(snapshot, "PATH");
#else
    path = _RIN_POSIX_SPAWN_GETENV("PATH");
#endif
    if (!path) {
        *path_out = NULL;
        return 0;
    }
    result = _rin_posix_spawn_bounded_string_length(
        path, _RIN_POSIX_SPAWN_ENV_PATH_CAPACITY, &path_length);
    if (result != 0) {
        __rin_unistd_environment_release(*epoch_out);
        *epoch_out = 0u;
        *environment_out = NULL;
        return result;
    }
    if (path_length == SIZE_MAX)
        result = ENOMEM;
    else {
        storage = (char*)malloc(path_length + 1u);
        if (!storage)
            result = ENOMEM;
        else {
            memcpy(storage, path, path_length + 1u);
            *storage_out = storage;
            *path_out = storage;
            return 0;
        }
    }
    __rin_unistd_environment_release(*epoch_out);
    *epoch_out = 0u;
    *environment_out = NULL;
    return result;
}

static inline int _rin_posix_spawn_validate(
    const posix_spawn_file_actions_t* file_actions,
    const posix_spawnattr_t* attr)
{
    const short supported_flags = (short)(POSIX_SPAWN_RESETIDS |
                                          POSIX_SPAWN_SETPGROUP |
                                          POSIX_SPAWN_SETSIGDEF |
                                          POSIX_SPAWN_SETSIGMASK |
                                          POSIX_SPAWN_SETSCHEDPARAM |
                                          POSIX_SPAWN_SETSCHEDULER |
                                          POSIX_SPAWN_SETSID);
    if (attr && (attr->flags & (short)~supported_flags) != 0)
        return EINVAL;
    if (attr && (attr->flags & POSIX_SPAWN_SETSID) != 0 &&
        (attr->flags & (POSIX_SPAWN_SETPGROUP |
                        POSIX_SPAWN_SETSCHEDPARAM |
                        POSIX_SPAWN_SETSCHEDULER)) != 0)
        return EINVAL;
    if (attr && (attr->flags & POSIX_SPAWN_SETPGROUP) != 0 &&
        attr->pgroup < 0)
        return EINVAL;
    if (attr && (attr->flags & (POSIX_SPAWN_SETSCHEDPARAM |
                                POSIX_SPAWN_SETSCHEDULER)) != 0) {
        int policy = attr->sched_policy;
        if ((attr->flags & POSIX_SPAWN_SETSCHEDULER) == 0) {
            policy = sched_getscheduler(0);
            if (policy < 0) return errno != 0 ? errno : EINVAL;
        }
        if (!_rin_posix_spawn_scheduler_policy_supported(policy))
            return EINVAL;
        {
            const int minimum = sched_get_priority_min(policy);
            const int maximum = sched_get_priority_max(policy);
            if (minimum < 0 || maximum < 0 ||
                attr->sched_priority < minimum ||
                attr->sched_priority > maximum)
                return EINVAL;
        }
    }
    if (!file_actions)
        return 0;
    if (file_actions->state_cookie != _RIN_POSIX_SPAWN_ACTIONS_MAGIC ||
        file_actions->action_count < 0 ||
        file_actions->action_count > (int)_RIN_POSIX_SPAWN_MAX_ACTIONS)
        return EINVAL;
    if (file_actions->action_count == 0)
        return (file_actions->action_capacity <=
                    _RIN_POSIX_SPAWN_MAX_ACTIONS &&
                ((file_actions->action_capacity == 0u) ==
                 (file_actions->actions == NULL)))
                   ? 0
                   : EINVAL;
    if (file_actions->action_capacity > _RIN_POSIX_SPAWN_MAX_ACTIONS ||
        file_actions->action_capacity < (size_t)file_actions->action_count ||
        !file_actions->actions)
        return EINVAL;
    for (int index = 0; index < file_actions->action_count; ++index) {
        int action_error = _rin_posix_spawn_action_is_valid(
            &file_actions->actions[index]);
        if (action_error != 0)
            return action_error;
    }
    return 0;
}

static inline int _rin_posix_spawn_wire_flags(
    const posix_spawnattr_t* attr, uintptr_t* flags_out)
{
    if (!flags_out) return EINVAL;
    *flags_out = 0u;
    if (!attr) return 0;
    if ((attr->flags & (short)~(POSIX_SPAWN_RESETIDS |
                                POSIX_SPAWN_SETPGROUP |
                                POSIX_SPAWN_SETSIGDEF |
                                POSIX_SPAWN_SETSIGMASK |
                                POSIX_SPAWN_SETSCHEDPARAM |
                                POSIX_SPAWN_SETSCHEDULER |
                                POSIX_SPAWN_SETSID)) != 0)
        return EINVAL;
    if ((attr->flags & POSIX_SPAWN_RESETIDS) != 0)
        *flags_out |= (uintptr_t)RIN_POSIX_SPAWN_FLAG_RESETIDS;
    if ((attr->flags & POSIX_SPAWN_SETSID) != 0)
        *flags_out |= (uintptr_t)RIN_POSIX_SPAWN_FLAG_SETSID;
    return 0;
}

static inline int _rin_posix_spawn_attribute_action(
    const posix_spawnattr_t* attr,
    _rin_posix_spawn_file_action_t* action)
{
    RinPosixSpawnAttributesV1 wire;
    const short payload_flags = (short)(POSIX_SPAWN_SETPGROUP |
                                        POSIX_SPAWN_SETSIGDEF |
                                        POSIX_SPAWN_SETSIGMASK |
                                        POSIX_SPAWN_SETSCHEDPARAM |
                                        POSIX_SPAWN_SETSCHEDULER);
    if (!attr || !action) return EINVAL;
    if ((attr->flags & payload_flags) == 0) return 0;
    if ((attr->flags & (short)~(payload_flags | POSIX_SPAWN_SETSID)) != 0)
        return EINVAL;
    memset(action, 0, sizeof(*action));
    wire.struct_size = (uint32_t)sizeof(wire);
    wire.version = (uint16_t)RIN_POSIX_SPAWN_ATTRIBUTE_ABI_VERSION;
    wire.reserved0 = 0u;
    wire.flags = 0u;
    wire.process_group = 0u;
    wire.signal_default = 0u;
    wire.signal_mask = 0u;
    wire.sched_policy = (int32_t)SCHED_OTHER;
    wire.sched_priority = 0;
    if ((attr->flags & POSIX_SPAWN_SETPGROUP) != 0) {
        if (attr->pgroup < 0 || (uint64_t)attr->pgroup > UINT32_MAX)
            return EOVERFLOW;
        wire.flags |= RIN_POSIX_SPAWN_ATTRIBUTE_SETPGROUP;
        wire.process_group = (uint32_t)attr->pgroup;
    }
    if ((attr->flags & POSIX_SPAWN_SETSIGDEF) != 0) {
        wire.flags |= RIN_POSIX_SPAWN_ATTRIBUTE_SETSIGDEF;
        wire.signal_default = (uint64_t)attr->sigdefault;
    }
    if ((attr->flags & POSIX_SPAWN_SETSIGMASK) != 0) {
        wire.flags |= RIN_POSIX_SPAWN_ATTRIBUTE_SETSIGMASK;
        wire.signal_mask = (uint64_t)attr->sigmask;
    }
    if ((attr->flags & POSIX_SPAWN_SETSCHEDPARAM) != 0) {
        wire.flags |= RIN_POSIX_SPAWN_ATTRIBUTE_SETSCHEDPARAM;
    }
    if ((attr->flags & (POSIX_SPAWN_SETSCHEDPARAM |
                        POSIX_SPAWN_SETSCHEDULER)) != 0) {
        wire.sched_priority = (int32_t)attr->sched_priority;
    }
    if ((attr->flags & POSIX_SPAWN_SETSCHEDULER) != 0) {
        wire.flags |= RIN_POSIX_SPAWN_ATTRIBUTE_SETSCHEDULER;
        wire.sched_policy = (int32_t)attr->sched_policy;
    }
    action->type = RIN_POSIX_SPAWN_ACTION_ATTRIBUTES;
    memcpy(action->path, &wire, sizeof(wire));
    return 0;
}

static inline int _rin_posix_spawn_build_actions(
    const posix_spawn_file_actions_t* file_actions,
    const posix_spawnattr_t* attr,
    _rin_posix_spawn_file_action_t** output_out,
    uintptr_t* count_out)
{
    uintptr_t count = 0u;
    uintptr_t file_action_count = file_actions
        ? (uintptr_t)file_actions->action_count : 0u;
    int has_attribute_action = attr &&
        (attr->flags & (POSIX_SPAWN_SETPGROUP |
                        POSIX_SPAWN_SETSIGDEF |
                        POSIX_SPAWN_SETSIGMASK |
                        POSIX_SPAWN_SETSCHEDPARAM |
                        POSIX_SPAWN_SETSCHEDULER)) != 0;
    uintptr_t required_count = file_action_count +
        (has_attribute_action ? 1u : 0u);
    _rin_posix_spawn_file_action_t* output;
    int error;
    if (!output_out || !count_out) return EINVAL;
    *output_out = NULL;
    *count_out = 0u;
    if (required_count > _RIN_POSIX_SPAWN_MAX_ACTIONS)
        return E2BIG;
    if (required_count == 0u)
        return 0;
    if (required_count > SIZE_MAX / sizeof(*output))
        return ENOMEM;
    output = (_rin_posix_spawn_file_action_t*)malloc(
        (size_t)required_count * sizeof(*output));
    if (!output)
        return ENOMEM;
    memset(output, 0, (size_t)required_count * sizeof(*output));
    if (has_attribute_action) {
        error = _rin_posix_spawn_attribute_action(attr, &output[count]);
        if (error != 0) {
            free(output);
            return error;
        }
        ++count;
    }
    if (file_action_count != 0u) {
        memcpy(output + count, file_actions->actions,
               (size_t)file_action_count * sizeof(*output));
        count += file_action_count;
    }
    *output_out = output;
    *count_out = count;
    return 0;
}

static inline int _rin_posix_spawn_direct(
    pid_t* pid, const char* path,
    const posix_spawn_file_actions_t* file_actions,
    const posix_spawnattr_t* attr, char* const argv[], char* const envp[])
{
    intptr_t result;
    int validation;
    size_t path_length;
    uintptr_t spawn_flags = 0u;
    unsigned int environment_epoch = 0u;
    char* const* environment = envp;
    _rin_posix_spawn_file_action_t* wire_actions = NULL;
    uintptr_t wire_action_count = 0u;

    if (!pid || !path)
        return EINVAL;
    validation = _rin_posix_spawn_bounded_string_length(
        path, _RIN_POSIX_SPAWN_EXEC_PATH_CAPACITY, &path_length);
    if (validation != 0)
        return validation;
    if (path_length == 0u)
        return EINVAL;
    validation = _rin_posix_spawn_validate(file_actions, attr);
    if (validation != 0)
        return validation;
    validation = _rin_posix_spawn_wire_flags(attr, &spawn_flags);
    if (validation != 0)
        return validation;
    validation = _rin_posix_spawn_build_actions(
        file_actions, attr, &wire_actions, &wire_action_count);
    if (validation != 0)
        return validation;

    if (!environment)
        environment = __rin_unistd_environment_snapshot(&environment_epoch);
    result = _RIN_POSIX_SPAWN_ACTION_SYSCALL6(
        path, argv, environment,
        wire_action_count != 0u ? wire_actions : NULL,
        wire_action_count,
        spawn_flags);
    free(wire_actions);
    if (!envp)
        __rin_unistd_environment_release(environment_epoch);
    if (result < 0 && result >= -4095)
        return (int)-result;
    if (result <= 0 || result > 0x7fffffffL)
        return EIO;
    *pid = (pid_t)result;
    return 0;
}

static inline int posix_spawn(
    pid_t* pid, const char* path,
    const posix_spawn_file_actions_t* file_actions,
    const posix_spawnattr_t* attr, char* const argv[], char* const envp[])
{
    return _rin_posix_spawn_direct(
        pid, path, file_actions, attr, argv, envp);
}

static inline int posix_spawnp(
    pid_t* pid, const char* file,
    const posix_spawn_file_actions_t* file_actions,
    const posix_spawnattr_t* attr, char* const argv[], char* const envp[])
{
    const char* search_path;
    const char* segment_begin;
    size_t file_len;
    size_t search_path_length;
    int saved_error = ENOENT;
    int validation;
    unsigned int environment_epoch = 0u;
    char** inherited_environment = NULL;
    char* const* spawn_environment = envp;
    char* candidate = NULL;
    char* path_storage = NULL;

    if (!pid || !file)
        return EINVAL;
    validation = _rin_posix_spawn_bounded_string_length(
        file, _RIN_POSIX_SPAWN_EXEC_PATH_CAPACITY, &file_len);
    if (validation != 0)
        return validation;
    if (file_len == 0u)
        return EINVAL;
    validation = _rin_posix_spawn_validate(file_actions, attr);
    if (validation != 0)
        return validation;
    if (strchr(file, '/'))
        return _rin_posix_spawn_direct(
            pid, file, file_actions, attr, argv, envp);

    validation = _rin_posix_spawn_capture_search_path(
        &path_storage, &search_path,
        &inherited_environment, &environment_epoch);
    if (validation != 0)
        return validation;
    if (!spawn_environment)
        spawn_environment = inherited_environment;
    /* An unset PATH uses this implementation's default.  An explicitly
     * empty PATH is one empty component, which searches the child cwd. */
    if (!search_path)
        search_path = "/bin:/sys/bin:/sys/apps";
    validation = _rin_posix_spawn_bounded_string_length(
        search_path, _RIN_POSIX_SPAWN_ENV_PATH_CAPACITY, &search_path_length);
    if (validation != 0) {
        free(path_storage);
        return __rin_unistd_environment_finish(
            validation, environment_epoch);
    }
    candidate = (char*)malloc(_RIN_POSIX_SPAWN_EXEC_PATH_CAPACITY);
    if (!candidate) {
        free(path_storage);
        return __rin_unistd_environment_finish(ENOMEM, environment_epoch);
    }
    segment_begin = search_path;

    for (;;) {
        const char* segment_end = segment_begin;
        size_t dir_len;
        size_t required;
        int result;

        while (*segment_end && *segment_end != ':')
            ++segment_end;
        dir_len = (size_t)(segment_end - segment_begin);
        required = dir_len + (dir_len ? 1u : 0u) + file_len + 1u;
        if (required <= _RIN_POSIX_SPAWN_EXEC_PATH_CAPACITY) {
            size_t offset = 0;
            if (dir_len) {
                memcpy(candidate, segment_begin, dir_len);
                offset = dir_len;
                candidate[offset++] = '/';
            }
            memcpy(candidate + offset, file, file_len + 1u);
            result = _rin_posix_spawn_direct(
                pid, candidate, file_actions, attr, argv, spawn_environment);
            if (result == 0) {
                free(candidate);
                free(path_storage);
                return __rin_unistd_environment_finish(
                    0, environment_epoch);
            }
            if (result == EACCES) {
                saved_error = EACCES;
            } else if (result != ENOENT && result != ENOTDIR) {
                free(candidate);
                free(path_storage);
                return __rin_unistd_environment_finish(
                    result, environment_epoch);
            }
        } else if (saved_error == ENOENT) {
            saved_error = ENAMETOOLONG;
        }
        if (!*segment_end)
            break;
        segment_begin = segment_end + 1;
    }
    free(candidate);
    free(path_storage);
    return __rin_unistd_environment_finish(
        saved_error, environment_epoch);
}

#ifdef __cplusplus
}
#endif

#endif /* _SPAWN_H */

#endif /* hosted Linux delegates to its native spawn provider */
