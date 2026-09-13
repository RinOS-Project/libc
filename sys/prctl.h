/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - sys/prctl.h
 * Process control - full implementation for RinOS
 */

#ifndef _SYS_PRCTL_H
#define _SYS_PRCTL_H

#include "../stddef.h"
#include "../stdint.h"
#include "../errno.h"
#include "../sys/syscall.h"
#include "../../../src/shared/rin_thread_name_abi.h"

#ifndef _RIN_PRCTL_NAME_SYSCALL1
#define _RIN_PRCTL_NAME_SYSCALL1(number, argument) \
    _syscall1((uintptr_t)(number), (uintptr_t)(argument))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * Process Control Options
 * ═══════════════════════════════════════════════════════════════*/

#define PR_SET_PDEATHSIG    1   /* Set signal on parent death */
#define PR_GET_PDEATHSIG    2   /* Get signal on parent death */
#define PR_GET_DUMPABLE     3   /* Get dumpable flag */
#define PR_SET_DUMPABLE     4   /* Set dumpable flag */
#define PR_GET_UNALIGN      5   /* Get unaligned access control */
#define PR_SET_UNALIGN      6   /* Set unaligned access control */
#define PR_GET_KEEPCAPS     7   /* Get capability bounding set */
#define PR_SET_KEEPCAPS     8   /* Set capability bounding set */
#define PR_GET_FPEMU        9   /* Get floating-point emulation */
#define PR_SET_FPEMU        10  /* Set floating-point emulation */
#define PR_GET_FPEXC        11  /* Get floating-point exception mode */
#define PR_SET_FPEXC        12  /* Set floating-point exception mode */
#define PR_GET_TIMING       13  /* Get timing mode */
#define PR_SET_TIMING       14  /* Set timing mode */
#define PR_SET_NAME         15  /* Set process/thread name */
#define PR_GET_NAME         16  /* Get process/thread name */
#define PR_GET_ENDIAN       19  /* Get endian mode */
#define PR_SET_ENDIAN       20  /* Set endian mode */
#define PR_GET_SECCOMP      21  /* Get seccomp mode */
#define PR_SET_SECCOMP      22  /* Set seccomp mode */
#define PR_CAPBSET_READ     23  /* Read capability bounding set */
#define PR_CAPBSET_DROP     24  /* Drop capability from bounding set */
#define PR_GET_TSC          25  /* Get TSC access mode */
#define PR_SET_TSC          26  /* Set TSC access mode */
#define PR_GET_SECUREBITS   27  /* Get securebits */
#define PR_SET_SECUREBITS   28  /* Set securebits */
#define PR_SET_TIMERSLACK   29  /* Set timer slack */
#define PR_GET_TIMERSLACK   30  /* Get timer slack */
#define PR_TASK_PERF_EVENTS_DISABLE 31
#define PR_TASK_PERF_EVENTS_ENABLE  32
#define PR_MCE_KILL         33  /* Set machine check exception mode */
#define PR_MCE_KILL_GET     34  /* Get machine check exception mode */
#define PR_SET_MM           35  /* Modify memory management settings */
#define PR_SET_CHILD_SUBREAPER 36
#define PR_GET_CHILD_SUBREAPER 37
#define PR_SET_NO_NEW_PRIVS 38  /* Set no_new_privs flag */
#define PR_GET_NO_NEW_PRIVS 39  /* Get no_new_privs flag */
#define PR_GET_TID_ADDRESS  40  /* Get clear_child_tid address */
#define PR_SET_THP_DISABLE  41  /* Disable transparent huge pages */
#define PR_GET_THP_DISABLE  42  /* Get THP disable status */
#define PR_MPX_ENABLE_MANAGEMENT    43
#define PR_MPX_DISABLE_MANAGEMENT   44
#define PR_SET_FP_MODE      45  /* Set FP mode */
#define PR_GET_FP_MODE      46  /* Get FP mode */
#define PR_CAP_AMBIENT      47  /* Ambient capabilities */
#define PR_SVE_SET_VL       50  /* Set SVE vector length */
#define PR_SVE_GET_VL       51  /* Get SVE vector length */
#define PR_GET_SPECULATION_CTRL 52
#define PR_SET_SPECULATION_CTRL 53
#define PR_PAC_RESET_KEYS   54
#define PR_SET_TAGGED_ADDR_CTRL 55
#define PR_GET_TAGGED_ADDR_CTRL 56
#define PR_SET_IO_FLUSHER   57
#define PR_GET_IO_FLUSHER   58
#define PR_SET_SYSCALL_USER_DISPATCH 59
#define PR_PAC_SET_ENABLED_KEYS     60
#define PR_PAC_GET_ENABLED_KEYS     61
#define PR_SCHED_CORE       62
#define PR_SME_SET_VL       63
#define PR_SME_GET_VL       64
#define PR_SET_VMA          0x53564d41  /* Set VMA name */
#define PR_SET_PTRACER      0x59616d61  /* Yama: allow ptrace */

