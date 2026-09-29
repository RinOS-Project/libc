/* SPDX-License-Identifier: MIT */
/* Process-wide ownership and reclamation for the freestanding environment. */

#include "stdlib.h"
#include "internal/rin_environment_runtime.h"
#include "rin_account_compat.h"

typedef struct RinEnvironmentAllocation {
    void* pointer;
    unsigned int is_table;
    struct RinEnvironmentAllocation* next;
} RinEnvironmentAllocation;

static volatile int g_environment_writer_lock;
static volatile unsigned int g_environment_epoch;
static volatile unsigned int g_environment_readers[2];
static RinEnvironmentAllocation* g_environment_allocations;

static void environment_lock(void) {
    while (__sync_lock_test_and_set(&g_environment_writer_lock, 1)) {
        while (__atomic_load_n(&g_environment_writer_lock, __ATOMIC_RELAXED))
            __asm__ volatile("pause" ::: "memory");
    }
}

static void environment_unlock(void) {
    __sync_lock_release(&g_environment_writer_lock);
}

char** __rin_env_read_begin(unsigned int* epoch_out) {
    unsigned int epoch;
    if (!epoch_out) return NULL;
    for (;;) {
        epoch = __atomic_load_n(&g_environment_epoch, __ATOMIC_ACQUIRE) & 1u;
        __atomic_fetch_add(&g_environment_readers[epoch], 1u,
                           __ATOMIC_ACQ_REL);
        if ((__atomic_load_n(&g_environment_epoch, __ATOMIC_ACQUIRE) & 1u) ==
            epoch) {
            *epoch_out = epoch;
            return __atomic_load_n(&environ, __ATOMIC_ACQUIRE);
        }
        __atomic_fetch_sub(&g_environment_readers[epoch], 1u,
                           __ATOMIC_RELEASE);
    }
}

void __rin_env_read_end(unsigned int epoch) {
    __atomic_fetch_sub(&g_environment_readers[epoch & 1u], 1u,
                       __ATOMIC_RELEASE);
}

static size_t environment_length(const char* string) {
    size_t length = 0u;
    if (!string) return 0u;
    while (string[length] != '\0') ++length;
    return length;
}

static int environment_name_valid(const char* name) {
    if (!name || *name == '\0') return 0;
    for (const char* current = name; *current; ++current)
        if (*current == '=') return 0;
    return 1;
}

static int environment_name_matches(const char* entry, const char* name) {
    size_t index = 0u;
    if (!entry || !name) return 0;
    while (name[index] != '\0') {
        if (entry[index] != name[index]) return 0;
        ++index;
    }
    return entry[index] == '=';
}

char* __rin_env_get_from_snapshot(char** snapshot, const char* name) {
    if (!environment_name_valid(name) || !snapshot) return NULL;
    for (size_t index = 0u; snapshot[index] != NULL; ++index) {
        if (environment_name_matches(snapshot[index], name))
            return snapshot[index] + environment_length(name) + 1u;
    }
    return NULL;
}

static size_t environment_count(char** table) {
    size_t count = 0u;
    if (!table) return 0u;
    while (table[count] != NULL) {
        if (count >= SIZE_MAX / sizeof(char*) - 1u) {
            errno = EOVERFLOW;
            return SIZE_MAX;
        }
        ++count;
    }
    return count;
}

static int environment_track_allocation(void* pointer,
                                        unsigned int is_table) {
    RinEnvironmentAllocation* allocation;
    if (!pointer) return 0;
    allocation = (RinEnvironmentAllocation*)malloc(sizeof(*allocation));
    if (!allocation) return 0;
    allocation->pointer = pointer;
    allocation->is_table = is_table;
    allocation->next = g_environment_allocations;
    g_environment_allocations = allocation;
    return 1;
}

static void environment_untrack_and_free(void* pointer,
                                         unsigned int is_table) {
    RinEnvironmentAllocation** link = &g_environment_allocations;
    while (*link) {
        RinEnvironmentAllocation* allocation = *link;
        if (allocation->pointer == pointer &&
            allocation->is_table == is_table) {
            *link = allocation->next;
            free(allocation->pointer);
            free(allocation);
            return;
        }
        link = &allocation->next;
    }
}

static char** environment_allocate_table(size_t entry_count) {
    char** table;
    if (entry_count >= SIZE_MAX / sizeof(char*)) {
        errno = EOVERFLOW;
        return NULL;
    }
    table = (char**)malloc(sizeof(char*) * (entry_count + 1u));
    if (!table) {
        errno = ENOMEM;
        return NULL;
    }
    if (!environment_track_allocation(table, 1u)) {
        free(table);
        errno = ENOMEM;
        return NULL;
    }
    return table;
}

static int environment_table_references(char** table, const void* pointer) {
    if (!table) return 0;
    for (size_t index = 0u; table[index] != NULL; ++index)
        if (table[index] == pointer) return 1;
    return 0;
}

/* Called only after the previous reader epoch has drained.  Values referenced
 * by the surviving table remain owned; retired tables and replaced values
 * can now be reclaimed. */
