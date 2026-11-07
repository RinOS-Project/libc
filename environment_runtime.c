/* SPDX-License-Identifier: MIT */
/* Process-wide ownership and reclamation for the freestanding environment. */

#define RIN_STDLIB_SECURE_GETENV_IMPLEMENTATION 1
#include "stdlib.h"
#include "internal/rin_environment_runtime.h"
#include "rin_account_compat.h"
#include "linux/futex.h"
#include "sys/syscall.h"
#include <limits.h>
#include <stdint.h>

typedef struct RinEnvironmentOwnedString {
    char* value;
    volatile size_t references;
    volatile unsigned int owned;
} RinEnvironmentOwnedString;

typedef struct RinEnvironmentSnapshot {
    char** entries;
    RinEnvironmentOwnedString** owners;
    unsigned char* new_owners;
    size_t count;
    size_t readers;
    unsigned int generation;
    unsigned int retired;
    unsigned int owners_acquired;
    struct RinEnvironmentSnapshot* next;
    struct RinEnvironmentSnapshot* previous;
    struct RinEnvironmentSnapshot* hash_next;
} RinEnvironmentSnapshot;

#define ENVIRONMENT_SNAPSHOT_HASH_BUCKET_COUNT 1024u

#if defined(RIN_ENVIRONMENT_RUNTIME_TESTING)
typedef struct RinEnvironmentAllocation {
    void* pointer;
    unsigned int is_table;
    struct RinEnvironmentAllocation* next;
} RinEnvironmentAllocation;

static RinEnvironmentAllocation* g_environment_allocations;
#endif

static volatile unsigned int g_environment_writer_lock;
static volatile unsigned int g_environment_epoch;
static RinEnvironmentSnapshot* g_environment_current;
static RinEnvironmentSnapshot* g_environment_snapshots;
static RinEnvironmentSnapshot*
    g_environment_snapshot_buckets[ENVIRONMENT_SNAPSHOT_HASH_BUCKET_COUNT];

#if defined(RIN_ENVIRONMENT_RUNTIME_TESTING)
static size_t g_environment_reclaim_comparisons;

void __rin_env_test_reset_reclaim_comparisons(void) {
    g_environment_reclaim_comparisons = 0u;
}

size_t __rin_env_test_reclaim_comparisons(void) {
    return g_environment_reclaim_comparisons;
}
#define ENVIRONMENT_RECLAIM_COUNT_COMPARISON() \
    (++g_environment_reclaim_comparisons)
#else
#define ENVIRONMENT_RECLAIM_COUNT_COMPARISON() ((void)0)
#endif

static void environment_yield_after_futex_error(void) {
    const int saved_errno = errno;
    (void)syscall(SYS_sched_yield);
    errno = saved_errno;
}

static void environment_futex_wait(volatile unsigned int* address,
                                  unsigned int expected) {
    const int saved_errno = errno;
    const long result = syscall(SYS_futex, address, FUTEX_WAIT_PRIVATE,
                                expected, NULL, NULL, 0);
    if (result == -1 && errno != EAGAIN && errno != EINTR)
        environment_yield_after_futex_error();
    errno = saved_errno;
}

static void environment_futex_wake(volatile unsigned int* address,
                                   int count) {
    const int saved_errno = errno;
    (void)syscall(SYS_futex, address, FUTEX_WAKE_PRIVATE, count,
                  NULL, NULL, 0);
    errno = saved_errno;
}

static void environment_lock(void) {
    if (__atomic_exchange_n(&g_environment_writer_lock, 1u,
                            __ATOMIC_ACQUIRE) == 0u)
        return;
    for (;;) {
        if (__atomic_exchange_n(&g_environment_writer_lock, 2u,
                                __ATOMIC_ACQUIRE) == 0u)
            return;
        environment_futex_wait(&g_environment_writer_lock, 2u);
    }
}