/* ═══════════════════════════════════════════════════════════════
 * Option-specific values
 * ═══════════════════════════════════════════════════════════════*/

/* PR_SET_VMA options */
#define PR_SET_VMA_ANON_NAME 0

/* PR_SET_PTRACER values */
#define PR_SET_PTRACER_ANY  ((unsigned long)-1)

/* Dumpable values */
#define SUID_DUMP_DISABLE   0
#define SUID_DUMP_USER      1
#define SUID_DUMP_ROOT      2

/* TSC control */
#define PR_TSC_ENABLE       1
#define PR_TSC_SIGSEGV      2

/* Unalign control */
#define PR_UNALIGN_NOPRINT  1
#define PR_UNALIGN_SIGBUS   2

/* Seccomp modes */
#define SECCOMP_MODE_DISABLED   0
#define SECCOMP_MODE_STRICT     1
#define SECCOMP_MODE_FILTER     2

/* MCE kill modes */
#define PR_MCE_KILL_CLEAR   0
#define PR_MCE_KILL_SET     1
#define PR_MCE_KILL_LATE    0
#define PR_MCE_KILL_EARLY   1
#define PR_MCE_KILL_DEFAULT 2

/* FP mode */
#define PR_FP_MODE_FR       (1 << 0)
#define PR_FP_MODE_FRE      (1 << 1)

/* Timing modes */
#define PR_TIMING_STATISTICAL   0
#define PR_TIMING_TIMESTAMP     1

/* Endian modes */
#define PR_ENDIAN_BIG       0
#define PR_ENDIAN_LITTLE    1
#define PR_ENDIAN_PPC_LITTLE 2

/* CAP_AMBIENT operations */
#define PR_CAP_AMBIENT_IS_SET    1
#define PR_CAP_AMBIENT_RAISE     2
#define PR_CAP_AMBIENT_LOWER     3
#define PR_CAP_AMBIENT_CLEAR_ALL 4

/* Speculation control */
#define PR_SPEC_STORE_BYPASS    0
#define PR_SPEC_INDIRECT_BRANCH 1
#define PR_SPEC_L1D_FLUSH       2

#define PR_SPEC_NOT_AFFECTED    0
#define PR_SPEC_PRCTL           (1UL << 0)
#define PR_SPEC_ENABLE          (1UL << 1)
#define PR_SPEC_DISABLE         (1UL << 2)
#define PR_SPEC_FORCE_DISABLE   (1UL << 3)
#define PR_SPEC_DISABLE_NOEXEC  (1UL << 4)

static inline int _rin_prctl_thread_name_call(RinThreadNameCallV1* request,
                                              int require_current_id) {
    intptr_t result;
    size_t terminator = RIN_THREAD_NAME_CAPACITY;
    if (!request) {
        errno = EINVAL;
        return -1;
    }
    result = (intptr_t)_RIN_PRCTL_NAME_SYSCALL1(SYS_THREAD_NAME_CALL, request);
    if (result < 0) {
        /* Keep the error magnitude in the target-width carrier.  An LLP64
         * `unsigned long` would truncate e.g. -(2^32 + 2) to -2 and expose
         * ENOENT instead of rejecting the malformed syscall result. */
        uintptr_t error = (uintptr_t)0 - (uintptr_t)result;
        errno = error != 0u && error <= (uintptr_t)4095u
                    ? (int)error : EIO;
        return -1;
    }
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    if (request->struct_size != sizeof(*request) ||
        request->version != RIN_THREAD_NAME_CALL_VERSION ||
        (request->operation != RIN_THREAD_NAME_OPERATION_SET &&
         request->operation != RIN_THREAD_NAME_OPERATION_GET) ||
        request->reserved[0] != 0u || request->reserved[1] != 0u ||
        (require_current_id && request->thread_id == 0u)) {
        errno = EIO;
        return -1;
    }
    if (request->operation == RIN_THREAD_NAME_OPERATION_GET) {
        for (size_t index = 0u; index < RIN_THREAD_NAME_CAPACITY; ++index) {
            if (request->name[index] == '\0') {
                terminator = index;
                break;
            }
        }
        if (terminator == RIN_THREAD_NAME_CAPACITY) {
            errno = EIO;
            return -1;
        }
        for (size_t index = terminator + 1u;
             index < RIN_THREAD_NAME_CAPACITY; ++index) {
            if (request->name[index] != '\0') {
                errno = EIO;
                return -1;
            }
        }
    }
    return 0;
}

