/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - bounded process-local named semaphore owner
 *
 * This is intentionally a process-local namespace.  The public POSIX entry
 * points are useful to applications that only need a named synchronization
 * object within one RinOS process, but no filesystem or kernel object is
 * implied by this implementation.
 */

#include "semaphore.h"
#include "fcntl_flags.h"
#include "stdarg.h"

#define RIN_NAMED_SEMAPHORE_MAX 32u
#define RIN_NAMED_SEMAPHORE_NAME_MAX 63u

typedef struct rin_named_semaphore_record {
    sem_t semaphore;
    unsigned int open_count;
    unsigned int linked;
    char name[RIN_NAMED_SEMAPHORE_NAME_MAX + 1u];
} rin_named_semaphore_record_t;

static rin_named_semaphore_record_t
    g_rin_named_semaphores[RIN_NAMED_SEMAPHORE_MAX];
static volatile unsigned int g_rin_named_semaphore_lock;

static void rin_named_semaphore_lock_acquire(void)
{
    while (__sync_lock_test_and_set(&g_rin_named_semaphore_lock, 1u)) {
        /* The critical section only examines a fixed 32-entry table. */
    }
}

static void rin_named_semaphore_lock_release(void)
{
    __sync_lock_release(&g_rin_named_semaphore_lock);
}

static int rin_named_semaphore_name_copy(char destination[
    RIN_NAMED_SEMAPHORE_NAME_MAX + 1u], const char* source)
{
    unsigned int index;

    if (!source || source[0] != '/') {
        errno = EINVAL;
        return -1;
    }
    destination[0] = '/';

    for (index = 1u; index <= RIN_NAMED_SEMAPHORE_NAME_MAX; ++index) {
        char ch = source[index];
        if (ch == '\0') {
            if (index == 1u) {
                errno = EINVAL;
                return -1;
            }
            destination[index] = '\0';
            return 0;
        }
        if (ch == '/') {
            errno = EINVAL;
            return -1;
        }
        destination[index] = ch;
    }

    errno = ENAMETOOLONG;
    return -1;
}

static int rin_named_semaphore_name_equal(const char* left, const char* right)
{
    unsigned int index;

    for (index = 0u; index <= RIN_NAMED_SEMAPHORE_NAME_MAX; ++index) {
        if (left[index] != right[index]) {
            return 0;
        }
        if (left[index] == '\0') {
            return 1;
        }
    }
    return 0;
}

static void rin_named_semaphore_clear(rin_named_semaphore_record_t* record)
{
    unsigned int index;

    record->semaphore.value = 0;
    record->semaphore.waiters = 0;
    record->open_count = 0u;
    record->linked = 0u;
    for (index = 0u; index <= RIN_NAMED_SEMAPHORE_NAME_MAX; ++index) {
        record->name[index] = '\0';
    }
}

static rin_named_semaphore_record_t*
rin_named_semaphore_find_linked(const char* name)
{
    unsigned int index;

    for (index = 0u; index < RIN_NAMED_SEMAPHORE_MAX; ++index) {
        rin_named_semaphore_record_t* record = &g_rin_named_semaphores[index];
        if (record->linked && rin_named_semaphore_name_equal(record->name, name)) {
            return record;
        }
    }
    return 0;
}

static rin_named_semaphore_record_t* rin_named_semaphore_find_free(void)
{
    unsigned int index;

    for (index = 0u; index < RIN_NAMED_SEMAPHORE_MAX; ++index) {
        rin_named_semaphore_record_t* record = &g_rin_named_semaphores[index];
        if (!record->linked && record->open_count == 0u) {
            return record;
        }
    }
    return 0;
}

static rin_named_semaphore_record_t*
rin_named_semaphore_find_handle(const sem_t* semaphore)
{
    unsigned int index;

    for (index = 0u; index < RIN_NAMED_SEMAPHORE_MAX; ++index) {
        rin_named_semaphore_record_t* record = &g_rin_named_semaphores[index];
        if (&record->semaphore == semaphore &&
            (record->linked || record->open_count != 0u)) {
            return record;
        }
    }
    return 0;
}

sem_t* sem_open(const char* name, int oflag, ...)
{
    char canonical_name[RIN_NAMED_SEMAPHORE_NAME_MAX + 1u];
    rin_named_semaphore_record_t* record;
    unsigned int initial_value = 0u;

    if ((oflag & ~(O_CREAT | O_EXCL)) != 0 ||
        ((oflag & O_EXCL) != 0 && (oflag & O_CREAT) == 0)) {
        errno = EINVAL;
        return SEM_FAILED;
    }
    if (rin_named_semaphore_name_copy(canonical_name, name) != 0) {
        return SEM_FAILED;
    }

    if (oflag & O_CREAT) {
        va_list arguments;
        mode_t ignored_mode;

        va_start(arguments, oflag);
        ignored_mode = va_arg(arguments, mode_t);
        initial_value = va_arg(arguments, unsigned int);
        va_end(arguments);
        (void)ignored_mode;
        if (initial_value > SEM_VALUE_MAX) {
            errno = EINVAL;
            return SEM_FAILED;
        }
    }

    rin_named_semaphore_lock_acquire();
    record = rin_named_semaphore_find_linked(canonical_name);
    if (record) {
        if ((oflag & O_CREAT) && (oflag & O_EXCL)) {
            rin_named_semaphore_lock_release();
            errno = EEXIST;
            return SEM_FAILED;
        }
        ++record->open_count;
        rin_named_semaphore_lock_release();
        return &record->semaphore;
    }

    if (!(oflag & O_CREAT)) {
        rin_named_semaphore_lock_release();
        errno = ENOENT;
        return SEM_FAILED;
    }

    record = rin_named_semaphore_find_free();
    if (!record) {
        rin_named_semaphore_lock_release();
        errno = ENFILE;
        return SEM_FAILED;
    }

    rin_named_semaphore_clear(record);
    {
        unsigned int index;
        for (index = 0u; index <= RIN_NAMED_SEMAPHORE_NAME_MAX; ++index) {
            record->name[index] = canonical_name[index];
            if (canonical_name[index] == '\0') {
                break;
            }
        }
    }
    record->semaphore.value = (int)initial_value;
    record->open_count = 1u;
    record->linked = 1u;
    rin_named_semaphore_lock_release();
    return &record->semaphore;
}

int sem_close(sem_t* semaphore)
{
    rin_named_semaphore_record_t* record;

    if (!semaphore) {
        errno = EINVAL;
        return -1;
    }

    rin_named_semaphore_lock_acquire();
    record = rin_named_semaphore_find_handle(semaphore);
    if (!record || record->open_count == 0u) {
        rin_named_semaphore_lock_release();
        errno = EINVAL;
        return -1;
    }

    --record->open_count;
    if (!record->linked && record->open_count == 0u) {
        rin_named_semaphore_clear(record);
    }
    rin_named_semaphore_lock_release();
    return 0;
}

int sem_unlink(const char* name)
{
    char canonical_name[RIN_NAMED_SEMAPHORE_NAME_MAX + 1u];
    rin_named_semaphore_record_t* record;

    if (rin_named_semaphore_name_copy(canonical_name, name) != 0) {
        return -1;
    }

    rin_named_semaphore_lock_acquire();
    record = rin_named_semaphore_find_linked(canonical_name);
    if (!record) {
        rin_named_semaphore_lock_release();
        errno = ENOENT;
        return -1;
    }

    record->linked = 0u;
    record->name[0] = '\0';
    if (record->open_count == 0u) {
        rin_named_semaphore_clear(record);
    }
    rin_named_semaphore_lock_release();
    return 0;
}