static void environment_unlock(void) {
    if (__atomic_exchange_n(&g_environment_writer_lock, 0u,
                            __ATOMIC_RELEASE) == 2u)
        environment_futex_wake(&g_environment_writer_lock, 1);
}

static RinEnvironmentSnapshot* environment_snapshot_find_generation(
    unsigned int generation) {
    unsigned int bucket =
        generation % ENVIRONMENT_SNAPSHOT_HASH_BUCKET_COUNT;
    for (RinEnvironmentSnapshot* snapshot =
             g_environment_snapshot_buckets[bucket];
         snapshot; snapshot = snapshot->hash_next) {
        if (snapshot->generation == generation) return snapshot;
    }
    return NULL;
}

static void environment_snapshot_link_locked(
    RinEnvironmentSnapshot* snapshot) {
    unsigned int bucket =
        snapshot->generation % ENVIRONMENT_SNAPSHOT_HASH_BUCKET_COUNT;
    snapshot->previous = NULL;
    snapshot->next = g_environment_snapshots;
    if (g_environment_snapshots)
        g_environment_snapshots->previous = snapshot;
    g_environment_snapshots = snapshot;
    snapshot->hash_next = g_environment_snapshot_buckets[bucket];
    g_environment_snapshot_buckets[bucket] = snapshot;
}

static void environment_snapshot_unlink_locked(
    RinEnvironmentSnapshot* snapshot) {
    unsigned int bucket =
        snapshot->generation % ENVIRONMENT_SNAPSHOT_HASH_BUCKET_COUNT;
    RinEnvironmentSnapshot** hash_link =
        &g_environment_snapshot_buckets[bucket];
    if (snapshot->previous)
        snapshot->previous->next = snapshot->next;
    else
        g_environment_snapshots = snapshot->next;
    if (snapshot->next)
        snapshot->next->previous = snapshot->previous;
    while (*hash_link && *hash_link != snapshot)
        hash_link = &(*hash_link)->hash_next;
    if (*hash_link == snapshot) *hash_link = snapshot->hash_next;
    snapshot->next = NULL;
    snapshot->previous = NULL;
    snapshot->hash_next = NULL;
}

static RinEnvironmentSnapshot* environment_snapshot_find_table(char** table) {
    for (RinEnvironmentSnapshot* snapshot = g_environment_snapshots;
         snapshot; snapshot = snapshot->next) {
        if (snapshot->entries == table) return snapshot;
    }
    return NULL;
}

static void environment_owned_string_release(RinEnvironmentOwnedString* owner) {
    if (!owner) return;
    if (__atomic_fetch_sub(&owner->references, 1u, __ATOMIC_ACQ_REL) != 1u)
        return;
    if (__atomic_load_n(&owner->owned, __ATOMIC_ACQUIRE))
        free(owner->value);
    free(owner);
}

/* The caller has removed the snapshot from the protected list. A reader lease
 * keeps the entire snapshot, including every owned string, alive. */
static void environment_snapshot_destroy(RinEnvironmentSnapshot* snapshot) {
    if (!snapshot) return;
    int saved_errno = errno;
    for (size_t index = 0u; index < snapshot->count; ++index)
        environment_owned_string_release(snapshot->owners[index]);
    free(snapshot->new_owners);
    free(snapshot->owners);
    free(snapshot->entries);
    free(snapshot);
    errno = saved_errno;
}

static void environment_snapshot_retire_current_locked(
    RinEnvironmentSnapshot** destroy_out) {
    RinEnvironmentSnapshot* current = g_environment_current;
    if (!current) return;
    current->retired = 1u;
    g_environment_current = NULL;
    if (current->readers != 0u) return;
    environment_snapshot_unlink_locked(current);
    *destroy_out = current;
}

