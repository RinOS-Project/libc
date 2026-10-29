#ifndef RIN_ALIGNED_ALLOC_META_H
#define RIN_ALIGNED_ALLOC_META_H

#ifndef MIDL_PASS

#include "stddef.h"
#include "stdint.h"

#if __SIZEOF_SIZE_T__ == 8
#    define RIN_ALIGNED_META_MARKER ((size_t)0xC0DEC0DEBADC0FFEULL)
#else
#    define RIN_ALIGNED_META_MARKER ((size_t)0xC0DEC0DEUL)
#endif

typedef struct RinAlignedAllocationMeta {
    size_t guard;
    void* raw;
    size_t alignment;
    size_t requested_size;
} RinAlignedAllocationMeta;

static inline size_t rin_aligned_allocation_meta_size(void)
{
    return sizeof(RinAlignedAllocationMeta);
}

static inline RinAlignedAllocationMeta* rin_aligned_allocation_meta_from_ptr(void* ptr)
{
    if (!ptr)
        return (RinAlignedAllocationMeta*)0;
    return (RinAlignedAllocationMeta*)((char*)ptr - sizeof(RinAlignedAllocationMeta));
}

static inline int rin_is_canonical_user_address_uintptr(uintptr_t addr)
{
#if UINTPTR_MAX > 0xffffffffu
    uint64_t high = (uint64_t)addr >> 48;
    if (high != 0ULL && high != 0xFFFFULL)
        return 0;
    if (addr >= UINT64_C(0x0000800000000000))
        return 0;
#else
    (void)addr;
#endif
    return 1;
}

static inline int rin_is_valid_aligned_allocation_metadata(void* candidate_ptr, void* raw_ptr)
{
    uintptr_t raw_value;
    uintptr_t ptr_value;

    if (!candidate_ptr || !raw_ptr)
        return 0;

    raw_value = (uintptr_t)raw_ptr;
    ptr_value = (uintptr_t)candidate_ptr;
    if ((raw_value & (sizeof(void*) - 1)) != 0)
        return 0;
    if (raw_value >= ptr_value)
        return 0;
    if ((ptr_value - raw_value) < sizeof(RinAlignedAllocationMeta))
        return 0;
    if (!rin_is_canonical_user_address_uintptr(raw_value))
        return 0;
    return 1;
}

static inline size_t rin_aligned_allocation_guard_value(void* aligned_ptr)
{
    return RIN_ALIGNED_META_MARKER ^ (size_t)(uintptr_t)aligned_ptr;
}

static inline void rin_store_aligned_allocation_metadata_ex(
    void* aligned_ptr, void* raw_ptr, size_t alignment, size_t requested_size)
{
    RinAlignedAllocationMeta* meta = rin_aligned_allocation_meta_from_ptr(aligned_ptr);
    meta->guard = rin_aligned_allocation_guard_value(aligned_ptr);
    meta->raw = raw_ptr;
    meta->alignment = alignment;
    meta->requested_size = requested_size;
}

static inline void rin_store_aligned_allocation_metadata(void* aligned_ptr,
                                                         void* raw_ptr)
{
    rin_store_aligned_allocation_metadata_ex(aligned_ptr, raw_ptr,
                                              sizeof(void*), 0u);
}

static inline int rin_load_aligned_allocation_metadata(
    void* aligned_ptr, void** out_raw, size_t* out_alignment,
    size_t* out_requested_size)
{
    RinAlignedAllocationMeta* meta;

    if (!aligned_ptr || !out_raw)
        return 0;

    meta = rin_aligned_allocation_meta_from_ptr(aligned_ptr);
    if (meta->guard != rin_aligned_allocation_guard_value(aligned_ptr))
        return 0;
    if (!rin_is_valid_aligned_allocation_metadata(aligned_ptr, meta->raw))
        return 0;
    if (meta->alignment < sizeof(void*) ||
        (meta->alignment & (meta->alignment - 1u)) != 0u ||
        ((uintptr_t)aligned_ptr & (meta->alignment - 1u)) != 0u)
        return 0;
    if (meta->requested_size > (uintptr_t)-1 -
                                   ((uintptr_t)aligned_ptr -
                                    (uintptr_t)meta->raw))
        return 0;

    *out_raw = meta->raw;
    if (out_alignment) *out_alignment = meta->alignment;
    if (out_requested_size) *out_requested_size = meta->requested_size;
    return 1;
}

static inline int rin_load_aligned_allocation_raw_pointer(void* aligned_ptr,
                                                          void** out_raw)
{
    return rin_load_aligned_allocation_metadata(aligned_ptr, out_raw, NULL,
                                                NULL);
}

#endif /* !MIDL_PASS */

#endif
