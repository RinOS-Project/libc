/* SPDX-License-Identifier: MIT */
/*
 * RinOS user-space allocator.
 *
 * The kernel-facing contract is deliberately small: the runtime supplies
 * anonymous-map and unmap hooks, while all block metadata, bins, splitting,
 * coalescing, and ownership decisions remain in this file.  A map header is
 * stored inside the mapping itself, so the allocator does not need a hidden
 * kernel-side allocation table.
 */

#include "rin_user_allocator.h"

#include "stdint.h"

#define RIN_USER_PAGE_SIZE ((size_t)4096u)
#define RIN_USER_BLOCK_ALIGNMENT ((size_t)16u)
#define RIN_USER_ARENA_CHUNK_SIZE ((size_t)64u * 1024u)
#define RIN_USER_ARENA_MIN_BLOCKS 8u
#define RIN_USER_ARENA_MAX_ALLOCATION ((size_t)32768u)
#define RIN_USER_ARENA_CLASS_COUNT 12u
#define RIN_USER_ARENA_LARGE_CLASS UINT16_MAX
#define RIN_USER_ARENA_MAP_MAGIC UINT32_C(0x52494e4d)
#define RIN_USER_ARENA_MAGIC_ALLOCATED UINT32_C(0x52494e41)
#define RIN_USER_ARENA_MAGIC_FREE UINT32_C(0x46524545)
#define RIN_USER_ARENA_KIND 1u
#define RIN_USER_LARGE_KIND 2u
#define RIN_USER_BLOCK_FLAG_ALLOCATED 1u
#define RIN_USER_BLOCK_FLAG_FREE 2u
#define RIN_USER_BLOCK_FLAG_LARGE 4u
#define RIN_USER_ARENA_TAIL_GUARD UINT64_C(0x72696e2d68656170)
#define RIN_USER_ARENA_TAIL_GUARD_SIZE ((size_t)16u)
#define RIN_USER_FREE_POISON ((unsigned char)0xddu)
#define RIN_USER_ALLOCATOR_BACKTRACE_DEPTH 4u

/* Keep the first-fit arena selection thread-local.  This is intentionally a
 * map hint rather than a private free-list: every block remains owned by its
 * arena lock, so a block may be freed by a different thread without a stale
 * cross-thread cache entry.  The hint is the extension point for a future
 * magazine cache, while preserving the current fork and corruption rules. */
#if defined(__GNUC__) || defined(__clang__)
#define RIN_USER_THREAD_LOCAL __thread
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define RIN_USER_THREAD_LOCAL _Thread_local
#else
#define RIN_USER_THREAD_LOCAL
#endif

#ifndef RIN_USER_ALLOCATOR_DEBUG_GUARD
#define RIN_USER_ALLOCATOR_DEBUG_GUARD 0
#endif

#define RIN_USER_PROT_NONE 0

typedef struct RinUserAllocatorMap RinUserAllocatorMap;
typedef struct RinUserAllocationHeader RinUserAllocationHeader;

typedef struct RinUserFreeLinks {
    struct RinUserFreeLinks* previous;
    struct RinUserFreeLinks* next;
} RinUserFreeLinks;

struct RinUserAllocationHeader {
    size_t total_size;
    size_t previous_size;
    size_t requested_size;
    uintptr_t map;
    uint32_t magic;
    uint16_t flags;
    uint16_t bin_index;
    uint64_t reserved;
    uint32_t backtrace_count;
    uint32_t reserved2;
    uint64_t reserved3;
    uintptr_t backtrace[RIN_USER_ALLOCATOR_BACKTRACE_DEPTH];
};

/* This keeps every user payload naturally 16-byte aligned on both targets. */
_Static_assert(sizeof(RinUserAllocationHeader) % RIN_USER_BLOCK_ALIGNMENT == 0u,
               "allocator block headers must be 16-byte aligned");

struct RinUserAllocatorMap {
    uint32_t magic;
    uint16_t kind;
    uint16_t reserved;
    size_t map_size;
    size_t data_offset;
    size_t data_end;
    RinUserAllocatorMap* next;
    volatile unsigned lock;
    uint32_t reserved2;
    RinUserAllocationHeader* first;
    RinUserFreeLinks* free_lists[RIN_USER_ARENA_CLASS_COUNT];
};

static RinUserAllocatorMap* rin_user_allocator_maps;
static volatile unsigned rin_user_allocator_directory_lock;

typedef struct RinUserAllocatorTlsCache {
    RinUserAllocatorMap* arena_by_class[RIN_USER_ARENA_CLASS_COUNT];
} RinUserAllocatorTlsCache;

static RIN_USER_THREAD_LOCAL RinUserAllocatorTlsCache
    rin_user_allocator_tls_cache;

static const uint32_t rin_user_size_classes[RIN_USER_ARENA_CLASS_COUNT] = {
    16u, 32u, 64u, 128u, 256u, 512u,
    1024u, 2048u, 4096u, 8192u, 16384u, 32768u,
};

static void rin_user_allocator_lock(volatile unsigned* lock)
{
    while (__atomic_exchange_n(lock, 1u, __ATOMIC_ACQUIRE) != 0u) {
#if defined(__i386__) || defined(__x86_64__)
        __asm__ volatile("pause");
#endif
    }
}

static void rin_user_allocator_unlock(volatile unsigned* lock)
{
    __atomic_store_n(lock, 0u, __ATOMIC_RELEASE);
}

/* Compatibility declarations let older host fixtures keep using the old
 * bump backend while product runtimes migrate to map/unmap.  New product
 * code must provide the map hooks. */
extern void* rin_user_allocator_backend_allocate(size_t size)
    __attribute__((weak));
extern void rin_user_allocator_backend_release(void* pointer)
    __attribute__((weak));

