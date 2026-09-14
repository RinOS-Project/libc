/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - unistd.h
 * POSIX標準定義
 */

#ifndef _UNISTD_H
#define _UNISTD_H

#include "stddef.h"
#include "stdint.h"
#include "stdarg.h"
#include "limits.h"
#include "errno.h"
#include "stdlib.h"
#include "sys/syscall.h"
#include "fcntl.h"
#include "sys/stat.h"
#include "rin_account_compat.h"
#include <rin/fs/path_at_abi.h>
#include <rin/utsname_abi.h>
#include <rin/process/process_group_abi.h>
#include <rin/process/namespace_abi.h>
#include <rin/process/image_path_abi.h>

/* The allocator is linked by the target libc/rincrt image.  Keep this weak
 * so hosted syscall fixtures can include unistd.h without pulling allocator
 * implementation into the fixture. */
extern void rin_user_allocator_after_fork_child(void)
    __attribute__((weak));

/* Preserve caller-provided syscall hooks before installing defaults.  A
 * hosted unit that supplies a hook is intentionally asking for the Rin
 * frontend even when the compiler reports a hosted environment. */
#if defined(_RIN_UNISTD_SYSCALL0) || defined(_RIN_UNISTD_SYSCALL1) || \
    defined(_RIN_UNISTD_SYSCALL2) || defined(_RIN_UNISTD_SYSCALL3) || \
    defined(_RIN_UNISTD_SYSCALL5)
#define RIN_UNISTD_CUSTOM_SYSCALL_HOOK 1
#endif

#ifndef _RIN_UNISTD_SYSCALL0
#define _RIN_UNISTD_SYSCALL0(number) \
    _syscall0((uintptr_t)(number))
#endif
#ifndef _RIN_UNISTD_SYSCALL1
#define _RIN_UNISTD_SYSCALL1(number, argument1) \
    _syscall1((uintptr_t)(number), (uintptr_t)(argument1))
#endif
#ifndef _RIN_UNISTD_SYSCALL3
#define _RIN_UNISTD_SYSCALL3(number, argument1, argument2, argument3) \
    _syscall3((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3))
#endif
#ifndef _RIN_UNISTD_SYSCALL2
#define _RIN_UNISTD_SYSCALL2(number, argument1, argument2) \
    _syscall2((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2))
#endif
#ifndef _RIN_UNISTD_SYSCALL5
#define _RIN_UNISTD_SYSCALL5(number, argument1, argument2, argument3, argument4, argument5) \
    _syscall5((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3), \
              (uintptr_t)(argument4), (uintptr_t)(argument5))
#endif

#ifndef _RIN_UNISTD_SLEEP
#define _RIN_UNISTD_SLEEP(milliseconds) \
    _RIN_UNISTD_SYSCALL1(SYS_SLEEP, (uintptr_t)(milliseconds))
#endif

#ifndef _RIN_UNISTD_EXEC_SYSCALL3
#define _RIN_UNISTD_EXEC_SYSCALL3(number, argument1, argument2, argument3) \
    _RIN_UNISTD_SYSCALL3((number), (uintptr_t)(argument1), \
                         (uintptr_t)(argument2), (uintptr_t)(argument3))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* The process/exec frontend is a target-side syscall surface.  Hosted
 * consumers may include this header after a host pthread/process header,
 * whose external declarations own the same POSIX names.  Keep the Rin
 * wrappers for freestanding builds (and explicit RIN_FREESTANDING targets),
 * while letting hosted C runtimes provide their own process ABI. */
#if (defined(RIN_FREESTANDING) && RIN_FREESTANDING) || \
    !defined(__STDC_HOSTED__) || !__STDC_HOSTED__ || \
    defined(RIN_UNISTD_CUSTOM_SYSCALL_HOOK)
#define RIN_UNISTD_TARGET_PROCESS_FRONTEND 1
#endif

#ifndef __RIN_ENVIRON_DECLARED
#define __RIN_ENVIRON_DECLARED
extern char** environ;
#endif

/* ═══════════════════════════════════════════════════════════════
 * 型定義
 * ═══════════════════════════════════════════════════════════════*/

static inline intptr_t _rin_unistd_result(intptr_t result) {
    result = __rin_syscall_posixize(result);
    if (result < 0) {
        if (result != -1) errno = EIO;
        return -1;
    }
    return result;
}

