/*
 * RinOS libc - dirent.h
 * ディレクトリ操作
 */

#ifndef _DIRENT_H
#define _DIRENT_H

#include "stddef.h"
#include "stdint.h"
#include "errno.h"
#include "limits.h"
#include "fcntl_flags.h"
#include "stdlib.h"
#include "sys/syscall.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * 定数
 * ═══════════════════════════════════════════════════════════════*/

#define NAME_MAX 255
#define DT_UNKNOWN  0
#define DT_FIFO     1
#define DT_CHR      2
#define DT_DIR      4
#define DT_BLK      6
#define DT_REG      8
#define DT_LNK      10
#define DT_SOCK     12
#define DT_WHT      14

/* ═══════════════════════════════════════════════════════════════
 * 構造体
 * ═══════════════════════════════════════════════════════════════*/

struct dirent {
    unsigned long  d_ino;           /* inode番号 */
    unsigned long  d_off;           /* 次のエントリへのオフセット */
    unsigned short d_reclen;        /* このエントリのサイズ */
    unsigned char  d_type;          /* ファイルタイプ */
    char           d_name[256];     /* ファイル名 */
};

typedef struct {
    int fd;                         /* ディレクトリのファイルディスクリプタ */
    struct dirent entry;            /* 現在のエントリ */
    int eof;                        /* 終端フラグ */
    unsigned int magic;             /* 有効なストリームか */
} DIR;

#define _RIN_DIR_MAGIC 0x52444952u

#ifndef _RIN_DIRENT_SYSCALL1
#define _RIN_DIRENT_SYSCALL1(number, arg1) \
    _syscall1((uintptr_t)(number), (uintptr_t)(arg1))
#endif
#ifndef _RIN_DIRENT_SYSCALL3
#define _RIN_DIRENT_SYSCALL3(number, arg1, arg2, arg3) \
    _syscall3((uintptr_t)(number), (uintptr_t)(arg1), \
              (uintptr_t)(arg2), (uintptr_t)(arg3))
#endif

static inline intptr_t _rin_dir_result(intptr_t result) {
    result = __rin_syscall_posixize(result);
    if (result < 0) {
        if (result != -1) errno = EIO;
        return -1;
    }
    return result;
}

static inline int _rin_dir_status_result(intptr_t result) {
    result = _rin_dir_result(result);
    if (result < 0) return -1;
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

static inline int _rin_dir_valid(const DIR* dirp) {
    return dirp && dirp->magic == _RIN_DIR_MAGIC && dirp->fd >= 0;
}

/* ═══════════════════════════════════════════════════════════════
 * 関数
 * ═══════════════════════════════════════════════════════════════*/

static inline DIR* opendir(const char* name) {
    DIR* dir;
    intptr_t fd;
    if (!name) {
        errno = EFAULT;
        return NULL;
    }

    /* ディレクトリをオープン */
    fd = _rin_dir_result(_RIN_DIRENT_SYSCALL3(
        SYS_OPEN, (uintptr_t)name,
        (uintptr_t)(O_RDONLY | O_DIRECTORY), 0));
    if (fd < 0) return NULL;
    if (fd > (intptr_t)INT_MAX) {
        (void)_RIN_DIRENT_SYSCALL1(SYS_CLOSE, (uintptr_t)fd);
        errno = EOVERFLOW;
        return NULL;
    }

    dir = (DIR*)malloc(sizeof(*dir));
    if (!dir) {
        int saved_errno = ENOMEM;
        (void)_RIN_DIRENT_SYSCALL1(SYS_CLOSE, (uintptr_t)fd);
        errno = saved_errno;
        return NULL;
    }
    dir->fd = (int)fd;
    dir->eof = 0;
    dir->magic = _RIN_DIR_MAGIC;
    return dir;
}

/* Transfer ownership of an already-open directory descriptor into the
 * caller-owned DIR stream.  WASI fd_readdir duplicates its retained VFS fd
 * before using this boundary, so closing the stream cannot close the WASI
 * descriptor itself. */
static inline DIR* fdopendir(int fd) {
    DIR* dir;
    if (fd < 0) {
        errno = EBADF;
        return NULL;
    }
    dir = (DIR*)malloc(sizeof(*dir));
    if (!dir) {
        errno = ENOMEM;
        return NULL;
    }
    dir->fd = fd;
    dir->eof = 0;
    dir->magic = _RIN_DIR_MAGIC;
    return dir;
}

static inline int closedir(DIR* dirp) {
    intptr_t result;
    int saved_errno = 0;
    if (!_rin_dir_valid(dirp)) {
        errno = EBADF;
        return -1;
    }

    result = _rin_dir_status_result(
        _RIN_DIRENT_SYSCALL1(SYS_CLOSE, (uintptr_t)dirp->fd));
    if (result < 0) saved_errno = errno;
    dirp->magic = 0;
    dirp->fd = -1;
    free(dirp);
    if (result < 0) {
        errno = saved_errno;
        return -1;
    }
    return 0;
}

static inline struct dirent* readdir(DIR* dirp) {
    intptr_t result;
    if (!_rin_dir_valid(dirp)) {
        errno = EBADF;
        return NULL;
    }
    if (dirp->eof) return NULL;

    result = _RIN_DIRENT_SYSCALL3(
        SYS_READDIR, (uintptr_t)dirp->fd,
        (uintptr_t)&dirp->entry, (uintptr_t)sizeof(struct dirent));
    if (result == 0) {
        dirp->eof = 1;
        return NULL;
    }
    result = _rin_dir_result(result);
    if (result < 0) return NULL;
    /* SYS_READDIR has a one-token success contract; other positives are malformed. */
    if (result != 1) {
        errno = EIO;
        return NULL;
    }

    return &dirp->entry;
}

static inline void rewinddir(DIR* dirp) {
    intptr_t result;
    if (!_rin_dir_valid(dirp)) {
        errno = EBADF;
        return;
    }
    result = _RIN_DIRENT_SYSCALL3(
        SYS_SEEK, (uintptr_t)dirp->fd, 0, 0); /* SEEK_SET */
    if (_rin_dir_result(result) >= 0) dirp->eof = 0;
}

static inline long telldir(DIR* dirp) {
    if (!_rin_dir_valid(dirp)) {
        errno = EBADF;
        return -1;
    }
    intptr_t result = _rin_dir_result(_RIN_DIRENT_SYSCALL3(
        SYS_SEEK, (uintptr_t)dirp->fd, 0, 1)); /* SEEK_CUR */
    if (result < 0) return -1;
    if (result > (intptr_t)LONG_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (long)result;
}

static inline void seekdir(DIR* dirp, long loc) {
    intptr_t result;
    if (!_rin_dir_valid(dirp)) {
        errno = EBADF;
        return;
    }
    result = _RIN_DIRENT_SYSCALL3(
        SYS_SEEK, (uintptr_t)dirp->fd, (uintptr_t)(intptr_t)loc, 0);
    if (_rin_dir_result(result) >= 0) dirp->eof = 0;
}

static inline int dirfd(DIR* dirp) {
    if (!_rin_dir_valid(dirp)) {
        errno = EBADF;
        return -1;
    }
    return dirp->fd;
}

#ifdef __cplusplus
}
#endif

#endif /* _DIRENT_H */