/* Keep sanitizer integration optional.  The allocator remains usable by
 * freestanding/product runtimes that do not link an ASan runtime, while a
 * hosted ASan build can observe user-after-free accesses through the normal
 * sanitizer annotations.  The free-list link prefix is intentionally kept
 * addressable because allocator metadata is stored there while a block is
 * free; only the remainder of the payload is poisoned. */
extern void __asan_poison_memory_region(const void* address, size_t size)
    __attribute__((weak));
extern void __asan_unpoison_memory_region(const void* address, size_t size)
    __attribute__((weak));

static void rin_user_asan_poison(const void* address, size_t size)
{
    if (address && size != 0u && __asan_poison_memory_region)
        __asan_poison_memory_region(address, size);
}

static void rin_user_asan_unpoison(const void* address, size_t size)
{
    if (address && size != 0u && __asan_unpoison_memory_region)
        __asan_unpoison_memory_region(address, size);
}

__attribute__((weak)) void* rin_user_allocator_backend_map(size_t size)
{
    if (rin_user_allocator_backend_allocate)
        return rin_user_allocator_backend_allocate(size);
    return (void*)0;
}

__attribute__((weak)) int rin_user_allocator_backend_unmap(void* pointer,
                                                            size_t size)
{
    (void)size;
    if (rin_user_allocator_backend_release) {
        rin_user_allocator_backend_release(pointer);
        return 0;
    }
    return -1;
}

__attribute__((weak)) int rin_user_allocator_backend_protect(void* pointer,
                                                              size_t size,
                                                              int prot)
{
    (void)pointer;
    (void)size;
    (void)prot;
    return -1;
}

__attribute__((weak)) void rin_user_allocator_corruption(void)
{
    /* A hosted test can observe this hook.  Product runtimes provide a
     * strong implementation that logs and terminates the process. */
}

__attribute__((weak)) size_t rin_user_allocator_capture_backtrace(
    uintptr_t* frames, size_t capacity)
{
    (void)frames;
    (void)capacity;
    return 0u;
}

static size_t rin_user_align_up(size_t value)
{
    const size_t mask = RIN_USER_BLOCK_ALIGNMENT - 1u;
    if (value > (size_t)-1 - mask) return 0u;
    return (value + mask) & ~mask;
}

static size_t rin_user_page_align_up(size_t value)
{
    const size_t mask = RIN_USER_PAGE_SIZE - 1u;
    if (value > (size_t)-1 - mask) return 0u;
    return (value + mask) & ~mask;
}

static size_t rin_user_min_block_size(void)
{
    size_t size = sizeof(RinUserAllocationHeader) +
                  RIN_USER_ARENA_TAIL_GUARD_SIZE +
                  RIN_USER_BLOCK_ALIGNMENT;
    return rin_user_align_up(size);
}

static size_t rin_user_block_total_for_request(size_t request)
{
    size_t payload = rin_user_align_up(request);
    size_t total;
    if (payload == 0u && request != 0u) return 0u;
    if (payload > (size_t)-1 - sizeof(RinUserAllocationHeader) -
                      RIN_USER_ARENA_TAIL_GUARD_SIZE)
        return 0u;
    total = payload + sizeof(RinUserAllocationHeader) +
            RIN_USER_ARENA_TAIL_GUARD_SIZE;
    return rin_user_align_up(total);
}

static unsigned rin_user_bin_for_size(size_t size)
{
    unsigned index;
    for (index = 0u; index < RIN_USER_ARENA_CLASS_COUNT; ++index) {
        if (size <= (size_t)rin_user_size_classes[index]) return index;
    }
    return RIN_USER_ARENA_CLASS_COUNT - 1u;
}

static unsigned char* rin_user_block_payload(RinUserAllocationHeader* block)
{
    return (unsigned char*)(block + 1);
}

static unsigned char* rin_user_block_tail(const RinUserAllocationHeader* block)
{
    return (unsigned char*)(uintptr_t)block + block->total_size -
           RIN_USER_ARENA_TAIL_GUARD_SIZE;
}

static void rin_user_block_store_tail(RinUserAllocationHeader* block)
{
    uint64_t guard = RIN_USER_ARENA_TAIL_GUARD;
    uint64_t inverse = ~RIN_USER_ARENA_TAIL_GUARD;
    unsigned char* tail = rin_user_block_tail(block);
    unsigned index;
    for (index = 0u; index < 8u; ++index) {
        tail[index] = (unsigned char)(guard & 0xffu);
        guard >>= 8u;
        tail[8u + index] = (unsigned char)(inverse & 0xffu);
        inverse >>= 8u;
    }
}

static void rin_user_block_poison_free(RinUserAllocationHeader* block)
{
    unsigned char* payload = rin_user_block_payload(block);
    size_t payload_size = block->total_size - sizeof(*block) -
                          RIN_USER_ARENA_TAIL_GUARD_SIZE;
    size_t index;
    for (index = 0u; index < payload_size; ++index)
        payload[index] = RIN_USER_FREE_POISON;
}

static int rin_user_block_tail_valid(const RinUserAllocationHeader* block)
{
    const unsigned char* tail = rin_user_block_tail(block);
    uint64_t guard = 0u;
    uint64_t inverse = 0u;
    unsigned index;
    for (index = 0u; index < 8u; ++index) {
        guard |= (uint64_t)tail[index] << (index * 8u);
        inverse |= (uint64_t)tail[8u + index] << (index * 8u);
    }
    return guard == RIN_USER_ARENA_TAIL_GUARD &&
           inverse == ~RIN_USER_ARENA_TAIL_GUARD;
}