static unsigned int environment_next_generation_locked(void) {
    unsigned int previous = g_environment_epoch;
    unsigned int generation = previous + 1u;
    if (generation == UINT_MAX) generation = 0u;
    if (generation <= previous) {
        while (environment_snapshot_find_generation(generation)) {
            ++generation;
            if (generation == UINT_MAX) generation = 0u;
        }
    }
    return generation;
}

char** __rin_env_read_begin(unsigned int* epoch_out) {
    char** table;
    if (!epoch_out) return NULL;
    for (;;) {
        RinEnvironmentSnapshot* snapshot;
        environment_lock();
        table = __atomic_load_n(&environ, __ATOMIC_ACQUIRE);
        snapshot = g_environment_current &&
                g_environment_current->entries == table
            ? g_environment_current
            : environment_snapshot_find_table(table);
        if (!snapshot && g_environment_current) {
            /* Applications may replace environ directly. Pin the last managed
             * snapshot as an ownership source while returning the external
             * table. */
            snapshot = g_environment_current;
        }
        if (snapshot && snapshot->readers == SIZE_MAX) {
            environment_unlock();
            environment_yield_after_futex_error();
            continue;
        }
        if (snapshot) {
            ++snapshot->readers;
            *epoch_out = snapshot->generation;
        } else {
            *epoch_out = UINT_MAX;
        }
        environment_unlock();
        return table;
    }
}

void __rin_env_read_end(unsigned int epoch) {
    RinEnvironmentSnapshot* destroy = NULL;
    if (epoch == UINT_MAX) return;
    environment_lock();
    RinEnvironmentSnapshot* snapshot =
        environment_snapshot_find_generation(epoch);
    if (snapshot && snapshot->readers != 0u) {
        --snapshot->readers;
        if (snapshot->readers == 0u && snapshot->retired) {
            environment_snapshot_unlink_locked(snapshot);
            destroy = snapshot;
        }
    }
    environment_unlock();
    environment_snapshot_destroy(destroy);
}

