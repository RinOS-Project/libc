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
#include "string.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "sys/syscall.h"
#include <rin/fs/dirent_abi.h>

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

/* 64-bit inode/cookie を全アーキテクチャで同じ幅に保つ拡張record。 */
struct dirent64 {
    uint64_t       d_ino;
    int64_t        d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[256];
    uint8_t        d_reserved[5];
};

#if defined(__cplusplus)
static_assert(sizeof(struct dirent64) == sizeof(RinDirent64Abi),
              "dirent64 libc/kernel ABI drift");
static_assert(offsetof(struct dirent64, d_name) ==
                  offsetof(RinDirent64Abi, name),
              "dirent64 libc/kernel name offset drift");
#else
_Static_assert(sizeof(struct dirent64) == sizeof(RinDirent64Abi),
               "dirent64 libc/kernel ABI drift");
_Static_assert(offsetof(struct dirent64, d_name) ==
                   offsetof(RinDirent64Abi, name),
               "dirent64 libc/kernel name offset drift");
#endif

typedef struct {
    int fd;                         /* ディレクトリのファイルディスクリプタ */
    struct dirent entry;            /* 現在のエントリ */
    struct dirent64 entry64;        /* readdir64() の現在のエントリ */
    int eof;                        /* 終端フラグ */
    unsigned int magic;             /* 有効なストリームか */
    volatile uint32_t lock;         /* 同じストリームの操作を直列化 */
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
#ifndef _RIN_DIRENT_SYSCALL5
#define _RIN_DIRENT_SYSCALL5(number, arg1, arg2, arg3, arg4, arg5) \
    _syscall5((uintptr_t)(number), (uintptr_t)(arg1), (uintptr_t)(arg2), \
              (uintptr_t)(arg3), (uintptr_t)(arg4), (uintptr_t)(arg5))
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

static inline void _rin_dir_lock(DIR* dirp) {
    while (__atomic_exchange_n(&dirp->lock, 1u, __ATOMIC_ACQUIRE) != 0u) {
        while (__atomic_load_n(&dirp->lock, __ATOMIC_RELAXED) != 0u) {
#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || \
    defined(_M_X64)
            __asm__ volatile("pause" ::: "memory");
#endif
        }
    }
}

static inline void _rin_dir_unlock(DIR* dirp) {
    __atomic_store_n(&dirp->lock, 0u, __ATOMIC_RELEASE);
}

static inline int _rin_dir_read_entry_locked(DIR* dirp,
                                             struct dirent* output) {
    intptr_t result;
    if (dirp->eof) return 0;

    result = _RIN_DIRENT_SYSCALL3(
        SYS_READDIR, (uintptr_t)dirp->fd,
        (uintptr_t)&dirp->entry, (uintptr_t)sizeof(struct dirent));
    if (result == 0) {
        dirp->eof = 1;
        return 0;
    }
    result = _rin_dir_result(result);
    if (result < 0) return -1;
    /* SYS_READDIR has a one-token success contract; other positives are malformed. */
    if (result != 1) {
        errno = EIO;
        return -1;
    }

    if (output != &dirp->entry)
        memcpy(output, &dirp->entry, sizeof(*output));
    return 1;
}

static inline int _rin_dir_read_entry64_locked(DIR* dirp,
                                               struct dirent64* output) {
    intptr_t result;
    size_t index;
    int has_terminator = 0;
    if (dirp->eof) return 0;
    result = _RIN_DIRENT_SYSCALL3(
        SYS_READDIR64, (uintptr_t)dirp->fd, (uintptr_t)output,
        (uintptr_t)sizeof(*output));
    if (result == 0) {
        dirp->eof = 1;
        return 0;
    }
    result = _rin_dir_result(result);
    if (result < 0) return -1;
    if (result != 1 || output->d_reclen != sizeof(*output)) {
        errno = EIO;
        return -1;
    }
    for (index = 0u; index < sizeof(output->d_reserved); ++index) {
        if (output->d_reserved[index] != 0u) {
            errno = EIO;
            return -1;
        }
    }
    for (index = 0u; index < sizeof(output->d_name); ++index) {
        if (output->d_name[index] == '\0') {
            has_terminator = 1;
            break;
        }
    }
    if (!has_terminator) {
        errno = EIO;
        return -1;
    }
    return 1;
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
        (uintptr_t)(O_RDONLY | O_DIRECTORY | O_CLOEXEC), 0));
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
    dir->lock = 0u;
    return dir;
}

/* Transfer ownership of an already-open directory descriptor into the
 * caller-owned DIR stream.  WASI fd_readdir duplicates its retained VFS fd
 * before using this boundary, so closing the stream cannot close the WASI
 * descriptor itself. */