static int rin_user_map_shape_valid(const RinUserAllocatorMap* map)
{
    uintptr_t base;
    uintptr_t end;
    size_t minimum_offset;
    size_t minimum_map_size;
    if (!map || map->magic != RIN_USER_ARENA_MAP_MAGIC ||
        (map->kind != RIN_USER_ARENA_KIND &&
         map->kind != RIN_USER_LARGE_KIND))
        return 0;
    minimum_offset = rin_user_align_up(sizeof(RinUserAllocatorMap));
    minimum_map_size = rin_user_min_block_size();
    if (minimum_offset == 0u || minimum_map_size == 0u ||
        map->data_offset < minimum_offset ||
        map->data_offset > (size_t)-1 - minimum_map_size ||
        map->data_offset + minimum_map_size > map->data_end ||
        map->data_end > map->map_size)
        return 0;
    base = (uintptr_t)map;
    if (base > UINTPTR_MAX - map->map_size) return 0;
    end = base + map->map_size;
    if (end <= base || ((base | map->data_offset) &
                        (RIN_USER_BLOCK_ALIGNMENT - 1u)) != 0u)
        return 0;
    return 1;
}

static int rin_user_map_range_valid(const RinUserAllocatorMap* map,
                                    uintptr_t address, size_t size)
{
    uintptr_t base;
    uintptr_t end;
    if (!rin_user_map_shape_valid(map)) return 0;
    base = (uintptr_t)map;
    end = base + map->map_size;
    if (address < base || address > end) return 0;
    return size <= (size_t)(end - address);
}

static RinUserAllocationHeader* rin_user_next_block(
    const RinUserAllocatorMap* map, RinUserAllocationHeader* block)
{
    uintptr_t address;
    uintptr_t end;
    if (!map || !block || block->total_size == 0u)
        return (RinUserAllocationHeader*)0;
    address = (uintptr_t)block;
    end = (uintptr_t)map + map->data_end;
    if (address < (uintptr_t)map + map->data_offset ||
        address > end || block->total_size > (size_t)(end - address))
        return (RinUserAllocationHeader*)0;
    address += block->total_size;
    return address == end ? (RinUserAllocationHeader*)0
                          : (RinUserAllocationHeader*)(uintptr_t)address;
}

static RinUserAllocationHeader* rin_user_previous_block(
    const RinUserAllocatorMap* map, RinUserAllocationHeader* block)
{
    uintptr_t address;
    if (!map || !block || block->previous_size == 0u)
        return (RinUserAllocationHeader*)0;
    address = (uintptr_t)block;
    if (address < (uintptr_t)map + map->data_offset + block->previous_size)
        return (RinUserAllocationHeader*)0;
    return (RinUserAllocationHeader*)(uintptr_t)(address -
                                                  block->previous_size);
}

static int rin_user_map_block_graph_valid(const RinUserAllocatorMap* map)
{
    uintptr_t begin;
    uintptr_t end;
    uintptr_t cursor;
    size_t previous_size = 0u;
    size_t maximum_blocks;
    size_t block_count = 0u;
    int saw_block = 0;

    if (!rin_user_map_shape_valid(map)) return 0;
    begin = (uintptr_t)map + map->data_offset;
    end = (uintptr_t)map + map->data_end;
    cursor = begin;
    maximum_blocks = (map->data_end - map->data_offset) /
                         rin_user_min_block_size() + 1u;
    while (cursor < end) {
        RinUserAllocationHeader* block =
            (RinUserAllocationHeader*)(uintptr_t)cursor;
        size_t capacity;
        if (++block_count > maximum_blocks ||
            block->map != (uintptr_t)map ||
            block->previous_size != previous_size ||
            block->total_size < rin_user_min_block_size() ||
            (block->total_size & (RIN_USER_BLOCK_ALIGNMENT - 1u)) != 0u ||
            block->total_size > (size_t)(end - cursor) ||
            (block->magic != RIN_USER_ARENA_MAGIC_ALLOCATED &&
             block->magic != RIN_USER_ARENA_MAGIC_FREE))
            return 0;
        capacity = block->total_size - sizeof(*block) -
                   RIN_USER_ARENA_TAIL_GUARD_SIZE;
        if (block->magic == RIN_USER_ARENA_MAGIC_FREE) {
            if (block->flags != RIN_USER_BLOCK_FLAG_FREE ||
                block->requested_size != 0u ||
                block->bin_index != rin_user_bin_for_size(capacity) ||
                block->backtrace_count != 0u)
                return 0;
        } else if (block->flags != RIN_USER_BLOCK_FLAG_ALLOCATED &&
                   block->flags != RIN_USER_BLOCK_FLAG_LARGE) {
            return 0;
        } else if (block->requested_size == 0u ||
                   block->requested_size > capacity ||
                   block->backtrace_count > RIN_USER_ALLOCATOR_BACKTRACE_DEPTH) {
            return 0;
        }
        if (!rin_user_block_tail_valid(block)) return 0;
        saw_block = 1;
        previous_size = block->total_size;
        cursor += block->total_size;
    }
    return saw_block && cursor == end;
}

static int rin_user_header_is_exact(const RinUserAllocatorMap* map,
                                    RinUserAllocationHeader* candidate)
{
    RinUserAllocationHeader* block;
    uintptr_t cursor;
    uintptr_t end;
    if (!rin_user_map_shape_valid(map) ||
        !rin_user_map_range_valid(map, (uintptr_t)candidate,
                                  sizeof(*candidate)))
        return 0;
    cursor = (uintptr_t)map + map->data_offset;
    end = (uintptr_t)map + map->data_end;
    while (cursor < end) {
        block = (RinUserAllocationHeader*)(uintptr_t)cursor;
        if (block == candidate) return 1;
        if (block->total_size < rin_user_min_block_size() ||
            block->total_size > (size_t)(end - cursor))
            return 0;
        cursor += block->total_size;
    }
    return 0;
}