static inline int _rin_prctl_set_name(uintptr_t argument) {
    const char* name = (const char*)(uintptr_t)argument;
    RinThreadNameCallV1 request;
    size_t length = 0u;
    if (!name) {
        errno = EINVAL;
        return -1;
    }
    for (size_t index = 0u; index < sizeof(request); ++index)
        ((unsigned char*)&request)[index] = 0u;
    while (length + 1u < RIN_THREAD_NAME_CAPACITY && name[length] != '\0')
        ++length;
    request.struct_size = (uint32_t)sizeof(request);
    request.version = RIN_THREAD_NAME_CALL_VERSION;
    request.operation = RIN_THREAD_NAME_OPERATION_SET;
    request.thread_id = 0u;
    for (size_t index = 0u; index < length; ++index)
        request.name[index] = name[index];
    return _rin_prctl_thread_name_call(&request, 0);
}

static inline int _rin_prctl_get_name(uintptr_t argument) {
    char* name = (char*)(uintptr_t)argument;
    RinThreadNameCallV1 request;
    if (!name) {
        errno = EINVAL;
        return -1;
    }
    for (size_t index = 0u; index < sizeof(request); ++index)
        ((unsigned char*)&request)[index] = 0u;
    request.struct_size = (uint32_t)sizeof(request);
    request.version = RIN_THREAD_NAME_CALL_VERSION;
    request.operation = RIN_THREAD_NAME_OPERATION_GET;
    request.thread_id = 0u;
    if (_rin_prctl_thread_name_call(&request, 1) != 0) return -1;
    for (size_t index = 0u; index < RIN_THREAD_NAME_CAPACITY; ++index)
        name[index] = request.name[index];
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * prctl() Implementation
 * ═══════════════════════════════════════════════════════════════*/

static inline int _prctl_impl(int option, uintptr_t arg2, uintptr_t arg3,
                              uintptr_t arg4, uintptr_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;

    /* Do not emulate process-security state in a per-translation-unit header.
     * Only architecture facts that this implementation can report truthfully
     * are accepted until a kernel prctl contract exists. */
    if (option == PR_SET_NAME) return _rin_prctl_set_name(arg2);
    if (option == PR_GET_NAME) return _rin_prctl_get_name(arg2);
    /* Seccomp mode is process state, not an architecture fact.  Until a
     * kernel-owned process security snapshot is available, do not report a
     * synthetic disabled mode to callers. */
    if (option == PR_GET_SECCOMP) {
        errno = ENOSYS;
        return -1;
    }
    if (option == PR_GET_ENDIAN) {
        if (!arg2) { errno = EINVAL; return -1; }
        *(int*)(uintptr_t)arg2 = PR_ENDIAN_LITTLE;
        return 0;
    }
    if (option == PR_GET_TSC) {
        if (!arg2) { errno = EINVAL; return -1; }
        *(int*)(uintptr_t)arg2 = PR_TSC_ENABLE;
        return 0;
    }
    errno = ENOSYS;
    return -1;
}

/* ═══════════════════════════════════════════════════════════════
 * prctl() wrapper - only the supported GET options consume an int pointer.
 * Keep that value as a target pointer-width word across the varargs boundary.
 * ═══════════════════════════════════════════════════════════════*/

static inline int prctl(int option, ...) {
    uintptr_t arg2 = 0;
    if (option == PR_GET_ENDIAN || option == PR_GET_TSC ||
        option == PR_SET_NAME || option == PR_GET_NAME) {
        __builtin_va_list args;
        __builtin_va_start(args, option);
        if (option == PR_GET_ENDIAN || option == PR_GET_TSC)
            arg2 = (uintptr_t)__builtin_va_arg(args, int*);
        else
            arg2 = __builtin_va_arg(args, uintptr_t);
        __builtin_va_end(args);
    }
    return _prctl_impl(option, arg2, 0, 0, 0);
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_PRCTL_H */