static inline int _rin_unistd_status(intptr_t result) {
    result = _rin_unistd_result(result);
    if (result < 0) return -1;
    if (result != 0) {
        errno = (uintptr_t)result > (uintptr_t)INT_MAX ? EOVERFLOW : EIO;
        return -1;
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * 定数
 * ═══════════════════════════════════════════════════════════════*/

/* シーク位置 */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* 標準ファイルディスクリプタ */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* アクセスモード */
#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1

/* ═══════════════════════════════════════════════════════════════
 * sysconf定数
 * ═══════════════════════════════════════════════════════════════*/

#define _SC_ARG_MAX             0   /* execの引数最大長 */
#define _SC_CHILD_MAX           1   /* プロセスあたりの子プロセス最大数 */
#define _SC_CLK_TCK             2   /* クロック刻み/秒 */
#define _SC_NGROUPS_MAX         3   /* グループID最大数 */
#define _SC_OPEN_MAX            4   /* オープンファイル最大数 */
#define _SC_STREAM_MAX          5   /* ストリーム最大数 */
#define _SC_TZNAME_MAX          6   /* タイムゾーン名最大長 */
#define _SC_JOB_CONTROL         7   /* ジョブ制御サポート */
#define _SC_SAVED_IDS           8   /* 保存ID */
#define _SC_REALTIME_SIGNALS    9   /* リアルタイムシグナル */
#define _SC_PRIORITY_SCHEDULING 10  /* 優先度スケジューリング */
#define _SC_TIMERS              11  /* タイマー */
#define _SC_ASYNCHRONOUS_IO     12  /* 非同期I/O */
#define _SC_PRIORITIZED_IO      13  /* 優先度付きI/O */
#define _SC_SYNCHRONIZED_IO     14  /* 同期I/O */
#define _SC_FSYNC               15  /* fsyncサポート */
#define _SC_MAPPED_FILES        16  /* メモリマップドファイル */
#define _SC_MEMLOCK             17  /* メモリロック */
#define _SC_MEMLOCK_RANGE       18  /* メモリロック範囲 */
#define _SC_MEMORY_PROTECTION   19  /* メモリ保護 */
#define _SC_MESSAGE_PASSING     20  /* メッセージパッシング */
#define _SC_SEMAPHORES          21  /* セマフォ */
#define _SC_SHARED_MEMORY_OBJECTS 22 /* 共有メモリ */
#define _SC_AIO_LISTIO_MAX      23  /* AIOリスト最大 */
#define _SC_AIO_MAX             24  /* AIO最大 */
#define _SC_AIO_PRIO_DELTA_MAX  25  /* AIO優先度差最大 */
#define _SC_DELAYTIMER_MAX      26  /* 遅延タイマー最大 */
#define _SC_MQ_OPEN_MAX         27  /* メッセージキュー最大 */
#define _SC_MQ_PRIO_MAX         28  /* メッセージキュー優先度最大 */
#define _SC_VERSION             29  /* POSIXバージョン */
#define _SC_PAGESIZE            30  /* ページサイズ */
#define _SC_PAGE_SIZE           _SC_PAGESIZE
#define _SC_RTSIG_MAX           31  /* リアルタイムシグナル最大 */
#define _SC_SEM_NSEMS_MAX       32  /* セマフォ数最大 */
#define _SC_SEM_VALUE_MAX       33  /* セマフォ値最大 */
#define _SC_SIGQUEUE_MAX        34  /* シグナルキュー最大 */
#define _SC_TIMER_MAX           35  /* タイマー最大 */
#define _SC_BC_BASE_MAX         36  /* bc基数最大 */
#define _SC_BC_DIM_MAX          37  /* bc配列次元最大 */
#define _SC_BC_SCALE_MAX        38  /* bcスケール最大 */
#define _SC_BC_STRING_MAX       39  /* bc文字列最大 */
#define _SC_COLL_WEIGHTS_MAX    40  /* 照合重み最大 */
#define _SC_EXPR_NEST_MAX       41  /* 式のネスト最大 */
#define _SC_LINE_MAX            42  /* 行最大長 */
#define _SC_RE_DUP_MAX          43  /* 正規表現繰り返し最大 */
#define _SC_2_VERSION           44  /* POSIX.2バージョン */
#define _SC_2_C_DEV             45  /* C開発サポート */
#define _SC_2_FORT_DEV          46  /* Fortran開発サポート */
#define _SC_2_FORT_RUN          47  /* Fortranランタイム */
#define _SC_2_LOCALEDEF         48  /* localedefサポート */
#define _SC_2_SW_DEV            49  /* ソフトウェア開発 */
#define _SC_NPROCESSORS_CONF    83  /* 設定されたプロセッサ数 */
#define _SC_NPROCESSORS_ONLN    84  /* オンラインプロセッサ数 */
#define _SC_PHYS_PAGES          85  /* 物理ページ数 */
#define _SC_AVPHYS_PAGES        86  /* 利用可能物理ページ数 */
#define _SC_HOST_NAME_MAX       180 /* ホスト名最大長 */
#define _SC_LOGIN_NAME_MAX      71  /* ログイン名最大長 */
#define _SC_TTY_NAME_MAX        72  /* TTY名最大長 */

/* pathconf/fpathconf定数 */
#define _PC_LINK_MAX            0
#define _PC_MAX_CANON           1
#define _PC_MAX_INPUT           2
#define _PC_NAME_MAX            3
#define _PC_PATH_MAX            4
#define _PC_PIPE_BUF            5
#define _PC_CHOWN_RESTRICTED    6
#define _PC_NO_TRUNC            7
#define _PC_VDISABLE            8

/* confstr constants */
#define _CS_PATH                0

/* ═══════════════════════════════════════════════════════════════
 * ファイル操作
 * ═══════════════════════════════════════════════════════════════*/

static inline ssize_t read(int fd, void* buf, size_t count) {
    intptr_t result = _rin_unistd_result(_RIN_UNISTD_SYSCALL3(
        SYS_READ, (uintptr_t)(intptr_t)fd, (uintptr_t)buf, (uintptr_t)count));
    if (result < 0) return -1;
    if ((uintptr_t)result > (uintptr_t)count) {
        errno = EIO;
        return -1;
    }
    if ((uintptr_t)result > (uintptr_t)SSIZE_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (ssize_t)result;
}

static inline ssize_t write(int fd, const void* buf, size_t count) {
    intptr_t result = _rin_unistd_result(_RIN_UNISTD_SYSCALL3(
        SYS_WRITE, (uintptr_t)(intptr_t)fd, (uintptr_t)buf, (uintptr_t)count));
    if (result < 0) return -1;
    if ((uintptr_t)result > (uintptr_t)count) {
        errno = EIO;
        return -1;
    }
    if ((uintptr_t)result > (uintptr_t)SSIZE_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (ssize_t)result;
}

/* The wire ABI keeps a 64-bit offset as two explicit 32-bit words so the
 * same syscall contract is exact on i386 and x86_64.  Large libc requests
 * are split at the kernel's bounded-transfer limit without changing the
 * open-file description position. */
#define _RIN_UNISTD_POSITIONAL_IO_MAX 16777216u

static inline ssize_t _rin_unistd_positional_io(int fd, void const* buffer,
                                                 size_t count, off_t offset,
                                                 int is_write) {
    uint64_t base_offset;
    size_t total = 0;

    if (offset < 0 || count > (size_t)SSIZE_MAX) {
        errno = EINVAL;
        return -1;
    }
    base_offset = (uint64_t)offset;

    do {
        size_t chunk = count - total;
        uint64_t current_offset;
        intptr_t result;

        if (chunk > _RIN_UNISTD_POSITIONAL_IO_MAX)
            chunk = _RIN_UNISTD_POSITIONAL_IO_MAX;
        if (base_offset > (uint64_t)LONG_MAX - (uint64_t)total) {
            errno = EOVERFLOW;
            return total ? (ssize_t)total : -1;
        }
        current_offset = base_offset + (uint64_t)total;
        result = _rin_unistd_result(_RIN_UNISTD_SYSCALL5(
            is_write ? SYS_PWRITE : SYS_PREAD,
            (uintptr_t)(intptr_t)fd, (uintptr_t)buffer + total,
            (uintptr_t)chunk, (uintptr_t)(uint32_t)current_offset,
            (uintptr_t)(uint32_t)(current_offset >> 32)));
        if (result < 0)
            return total ? (ssize_t)total : -1;
        if ((uintptr_t)result > (uintptr_t)chunk) {
            errno = EIO;
            return -1;
        }
        total += (size_t)result;
        if ((size_t)result < chunk)
            break;
    } while (total < count);

    return (ssize_t)total;
}

static inline ssize_t pread(int fd, void* buf, size_t count, off_t offset) {
    return _rin_unistd_positional_io(fd, buf, count, offset, 0);
}

static inline ssize_t pwrite(int fd, const void* buf, size_t count,
                             off_t offset) {
    return _rin_unistd_positional_io(fd, buf, count, offset, 1);
}

static inline int close(int fd) {
    int rc = _rin_unistd_status(_RIN_UNISTD_SYSCALL1(
        SYS_CLOSE, (uintptr_t)(intptr_t)fd));
    if (rc == 0) {
        _fcntl_track_close_fd(fd);
    }
    return rc;
}

static inline off_t lseek(int fd, off_t offset, int whence) {
    intptr_t result = _rin_unistd_result(_RIN_UNISTD_SYSCALL3(
        SYS_SEEK, (uintptr_t)(intptr_t)fd, (uintptr_t)(intptr_t)offset,
        (uintptr_t)(intptr_t)whence));
    if (result < 0) return (off_t)-1;
    if ((uintptr_t)result > (uintptr_t)LONG_MAX) {
        errno = EOVERFLOW;
        return (off_t)-1;
    }
    return (off_t)result;
}

static inline int dup(int oldfd) {
    intptr_t raw = _rin_unistd_result(_RIN_UNISTD_SYSCALL1(
        SYS_DUP, (uintptr_t)(intptr_t)oldfd));
    int rc;
    if (raw < 0) return -1;
    if ((uintptr_t)raw > (uintptr_t)INT_MAX) {
        (void)_RIN_UNISTD_SYSCALL1(SYS_CLOSE, (uintptr_t)raw);
        errno = EOVERFLOW;
        return -1;
    }
    rc = (int)raw;
    _fcntl_init();
    if (rc >= 0 && rc < _FCNTL_MAX_FDS && oldfd >= 0 && oldfd < _FCNTL_MAX_FDS) {
        _fcntl_fl_flags[rc] = _fcntl_fl_flags[oldfd];
        _fcntl_fd_flags[rc] = 0;
    }
    return rc;
}

static inline int dup2(int oldfd, int newfd) {
    intptr_t raw = _rin_unistd_result(_RIN_UNISTD_SYSCALL2(
        SYS_DUP2, (uintptr_t)(intptr_t)oldfd, (uintptr_t)(intptr_t)newfd));
    int rc;
    if (raw < 0) return -1;
    if ((uintptr_t)raw > (uintptr_t)INT_MAX) {
        (void)_RIN_UNISTD_SYSCALL1(SYS_CLOSE, (uintptr_t)raw);
        errno = EOVERFLOW;
        return -1;
    }
    rc = (int)raw;
    _fcntl_init();
    if (rc >= 0 && rc < _FCNTL_MAX_FDS && oldfd >= 0 && oldfd < _FCNTL_MAX_FDS) {
        if (oldfd != newfd) {
            _fcntl_fl_flags[rc] = _fcntl_fl_flags[oldfd];
            _fcntl_fd_flags[rc] = 0;
        }
    }
    return rc;
}

static inline int pipe(int pipefd[2]) {
    int rc = _rin_unistd_status(_RIN_UNISTD_SYSCALL2(
        SYS_PIPE, (uintptr_t)pipefd, 0u));
    if (rc == 0 && pipefd) {
        _fcntl_track_new_fd(pipefd[0], O_RDONLY, 0);
        _fcntl_track_new_fd(pipefd[1], O_WRONLY, 0);
    }
    return rc;
}

static inline int pipe2(int pipefd[2], int flags) {
    int rc = _rin_unistd_status(_RIN_UNISTD_SYSCALL2(
        SYS_PIPE, (uintptr_t)pipefd, (uintptr_t)(intptr_t)flags));
    if (rc == 0 && pipefd) {
        int status_flags = flags & O_NONBLOCK;
        int fd_flags = (flags & O_CLOEXEC) ? FD_CLOEXEC : 0;
        _fcntl_track_new_fd(pipefd[0], O_RDONLY | status_flags, fd_flags);
        _fcntl_track_new_fd(pipefd[1], O_WRONLY | status_flags, fd_flags);
    }
    return rc;
}

/* ═══════════════════════════════════════════════════════════════
 * ファイルシステム操作
 * ═══════════════════════════════════════════════════════════════*/

static inline int access(const char* pathname, int mode) {
    intptr_t result;
    if (!pathname) {
        errno = EFAULT;
        return -1;
    }
    if ((mode & ~7) != 0) {
        errno = EINVAL;
        return -1;
    }
    result = _RIN_UNISTD_SYSCALL2(SYS_ACCESS, (uintptr_t)pathname,
                                  (uintptr_t)(unsigned int)mode);
    return _rin_unistd_status(result);
}

static inline int chdir(const char* path) {
    return _rin_unistd_status(_RIN_UNISTD_SYSCALL1(SYS_CHDIR, (uintptr_t)path));
}

static inline char* getcwd(char* buf, size_t size) {
    intptr_t ret;
    if (!buf || size == 0u) {
        errno = EINVAL;
        return NULL;
    }
    ret = _rin_unistd_result(_RIN_UNISTD_SYSCALL2(
        SYS_GETCWD, (uintptr_t)buf, (uintptr_t)size));
    if (ret < 0) return NULL;
    /* The kernel returns the target buffer address on success and zero on
     * failure.  Require the exact pointer token so a malformed positive word
     * cannot turn an unsuccessful or unrelated copy-out into success. */
    if ((uintptr_t)ret != (uintptr_t)buf) {
        errno = EIO;
        return NULL;
    }
    return buf;
}

/* readlink is declared here because realpath performs the component walk
 * before the full readlink wrapper section below. */
static inline ssize_t readlink(const char* pathname, char* buf, size_t bufsiz);

/* realpath - canonicalize a target path without relying on a host libc.
 *
 * The kernel exposes lstat/readlink/getcwd as the primitive path operations,
 * so the libc implementation performs the POSIX component walk here.  A
 * symlink is resolved relative to the directory containing the link; the
 * bounded expansion count prevents cyclic links from consuming unbounded
 * memory.  Both caller-provided PATH_MAX storage and the POSIX NULL-output
 * form are supported. */
static inline size_t _rin_realpath_length(const char* value) {
    size_t length = 0u;
    while (value[length] != '\0') ++length;
    return length;
}

static inline int _rin_realpath_copy(char* destination, size_t capacity,
                                     const char* source) {
    size_t length = _rin_realpath_length(source);
    if (length + 1u > capacity) {
        errno = ENAMETOOLONG;
        return -1;
    }
    for (size_t index = 0u; index <= length; ++index)
        destination[index] = source[index];
    return 0;
}

/* Return the authenticated launch/exec image path owned by the kernel.  This
 * is deliberately not implemented through argv[0], a raw PID, or /proc. */
static inline int rin_process_image_path_get(
    RinProcessImagePathResponseV1* response) {
    intptr_t result;
    if (response == NULL) {
        errno = EINVAL;
        return -1;
    }
    result = _rin_unistd_result(_RIN_UNISTD_SYSCALL2(
        SYS_PROCESS_IMAGE_PATH_GET, (uintptr_t)response,
        (uintptr_t)sizeof(*response)));
    return result < 0 ? -1 : 0;
}

static inline char* realpath(const char* path, char* resolved_path) {
    enum { RIN_REALPATH_MAX_SYMLINKS = 40, RIN_REALPATH_PENDING_CAPACITY = PATH_MAX * 2 };
    char* pending = NULL;
    char* next_pending = NULL;
    char resolved[PATH_MAX];
    char component[NAME_MAX + 1u];
    char link_target[PATH_MAX];
    char candidate[PATH_MAX];
    size_t cursor;
    size_t resolved_length;
    unsigned int symlink_count = 0u;

    if (!path || path[0] == '\0') {
        errno = path ? ENOENT : EFAULT;
        return NULL;
    }

    pending = (char*)malloc(RIN_REALPATH_PENDING_CAPACITY);
    if (!pending) {
        errno = ENOMEM;
        return NULL;
    }

    if (path[0] == '/') {
        if (_rin_realpath_copy(pending, RIN_REALPATH_PENDING_CAPACITY, path) < 0)
            goto fail;
    } else {
        char cwd[PATH_MAX];
        size_t cwd_length;
        size_t path_length = _rin_realpath_length(path);
        if (!getcwd(cwd, sizeof(cwd))) goto fail;
        cwd_length = _rin_realpath_length(cwd);
        if (cwd_length + 1u + path_length + 1u > RIN_REALPATH_PENDING_CAPACITY) {
            errno = ENAMETOOLONG;
            goto fail;
        }
        for (size_t index = 0u; index < cwd_length; ++index)
            pending[index] = cwd[index];
        pending[cwd_length] = '/';
        for (size_t index = 0u; index <= path_length; ++index)
            pending[cwd_length + 1u + index] = path[index];
    }

    resolved[0] = '/';
    resolved[1] = '\0';
    resolved_length = 1u;
    cursor = 0u;

    for (;;) {
        size_t component_length = 0u;
        size_t component_start;
        struct stat status;

        while (pending[cursor] == '/') ++cursor;
        if (pending[cursor] == '\0') break;
        component_start = cursor;
        while (pending[cursor] != '/' && pending[cursor] != '\0') ++cursor;
        component_length = cursor - component_start;
        if (component_length > NAME_MAX) {
            errno = ENAMETOOLONG;
            goto fail;
        }
        for (size_t index = 0u; index < component_length; ++index)
            component[index] = pending[component_start + index];
        component[component_length] = '\0';

        if (component_length == 1u && component[0] == '.') continue;
        if (component_length == 2u && component[0] == '.' && component[1] == '.') {
            while (resolved_length > 1u && resolved[resolved_length - 1u] != '/')
                --resolved_length;
            if (resolved_length > 1u) --resolved_length;
            resolved[resolved_length] = '\0';
            continue;
        }

        if (resolved_length > 1u) {
            if (resolved_length + 1u + component_length + 1u > sizeof(candidate)) {
                errno = ENAMETOOLONG;
                goto fail;
            }
            for (size_t index = 0u; index < resolved_length; ++index)
                candidate[index] = resolved[index];
            candidate[resolved_length] = '/';
            for (size_t index = 0u; index <= component_length; ++index)
                candidate[resolved_length + 1u + index] = component[index];
        } else {
            if (component_length + 2u > sizeof(candidate)) {
                errno = ENAMETOOLONG;
                goto fail;
            }
            candidate[0] = '/';
            for (size_t index = 0u; index <= component_length; ++index)
                candidate[1u + index] = component[index];
        }

        if (lstat(candidate, &status) < 0) goto fail;
        if (S_ISLNK(status.st_mode)) {
            ssize_t link_length;
            size_t remaining_length = _rin_realpath_length(pending + cursor);
            size_t prefix_length = (pending[component_start] == '/') ? 0u : resolved_length;
            if (++symlink_count > RIN_REALPATH_MAX_SYMLINKS) {
                errno = ELOOP;
                goto fail;
            }
            link_length = readlink(candidate, link_target, sizeof(link_target) - 1u);
            if (link_length < 0) goto fail;
            if ((size_t)link_length >= sizeof(link_target)) {
                errno = ENAMETOOLONG;
                goto fail;
            }
            link_target[link_length] = '\0';
            next_pending = (char*)malloc(RIN_REALPATH_PENDING_CAPACITY);
            if (!next_pending) {
                errno = ENOMEM;
                goto fail;
            }
            if (link_target[0] == '/') {
                prefix_length = 0u;
            }
            if (prefix_length + (prefix_length ? 1u : 0u) +
                    (size_t)link_length + (remaining_length ? 1u : 0u) +
                    remaining_length + 1u > RIN_REALPATH_PENDING_CAPACITY) {
                errno = ENAMETOOLONG;
                goto fail;
            }
            {
                size_t output = 0u;
                if (prefix_length != 0u) {
                    for (size_t index = 0u; index < prefix_length; ++index)
                        next_pending[output++] = resolved[index];
                    next_pending[output++] = '/';
                }
                for (size_t index = 0u; index < (size_t)link_length; ++index)
                    next_pending[output++] = link_target[index];
                if (remaining_length != 0u) {
                    next_pending[output++] = '/';
                    for (size_t index = 0u; index < remaining_length; ++index)
                        next_pending[output++] = pending[cursor + index];
                }
                next_pending[output] = '\0';
            }
            free(pending);
            pending = next_pending;
            next_pending = NULL;
            cursor = 0u;
            continue;
        }

        resolved_length = _rin_realpath_length(candidate);
        if (_rin_realpath_copy(resolved, sizeof(resolved), candidate) < 0) goto fail;
    }

    free(pending);
    if (resolved_path) {
        if (_rin_realpath_copy(resolved_path, PATH_MAX, resolved) < 0) return NULL;
        return resolved_path;
    }
    resolved_path = (char*)malloc(resolved_length + 1u);
    if (!resolved_path) {
        errno = ENOMEM;
        return NULL;
    }
    _rin_realpath_copy(resolved_path, resolved_length + 1u, resolved);
    return resolved_path;

fail:
    free(next_pending);
    free(pending);
    return NULL;
}

/* mkdir() is defined in sys/stat.h (POSIX standard location) */
#ifndef __MKDIR_DEFINED
#define __MKDIR_DEFINED
static inline int mkdir(const char* pathname, unsigned int mode) {
    return _rin_unistd_status(_RIN_UNISTD_SYSCALL2(
        SYS_MKDIR, (uintptr_t)pathname, (uintptr_t)mode));
}
#endif

static inline int rmdir(const char* pathname) {
    return _rin_unistd_status(_RIN_UNISTD_SYSCALL1(SYS_RMDIR, (uintptr_t)pathname));
}

static inline int unlink(const char* pathname) {
    return _rin_unistd_status(_RIN_UNISTD_SYSCALL1(SYS_UNLINK, (uintptr_t)pathname));
}

/* rename() は stdio.h で定義 (ANSI C) */

/* ═══════════════════════════════════════════════════════════════
 * プロセス制御
 * ═══════════════════════════════════════════════════════════════*/

#if defined(RIN_UNISTD_TARGET_PROCESS_FRONTEND)
static inline pid_t getpid(void) {
    intptr_t result = _rin_unistd_result(_RIN_UNISTD_SYSCALL0(SYS_GETPID));
    if (result < 0) return -1;
    if (result == 0) {
        errno = EIO;
        return -1;
    }
    if ((uintptr_t)result > (uintptr_t)INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (pid_t)result;
}

/* Return the namespace-visible ID of the calling thread.  The kernel owns
 * the process/thread generation translation; libc only validates the raw
 * result so zero or an out-of-range positive word cannot be exposed as a
 * successful POSIX pid_t. */
static inline pid_t gettid(void) {
    intptr_t result = _rin_unistd_result(_RIN_UNISTD_SYSCALL0(SYS_GETTID));
    if (result < 0) return (pid_t)-1;
    if (result == 0) {
        errno = EIO;
        return (pid_t)-1;
    }
    if ((uintptr_t)result > (uintptr_t)INT_MAX) {
        errno = EOVERFLOW;
        return (pid_t)-1;
    }
    return (pid_t)result;
}

static inline pid_t getppid(void) {
    intptr_t result = _rin_unistd_result(_RIN_UNISTD_SYSCALL0(SYS_GETPPID));
    if (result < 0) return -1;
    if ((uintptr_t)result > (uintptr_t)INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (pid_t)result;
}

/* Linux-compatible clear_child_tid registration.  The kernel stores the
 * pointer on the calling thread, clears it, and wakes a futex waiter during
 * thread exit.  NULL disables the exit-time clear/wake operation. */
static inline pid_t set_tid_address(int* clear_child_tid) {
    intptr_t result = _rin_unistd_result(_RIN_UNISTD_SYSCALL1(
        SYS_set_tid_address, (uintptr_t)clear_child_tid));
    if (result < 0) return (pid_t)-1;
    if ((uintptr_t)result > (uintptr_t)INT_MAX) {
        errno = EOVERFLOW;
        return (pid_t)-1;
    }
    return (pid_t)result;
}


/* Session/process-group operations use one fixed-width request/response
 * transaction so native, compat32, and standalone IA-32 portals share the
 * same generation and namespace checks. */
static inline int __rin_process_group_call(uint16_t operation,
                                           pid_t target_pid,
                                           pid_t process_group_id,
                                           RinProcessGroupSyscallResponseV1* response)
{
    RinProcessGroupSyscallRequestV1 request = {
        0u, 0u, 0u, 0u, 0u, 0u, 0u, {0u, 0u, 0u, 0u, 0u}
    };
    intptr_t result;
    if (response == NULL) {
        errno = EFAULT;
        return -1;
    }
    response->struct_size = 0u;
    request.struct_size = sizeof(request);
    request.version = RIN_PROCESS_GROUP_SYSCALL_ABI_VERSION;
    request.operation = operation;
    if (target_pid < 0 || process_group_id < 0) {
        errno = EINVAL;
        return -1;
    }
    request.target_pid = (uint32_t)target_pid;
    request.process_group_id = (uint32_t)process_group_id;
    result = _rin_unistd_result(_RIN_UNISTD_SYSCALL2(
        SYS_PROCESS_GROUP_CALL, (uintptr_t)&request,
        (uintptr_t)response));
    return result < 0 ? -1 : 0;
}

static inline pid_t getpgrp(void)
{
    RinProcessGroupSyscallResponseV1 response;
    if (__rin_process_group_call(RIN_PROCESS_GROUP_OP_GETPGRP, 0, 0,
                                 &response) != 0)
        return (pid_t)-1;
    if (response.process_group_id > (uint32_t)INT_MAX) {
        errno = EOVERFLOW;
        return (pid_t)-1;
    }
    return (pid_t)response.process_group_id;
}

static inline pid_t getpgid(pid_t pid)
{
    RinProcessGroupSyscallResponseV1 response;
    if (__rin_process_group_call(RIN_PROCESS_GROUP_OP_GETPGRP, pid, 0,
                                 &response) != 0)
        return (pid_t)-1;
    if (response.process_group_id > (uint32_t)INT_MAX) {
        errno = EOVERFLOW;
        return (pid_t)-1;
    }
    return (pid_t)response.process_group_id;
}

static inline int setpgid(pid_t pid, pid_t pgid)
{
    RinProcessGroupSyscallResponseV1 response;
    return __rin_process_group_call(RIN_PROCESS_GROUP_OP_SETPGID, pid, pgid,
                                    &response);
}

static inline pid_t getsid(pid_t pid)
{
    RinProcessGroupSyscallResponseV1 response;
    if (__rin_process_group_call(RIN_PROCESS_GROUP_OP_GETSID, pid, 0,
                                 &response) != 0)
        return (pid_t)-1;
    if (response.session_id > (uint32_t)INT_MAX) {
        errno = EOVERFLOW;
        return (pid_t)-1;
    }
    return (pid_t)response.session_id;
}

static inline pid_t setsid(void)
{
    RinProcessGroupSyscallResponseV1 response;
    if (__rin_process_group_call(RIN_PROCESS_GROUP_OP_SETSID, 0, 0,
                                 &response) != 0)
        return (pid_t)-1;
    if (response.session_id > (uint32_t)INT_MAX) {
        errno = EOVERFLOW;
        return (pid_t)-1;
    }
    return (pid_t)response.session_id;
}

static inline pid_t fork(void) {
    intptr_t result = _rin_unistd_result(_RIN_UNISTD_SYSCALL0(SYS_FORK));
    if (result < 0) return -1;
    if ((uintptr_t)result > (uintptr_t)INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    if (result == 0 && rin_user_allocator_after_fork_child != NULL)
        rin_user_allocator_after_fork_child();
    return (pid_t)result;
}

/* Return the generation-stable identity cookie for a process visible in the
 * caller's process namespace.  CoreCLR uses this as its transport/semaphore
 * disambiguation key because RinOS deliberately has no procfs start-time
 * file.  The kernel performs the namespace and generation lookup; libc only
 * validates the fixed-width public ABI and never falls back to a raw PID. */
static inline int rin_process_instance_cookie(uint32_t local_pid,
                                              uint64_t* cookie_out) {
    RinProcessNamespaceInspectRequestV1 request;
    RinProcessNamespaceInspectResponseV1 response;
    intptr_t result;
    uint32_t index;

    if (!cookie_out || local_pid == 0u) {
        errno = EINVAL;
        return -1;
    }
    request.struct_size = sizeof(request);
    request.version = RIN_PROCESS_NAMESPACE_INSPECT_ABI_VERSION;
    request.flags = 0u;
    request.local_pid = local_pid;
    request.reserved0 = 0u;
    for (index = 0u; index < 4u; ++index) request.reserved[index] = 0u;
    for (index = 0u; index < sizeof(response); ++index)
        ((unsigned char*)&response)[index] = 0u;

    result = _rin_unistd_result(_RIN_UNISTD_SYSCALL2(
        SYS_PROCESS_NAMESPACE_INSPECT, (uintptr_t)&request,
        (uintptr_t)&response));
    if (result < 0) return -1;
    if (result != 0 || response.struct_size != sizeof(response) ||
        response.version != RIN_PROCESS_NAMESPACE_INSPECT_ABI_VERSION ||
        response.local_pid != local_pid || response.target_process_id != local_pid ||
        response.target_process_instance_cookie == 0u) {
        errno = EIO;
        return -1;
    }
    *cookie_out = response.target_process_instance_cookie;
    return 0;
}

static inline int execve(const char* pathname, char* const argv[], char* const envp[]) {
    intptr_t result;
    if (!pathname || !argv) {
        errno = EINVAL;
        return -1;
    }
    if (pathname[0] == '\0') {
        errno = ENOENT;
        return -1;
    }
    result = _RIN_UNISTD_EXEC_SYSCALL3(
        SYS_EXEC, (uintptr_t)pathname, (uintptr_t)argv, (uintptr_t)envp);
    return _rin_unistd_result(result) < 0 ? -1 : 0;
}

#ifndef _RIN_UNISTD_EXECVE
#define _RIN_UNISTD_EXECVE(pathname, arguments, environment) \
    execve((pathname), (arguments), (environment))
#endif

#if defined(RIN_UNISTD_TARGET_PROCESS_FRONTEND)
__attribute__((noreturn)) static inline void _exit(int status) {
    (void)_RIN_UNISTD_SYSCALL1(SYS_EXIT, (uintptr_t)(intptr_t)status);
    for(;;) {} /* Never returns */
}
#endif

#ifndef _RIN_WAIT_DEFINED
#define _RIN_WAIT_DEFINED
#ifndef _RIN_WAIT_SYSCALL3
#define _RIN_WAIT_SYSCALL3(number, argument1, argument2, argument3) \
    _RIN_UNISTD_SYSCALL3((number), (argument1), (argument2), (argument3))
#endif
static inline pid_t wait(int* wstatus) {
    intptr_t result = _rin_unistd_result(_RIN_WAIT_SYSCALL3(
        SYS_WAIT, (uintptr_t)(intptr_t)-1, (uintptr_t)wstatus, 0u));
    if (result < 0) return -1;
    if ((uintptr_t)result > (uintptr_t)INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (pid_t)result;
}
#endif /* _RIN_WAIT_DEFINED */

#endif /* RIN_UNISTD_TARGET_PROCESS_FRONTEND */

/* ═══════════════════════════════════════════════════════════════
 * ユーザー/グループ ID
 * ═══════════════════════════════════════════════════════════════*/

static inline int __rin_get_credentials(__rin_credentials_v1* credentials) {
    int error = __rin_credentials_get(credentials);
    if (error != 0) {
        errno = error;
        return -1;
    }
    return 0;
}

static inline uid_t getuid(void) {
    __rin_credentials_v1 credentials;
    return __rin_get_credentials(&credentials) == 0
        ? (uid_t)credentials.uid : (uid_t)-1;
}

static inline uid_t geteuid(void) {
    __rin_credentials_v1 credentials;
    return __rin_get_credentials(&credentials) == 0
        ? (uid_t)credentials.effective_uid : (uid_t)-1;
}

static inline gid_t getgid(void) {
    __rin_credentials_v1 credentials;
    return __rin_get_credentials(&credentials) == 0
        ? (gid_t)credentials.gid : (gid_t)-1;
}

static inline gid_t getegid(void) {
    __rin_credentials_v1 credentials;
    return __rin_get_credentials(&credentials) == 0
        ? (gid_t)credentials.effective_gid : (gid_t)-1;
}

static inline int __rin_set_credential(uint16_t operation, uint32_t value) {
    __rin_credential_set_v1 request;
    __rin_account_zero(&request, sizeof(request));
    request.struct_size = sizeof(request);
    request.version = __RIN_CREDENTIAL_SET_VERSION;
    request.operation = operation;
    request.value = value;
    return _rin_unistd_status(_RIN_CREDENTIAL_SYSCALL1(
        RIN_SYS_CREDENTIALS_SET, &request));
}

static inline int setuid(uid_t uid) {
#if !defined(RIN_SYS_TYPES_UNSIGNED_ID)
    if ((int64_t)uid < 0) {
        errno = EINVAL;
        return -1;
    }
#else
    (void)uid;
#endif
    return __rin_set_credential(__RIN_CREDENTIAL_SET_UID, (uint32_t)uid);
}

static inline int setgid(gid_t gid) {
#if !defined(RIN_SYS_TYPES_UNSIGNED_ID)
    if ((int64_t)gid < 0) {
        errno = EINVAL;
        return -1;
    }
#else
    (void)gid;
#endif
    return __rin_set_credential(__RIN_CREDENTIAL_SET_GID, (uint32_t)gid);
}

/* ═══════════════════════════════════════════════════════════════
 * スリープ関連
 * ═══════════════════════════════════════════════════════════════*/

static inline unsigned int sleep(unsigned int seconds) {
    unsigned long long remaining_ms = (unsigned long long)seconds * 1000ULL;

    while (remaining_ms != 0) {
        /* SYS_SLEEP is a uint32_t millisecond ABI on both x86 targets. */
        unsigned long chunk = remaining_ms > 0xffffffffULL
            ? 0xffffffffUL : (unsigned long)remaining_ms;
        intptr_t result = _RIN_UNISTD_SLEEP(chunk);
        if (result != 0) {
            if (result < 0) {
                if (__rin_syscall_posixize(result) != -1) errno = EIO;
            } else {
                errno = EIO;
            }
            return (unsigned int)((remaining_ms + 999ULL) / 1000ULL);
        }
        remaining_ms -= (unsigned long long)chunk;
    }
    return 0;
}

static inline int usleep(useconds_t usec) {
    intptr_t result;
    if (usec >= 1000000U) {
        errno = EINVAL;
        return -1;
    }
    if (usec == 0) return 0;

    /* マイクロ秒をミリ秒に変換 (最小1ms) */
    uintptr_t ms = (uintptr_t)(usec / 1000u);
    if (ms == 0) ms = 1;
    result = _RIN_UNISTD_SLEEP(ms);
    if (result < 0) {
        if (__rin_syscall_posixize(result) != -1) errno = EIO;
        return -1;
    }
    if (result > 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * その他
 * ═══════════════════════════════════════════════════════════════*/

static inline int isatty(int fd) {
    struct stat st;
    if (fstat(fd, &st) < 0) return 0;
    return S_ISCHR(st.st_mode) ? 1 : 0;
}

static inline long sysconf(int name) {
    switch (name) {
        case _SC_ARG_MAX:
            return ARG_MAX;
        case _SC_CHILD_MAX:
            return CHILD_MAX;
        case _SC_PAGESIZE:
            return 4096;
        case _SC_CLK_TCK:
            return 1000;  /* 1000 Hz (1ms tick) */
        case _SC_OPEN_MAX:
            return OPEN_MAX;
        case _SC_STREAM_MAX:
            return 16;
        case _SC_LINE_MAX:
            return 2048;
        case _SC_HOST_NAME_MAX:
            return HOST_NAME_MAX;
        case _SC_LOGIN_NAME_MAX:
            return LOGIN_NAME_MAX;
        case _SC_NGROUPS_MAX:
            return NGROUPS_MAX;
        case _SC_TZNAME_MAX:
            return TZNAME_MAX;
        case _SC_TTY_NAME_MAX:
            /* ttyname() publishes /dev/tty0..2 through a 16-byte TLS
             * buffer; expose that actual device-name bound. */
            return 16;
        case _SC_VERSION:
            return 200809L;
        case _SC_NPROCESSORS_CONF:
        case _SC_NPROCESSORS_ONLN: {
            intptr_t count = _rin_unistd_result(_RIN_UNISTD_SYSCALL0(
                SYS_GETCPU_COUNT));
            if (count < 0) return -1;
            if (count == 0) {
                errno = EIO;
                return -1;
            }
            if ((uintptr_t)count > (uintptr_t)LONG_MAX) {
                errno = EOVERFLOW;
                return -1;
            }
            return (long)count;
        }
        case _SC_PHYS_PAGES: {
            /* カーネルのSYS_SYSINFOから実メモリ量を取得 */
            struct { unsigned long long uptime; unsigned long long total_ram;
                     unsigned long long free_ram; unsigned int procs; } si;
            uintptr_t pages;
            if (_rin_unistd_status(_RIN_UNISTD_SYSCALL1(
                    SYS_SYSINFO, (uintptr_t)&si)) < 0)
                return -1;
            /* A successful provider must publish a complete, coherent
             * snapshot.  Treat an empty total or free > total as an error
             * instead of exposing a fabricated page count. */
            if (si.total_ram == 0 || si.free_ram > si.total_ram) {
                errno = EIO;
                return -1;
            }
            pages = (uintptr_t)(si.total_ram / 4096u);
            if (pages > (uintptr_t)LONG_MAX) {
                errno = EOVERFLOW;
                return -1;
            }
            return (long)pages;
        }
        case _SC_AVPHYS_PAGES: {
            struct { unsigned long long uptime; unsigned long long total_ram;
                     unsigned long long free_ram; unsigned int procs; } si;
            uintptr_t pages;
            if (_rin_unistd_status(_RIN_UNISTD_SYSCALL1(
                    SYS_SYSINFO, (uintptr_t)&si)) < 0)
                return -1;
            if (si.total_ram == 0 || si.free_ram > si.total_ram) {
                errno = EIO;
                return -1;
            }
            pages = (uintptr_t)(si.free_ram / 4096u);
            if (pages > (uintptr_t)LONG_MAX) {
                errno = EOVERFLOW;
                return -1;
            }
            return (long)pages;
        }
        case _SC_JOB_CONTROL:
            /* Process groups, sessions, controlling TTY ownership and the
             * signal paths used for job control are kernel-owned. */
            return 1;
        case _SC_PRIORITY_SCHEDULING:
            return 1;
        case _SC_TIMERS:
            return 1;
        case _SC_SEMAPHORES:
            return 1;
        case _SC_MEMLOCK:
            return 1;
        case _SC_SAVED_IDS:
        case _SC_REALTIME_SIGNALS:
        case _SC_ASYNCHRONOUS_IO:
        case _SC_PRIORITIZED_IO:
        case _SC_SHARED_MEMORY_OBJECTS:
        case _SC_MESSAGE_PASSING:
        case _SC_AIO_LISTIO_MAX:
        case _SC_AIO_MAX:
        case _SC_AIO_PRIO_DELTA_MAX:
        case _SC_DELAYTIMER_MAX:
        case _SC_MQ_OPEN_MAX:
        case _SC_MQ_PRIO_MAX:
        case _SC_RTSIG_MAX:
        case _SC_SEM_NSEMS_MAX:
        case _SC_SEM_VALUE_MAX:
        case _SC_SIGQUEUE_MAX:
        case _SC_TIMER_MAX:
        case _SC_BC_BASE_MAX:
        case _SC_BC_DIM_MAX:
        case _SC_BC_SCALE_MAX:
        case _SC_BC_STRING_MAX:
        case _SC_COLL_WEIGHTS_MAX:
        case _SC_EXPR_NEST_MAX:
        case _SC_RE_DUP_MAX:
        case _SC_2_VERSION:
        case _SC_2_C_DEV:
        case _SC_2_FORT_DEV:
        case _SC_2_FORT_RUN:
        case _SC_2_LOCALEDEF:
        case _SC_2_SW_DEV:
            errno = ENOSYS;
            return -1;
        case _SC_SYNCHRONIZED_IO:
        case _SC_FSYNC:
        case _SC_MAPPED_FILES:
        case _SC_MEMLOCK_RANGE:
        case _SC_MEMORY_PROTECTION:
            return 200809L;
        default:
            errno = EINVAL;
            return -1;
    }
}

static inline int getpagesize(void) {
    return 4096;
}

static inline long pathconf(const char* path, int name) {
    struct stat status;
    if (!path) {
        errno = EINVAL;
        return -1;
    }
    if (stat(path, &status) < 0) return -1;
    switch (name) {
        case _PC_NAME_MAX:
            return 255;
        case _PC_PATH_MAX:
            return 4096;
        case _PC_PIPE_BUF:
            return 4096;
        case _PC_CHOWN_RESTRICTED:
            return 1;
        case _PC_NO_TRUNC:
            return 1;
        case _PC_MAX_CANON:
        case _PC_MAX_INPUT:
            if (!S_ISCHR(status.st_mode)) {
                errno = EINVAL;
                return -1;
            }
            return 256;
        case _PC_VDISABLE:
            if (!S_ISCHR(status.st_mode)) {
                errno = EINVAL;
                return -1;
            }
            return 0;
        case _PC_LINK_MAX:
            /* RinFS persists i_links as uint32_t and rejects the next link
             * with EMLINK before incrementing.  A 32-bit long cannot carry
             * UINT32_MAX, so expose the largest representable value there. */
            return UINT32_MAX > (uintmax_t)LONG_MAX
                ? LONG_MAX : (long)UINT32_MAX;
        default:
            errno = EINVAL;
            return -1;
    }
}

static inline long fpathconf(int fd, int name) {
    struct stat status;
    if (fstat(fd, &status) < 0) return -1;
    switch (name) {
        case _PC_NAME_MAX:
            return 255;
        case _PC_PATH_MAX:
            return 4096;
        case _PC_PIPE_BUF:
            return 4096;
        case _PC_CHOWN_RESTRICTED:
        case _PC_NO_TRUNC:
            return 1;
        case _PC_MAX_CANON:
        case _PC_MAX_INPUT:
            if (!S_ISCHR(status.st_mode)) {
                errno = EINVAL;
                return -1;
            }
            return 256;
        case _PC_VDISABLE:
            if (!S_ISCHR(status.st_mode)) {
                errno = EINVAL;
                return -1;
            }
            return 0;
        case _PC_LINK_MAX:
            return UINT32_MAX > (uintmax_t)LONG_MAX
                ? LONG_MAX : (long)UINT32_MAX;
        default:
            errno = EINVAL;
            return -1;
    }
}

static inline size_t confstr(int name, char* buf, size_t len) {
    static const char path[] = "/bin:/sys/bin:/sys/apps";
    const char* value;
    size_t required;
    if (name != _CS_PATH) {
        errno = EINVAL;
        return 0u;
    }
    value = path;
    required = sizeof(path);
    if (len != 0u) {
        size_t copy_count;
        if (!buf) {
            errno = EFAULT;
            return 0u;
        }
        copy_count = required < len ? required : len;
        for (size_t index = 0u; index + 1u < copy_count; ++index)
            buf[index] = value[index];
        buf[copy_count - 1u] = '\0';
    }
    return required;
}

/* gethostname - ホスト名取得 */
static inline int gethostname(char* name, size_t len) {
    RinUtsnameAbiV1 identity;
    size_t size = 0;
    if (!name || len == 0) {
        errno = EINVAL;
        return -1;
    }
    if (_rin_unistd_status(_RIN_UNISTD_SYSCALL1(
            SYS_UNAME, (uintptr_t)&identity)) < 0)
        return -1;
    while (size < RIN_UTSNAME_ABI_V1_FIELD_SIZE && identity.nodename[size])
        ++size;
    if (size == RIN_UTSNAME_ABI_V1_FIELD_SIZE) {
        errno = EIO;
        return -1;
    }
    if (size + 1u > len) {
        errno = ENAMETOOLONG;
        return -1;
    }
    for (size_t index = 0; index <= size; ++index)
        name[index] = identity.nodename[index];
    return 0;
}

static inline int sethostname(const char* name, size_t len) {
    if (len > (size_t)(RIN_UTSNAME_ABI_V1_FIELD_SIZE - 1u) ||
        (len != 0u && name == NULL)) {
        errno = len != 0u && name == NULL ? EFAULT : EINVAL;
        return -1;
    }
    return _rin_unistd_status(_RIN_UNISTD_SYSCALL2(
        SYS_SETHOSTNAME, (uintptr_t)name, (uintptr_t)len));
}

/* setdomainname - update the authenticated kernel UTS domain identity. */
static inline int setdomainname(const char* name, size_t len) {
    if (len > (size_t)(RIN_UTSNAME_ABI_V1_FIELD_SIZE - 1u) ||
        (len != 0u && name == NULL)) {
        errno = len != 0u && name == NULL ? EFAULT : EINVAL;
        return -1;
    }
    return _rin_unistd_status(_RIN_UNISTD_SYSCALL2(
        SYS_SETDOMAINNAME, (uintptr_t)name, (uintptr_t)len));
}

/* getdomainname - ドメイン名取得 */
static inline int getdomainname(char* name, size_t len) {
    RinUtsnameAbiV1 identity;
    size_t size = 0;
    if (!name || len == 0) {
        errno = EINVAL;
        return -1;
    }
    if (_rin_unistd_status(_RIN_UNISTD_SYSCALL1(
            SYS_UNAME, (uintptr_t)&identity)) < 0)
        return -1;
    while (size < RIN_UTSNAME_ABI_V1_FIELD_SIZE && identity.domainname[size])
        ++size;
    if (size == RIN_UTSNAME_ABI_V1_FIELD_SIZE) {
        errno = EIO;
        return -1;
    }
    if (size + 1u > len) {
        errno = ENAMETOOLONG;
        return -1;
    }
    for (size_t index = 0; index <= size; ++index)
        name[index] = identity.domainname[index];
    return 0;
}

/* ttyname - TTY名取得
 *
 * The kernel's stdio owner exposes an authenticated character-device
 * identity: descriptors 0..2 have st_rdev equal to their public descriptor
 * number.  Resolve only that exact identity.  A random character device is
 * not enough evidence for a TTY name, so it remains ENOTTY instead of being
 * turned into a guessed path. */
static inline int _rin_ttyname_resolve(int fd, char* output, size_t capacity) {
    struct stat status;
    const char* name;
    size_t length;
    size_t index;

    if (fd < 0) return EBADF;
    if (fstat(fd, &status) < 0) return errno ? errno : EIO;
    if (!S_ISCHR(status.st_mode) || fd > 2 ||
        status.st_rdev != (dev_t)fd)
        return ENOTTY;

    name = (fd == 0) ? "/dev/tty0" :
           (fd == 1) ? "/dev/tty1" : "/dev/tty2";
    length = 0;
    while (name[length] != '\0') ++length;
    if (!output || capacity <= length) return ERANGE;
    for (index = 0; index <= length; ++index) output[index] = name[index];
    return 0;
}

static inline char* ttyname(int fd) {
#if defined(__cplusplus)
    static thread_local char buffer[16];
#else
    static _Thread_local char buffer[16];
#endif
    int saved_errno = errno;
    int error = _rin_ttyname_resolve(fd, buffer, sizeof(buffer));
    if (error != 0) {
        errno = error;
        return NULL;
    }
    errno = saved_errno;
    return buffer;
}

static inline int ttyname_r(int fd, char* buf, size_t buflen) {
    int saved_errno = errno;
    int error;
    if (!buf) return EINVAL;
    if (buflen == 0) return ERANGE;
    error = _rin_ttyname_resolve(fd, buf, buflen);
    errno = saved_errno;
    return error;
}

/* alarm - アラーム設定 */
static inline unsigned int alarm(unsigned int seconds) {
    intptr_t result = _rin_unistd_result(_RIN_UNISTD_SYSCALL1(
        SYS_ALARM, (uintptr_t)seconds));
    if (result < 0) return 0;
    if ((uintptr_t)result > (uintptr_t)UINT_MAX) {
        errno = EOVERFLOW;
        return 0;
    }
    return (unsigned int)result;
}

/* pause - シグナルを待つ
 *
 * The syscall blocks the current generation-bound thread until a catchable
 * signal is queued.  A successful return is malformed (pause has no success
 * value), so it is converted to EIO instead of being exposed as success. */
static inline int pause(void) {
    intptr_t result = _rin_unistd_result(_RIN_UNISTD_SYSCALL0(SYS_PAUSE));
    if (result >= 0) errno = EIO;
    return -1;
}

/* link/symlink - リンク作成 */
static inline int link(const char* oldpath, const char* newpath) {
    return _rin_unistd_status(_RIN_UNISTD_SYSCALL2(
        SYS_LINK, (uintptr_t)oldpath, (uintptr_t)newpath));
}

static inline int symlink(const char* target, const char* linkpath) {
    return _rin_unistd_status(_RIN_UNISTD_SYSCALL2(
        SYS_SYMLINK, (uintptr_t)target, (uintptr_t)linkpath));
}

static inline ssize_t readlink(const char* pathname, char* buf, size_t bufsiz) {
    intptr_t result = _rin_unistd_result(_RIN_UNISTD_SYSCALL3(
        SYS_READLINK, (uintptr_t)pathname, (uintptr_t)buf, (uintptr_t)bufsiz));
    if (result < 0) return -1;
    if ((uintptr_t)result > (uintptr_t)bufsiz) {
        errno = EIO;
        return -1;
    }
    if ((uintptr_t)result > (uintptr_t)SSIZE_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (ssize_t)result;
}

/* truncate/ftruncate - ファイルサイズ変更 */
static inline int truncate(const char* path, off_t length) {
    return _rin_unistd_status(_RIN_UNISTD_SYSCALL2(
        SYS_TRUNCATE, (uintptr_t)path, (uintptr_t)(intptr_t)length));
}

static inline int ftruncate(int fd, off_t length) {
    return _rin_unistd_status(_RIN_UNISTD_SYSCALL2(
        SYS_FTRUNCATE, (uintptr_t)(intptr_t)fd,
        (uintptr_t)(intptr_t)length));
}

/* chown/fchown/lchown - 所有者変更 */
static inline int chown(const char* pathname, uid_t owner, gid_t group) {
    intptr_t result;
    if (!pathname) {
        errno = EFAULT;
        return -1;
    }
    result = _RIN_UNISTD_SYSCALL3(SYS_CHOWN, (uintptr_t)pathname,
                                  (uint32_t)owner, (uint32_t)group);
    return _rin_unistd_status(result);
}

static inline int fchown(int fd, uid_t owner, gid_t group) {
    intptr_t result;
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    result = _RIN_UNISTD_SYSCALL3(SYS_FCHOWN, (uintptr_t)(intptr_t)fd,
                                  (uint32_t)owner, (uint32_t)group);
    return _rin_unistd_status(result);
}

static inline int lchown(const char* pathname, uid_t owner, gid_t group) {
    intptr_t result;
    if (!pathname) {
        errno = EFAULT;
        return -1;
    }
    result = _RIN_UNISTD_SYSCALL3(SYS_LCHOWN, (uintptr_t)pathname,
                                  (uint32_t)owner, (uint32_t)group);
    return _rin_unistd_status(result);
}

/* sync/fsync/fdatasync - 同期 */
static inline void sync(void) {
    (void)_RIN_UNISTD_SYSCALL0(SYS_SYNC);
}

static inline int fsync(int fd) {
    return _rin_unistd_status(_RIN_UNISTD_SYSCALL1(
        SYS_FSYNC, (uintptr_t)(intptr_t)fd));
}

static inline int fdatasync(int fd) {
    return fsync(fd);
}

/* dup3 - フラグ付きdup2 */
static inline int dup3(int oldfd, int newfd, int flags) {
    if (oldfd == newfd) {
        errno = EINVAL;
        return -1;
    }
    if ((flags & ~O_CLOEXEC) != 0) {
        errno = EINVAL;
        return -1;
    }
    return _rin_unistd_status(_RIN_UNISTD_SYSCALL3(
        SYS_DUP3, (uintptr_t)(intptr_t)oldfd,
        (uintptr_t)(intptr_t)newfd, (uintptr_t)flags));
}

/* exec frontend.  These bounds match the kernel's pointer-free snapshot. */
#if defined(RIN_UNISTD_TARGET_PROCESS_FRONTEND)
#define __RIN_EXEC_ARGUMENT_COUNT_MAX 64u
#define __RIN_EXEC_PATH_MAX 512u
#define __RIN_EXEC_ENVIRONMENT_COUNT_MAX 128u
#define __RIN_EXEC_ENVIRONMENT_STRING_MAX 4096u

static inline int __rin_exec_collect_arguments(
    const char* first, va_list* arguments,
    char* output[__RIN_EXEC_ARGUMENT_COUNT_MAX + 1u]) {
    const char* current = first;
    size_t count = 0u;
    while (current) {
        if (count == __RIN_EXEC_ARGUMENT_COUNT_MAX) {
            errno = E2BIG;
            return -1;
        }
        output[count++] = (char*)current;
        current = va_arg(*arguments, const char*);
    }
    output[count] = NULL;
    return 0;
}

static inline int __rin_exec_text_length(
    const char* text, size_t limit, size_t* length) {
    size_t index = 0u;
    if (!text || !length) return 0;
    while (index < limit && text[index] != '\0') ++index;
    if (index == limit) return 0;
    *length = index;
    return 1;
}

static inline const char* __rin_exec_search_path(int* error) {
    size_t entry_index;
    if (error) *error = 0;
    if (!environ) return NULL;
    for (entry_index = 0u;
         entry_index < __RIN_EXEC_ENVIRONMENT_COUNT_MAX;
         ++entry_index) {
        const char* entry = environ[entry_index];
        size_t length;
        if (!entry) return NULL;
        if (!__rin_exec_text_length(
                entry, __RIN_EXEC_ENVIRONMENT_STRING_MAX, &length)) {
            if (error) *error = E2BIG;
            return NULL;
        }
        if (length >= 5u && entry[0] == 'P' && entry[1] == 'A' &&
            entry[2] == 'T' && entry[3] == 'H' && entry[4] == '=') {
            return entry + 5u;
        }
    }
    if (error) *error = E2BIG;
    return NULL;
}

static inline int __rin_exec_shell_fallback(
    const char* path, char* const argv[]) {
    char* shell_arguments[__RIN_EXEC_ARGUMENT_COUNT_MAX + 1u];
    size_t input_count = 0u;
    size_t output_count = 2u;
    if (!argv) {
        errno = EFAULT;
        return -1;
    }
    while (input_count < __RIN_EXEC_ARGUMENT_COUNT_MAX && argv[input_count])
        ++input_count;
    if (input_count == __RIN_EXEC_ARGUMENT_COUNT_MAX || input_count > 63u) {
        errno = E2BIG;
        return -1;
    }
    shell_arguments[0] = (char*)"sh";
    shell_arguments[1] = (char*)path;
    for (size_t index = 1u; index < input_count; ++index)
        shell_arguments[output_count++] = argv[index];
    shell_arguments[output_count] = NULL;
    return _RIN_UNISTD_EXECVE("/bin/sh", shell_arguments, environ);
}

static inline int __rin_execvp(const char* file, char* const argv[]) {
    static const char default_path[] = "/bin:/sys/bin:/sys/apps";
    const char* search_path;
    const char* segment;
    size_t file_length;
    int path_error = 0;
    int saved_error = ENOENT;
    char candidate[__RIN_EXEC_PATH_MAX];

    if (!file || !argv) {
        errno = EINVAL;
        return -1;
    }
    if (file[0] == '\0') {
        errno = ENOENT;
        return -1;
    }
    if (!__rin_exec_text_length(file, __RIN_EXEC_PATH_MAX, &file_length)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    for (size_t index = 0u; index < file_length; ++index) {
        if (file[index] == '/') {
            int result = _RIN_UNISTD_EXECVE(file, argv, environ);
            if (result < 0 && errno == ENOEXEC)
                return __rin_exec_shell_fallback(file, argv);
            return result;
        }
    }

    search_path = __rin_exec_search_path(&path_error);
    if (path_error != 0) {
        errno = path_error;
        return -1;
    }
    if (!search_path) search_path = default_path;
    if (!__rin_exec_text_length(
            search_path, __RIN_EXEC_ENVIRONMENT_STRING_MAX, &file_length)) {
        errno = E2BIG;
        return -1;
    }
    segment = search_path;
    for (;;) {
        const char* end = segment;
        size_t directory_length;
        size_t name_length;
        size_t required;
        size_t offset = 0u;
        int result;
        while (*end && *end != ':') ++end;
        directory_length = (size_t)(end - segment);
        if (!__rin_exec_text_length(file, __RIN_EXEC_PATH_MAX, &name_length)) {
            errno = ENAMETOOLONG;
            return -1;
        }
        required = directory_length + (directory_length ? 1u : 0u) +
                   name_length + 1u;
        if (required <= sizeof(candidate)) {
            for (size_t index = 0u; index < directory_length; ++index)
                candidate[offset++] = segment[index];
            if (directory_length) candidate[offset++] = '/';
            for (size_t index = 0u; index <= name_length; ++index)
                candidate[offset++] = file[index];
            result = _RIN_UNISTD_EXECVE(candidate, argv, environ);
            if (result < 0 && errno == ENOEXEC)
                return __rin_exec_shell_fallback(candidate, argv);
            if (result < 0 && (errno == ENOENT || errno == ENOTDIR)) {
                /* Continue searching. */
            } else if (result < 0 && errno == EACCES) {
                saved_error = EACCES;
            } else {
                return result;
            }
        } else if (saved_error == ENOENT) {
            saved_error = ENAMETOOLONG;
        }
        if (*end == '\0') break;
        segment = end + 1;
    }
    errno = saved_error;
    return -1;
}

static inline int execl(const char* pathname, const char* arg, ...) {
    char* arguments[__RIN_EXEC_ARGUMENT_COUNT_MAX + 1u];
    va_list list;
    int result;
    if (!pathname || !arg) {
        errno = EINVAL;
        return -1;
    }
    va_start(list, arg);
    result = __rin_exec_collect_arguments(arg, &list, arguments);
    va_end(list);
    if (result != 0) return -1;
    return _RIN_UNISTD_EXECVE(pathname, arguments, environ);
}

static inline int execlp(const char* file, const char* arg, ...) {
    char* arguments[__RIN_EXEC_ARGUMENT_COUNT_MAX + 1u];
    va_list list;
    int result;
    if (!file || !arg) {
        errno = EINVAL;
        return -1;
    }
    va_start(list, arg);
    result = __rin_exec_collect_arguments(arg, &list, arguments);
    va_end(list);
    if (result != 0) return -1;
    return __rin_execvp(file, arguments);
}

static inline int execle(const char* pathname, const char* arg, ...) {
    char* arguments[__RIN_EXEC_ARGUMENT_COUNT_MAX + 1u];
    char* const* environment;
    va_list list;
    int result;
    if (!pathname || !arg) {
        errno = EINVAL;
        return -1;
    }
    va_start(list, arg);
    result = __rin_exec_collect_arguments(arg, &list, arguments);
    if (result == 0)
        environment = va_arg(list, char* const*);
    else
        environment = NULL;
    va_end(list);
    if (result != 0) return -1;
    return _RIN_UNISTD_EXECVE(pathname, arguments, environment);
}

static inline int execv(const char* pathname, char* const argv[]) {
    return _RIN_UNISTD_EXECVE(pathname, argv, environ);
}

static inline int execvp(const char* file, char* const argv[]) {
    return __rin_execvp(file, argv);
}
#endif /* RIN_UNISTD_TARGET_PROCESS_FRONTEND */

/* getopt - コマンドライン引数解析 (簡易版) */
extern char* optarg;
extern int optind;
extern int opterr;
extern int optopt;

/* sbrk - ヒープポインタ操作
 * NOTE: noinline to prevent compiler optimization issues with syscalls */
__attribute__((unused, noinline)) static void* sbrk(long increment) {
    intptr_t result;

    /* The heap break is process-owned kernel state.  Use the relative owner
     * rather than caching a translation-unit-local value and issuing a racy
     * read/modify/write pair through SYS_BRK. */
    result = _rin_unistd_result(_RIN_UNISTD_SYSCALL1(
        SYS_SBRK, (uintptr_t)(intptr_t)increment));
    if (result < 0) return (void*)-1;
    return (void*)(uintptr_t)result;
}

#ifdef __cplusplus
}
#endif

#undef RIN_UNISTD_TARGET_PROCESS_FRONTEND

#endif /* _UNISTD_H */