static RinUserAllocationHeader* rin_user_block_from_free_link(
    const RinUserAllocatorMap* map, RinUserFreeLinks* link)
{
    uintptr_t link_address;
    uintptr_t header_address;
    if (!map || !link) return (RinUserAllocationHeader*)0;
    link_address = (uintptr_t)link;
    if (link_address < (uintptr_t)map + map->data_offset +
                           sizeof(RinUserAllocationHeader) ||
        link_address > UINTPTR_MAX - sizeof(RinUserAllocationHeader))
        return (RinUserAllocationHeader*)0;
    header_address = link_address - sizeof(RinUserAllocationHeader);
    if (!rin_user_header_is_exact(
            map, (RinUserAllocationHeader*)(uintptr_t)header_address))
        return (RinUserAllocationHeader*)0;
    return (RinUserAllocationHeader*)(uintptr_t)header_address;
}

static int rin_user_map_free_lists_valid(const RinUserAllocatorMap* map)
{
    unsigned index;
    size_t maximum_entries;
    if (!rin_user_map_block_graph_valid(map)) return 0;
    maximum_entries = (map->data_end - map->data_offset) /
                      rin_user_min_block_size() + 1u;
    for (index = 0u; index < RIN_USER_ARENA_CLASS_COUNT; ++index) {
        RinUserFreeLinks* previous = (RinUserFreeLinks*)0;
        RinUserFreeLinks* link = map->free_lists[index];
        size_t entries = 0u;
        while (link) {
            RinUserAllocationHeader* block;
            if (++entries > maximum_entries ||
                !rin_user_map_range_valid(map, (uintptr_t)link,
                                           sizeof(*link)) ||
                (block = rin_user_block_from_free_link(map, link)) == NULL ||
                block->magic != RIN_USER_ARENA_MAGIC_FREE ||
                block->bin_index != index ||
                link->previous != previous)
                return 0;
            previous = link;
            link = link->next;
        }
    }
    return 1;
}

static RinUserAllocationHeader* rin_user_find_block_locked(
    const RinUserAllocatorMap* map, const void* pointer)
{
    RinUserAllocationHeader* block;
    uintptr_t target;
    if (!pointer || !rin_user_map_shape_valid(map)) return NULL;
    target = (uintptr_t)pointer;
    if (!rin_user_map_range_valid(map, target, 1u)) return NULL;
    block = map->first;
    while (block) {
        if ((const void*)rin_user_block_payload(block) == pointer) return block;
        block = rin_user_next_block(map, block);
    }
    return NULL;
}

static int rin_user_remove_free_block(RinUserAllocatorMap* map,
                                      RinUserAllocationHeader* block)
{
    RinUserFreeLinks* links;
    RinUserFreeLinks* previous;
    RinUserFreeLinks* next;
    if (!map || !block || block->magic != RIN_USER_ARENA_MAGIC_FREE ||
        block->bin_index >= RIN_USER_ARENA_CLASS_COUNT)
        return 0;
    links = (RinUserFreeLinks*)rin_user_block_payload(block);
    previous = links->previous;
    next = links->next;
    if ((previous &&
         rin_user_block_from_free_link(map, previous) == NULL) ||
        (next && rin_user_block_from_free_link(map, next) == NULL))
        return 0;
    if (previous) previous->next = next;
    else map->free_lists[block->bin_index] = next;
    if (next) next->previous = previous;
    links->previous = NULL;
    links->next = NULL;
    return 1;
}

static void rin_user_insert_free_block(RinUserAllocatorMap* map,
                                       RinUserAllocationHeader* block)
{
    size_t payload_size = block->total_size - sizeof(*block) -
                          RIN_USER_ARENA_TAIL_GUARD_SIZE;
    unsigned char* payload = rin_user_block_payload(block);
    unsigned trace_index;
    unsigned index = rin_user_bin_for_size(payload_size);
    RinUserFreeLinks* links = (RinUserFreeLinks*)payload;
    RinUserFreeLinks* first = map->free_lists[index];
    /* A block may have been poisoned while it was allocated, and the poison
     * helper below writes the whole payload before the links are restored. */
    rin_user_asan_unpoison(payload, payload_size);
    rin_user_block_poison_free(block);
    block->magic = RIN_USER_ARENA_MAGIC_FREE;
    block->flags = RIN_USER_BLOCK_FLAG_FREE;
    block->requested_size = 0u;
    block->bin_index = (uint16_t)index;
    block->reserved = 0u;
    block->backtrace_count = 0u;
    block->reserved2 = 0u;
    block->reserved3 = 0u;
    for (trace_index = 0u;
         trace_index < RIN_USER_ALLOCATOR_BACKTRACE_DEPTH; ++trace_index)
        block->backtrace[trace_index] = 0u;
    links->previous = NULL;
    links->next = first;
    if (first) first->previous = links;
    map->free_lists[index] = links;
    rin_user_block_store_tail(block);
    if (payload_size > sizeof(RinUserFreeLinks))
        rin_user_asan_poison(payload + sizeof(RinUserFreeLinks),
                             payload_size - sizeof(RinUserFreeLinks));
}

static void rin_user_invalidate_block(RinUserAllocationHeader* block)
{
    unsigned index;
    block->magic = 0u;
    block->flags = 0u;
    block->total_size = 0u;
    block->previous_size = 0u;
    block->requested_size = 0u;
    block->map = 0u;
    block->bin_index = 0u;
    block->reserved = 0u;
    block->backtrace_count = 0u;
    block->reserved2 = 0u;
    block->reserved3 = 0u;
    for (index = 0u; index < RIN_USER_ALLOCATOR_BACKTRACE_DEPTH; ++index)
        block->backtrace[index] = 0u;
}