static void environment_reclaim(char** survivor) {
    RinEnvironmentAllocation** link = &g_environment_allocations;
    size_t survivor_count = environment_count(survivor);
    if (survivor_count == SIZE_MAX) return;
    while (*link) {
        RinEnvironmentAllocation* allocation = *link;
        int keep = allocation->is_table
            ? allocation->pointer == survivor
            : environment_table_references(survivor, allocation->pointer);
        if (keep) {
            link = &allocation->next;
            continue;
        }
        *link = allocation->next;
        free(allocation->pointer);
        free(allocation);
    }
}

static void environment_synchronize_and_reclaim(char** survivor) {
    unsigned int old_epoch =
        __atomic_load_n(&g_environment_epoch, __ATOMIC_ACQUIRE) & 1u;
    __atomic_store_n(&g_environment_epoch, old_epoch ^ 1u, __ATOMIC_RELEASE);
    while (__atomic_load_n(&g_environment_readers[old_epoch],
                           __ATOMIC_ACQUIRE) != 0u)
        __asm__ volatile("pause" ::: "memory");
    environment_reclaim(survivor);
}

static int environment_publish(char** expected, char** replacement) {
    unsigned int old_epoch;
    if (!__atomic_compare_exchange_n(&environ, &expected, replacement, 0,
                                     __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
        return 0;
    old_epoch = __atomic_load_n(&g_environment_epoch, __ATOMIC_ACQUIRE) & 1u;
    __atomic_store_n(&g_environment_epoch, old_epoch ^ 1u, __ATOMIC_RELEASE);
    while (__atomic_load_n(&g_environment_readers[old_epoch],
                           __ATOMIC_ACQUIRE) != 0u)
        __asm__ volatile("pause" ::: "memory");
    environment_reclaim(replacement);
    return 1;
}

static char* environment_make_entry(const char* name, const char* value) {
    size_t name_length = environment_length(name);
    size_t value_length = environment_length(value);
    char* entry;
    size_t offset = 0u;
    if (value_length > SIZE_MAX - 2u ||
        name_length > SIZE_MAX - value_length - 2u) {
        errno = EOVERFLOW;
        return NULL;
    }
    entry = (char*)malloc(name_length + value_length + 2u);
    if (!entry) {
        errno = ENOMEM;
        return NULL;
    }
    for (size_t index = 0u; index < name_length; ++index)
        entry[offset++] = name[index];
    entry[offset++] = '=';
    for (size_t index = 0u; index < value_length; ++index)
        entry[offset++] = value[index];
    entry[offset] = '\0';
    if (!environment_track_allocation(entry, 0u)) {
        free(entry);
        errno = ENOMEM;
        return NULL;
    }
    return entry;
}

static size_t environment_matching_count(char** table, size_t count,
                                         const char* name) {
    size_t matches = 0u;
    for (size_t index = 0u; index < count; ++index)
        if (environment_name_matches(table[index], name)) ++matches;
    return matches;
}

static void environment_reclaim_current(void) {
    char** current = __atomic_load_n(&environ, __ATOMIC_ACQUIRE);
    environment_synchronize_and_reclaim(current);
}

char* getenv(const char* name) {
    unsigned int epoch;
    char** snapshot;
    if (!environment_name_valid(name)) return NULL;
    snapshot = __rin_env_read_begin(&epoch);
    char* result = __rin_env_get_from_snapshot(snapshot, name);
    __rin_env_read_end(epoch);
    return result;
}

int __rin_env_is_secure(void) {
    __rin_credentials_v1 credentials;
    int saved_errno = errno;
    int error = __rin_credentials_get(&credentials);
    errno = saved_errno;
    /* The kernel snapshot carries per-image secure-exec state. If it cannot
     * be read, do not expose environment data to a caller we cannot classify. */
    if (error != 0) return 1;
    return (credentials.flags & __RIN_CREDENTIALS_FLAG_SECURE_EXEC) != 0u;
}

char* __rin_env_get_secure_from_snapshot(char** snapshot, const char* name) {
    if (__rin_env_is_secure()) return NULL;
    return __rin_env_get_from_snapshot(snapshot, name);
}

char* secure_getenv(const char* name) {
    if (__rin_env_is_secure()) return NULL;
    return getenv(name);
}

int setenv(const char* name, const char* value, int overwrite) {
    if (!environment_name_valid(name) || !value) {
        errno = EINVAL;
        return -1;
    }
    environment_lock();
    for (;;) {
        char** snapshot = __atomic_load_n(&environ, __ATOMIC_ACQUIRE);
        size_t count = environment_count(snapshot);
        size_t matches;
        size_t new_count;
        size_t write_index = 0u;
        int inserted = 0;
        char* new_entry;
        char** new_table;
        if (count == SIZE_MAX) {
            environment_reclaim_current();
            environment_unlock();
            return -1;
        }
        matches = environment_matching_count(snapshot, count, name);
        if (matches != 0u && !overwrite) {
            environment_reclaim_current();
            environment_unlock();
            return 0;
        }
        if (matches == 0u && count == SIZE_MAX - 1u) {
            errno = EOVERFLOW;
            environment_reclaim_current();
            environment_unlock();
            return -1;
        }
        new_entry = environment_make_entry(name, value);
        if (!new_entry) {
            environment_reclaim_current();
            environment_unlock();
            return -1;
        }
        new_count = matches == 0u ? count + 1u : count - matches + 1u;
        new_table = environment_allocate_table(new_count);
        if (!new_table) {
            environment_reclaim_current();
            environment_unlock();
            return -1;
        }
        for (size_t index = 0u; index < count; ++index) {
            if (environment_name_matches(snapshot[index], name)) {
                if (!inserted) {
                    new_table[write_index++] = new_entry;
                    inserted = 1;
                }
            } else {
                new_table[write_index++] = snapshot[index];
            }
        }
        if (!inserted) new_table[write_index++] = new_entry;
        new_table[write_index] = NULL;
        if (environment_publish(snapshot, new_table)) {
            environment_unlock();
            return 0;
        }
        environment_reclaim_current();
    }
}

int unsetenv(const char* name) {
    if (!environment_name_valid(name)) {
        errno = EINVAL;
        return -1;
    }
    environment_lock();
    for (;;) {
        char** snapshot = __atomic_load_n(&environ, __ATOMIC_ACQUIRE);
        size_t count = environment_count(snapshot);
        size_t matches;
        size_t write_index = 0u;
        char** new_table;
        if (count == SIZE_MAX) {
            environment_reclaim_current();
            environment_unlock();
            return -1;
        }
        matches = environment_matching_count(snapshot, count, name);
        if (matches == 0u) {
            environment_reclaim_current();
            environment_unlock();
            return 0;
        }
        new_table = environment_allocate_table(count - matches);
        if (!new_table) {
            environment_reclaim_current();
            environment_unlock();
            return -1;
        }
        for (size_t index = 0u; index < count; ++index) {
            if (!environment_name_matches(snapshot[index], name)) {
                new_table[write_index++] = snapshot[index];
            }
        }
        new_table[write_index] = NULL;
        if (environment_publish(snapshot, new_table)) {
            environment_unlock();
            return 0;
        }
        environment_reclaim_current();
    }
}

int putenv(char* string) {
    char* equals = NULL;
    size_t name_length;
    if (!string) {
        errno = EINVAL;
        return -1;
    }
    for (char* current = string; *current != '\0'; ++current) {
        if (*current == '=') {
            equals = current;
            break;
        }
    }
    if (!equals || equals == string) {
        errno = EINVAL;
        return -1;
    }
    name_length = (size_t)(equals - string);
    environment_lock();
    for (;;) {
        char** snapshot = __atomic_load_n(&environ, __ATOMIC_ACQUIRE);
        size_t count = environment_count(snapshot);
        size_t matches = 0u;
        size_t new_count;
        size_t write_index = 0u;
        int inserted = 0;
        char** new_table;
        if (count == SIZE_MAX) {
            environment_reclaim_current();
            environment_unlock();
            return -1;
        }
        for (size_t index = 0u; index < count; ++index) {
            size_t name_index = 0u;
            while (name_index < name_length &&
                   snapshot[index][name_index] != '\0' &&
                   snapshot[index][name_index] == string[name_index])
                ++name_index;
            if (name_index == name_length &&
                snapshot[index][name_index] == '=')
                ++matches;
        }
        if (matches == 0u && count == SIZE_MAX - 1u) {
            errno = EOVERFLOW;
            environment_reclaim_current();
            environment_unlock();
            return -1;
        }
        new_count = matches == 0u ? count + 1u : count - matches + 1u;
        new_table = environment_allocate_table(new_count);
        if (!new_table) {
            environment_reclaim_current();
            environment_unlock();
            return -1;
        }
        for (size_t index = 0u; index < count; ++index) {
            size_t name_index = 0u;
            while (name_index < name_length &&
                   snapshot[index][name_index] != '\0' &&
                   snapshot[index][name_index] == string[name_index])
                ++name_index;
            if (name_index == name_length &&
                snapshot[index][name_index] == '=') {
                if (!inserted) {
                    new_table[write_index++] = string;
                    inserted = 1;
                }
            } else {
                new_table[write_index++] = snapshot[index];
            }
        }
        if (!inserted) new_table[write_index++] = string;
        new_table[write_index] = NULL;
        if (environment_publish(snapshot, new_table)) {
            environment_unlock();
            return 0;
        }
        environment_reclaim_current();
    }
}

int clearenv(void) {
    environment_lock();
    for (;;) {
        char** snapshot = __atomic_load_n(&environ, __ATOMIC_ACQUIRE);
        char** empty_table = environment_allocate_table(0u);
        if (!empty_table) {
            environment_reclaim_current();
            environment_unlock();
            return -1;
        }
        empty_table[0] = NULL;
        if (environment_publish(snapshot, empty_table)) {
            environment_unlock();
            return 0;
        }
        environment_reclaim_current();
    }
}