static inline DIR* fdopendir(int fd) {
    DIR* dir;
    struct stat status;
    intptr_t open_flags;
    if (fd < 0) {
        errno = EBADF;
        return NULL;
    }
    if (fstat(fd, &status) < 0) return NULL;
    if (!S_ISDIR(status.st_mode)) {
        errno = ENOTDIR;
        return NULL;
    }
    open_flags = _rin_dir_result(_RIN_DIRENT_SYSCALL3(
        SYS_FCNTL, (uintptr_t)fd, (uintptr_t)F_GETFL, (uintptr_t)0u));
    if (open_flags < 0) return NULL;
    if ((open_flags & O_ACCMODE) == O_WRONLY || (open_flags & O_PATH) != 0) {
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
    dir->lock = 0u;
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
    struct dirent* output;
    int result;
    if (!_rin_dir_valid(dirp)) {
        errno = EBADF;
        return NULL;
    }
    output = &dirp->entry;
    _rin_dir_lock(dirp);
    result = _rin_dir_read_entry_locked(dirp, output);
    _rin_dir_unlock(dirp);
    return result > 0 ? output : NULL;
}

static inline struct dirent64* readdir64(DIR* dirp) {
    struct dirent64* output;
    int result;
    if (!_rin_dir_valid(dirp)) {
        errno = EBADF;
        return NULL;
    }
    output = &dirp->entry64;
    _rin_dir_lock(dirp);
    result = _rin_dir_read_entry64_locked(dirp, output);
    _rin_dir_unlock(dirp);
    return result > 0 ? output : NULL;
}

/* Caller-owned output keeps concurrent readers of one DIR from racing over
 * the library's internal dirent storage.  The return value is an errno code,
 * and end-of-directory is reported as success with *result == NULL. */
static inline int readdir_r(DIR* dirp, struct dirent* entry,
                            struct dirent** result) {
    int saved_errno = errno;
    int read_result;
    int error;
    if (result) *result = NULL;
    if (!entry || !result) {
        errno = saved_errno;
        return EINVAL;
    }
    if (!_rin_dir_valid(dirp)) {
        errno = saved_errno;
        return EBADF;
    }

    _rin_dir_lock(dirp);
    read_result = _rin_dir_read_entry_locked(dirp, entry);
    error = read_result < 0 ? (errno ? errno : EIO) : 0;
    _rin_dir_unlock(dirp);
    errno = saved_errno;
    if (error != 0) return error;
    if (read_result > 0) *result = entry;
    return 0;
}

static inline int readdir64_r(DIR* dirp, struct dirent64* entry,
                              struct dirent64** result) {
    int saved_errno = errno;
    int read_result;
    int error;
    if (result) *result = NULL;
    if (!entry || !result) {
        errno = saved_errno;
        return EINVAL;
    }
    if (!_rin_dir_valid(dirp)) {
        errno = saved_errno;
        return EBADF;
    }

    _rin_dir_lock(dirp);
    read_result = _rin_dir_read_entry64_locked(dirp, entry);
    error = read_result < 0 ? (errno ? errno : EIO) : 0;
    _rin_dir_unlock(dirp);
    errno = saved_errno;
    if (error != 0) return error;
    if (read_result > 0) *result = entry;
    return 0;
}

static inline void _rin_scandir_free_entries(struct dirent** entries,
                                              size_t count) {
    while (count != 0u) free(entries[--count]);
    free(entries);
}

static inline int _rin_scandir_sort(struct dirent** entries, size_t count,
                                    int (*compar)(const struct dirent**,
                                                  const struct dirent**)) {
    struct dirent** workspace;
    struct dirent** source;
    struct dirent** destination;
    size_t bytes;
    size_t width;

    if (count < 2u) return 0;
    if (count > SIZE_MAX / sizeof(*entries)) {
        errno = EOVERFLOW;
        return -1;
    }
    bytes = count * sizeof(*entries);
    workspace = (struct dirent**)malloc(bytes);
    if (!workspace) {
        errno = ENOMEM;
        return -1;
    }

    source = entries;
    destination = workspace;
    for (width = 1u; width < count;) {
        size_t start;
        for (start = 0u; start < count;) {
            size_t middle = count - start < width ? count : start + width;
            size_t end = count - middle < width ? count : middle + width;
            size_t left = start;
            size_t right = middle;
            size_t output = start;
            while (left < middle && right < end) {
                const struct dirent* left_entry = source[left];
                const struct dirent* right_entry = source[right];
                if (compar(&left_entry, &right_entry) <= 0)
                    destination[output++] = source[left++];
                else
                    destination[output++] = source[right++];
            }
            while (left < middle) destination[output++] = source[left++];
            while (right < end) destination[output++] = source[right++];
            start = end;
        }
        {
            struct dirent** swap = source;
            source = destination;
            destination = swap;
        }
        if (width > count / 2u) width = count;
        else width *= 2u;
    }
    if (source != entries) memcpy(entries, source, bytes);
    free(workspace);
    return 0;
}

static inline int scandir(
    const char* directory, struct dirent*** namelist,
    int (*selector)(const struct dirent*),
    int (*compar)(const struct dirent**, const struct dirent**)) {
    DIR* stream;
    struct dirent** entries = NULL;
    size_t count = 0u;
    size_t capacity = 0u;
    int saved_error = 0;

    if (!namelist) {
        errno = EINVAL;
        return -1;
    }
    *namelist = NULL;
    stream = opendir(directory);
    if (!stream) return -1;

    for (;;) {
        struct dirent* entry;
        struct dirent* copy;
        size_t name_length;
        errno = 0;
        entry = readdir(stream);
        if (!entry) {
            if (errno != 0) saved_error = errno;
            break;
        }
        if (selector && !selector(entry)) continue;
        for (name_length = 0u; name_length < sizeof(entry->d_name);
             ++name_length) {
            if (entry->d_name[name_length] == '\0') break;
        }
        if (name_length == sizeof(entry->d_name)) {
            saved_error = EIO;
            break;
        }
        if (count == (size_t)INT_MAX) {
            saved_error = EOVERFLOW;
            break;
        }
        copy = (struct dirent*)malloc(sizeof(*copy));
        if (!copy) {
            saved_error = ENOMEM;
            break;
        }
        memcpy(copy, entry, sizeof(*copy));
        if (count == capacity) {
            size_t next_capacity = capacity == 0u ? 16u : capacity * 2u;
            struct dirent** grown;
            if (next_capacity <= capacity ||
                next_capacity > SIZE_MAX / sizeof(*entries)) {
                free(copy);
                saved_error = EOVERFLOW;
                break;
            }
            grown = (struct dirent**)realloc(
                entries, next_capacity * sizeof(*entries));
            if (!grown) {
                free(copy);
                saved_error = ENOMEM;
                break;
            }
            entries = grown;
            capacity = next_capacity;
        }
        entries[count++] = copy;
    }

    if (closedir(stream) < 0 && saved_error == 0) saved_error = errno;
    if (saved_error != 0) {
        _rin_scandir_free_entries(entries, count);
        errno = saved_error;
        return -1;
    }
    if (count == 0u) {
        entries = (struct dirent**)malloc(sizeof(*entries));
        if (!entries) {
            errno = ENOMEM;
            return -1;
        }
    }
    if (compar && _rin_scandir_sort(entries, count, compar) < 0) {
        saved_error = errno;
        _rin_scandir_free_entries(entries, count);
        errno = saved_error;
        return -1;
    }
    *namelist = entries;
    return (int)count;
}

static inline int alphasort(const struct dirent** left,
                            const struct dirent** right) {
    if (!left || !right || !*left || !*right) {
        errno = EINVAL;
        return 0;
    }
    return strcoll((*left)->d_name, (*right)->d_name);
}

static inline int _rin_dir_seek64_locked(DIR* dirp, int64_t offset,
                                         int whence, int64_t* position) {
    uint64_t bits = (uint64_t)offset;
    int64_t returned_position = 0;
    intptr_t result;
    if (!position) {
        errno = EFAULT;
        return -1;
    }
    result = _RIN_DIRENT_SYSCALL5(
        SYS_LSEEK64, (uintptr_t)dirp->fd,
        (uintptr_t)(uint32_t)bits,
        (uintptr_t)(uint32_t)(bits >> 32), (uintptr_t)whence,
        (uintptr_t)&returned_position);
    if (_rin_dir_status_result(result) < 0) return -1;
    *position = returned_position;
    return 0;
}

static inline void rewinddir(DIR* dirp) {
    int64_t position = 0;
    if (!_rin_dir_valid(dirp)) {
        errno = EBADF;
        return;
    }
    _rin_dir_lock(dirp);
    if (_rin_dir_seek64_locked(dirp, 0, 0, &position) == 0)
        dirp->eof = 0;
    _rin_dir_unlock(dirp);
}

static inline off64_t telldir64(DIR* dirp) {
    int64_t position = 0;
    if (!_rin_dir_valid(dirp)) {
        errno = EBADF;
        return -1;
    }
    _rin_dir_lock(dirp);
    if (_rin_dir_seek64_locked(dirp, 0, 1, &position) < 0) {
        _rin_dir_unlock(dirp);
        return -1;
    }
    _rin_dir_unlock(dirp);
    if (position < 0) {
        errno = EIO;
        return -1;
    }
    return (off64_t)position;
}

static inline long telldir(DIR* dirp) {
    off64_t position = telldir64(dirp);
    if (position < 0) return -1;
    if ((uint64_t)position > (uint64_t)LONG_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (long)position;
}

static inline void seekdir64(DIR* dirp, off64_t loc) {
    int64_t position = 0;
    if (!_rin_dir_valid(dirp)) {
        errno = EBADF;
        return;
    }
    _rin_dir_lock(dirp);
    if (_rin_dir_seek64_locked(dirp, (int64_t)loc, 0, &position) == 0)
        dirp->eof = 0;
    _rin_dir_unlock(dirp);
}

static inline void seekdir(DIR* dirp, long loc) {
    seekdir64(dirp, (off64_t)loc);
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