static void rin_user_mark_allocated(RinUserAllocationHeader* block,
                                    size_t requested_size, uint16_t flags,
                                    const uintptr_t* backtrace)
{
    size_t payload_size = block->total_size - sizeof(*block) -
                          RIN_USER_ARENA_TAIL_GUARD_SIZE;
    unsigned index;
    block->magic = RIN_USER_ARENA_MAGIC_ALLOCATED;
    block->flags = flags;
    block->requested_size = requested_size;
    block->bin_index = RIN_USER_ARENA_LARGE_CLASS;
    block->reserved = 0u;
    block->backtrace_count = 0u;
    block->reserved2 = 0u;
    for (index = 0u; index < RIN_USER_ALLOCATOR_BACKTRACE_DEPTH; ++index) {
        block->backtrace[index] = backtrace ? backtrace[index] : 0u;
        if (block->backtrace[index] != 0u)
            block->backtrace_count = index + 1u;
    }
    rin_user_block_store_tail(block);
    rin_user_asan_unpoison(rin_user_block_payload(block), payload_size);
}

static int rin_user_split_allocated_tail(RinUserAllocatorMap* map,
                                         RinUserAllocationHeader* block,
                                         size_t requested_total)
{
    size_t remainder_size;
    RinUserAllocationHeader* remainder;
    RinUserAllocationHeader* following;
    if (requested_total > block->total_size) return 0;
    remainder_size = block->total_size - requested_total;
    if (remainder_size < rin_user_min_block_size()) return 1;
    following = rin_user_next_block(map, block);
    block->total_size = requested_total;
    remainder = (RinUserAllocationHeader*)((unsigned char*)block +
                                           requested_total);
    remainder->total_size = remainder_size;
    remainder->previous_size = requested_total;
    remainder->requested_size = 0u;
    remainder->map = (uintptr_t)map;
    remainder->magic = RIN_USER_ARENA_MAGIC_FREE;
    remainder->flags = RIN_USER_BLOCK_FLAG_FREE;
    remainder->bin_index = 0u;
    remainder->reserved = 0u;
    remainder->backtrace_count = 0u;
    remainder->reserved2 = 0u;
    remainder->reserved3 = 0u;
    for (unsigned trace_index = 0u;
         trace_index < RIN_USER_ALLOCATOR_BACKTRACE_DEPTH; ++trace_index)
        remainder->backtrace[trace_index] = 0u;
    if (following) following->previous_size = remainder_size;
    if (following && following->magic == RIN_USER_ARENA_MAGIC_FREE) {
        if (!rin_user_remove_free_block(map, following) ||
            remainder_size > (size_t)-1 - following->total_size)
            return 0;
        remainder->total_size += following->total_size;
        following = rin_user_next_block(map, remainder);
        if (following) following->previous_size = remainder->total_size;
        rin_user_invalidate_block((RinUserAllocationHeader*)
                                  ((unsigned char*)remainder +
                                   remainder_size));
    }
    rin_user_insert_free_block(map, remainder);
    rin_user_block_store_tail(block);
    return 1;
}

static void* rin_user_take_free_block(RinUserAllocatorMap* map,
                                      RinUserAllocationHeader* block,
                                      size_t requested_size,
                                      size_t requested_total,
                                      const uintptr_t* backtrace)
{
    if (!rin_user_remove_free_block(map, block)) return NULL;
    if (rin_user_split_allocated_tail(map, block, requested_total) == 0)
        return NULL;
    rin_user_mark_allocated(block, requested_size,
                            RIN_USER_BLOCK_FLAG_ALLOCATED, backtrace);
    return rin_user_block_payload(block);
}

static RinUserAllocationHeader* rin_user_find_free_block(
    RinUserAllocatorMap* map, size_t requested_size)
{
    unsigned index = rin_user_bin_for_size(requested_size);
    for (; index < RIN_USER_ARENA_CLASS_COUNT; ++index) {
        RinUserFreeLinks* link = map->free_lists[index];
        while (link) {
            RinUserAllocationHeader* block =
                rin_user_block_from_free_link(map, link);
            if (!block) return NULL;
            if (block->total_size >= rin_user_block_total_for_request(
                                        requested_size))
                return block;
            link = link->next;
        }
    }
    return NULL;
}

static void rin_user_map_metadata_clear(RinUserAllocatorMap* map,
                                        size_t map_size, uint16_t kind)
{
    unsigned index;
    map->magic = RIN_USER_ARENA_MAP_MAGIC;
    map->kind = kind;
    map->reserved = 0u;
    map->map_size = map_size;
    map->data_offset = rin_user_align_up(sizeof(*map));
    map->data_end = map_size;
    map->next = NULL;
    map->lock = 0u;
    map->reserved2 = 0u;
    map->first = (RinUserAllocationHeader*)((unsigned char*)map +
                                            map->data_offset);
    for (index = 0u; index < RIN_USER_ARENA_CLASS_COUNT; ++index)
        map->free_lists[index] = NULL;
}

