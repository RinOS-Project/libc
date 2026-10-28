/*
 * RinOS libc - sys/utsname.h
 * システム識別情報
 */

#ifndef _SYS_UTSNAME_H
#define _SYS_UTSNAME_H

#include "syscall.h"
#include "../errno.h"
#include "../limits.h"
#include <rin/utsname_abi.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * utsname構造体
 * ═══════════════════════════════════════════════════════════════*/

#define _UTSNAME_LENGTH RIN_UTSNAME_ABI_V1_FIELD_SIZE

struct utsname {
    RIN_UTSNAME_ABI_V1_FIELDS;
};

#ifdef __cplusplus
static_assert(sizeof(struct utsname) == sizeof(RinUtsnameAbiV1),
              "SYS_UNAME ABI layout mismatch");
#else
_Static_assert(sizeof(struct utsname) == sizeof(RinUtsnameAbiV1),
               "SYS_UNAME ABI layout mismatch");
#endif

/* ═══════════════════════════════════════════════════════════════
 * uname関数
 * ═══════════════════════════════════════════════════════════════*/

#ifndef _RIN_UTSNAME_SYSCALL1
#define _RIN_UTSNAME_SYSCALL1(number, output) \
    _syscall1((uintptr_t)(number), (uintptr_t)(output))
#endif

/* uname exposes an int status but receives a target-width syscall word. */
static inline int __rin_uname_status_result(intptr_t result) {
    result = __rin_syscall_posixize(result);
    if (result < 0) {
        if (result != -1) errno = EIO;
        return -1;
    }
    if (result > (intptr_t)INT_MAX || result < (intptr_t)INT_MIN) {
        errno = EOVERFLOW;
        return -1;
    }
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

static inline int uname(struct utsname* buf) {
    if (!buf) {
        errno = EFAULT;
        return -1;
    }

    /* SYS_UNAMEシステムコールを呼び出す */
    return __rin_uname_status_result(_RIN_UTSNAME_SYSCALL1(SYS_UNAME, buf));
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_UTSNAME_H */