unsigned int __rin_env_generation(void) {
    unsigned int generation;
    environment_lock();
    generation = g_environment_epoch;
    environment_unlock();
    return generation;
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

#if defined(RIN_ENVIRONMENT_RUNTIME_TESTING)
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

static int environment_pointer_compare(const void* left, const void* right) {
    uintptr_t left_value = *(const uintptr_t*)left;
    uintptr_t right_value = *(const uintptr_t*)right;
    ENVIRONMENT_RECLAIM_COUNT_COMPARISON();
    if (left_value < right_value) return -1;
    if (left_value > right_value) return 1;
    return 0;
}

static int environment_sorted_references(const uintptr_t* references,
                                         size_t count,
                                         const void* pointer) {
    uintptr_t target = (uintptr_t)pointer;
    size_t low = 0u;
    size_t high = count;
    while (low < high) {
        size_t middle = low + (high - low) / 2u;
        ENVIRONMENT_RECLAIM_COUNT_COMPARISON();
        if (references[middle] == target) return 1;
        if (references[middle] < target)
            low = middle + 1u;
        else
            high = middle;
    }
    return 0;
}

/* Keep the standalone reclamation fixture source-compatible. Production
 * ownership is now per-snapshot and does not use this test-only registry. */
static void environment_reclaim(char** survivor) {
    RinEnvironmentAllocation** link = &g_environment_allocations;
    uintptr_t* references = NULL;
    int saved_errno = errno;
    size_t survivor_count = environment_count(survivor);
    if (survivor_count == SIZE_MAX ||
        survivor_count > SIZE_MAX / sizeof(*references)) {
        errno = saved_errno;
        return;
    }
    if (survivor_count != 0u) {
        references = (uintptr_t*)malloc(survivor_count * sizeof(*references));
        if (!references) {
            errno = saved_errno;
            return;
        }
        for (size_t index = 0u; index < survivor_count; ++index)
            references[index] = (uintptr_t)survivor[index];
        qsort(references, survivor_count, sizeof(*references),
              environment_pointer_compare);
    }
    while (*link) {
        RinEnvironmentAllocation* allocation = *link;
        int keep = allocation->is_table
            ? allocation->pointer == survivor
            : environment_sorted_references(references, survivor_count,
                                            allocation->pointer);
        if (keep) {
            link = &allocation->next;
            continue;
        }
        *link = allocation->next;
        free(allocation->pointer);
        free(allocation);
    }
    free(references);
    errno = saved_errno;
}
#endif

typedef struct RinEnvironmentOwnerReference {
    uintptr_t value;
    RinEnvironmentOwnedString* owner;
} RinEnvironmentOwnerReference;

static int environment_owner_reference_compare(const void* left,
                                              const void* right) {
    uintptr_t left_value =
        ((const RinEnvironmentOwnerReference*)left)->value;
    uintptr_t right_value =
        ((const RinEnvironmentOwnerReference*)right)->value;
    if (left_value < right_value) return -1;
    if (left_value > right_value) return 1;
    return 0;
}

static RinEnvironmentOwnedString* environment_owner_reference_find(
    const RinEnvironmentOwnerReference* references, size_t count,
    const char* pointer) {
    uintptr_t target = (uintptr_t)pointer;
    size_t low = 0u;
    size_t high = count;
    while (low < high) {
        size_t middle = low + (high - low) / 2u;
        if (references[middle].value == target)
            return references[middle].owner;
        if (references[middle].value < target)
            low = middle + 1u;
        else
            high = middle;
    }
    return NULL;
}

static RinEnvironmentSnapshot* environment_snapshot_allocate(
    size_t entry_count) {
    RinEnvironmentSnapshot* snapshot;
    if (entry_count >= SIZE_MAX / sizeof(char*) ||
        entry_count > SIZE_MAX / sizeof(RinEnvironmentOwnedString*)) {
        errno = EOVERFLOW;
        return NULL;
    }
    snapshot = (RinEnvironmentSnapshot*)malloc(sizeof(*snapshot));
    if (!snapshot) {
        errno = ENOMEM;
        return NULL;
    }
    snapshot->entries =
        (char**)malloc(sizeof(char*) * (entry_count + 1u));
    snapshot->owners = entry_count
        ? (RinEnvironmentOwnedString**)malloc(
              sizeof(*snapshot->owners) * entry_count)
        : NULL;
    snapshot->new_owners =
        entry_count ? (unsigned char*)malloc(entry_count) : NULL;
    if (!snapshot->entries ||
        (entry_count && (!snapshot->owners || !snapshot->new_owners))) {
        free(snapshot->entries);
        free(snapshot->owners);
        free(snapshot->new_owners);
        free(snapshot);
        errno = ENOMEM;
        return NULL;
    }
    for (size_t index = 0u; index < entry_count; ++index) {
        snapshot->entries[index] = NULL;
        snapshot->owners[index] = NULL;
        snapshot->new_owners[index] = 0u;
    }
    snapshot->entries[entry_count] = NULL;
    snapshot->count = entry_count;
    snapshot->readers = 0u;
    snapshot->generation = UINT_MAX;
    snapshot->retired = 0u;
    snapshot->owners_acquired = 0u;
    snapshot->next = NULL;
    snapshot->previous = NULL;
    snapshot->hash_next = NULL;
    return snapshot;
}

static void environment_snapshot_discard(RinEnvironmentSnapshot* snapshot) {
    if (!snapshot) return;
    for (size_t index = 0u; index < snapshot->count; ++index) {
        RinEnvironmentOwnedString* owner = snapshot->owners[index];
        if (owner &&
            (snapshot->owners_acquired || snapshot->new_owners[index]))
            environment_owned_string_release(owner);
    }
    free(snapshot->new_owners);
    free(snapshot->owners);
    free(snapshot->entries);
    free(snapshot);
}

static int environment_snapshot_copy_owners(
    unsigned int source_generation, char** source, size_t count,
    RinEnvironmentOwnedString** owners_out) {
    RinEnvironmentSnapshot* source_snapshot;
    RinEnvironmentOwnerReference* references = NULL;
    size_t reference_count = 0u;
    int found = 0;
    int need_pointer_match = 0;
    if (!owners_out || count == 0u) return 1;
    for (size_t index = 0u; index < count; ++index)
        owners_out[index] = NULL;
    if (source_generation == UINT_MAX) return 1;

    environment_lock();
    source_snapshot =
        environment_snapshot_find_generation(source_generation);
    environment_unlock();
    if (source_snapshot) {
        found = 1;
        if (source_snapshot->entries == source) {
            size_t limit = count < source_snapshot->count
                ? count : source_snapshot->count;
            if (count != source_snapshot->count)
                need_pointer_match = 1;
            for (size_t index = 0u; index < limit; ++index) {
                RinEnvironmentOwnedString* owner =
                    source_snapshot->owners[index];
                if (owner && owner->value == source[index])
                    owners_out[index] = owner;
                else if (owner)
                    need_pointer_match = 1;
            }
        } else {
            need_pointer_match = 1;
        }
        if (need_pointer_match) {
            for (size_t index = 0u; index < source_snapshot->count; ++index)
                if (source_snapshot->owners[index]) ++reference_count;
            if (reference_count > SIZE_MAX / sizeof(*references)) {
                errno = EOVERFLOW;
                return 0;
            }
            if (reference_count != 0u) {
                references = (RinEnvironmentOwnerReference*)malloc(
                    reference_count * sizeof(*references));
                if (!references) {
                    errno = ENOMEM;
                    return 0;
                }
                size_t write_index = 0u;
                for (size_t index = 0u;
                     index < source_snapshot->count; ++index) {
                    RinEnvironmentOwnedString* owner =
                        source_snapshot->owners[index];
                    if (!owner) continue;
                    references[write_index].value = (uintptr_t)owner->value;
                    references[write_index++].owner = owner;
                }
                qsort(references, reference_count, sizeof(*references),
                      environment_owner_reference_compare);
            }
            /* read_begin() holds a lease on this owner snapshot. Match
             * reordered entries or an external table outside the lock. */
            for (size_t index = 0u; index < count; ++index) {
                if (!owners_out[index])
                    owners_out[index] = environment_owner_reference_find(
                        references, reference_count, source[index]);
            }
        }
    }
    free(references);
    if (!found && source_generation != UINT_MAX) {
        errno = EAGAIN;
        return 0;
    }
    return found;
}

static RinEnvironmentOwnedString* environment_owner_for_pointer(
    unsigned int source_generation, const char* pointer) {
    RinEnvironmentSnapshot* snapshot;
    if (!pointer || source_generation == UINT_MAX) return NULL;
    environment_lock();
    snapshot = environment_snapshot_find_generation(source_generation);
    environment_unlock();
    if (!snapshot) return NULL;
    for (size_t index = 0u; index < snapshot->count; ++index) {
        RinEnvironmentOwnedString* owner = snapshot->owners[index];
        if (owner && owner->value == pointer) return owner;
    }
    return NULL;
}

static void environment_snapshot_acquire_owners(
    RinEnvironmentSnapshot* snapshot) {
    if (!snapshot || snapshot->owners_acquired) return;
    for (size_t index = 0u; index < snapshot->count; ++index) {
        RinEnvironmentOwnedString* owner = snapshot->owners[index];
        if (owner && !snapshot->new_owners[index])
            __atomic_fetch_add(&owner->references, 1u, __ATOMIC_RELAXED);
    }
    snapshot->owners_acquired = 1u;
    free(snapshot->new_owners);
    snapshot->new_owners = NULL;
}

static int environment_publish(char** expected,
                               RinEnvironmentSnapshot* replacement,
                               RinEnvironmentOwnedString* transfer_to_caller) {
    RinEnvironmentSnapshot* destroy = NULL;
    char** compare = expected;
    unsigned int generation;
    if (!replacement) return 0;

    environment_snapshot_acquire_owners(replacement);
    environment_lock();
    if (!__atomic_compare_exchange_n(&environ, &compare,
                                     replacement->entries, 0,
                                     __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        environment_unlock();
        return 0;
    }
    if (transfer_to_caller)
        __atomic_store_n(&transfer_to_caller->owned, 0u, __ATOMIC_RELEASE);

    environment_snapshot_retire_current_locked(&destroy);
    generation = environment_next_generation_locked();
    replacement->generation = generation;
    environment_snapshot_link_locked(replacement);
    g_environment_current = replacement;
    g_environment_epoch = generation;
    environment_unlock();

    environment_snapshot_destroy(destroy);
    return 1;
}

static RinEnvironmentOwnedString* environment_make_owned_entry(
    const char* name, const char* value) {
    size_t name_length = environment_length(name);
    size_t value_length = environment_length(value);
    RinEnvironmentOwnedString* owner;
    size_t offset = 0u;
    if (value_length > SIZE_MAX - 2u ||
        name_length > SIZE_MAX - value_length - 2u) {
        errno = EOVERFLOW;
        return NULL;
    }
    owner = (RinEnvironmentOwnedString*)malloc(sizeof(*owner));
    if (!owner) {
        errno = ENOMEM;
        return NULL;
    }
    owner->value = (char*)malloc(name_length + value_length + 2u);
    if (!owner->value) {
        free(owner);
        errno = ENOMEM;
        return NULL;
    }
    for (size_t index = 0u; index < name_length; ++index)
        owner->value[offset++] = name[index];
    owner->value[offset++] = '=';
    for (size_t index = 0u; index < value_length; ++index)
        owner->value[offset++] = value[index];
    owner->value[offset] = '\0';
    owner->references = 1u;
    owner->owned = 1u;
    return owner;
}

static size_t environment_matching_count(char** table, size_t count,
                                         const char* name) {
    size_t matches = 0u;
    for (size_t index = 0u; index < count; ++index)
        if (environment_name_matches(table[index], name)) ++matches;
    return matches;
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

char* rin_getenv_copy(const char* name) {
    const int saved_errno = errno;
    unsigned int epoch;
    char** snapshot;
    const char* value;
    size_t length;
    char* copy;

    if (!environment_name_valid(name)) {
        errno = EINVAL;
        return NULL;
    }
    snapshot = __rin_env_read_begin(&epoch);
    value = __rin_env_get_from_snapshot(snapshot, name);
    if (!value) {
        __rin_env_read_end(epoch);
        errno = ENOENT;
        return NULL;
    }
    length = environment_length(value);
    if (length == SIZE_MAX) {
        __rin_env_read_end(epoch);
        errno = EOVERFLOW;
        return NULL;
    }
    copy = (char*)malloc(length + 1u);
    if (!copy) {
        __rin_env_read_end(epoch);
        errno = ENOMEM;
        return NULL;
    }
    for (size_t index = 0u; index <= length; ++index)
        copy[index] = value[index];
    __rin_env_read_end(epoch);
    errno = saved_errno;
    return copy;
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
    for (;;) {
        unsigned int source_generation;
        char** source = __rin_env_read_begin(&source_generation);
        size_t count = environment_count(source);
        size_t matches;
        size_t new_count;
        size_t write_index = 0u;
        int inserted = 0;
        RinEnvironmentOwnedString** source_owners = NULL;
        RinEnvironmentSnapshot* replacement;
        RinEnvironmentOwnedString* new_entry;
        if (count == SIZE_MAX) {
            __rin_env_read_end(source_generation);
            return -1;
        }
        matches = environment_matching_count(source, count, name);
        if (matches != 0u && !overwrite) {
            __rin_env_read_end(source_generation);
            return 0;
        }
        if (matches == 0u && count == SIZE_MAX - 1u) {
            errno = EOVERFLOW;
            __rin_env_read_end(source_generation);
            return -1;
        }
        new_count = matches == 0u ? count + 1u : count - matches + 1u;
        if (count != 0u) {
            if (count > SIZE_MAX / sizeof(*source_owners)) {
                errno = EOVERFLOW;
                __rin_env_read_end(source_generation);
                return -1;
            }
            source_owners = (RinEnvironmentOwnedString**)malloc(
                count * sizeof(*source_owners));
            if (!source_owners) {
                errno = ENOMEM;
                __rin_env_read_end(source_generation);
                return -1;
            }
            if (!environment_snapshot_copy_owners(
                    source_generation, source, count, source_owners)) {
                free(source_owners);
                __rin_env_read_end(source_generation);
                return -1;
            }
        }
        replacement = environment_snapshot_allocate(new_count);
        if (!replacement) {
            free(source_owners);
            __rin_env_read_end(source_generation);
            return -1;
        }
        new_entry = environment_make_owned_entry(name, value);
        if (!new_entry) {
            environment_snapshot_discard(replacement);
            free(source_owners);
            __rin_env_read_end(source_generation);
            return -1;
        }
        for (size_t index = 0u; index < count; ++index) {
            if (environment_name_matches(source[index], name)) {
                if (!inserted) {
                    replacement->entries[write_index] = new_entry->value;
                    replacement->owners[write_index++] = new_entry;
                    replacement->new_owners[write_index - 1u] = 1u;
                    inserted = 1;
                }
            } else {
                replacement->entries[write_index] = source[index];
                replacement->owners[write_index++] = source_owners[index];
            }
        }
        if (!inserted) {
            replacement->entries[write_index] = new_entry->value;
            replacement->owners[write_index++] = new_entry;
            replacement->new_owners[write_index - 1u] = 1u;
        }
        replacement->entries[write_index] = NULL;
        if (environment_publish(source, replacement, NULL)) {
            free(source_owners);
            __rin_env_read_end(source_generation);
            return 0;
        }
        environment_snapshot_discard(replacement);
        free(source_owners);
        __rin_env_read_end(source_generation);
    }
}

int unsetenv(const char* name) {
    if (!environment_name_valid(name)) {
        errno = EINVAL;
        return -1;
    }
    for (;;) {
        unsigned int source_generation;
        char** source = __rin_env_read_begin(&source_generation);
        size_t count = environment_count(source);
        size_t matches;
        size_t write_index = 0u;
        RinEnvironmentOwnedString** source_owners = NULL;
        RinEnvironmentSnapshot* replacement;
        if (count == SIZE_MAX) {
            __rin_env_read_end(source_generation);
            return -1;
        }
        matches = environment_matching_count(source, count, name);
        if (matches == 0u) {
            __rin_env_read_end(source_generation);
            return 0;
        }
        if (count > SIZE_MAX / sizeof(*source_owners)) {
            errno = EOVERFLOW;
            __rin_env_read_end(source_generation);
            return -1;
        }
        source_owners = (RinEnvironmentOwnedString**)malloc(
            count * sizeof(*source_owners));
        if (!source_owners) {
            errno = ENOMEM;
            __rin_env_read_end(source_generation);
            return -1;
        }
        if (!environment_snapshot_copy_owners(
                source_generation, source, count, source_owners)) {
            free(source_owners);
            __rin_env_read_end(source_generation);
            return -1;
        }
        replacement = environment_snapshot_allocate(count - matches);
        if (!replacement) {
            free(source_owners);
            __rin_env_read_end(source_generation);
            return -1;
        }
        for (size_t index = 0u; index < count; ++index) {
            if (!environment_name_matches(source[index], name)) {
                replacement->entries[write_index] = source[index];
                replacement->owners[write_index++] = source_owners[index];
            }
        }
        replacement->entries[write_index] = NULL;
        if (environment_publish(source, replacement, NULL)) {
            free(source_owners);
            __rin_env_read_end(source_generation);
            return 0;
        }
        environment_snapshot_discard(replacement);
        free(source_owners);
        __rin_env_read_end(source_generation);
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
    for (;;) {
        unsigned int source_generation;
        char** source = __rin_env_read_begin(&source_generation);
        size_t count = environment_count(source);
        size_t matches = 0u;
        size_t new_count;
        size_t write_index = 0u;
        int inserted = 0;
        RinEnvironmentOwnedString** source_owners = NULL;
        RinEnvironmentSnapshot* replacement;
        RinEnvironmentOwnedString* adopted_owner;
        if (count == SIZE_MAX) {
            __rin_env_read_end(source_generation);
            return -1;
        }
        for (size_t index = 0u; index < count; ++index) {
            size_t name_index = 0u;
            while (name_index < name_length &&
                   source[index][name_index] != '\0' &&
                   source[index][name_index] == string[name_index])
                ++name_index;
            if (name_index == name_length &&
                source[index][name_index] == '=')
                ++matches;
        }
        if (matches == 0u && count == SIZE_MAX - 1u) {
            errno = EOVERFLOW;
            __rin_env_read_end(source_generation);
            return -1;
        }
        new_count = matches == 0u ? count + 1u : count - matches + 1u;
        if (count != 0u) {
            if (count > SIZE_MAX / sizeof(*source_owners)) {
                errno = EOVERFLOW;
                __rin_env_read_end(source_generation);
                return -1;
            }
            source_owners = (RinEnvironmentOwnedString**)malloc(
                count * sizeof(*source_owners));
            if (!source_owners) {
                errno = ENOMEM;
                __rin_env_read_end(source_generation);
                return -1;
            }
            if (!environment_snapshot_copy_owners(
                    source_generation, source, count, source_owners)) {
                free(source_owners);
                __rin_env_read_end(source_generation);
                return -1;
            }
        }
        replacement = environment_snapshot_allocate(new_count);
        if (!replacement) {
            free(source_owners);
            __rin_env_read_end(source_generation);
            return -1;
        }
        adopted_owner = environment_owner_for_pointer(source_generation,
                                                       string);
        for (size_t index = 0u; index < count; ++index) {
            size_t name_index = 0u;
            while (name_index < name_length &&
                   source[index][name_index] != '\0' &&
                   source[index][name_index] == string[name_index])
                ++name_index;
            if (name_index == name_length &&
                source[index][name_index] == '=') {
                if (!inserted) {
                    replacement->entries[write_index] = string;
                    replacement->owners[write_index++] = adopted_owner;
                    inserted = 1;
                }
            } else {
                replacement->entries[write_index] = source[index];
                replacement->owners[write_index++] = source_owners[index];
            }
        }
        if (!inserted) {
            replacement->entries[write_index] = string;
            replacement->owners[write_index++] = adopted_owner;
        }
        replacement->entries[write_index] = NULL;
        if (environment_publish(source, replacement, adopted_owner)) {
            free(source_owners);
            __rin_env_read_end(source_generation);
            return 0;
        }
        environment_snapshot_discard(replacement);
        free(source_owners);
        __rin_env_read_end(source_generation);
    }
}

int clearenv(void) {
    for (;;) {
        unsigned int source_generation;
        char** source = __rin_env_read_begin(&source_generation);
        RinEnvironmentSnapshot* replacement =
            environment_snapshot_allocate(0u);
        if (!replacement) {
            __rin_env_read_end(source_generation);
            return -1;
        }
        if (environment_publish(source, replacement, NULL)) {
            __rin_env_read_end(source_generation);
            return 0;
        }
        environment_snapshot_discard(replacement);
        __rin_env_read_end(source_generation);
    }
}