static RinUserAllocatorMap* rin_user_create_arena(void)
{
    RinUserAllocatorMap* map = (RinUserAllocatorMap*)
        rin_user_allocator_backend_map(RIN_USER_ARENA_CHUNK_SIZE);
    size_t total;
    if (!map || ((uintptr_t)map & (RIN_USER_PAGE_SIZE - 1u)) != 0u) {
        if (map) (void)rin_user_allocator_backend_unmap(
            map, RIN_USER_ARENA_CHUNK_SIZE);
        return NULL;
    }
    rin_user_map_metadata_clear(map, RIN_USER_ARENA_CHUNK_SIZE,
                                RIN_USER_ARENA_KIND);
    total = map->map_size - map->data_offset;
    if (total < rin_user_min_block_size() * RIN_USER_ARENA_MIN_BLOCKS) {
        (void)rin_user_allocator_backend_unmap(map,
                                                RIN_USER_ARENA_CHUNK_SIZE);
        return NULL;
    }
    map->first->total_size = total;
    map->first->previous_size = 0u;
    map->first->requested_size = 0u;
    map->first->map = (uintptr_t)map;
    map->first->magic = RIN_USER_ARENA_MAGIC_FREE;
    map->first->flags = RIN_USER_BLOCK_FLAG_FREE;
    map->first->bin_index = 0u;
    map->first->reserved = 0u;
    map->first->backtrace_count = 0u;
    map->first->reserved2 = 0u;
    map->first->reserved3 = 0u;
    for (unsigned trace_index = 0u;
         trace_index < RIN_USER_ALLOCATOR_BACKTRACE_DEPTH; ++trace_index)
        map->first->backtrace[trace_index] = 0u;
    rin_user_insert_free_block(map, map->first);
    return map;
}

static RinUserAllocatorMap* rin_user_create_large(size_t requested_size,
                                                   size_t requested_total,
                                                   const uintptr_t* backtrace)
{
    size_t minimum_offset = rin_user_align_up(sizeof(RinUserAllocatorMap));
    size_t required;
    size_t data_end;
    size_t map_size;
    RinUserAllocatorMap* map;
    if (minimum_offset == 0u || requested_total == 0u ||
        minimum_offset > (size_t)-1 - requested_total)
        return NULL;
    required = minimum_offset + requested_total;
    data_end = rin_user_page_align_up(required);
    if (data_end == 0u) return NULL;
#if RIN_USER_ALLOCATOR_DEBUG_GUARD
    if (data_end > (size_t)-1 - RIN_USER_PAGE_SIZE) return NULL;
    map_size = data_end + RIN_USER_PAGE_SIZE;
#else
    map_size = data_end;
#endif
    map = (RinUserAllocatorMap*)rin_user_allocator_backend_map(map_size);
    if (!map || ((uintptr_t)map & (RIN_USER_PAGE_SIZE - 1u)) != 0u) {
        if (map) (void)rin_user_allocator_backend_unmap(map, map_size);
        return NULL;
    }
    rin_user_map_metadata_clear(map, map_size, RIN_USER_LARGE_KIND);
    map->data_end = data_end;
    map->first->total_size = data_end - map->data_offset;
    map->first->previous_size = 0u;
    map->first->map = (uintptr_t)map;
    rin_user_mark_allocated(map->first, requested_size,
                            RIN_USER_BLOCK_FLAG_LARGE, backtrace);
#if RIN_USER_ALLOCATOR_DEBUG_GUARD
    if (rin_user_allocator_backend_protect(
            (unsigned char*)map + data_end, RIN_USER_PAGE_SIZE,
            RIN_USER_PROT_NONE) != 0) {
        (void)rin_user_allocator_backend_unmap(map, map_size);
        return NULL;
    }
#endif
    return map;
}

static void rin_user_map_insert(RinUserAllocatorMap* map)
{
    map->next = rin_user_allocator_maps;
    rin_user_allocator_maps = map;
}

static RinUserAllocatorMap* rin_user_find_map(const void* pointer)
{
    RinUserAllocatorMap* map;
    uintptr_t address = (uintptr_t)pointer;
    for (map = rin_user_allocator_maps; map; map = map->next) {
        if (rin_user_map_shape_valid(map) &&
            rin_user_map_range_valid(map, address, 1u))
            return map;
    }
    return NULL;
}

static int rin_user_unlink_map(RinUserAllocatorMap* target)
{
    RinUserAllocatorMap** cursor = &rin_user_allocator_maps;
    while (*cursor) {
        if (*cursor == target) {
            *cursor = target->next;
            target->next = NULL;
            return 1;
        }
        cursor = &(*cursor)->next;
    }
    return 0;
}

static int rin_user_release_block(RinUserAllocatorMap* map,
                                  RinUserAllocationHeader* block)
{
    RinUserAllocationHeader* next;
    RinUserAllocationHeader* previous;
    if (!map || !block || block->flags != RIN_USER_BLOCK_FLAG_ALLOCATED ||
        block->magic != RIN_USER_ARENA_MAGIC_ALLOCATED)
        return 0;
    block->magic = RIN_USER_ARENA_MAGIC_FREE;
    block->flags = RIN_USER_BLOCK_FLAG_FREE;
    block->requested_size = 0u;
    block->bin_index = 0u;
    next = rin_user_next_block(map, block);
    if (next && next->magic == RIN_USER_ARENA_MAGIC_FREE) {
        if (!rin_user_remove_free_block(map, next) ||
            block->total_size > (size_t)-1 - next->total_size)
            return 0;
        block->total_size += next->total_size;
        rin_user_invalidate_block(next);
        next = rin_user_next_block(map, block);
        if (next) next->previous_size = block->total_size;
    }
    previous = rin_user_previous_block(map, block);
    if (previous && previous->magic == RIN_USER_ARENA_MAGIC_FREE) {
        if (!rin_user_remove_free_block(map, previous) ||
            previous->total_size > (size_t)-1 - block->total_size)
            return 0;
        previous->total_size += block->total_size;
        rin_user_invalidate_block(block);
        block = previous;
        next = rin_user_next_block(map, block);
        if (next) next->previous_size = block->total_size;
    }
    rin_user_insert_free_block(map, block);
    return 1;
}

void* rin_user_allocator_malloc(size_t size)
{
    size_t requested_total;
    size_t captured_backtrace;
    uintptr_t allocation_backtrace[RIN_USER_ALLOCATOR_BACKTRACE_DEPTH] = {0};
    RinUserAllocatorMap* map;
    RinUserAllocationHeader* block;
    void* result;

    if (size == 0u ||
        (requested_total = rin_user_block_total_for_request(size)) == 0u)
        return NULL;

    captured_backtrace = rin_user_allocator_capture_backtrace(
        allocation_backtrace, RIN_USER_ALLOCATOR_BACKTRACE_DEPTH);
    if (captured_backtrace > RIN_USER_ALLOCATOR_BACKTRACE_DEPTH)
        captured_backtrace = RIN_USER_ALLOCATOR_BACKTRACE_DEPTH;
    if (captured_backtrace == 0u) {
#if defined(__GNUC__) || defined(__clang__)
        allocation_backtrace[0] = (uintptr_t)__builtin_return_address(0);
#endif
    }

    rin_user_allocator_lock(&rin_user_allocator_directory_lock);
    if (size <= RIN_USER_ARENA_MAX_ALLOCATION) {
        unsigned hint_index = rin_user_bin_for_size(size);
        RinUserAllocatorMap* hinted =
            rin_user_allocator_tls_cache.arena_by_class[hint_index];
        if (hinted && hinted->kind == RIN_USER_ARENA_KIND) {
            rin_user_allocator_lock(&hinted->lock);
            if (!rin_user_map_free_lists_valid(hinted)) {
                rin_user_allocator_unlock(&hinted->lock);
                rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
                rin_user_allocator_corruption();
                return NULL;
            }
            block = rin_user_find_free_block(hinted, size);
            if (block) {
                result = rin_user_take_free_block(hinted, block, size,
                                                  requested_total,
                                                  allocation_backtrace);
                rin_user_allocator_unlock(&hinted->lock);
                rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
                return result;
            }
            rin_user_allocator_unlock(&hinted->lock);
        }
        for (map = rin_user_allocator_maps; map; map = map->next) {
            if (map->kind != RIN_USER_ARENA_KIND) continue;
            if (map == hinted) continue;
            rin_user_allocator_lock(&map->lock);
            if (!rin_user_map_free_lists_valid(map)) {
                rin_user_allocator_unlock(&map->lock);
                rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
                rin_user_allocator_corruption();
                return NULL;
            }
            block = rin_user_find_free_block(map, size);
            if (block) {
                result = rin_user_take_free_block(map, block, size,
                                                  requested_total,
                                                  allocation_backtrace);
                rin_user_allocator_tls_cache.arena_by_class[hint_index] = map;
                rin_user_allocator_unlock(&map->lock);
                rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
                return result;
            }
            rin_user_allocator_unlock(&map->lock);
        }
        map = rin_user_create_arena();
        if (map) {
            rin_user_map_insert(map);
            block = map->first;
            result = rin_user_take_free_block(map, block, size,
                                              requested_total,
                                              allocation_backtrace);
            rin_user_allocator_tls_cache.arena_by_class[hint_index] = map;
            rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
            return result;
        }
    } else {
        map = rin_user_create_large(size, requested_total,
                                    allocation_backtrace);
        if (map) {
            rin_user_map_insert(map);
            rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
            return rin_user_block_payload(map->first);
        }
    }
    rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
    return NULL;
}

void rin_user_allocator_after_fork_child(void)
{
    RinUserAllocatorMap* map;
    unsigned index;

    /* No lock can be held by a surviving child thread.  Reset the directory
     * first so the walk below cannot deadlock on the inherited owner. */
    rin_user_allocator_directory_lock = 0u;
    for (index = 0u; index < RIN_USER_ARENA_CLASS_COUNT; ++index)
        rin_user_allocator_tls_cache.arena_by_class[index] = NULL;
    for (map = rin_user_allocator_maps; map; map = map->next)
        map->lock = 0u;
}

void rin_user_allocator_free(void* pointer)
{
    RinUserAllocatorMap* map;
    RinUserAllocationHeader* block;
    size_t map_size;
    int large = 0;

    if (!pointer) return;
    rin_user_allocator_lock(&rin_user_allocator_directory_lock);
    map = rin_user_find_map(pointer);
    if (!map) {
        rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
        rin_user_allocator_corruption();
        return;
    }
    rin_user_allocator_lock(&map->lock);
    if (!rin_user_map_free_lists_valid(map) ||
        (block = rin_user_find_block_locked(map, pointer)) == NULL ||
        block->magic != RIN_USER_ARENA_MAGIC_ALLOCATED ||
        !rin_user_block_tail_valid(block)) {
        rin_user_allocator_unlock(&map->lock);
        rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
        rin_user_allocator_corruption();
        return;
    }
    if (map->kind == RIN_USER_LARGE_KIND) {
        if (block != map->first || block->flags != RIN_USER_BLOCK_FLAG_LARGE) {
            rin_user_allocator_unlock(&map->lock);
            rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
            rin_user_allocator_corruption();
            return;
        }
        map_size = map->map_size;
        large = 1;
        rin_user_asan_poison(
            rin_user_block_payload(block),
            block->total_size - sizeof(*block) - RIN_USER_ARENA_TAIL_GUARD_SIZE);
        (void)rin_user_unlink_map(map);
    } else if (!rin_user_release_block(map, block)) {
        rin_user_allocator_unlock(&map->lock);
        rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
        rin_user_allocator_corruption();
        return;
    }
    rin_user_allocator_unlock(&map->lock);
    rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
    if (large && rin_user_allocator_backend_unmap(map, map_size) != 0) {
        /* A failed unmap must not leave the allocator believing ownership was
         * released.  Re-publish the still-valid mapping and fail closed. */
        rin_user_allocator_lock(&rin_user_allocator_directory_lock);
        rin_user_map_insert(map);
        rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
        rin_user_allocator_corruption();
    }
}

size_t rin_user_allocator_usable_size(const void* pointer)
{
    RinUserAllocatorMap* map;
    RinUserAllocationHeader* block;
    size_t result;
    if (!pointer) return 0u;
    rin_user_allocator_lock(&rin_user_allocator_directory_lock);
    map = rin_user_find_map(pointer);
    if (!map) {
        rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
        rin_user_allocator_corruption();
        return 0u;
    }
    rin_user_allocator_lock(&map->lock);
    block = rin_user_find_block_locked(map, pointer);
    if (!rin_user_map_free_lists_valid(map) || !block ||
        block->magic != RIN_USER_ARENA_MAGIC_ALLOCATED ||
        !rin_user_block_tail_valid(block)) {
        rin_user_allocator_unlock(&map->lock);
        rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
        rin_user_allocator_corruption();
        return 0u;
    }
    result = map->kind == RIN_USER_LARGE_KIND
        ? block->requested_size
        : block->total_size - sizeof(*block) -
              RIN_USER_ARENA_TAIL_GUARD_SIZE;
    rin_user_allocator_unlock(&map->lock);
    rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
    return result;
}

size_t rin_user_allocator_get_backtrace(const void* pointer,
                                        uintptr_t* frames,
                                        size_t capacity)
{
    RinUserAllocatorMap* map;
    RinUserAllocationHeader* block;
    size_t total;
    size_t copy_count;
    size_t index;

    if (!pointer || (capacity != 0u && !frames)) {
        if (pointer) rin_user_allocator_corruption();
        return 0u;
    }
    rin_user_allocator_lock(&rin_user_allocator_directory_lock);
    map = rin_user_find_map(pointer);
    if (!map) {
        rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
        rin_user_allocator_corruption();
        return 0u;
    }
    rin_user_allocator_lock(&map->lock);
    block = rin_user_find_block_locked(map, pointer);
    if (!rin_user_map_free_lists_valid(map) || !block ||
        block->magic != RIN_USER_ARENA_MAGIC_ALLOCATED ||
        !rin_user_block_tail_valid(block) ||
        block->backtrace_count > RIN_USER_ALLOCATOR_BACKTRACE_DEPTH) {
        rin_user_allocator_unlock(&map->lock);
        rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
        rin_user_allocator_corruption();
        return 0u;
    }
    total = block->backtrace_count;
    copy_count = total < capacity ? total : capacity;
    for (index = 0u; index < copy_count; ++index)
        frames[index] = block->backtrace[index];
    rin_user_allocator_unlock(&map->lock);
    rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
    return total;
}

void* rin_user_allocator_realloc(void* pointer, size_t size)
{
    RinUserAllocatorMap* map;
    RinUserAllocationHeader* block;
    RinUserAllocationHeader* next;
    size_t requested_total;
    size_t old_size;
    size_t copy_size;
    unsigned char* replacement;
    const unsigned char* source;
    size_t index;

    if (!pointer) return rin_user_allocator_malloc(size);
    if (size == 0u) {
        rin_user_allocator_free(pointer);
        return NULL;
    }
    requested_total = rin_user_block_total_for_request(size);
    if (requested_total == 0u) return NULL;

    rin_user_allocator_lock(&rin_user_allocator_directory_lock);
    map = rin_user_find_map(pointer);
    if (!map) {
        rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
        rin_user_allocator_corruption();
        return NULL;
    }
    rin_user_allocator_lock(&map->lock);
    block = rin_user_find_block_locked(map, pointer);
    if (!rin_user_map_free_lists_valid(map) || !block ||
        block->magic != RIN_USER_ARENA_MAGIC_ALLOCATED ||
        !rin_user_block_tail_valid(block)) {
        rin_user_allocator_unlock(&map->lock);
        rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
        rin_user_allocator_corruption();
        return NULL;
    }
    old_size = block->requested_size;
    if (requested_total <= block->total_size) {
        if (map->kind == RIN_USER_ARENA_KIND &&
            !rin_user_split_allocated_tail(map, block, requested_total)) {
            rin_user_allocator_unlock(&map->lock);
            rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
            rin_user_allocator_corruption();
            return NULL;
        }
        block->requested_size = size;
        rin_user_block_store_tail(block);
        rin_user_allocator_unlock(&map->lock);
        rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
        return pointer;
    }
    next = rin_user_next_block(map, block);
    if (map->kind == RIN_USER_ARENA_KIND && next &&
        next->magic == RIN_USER_ARENA_MAGIC_FREE &&
        block->total_size <= (size_t)-1 - next->total_size &&
        block->total_size + next->total_size >= requested_total) {
        if (!rin_user_remove_free_block(map, next)) {
            rin_user_allocator_unlock(&map->lock);
            rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
            rin_user_allocator_corruption();
            return NULL;
        }
        block->total_size += next->total_size;
        rin_user_invalidate_block(next);
        next = rin_user_next_block(map, block);
        if (next) next->previous_size = block->total_size;
        if (!rin_user_split_allocated_tail(map, block, requested_total)) {
            rin_user_allocator_unlock(&map->lock);
            rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
            rin_user_allocator_corruption();
            return NULL;
        }
        block->requested_size = size;
        rin_user_block_store_tail(block);
        rin_user_allocator_unlock(&map->lock);
        rin_user_allocator_unlock(&rin_user_allocator_directory_lock);
        return pointer;
    }
    rin_user_allocator_unlock(&map->lock);
    rin_user_allocator_unlock(&rin_user_allocator_directory_lock);

    replacement = (unsigned char*)rin_user_allocator_malloc(size);
    if (!replacement) return NULL;
    copy_size = old_size < size ? old_size : size;
    source = (const unsigned char*)pointer;
    for (index = 0u; index < copy_size; ++index)
        replacement[index] = source[index];
    rin_user_allocator_free(pointer);
    return replacement;
}
