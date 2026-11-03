/* SPDX-License-Identifier: MIT */
/* Bounded POSIX iconv runtime for the explicitly supported codesets. */

#include "iconv.h"

#include "errno.h"
#include "stdint.h"
#include "stdlib.h"
#include "iconv_cp932_table.h"
#include "iconv_japanese_tables.h"
#include "../libunicode/rin_unicode.h"
#if defined(RIN_ICONV_RUNTIME_TESTING)
extern int rin_iconv_runtime_test_wait(volatile uint32_t* address,
                                      uint32_t expected);
extern int rin_iconv_runtime_test_wake(volatile uint32_t* address,
                                      int count);
#else
#include "rin_atomic_wait.h"
#define rin_iconv_runtime_test_wait(address, expected) \
    rin_atomic_wait_u32((address), (expected), NULL)
#define rin_iconv_runtime_test_wake(address, count) \
    ((count) == 1 ? rin_atomic_notify_one_u32((address)) : \
                    rin_atomic_notify_all_u32((address)))
#endif

#define RIN_ICONV_SLOT_INDEX_BITS 20u
#define RIN_ICONV_SLOT_MASK ((uintptr_t)0x000fffffu)
#define RIN_ICONV_SLOT_CHUNK_SHIFT 8u
#define RIN_ICONV_SLOT_CHUNK_SIZE (1u << RIN_ICONV_SLOT_CHUNK_SHIFT)
#define RIN_ICONV_SLOT_CHUNK_MASK (RIN_ICONV_SLOT_CHUNK_SIZE - 1u)
#define RIN_ICONV_SLOT_MAX ((uint32_t)RIN_ICONV_SLOT_MASK)
#define RIN_ICONV_SLOT_CHUNK_COUNT \
    ((RIN_ICONV_SLOT_MAX + RIN_ICONV_SLOT_CHUNK_SIZE - 1u) / \
     RIN_ICONV_SLOT_CHUNK_SIZE)
#define RIN_ICONV_NAME_INPUT_MAX 128u

typedef enum {
    RIN_ICONV_ENCODING_INVALID = 0,
    RIN_ICONV_ENCODING_ASCII,
    RIN_ICONV_ENCODING_UTF8,
    RIN_ICONV_ENCODING_LATIN1,
    RIN_ICONV_ENCODING_UTF16,
    RIN_ICONV_ENCODING_UTF16BE,
    RIN_ICONV_ENCODING_UTF16LE,
    RIN_ICONV_ENCODING_UTF32,
    RIN_ICONV_ENCODING_UTF32BE,
    RIN_ICONV_ENCODING_UTF32LE,
    RIN_ICONV_ENCODING_SHIFT_JIS,
    RIN_ICONV_ENCODING_CP932,
    RIN_ICONV_ENCODING_EUC_JP
} RinIconvEncoding;

typedef struct RinIconvDescriptor {
    uintptr_t token;
    uintptr_t generation;
    uint32_t slot_index;
    uint32_t input_encoding;
    uint32_t output_encoding;
    uint32_t input_byte_order;
    uint32_t input_started;
    uint32_t output_started;
    uint32_t pins;
    uint32_t descriptor_lock;
    uint32_t used;
    uint32_t closing;
    uint32_t retired;
    struct RinIconvDescriptor* free_next;
} RinIconvDescriptor;

/* Descriptor chunks are never moved, so a conversion pin remains valid while
 * iconv_open grows the directory. The 20-bit slot field supports over one
 * million descriptor slots on both IA-32 and x86_64. Chunks are allocated on
 * demand and retained so generation history survives close/reopen churn. */
static RinIconvDescriptor* g_iconv_chunks[RIN_ICONV_SLOT_CHUNK_COUNT];
static RinIconvDescriptor* g_iconv_free_list;
static uint32_t g_iconv_allocated_slots;
static uint32_t g_iconv_table_lock;
static volatile uint32_t g_iconv_table_wait_sequence;

static void rin_iconv_table_lock(void) {
    while (__sync_lock_test_and_set(&g_iconv_table_lock, 1u) != 0u) {
    }
}

static void rin_iconv_table_unlock(void) {
    __sync_lock_release(&g_iconv_table_lock);
}

static int rin_iconv_descriptor_lock(RinIconvDescriptor* descriptor) {
    uint32_t expected = 0u;
    if (__atomic_compare_exchange_n(&descriptor->descriptor_lock, &expected,
                                    1u, 0, __ATOMIC_ACQUIRE,
                                    __ATOMIC_RELAXED))
        return 0;
    for (;;) {
        uint32_t previous = __atomic_exchange_n(
            &descriptor->descriptor_lock, 2u, __ATOMIC_ACQUIRE);
        if (previous == 0u) return 0;
        if (rin_iconv_runtime_test_wait(&descriptor->descriptor_lock, 2u) != 0)
            return -1;
    }
}

static void rin_iconv_descriptor_unlock(RinIconvDescriptor* descriptor) {
    if (__atomic_exchange_n(&descriptor->descriptor_lock, 0u,
                            __ATOMIC_RELEASE) == 2u)
        (void)rin_iconv_runtime_test_wake(&descriptor->descriptor_lock, 1);
}

static int rin_iconv_name_normalize(const char* name, char* output,
                                    size_t capacity) {
    size_t length = 0u;
    size_t input_length = 0u;
    if (!name || !output || capacity < 2u) return 0;
    while (input_length < RIN_ICONV_NAME_INPUT_MAX &&
           name[input_length] != '\0') {
        unsigned char value = (unsigned char)name[input_length++];
        if (value == '-' || value == '_' || value == '.' ||
            value == ' ' || value == '\t')
            continue;
        if (value >= 'a' && value <= 'z') value -= (unsigned char)('a' - 'A');
        if (!((value >= 'A' && value <= 'Z') ||
              (value >= '0' && value <= '9')) || length + 1u >= capacity)
            return 0;
        output[length++] = (char)value;
    }
    if (input_length == RIN_ICONV_NAME_INPUT_MAX) return 0;
    if (length == 0u) return 0;
    output[length] = '\0';
    return 1;
}

static int rin_iconv_name_equals(const char* left, const char* right) {
    size_t index = 0u;
    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) return 0;
        ++index;
    }
    return left[index] == right[index];
}

static RinIconvEncoding rin_iconv_encoding_from_name(const char* name) {
    char normalized[32];
    if (!rin_iconv_name_normalize(name, normalized, sizeof(normalized)))
        return RIN_ICONV_ENCODING_INVALID;
    if (rin_iconv_name_equals(normalized, "C") ||
        rin_iconv_name_equals(normalized, "POSIX") ||
        rin_iconv_name_equals(normalized, "ASCII") ||
        rin_iconv_name_equals(normalized, "USASCII") ||
        rin_iconv_name_equals(normalized, "ANSIX341968"))
        return RIN_ICONV_ENCODING_ASCII;
    if (rin_iconv_name_equals(normalized, "CUTF8") ||
        rin_iconv_name_equals(normalized, "POSIXUTF8") ||
        rin_iconv_name_equals(normalized, "UTF8") ||
        rin_iconv_name_equals(normalized, "UNICODE11UTF8"))
        return RIN_ICONV_ENCODING_UTF8;
    if (rin_iconv_name_equals(normalized, "ISO88591") ||
        rin_iconv_name_equals(normalized, "ISOIR100") ||
        rin_iconv_name_equals(normalized, "LATIN1") ||
        rin_iconv_name_equals(normalized, "L1"))
        return RIN_ICONV_ENCODING_LATIN1;
    if (rin_iconv_name_equals(normalized, "UTF16") ||
        rin_iconv_name_equals(normalized, "UNICODE"))
        return RIN_ICONV_ENCODING_UTF16;
    if (rin_iconv_name_equals(normalized, "UTF16BE"))
        return RIN_ICONV_ENCODING_UTF16BE;
    if (rin_iconv_name_equals(normalized, "UTF16LE"))
        return RIN_ICONV_ENCODING_UTF16LE;
    if (rin_iconv_name_equals(normalized, "UTF32") ||
        rin_iconv_name_equals(normalized, "UNICODE32") ||
        rin_iconv_name_equals(normalized, "UCS4"))
        return RIN_ICONV_ENCODING_UTF32;
    if (rin_iconv_name_equals(normalized, "UTF32BE") ||
        rin_iconv_name_equals(normalized, "UCS4BE"))
        return RIN_ICONV_ENCODING_UTF32BE;
    if (rin_iconv_name_equals(normalized, "UTF32LE") ||
        rin_iconv_name_equals(normalized, "UCS4LE"))
        return RIN_ICONV_ENCODING_UTF32LE;
    if (rin_iconv_name_equals(normalized, "SHIFTJIS") ||
        rin_iconv_name_equals(normalized, "SJIS") ||
        rin_iconv_name_equals(normalized, "SHIFTJISX0208") ||
        rin_iconv_name_equals(normalized, "CSSHIFTJIS") ||
        rin_iconv_name_equals(normalized, "XSHIFTJIS"))
        return RIN_ICONV_ENCODING_SHIFT_JIS;
    if (rin_iconv_name_equals(normalized, "CP932") ||
        rin_iconv_name_equals(normalized, "WINDOWS932") ||
        rin_iconv_name_equals(normalized, "WINDOWS31J") ||
        rin_iconv_name_equals(normalized, "MSKANJI") ||
        rin_iconv_name_equals(normalized, "MS932") ||
        rin_iconv_name_equals(normalized, "IBM943"))
        return RIN_ICONV_ENCODING_CP932;
    if (rin_iconv_name_equals(normalized, "EUCJP") ||
        rin_iconv_name_equals(normalized, "XEUCJP") ||
        rin_iconv_name_equals(normalized, "CSEUCPKDFMTJAPANESE"))
        return RIN_ICONV_ENCODING_EUC_JP;
    return RIN_ICONV_ENCODING_INVALID;
}

static uintptr_t rin_iconv_token_for(uintptr_t generation, uint32_t index) {
    return (generation << RIN_ICONV_SLOT_INDEX_BITS) |
           ((uintptr_t)index + 1u);
}

static RinIconvDescriptor* rin_iconv_lookup_locked(uintptr_t token,
                                                   int allow_closing) {
    uintptr_t slot = token & RIN_ICONV_SLOT_MASK;
    uint32_t index;
    uint32_t chunk_index;
    RinIconvDescriptor* descriptor;
    if (slot == 0u || slot > RIN_ICONV_SLOT_MAX || token == 0u)
        return NULL;
    index = (uint32_t)(slot - 1u);
    chunk_index = index >> RIN_ICONV_SLOT_CHUNK_SHIFT;
    if (chunk_index >= RIN_ICONV_SLOT_CHUNK_COUNT ||
        !g_iconv_chunks[chunk_index])
        return NULL;
    descriptor = &g_iconv_chunks[chunk_index][
        index & RIN_ICONV_SLOT_CHUNK_MASK];
    if (!descriptor->used || descriptor->token != token ||
        (!allow_closing && descriptor->closing))
        return NULL;
    return descriptor;
}

static RinIconvDescriptor* rin_iconv_pin(iconv_t opaque) {
    uintptr_t token = (uintptr_t)opaque;
    RinIconvDescriptor* descriptor;
    rin_iconv_table_lock();
    descriptor = rin_iconv_lookup_locked(token, 0);
    if (!descriptor || descriptor->pins == UINT32_MAX) {
        rin_iconv_table_unlock();
        errno = descriptor ? EOVERFLOW : EBADF;
        return NULL;
    }
    ++descriptor->pins;
    rin_iconv_table_unlock();
    return descriptor;
}

static void rin_iconv_unpin(RinIconvDescriptor* descriptor) {
    int wake_close = 0;
    rin_iconv_table_lock();
    if (descriptor->pins != 0u) {
        --descriptor->pins;
        if (descriptor->closing && descriptor->pins == 0u) {
            __atomic_add_fetch(&g_iconv_table_wait_sequence, 1u,
                               __ATOMIC_RELEASE);
            wake_close = 1;
        }
    }
    rin_iconv_table_unlock();
    if (wake_close)
        (void)rin_iconv_runtime_test_wake(&g_iconv_table_wait_sequence, -1);
}

iconv_t iconv_open(const char* tocode, const char* fromcode) {
    RinIconvEncoding output_encoding =
        rin_iconv_encoding_from_name(tocode);
    RinIconvEncoding input_encoding =
        rin_iconv_encoding_from_name(fromcode);
    if (input_encoding == RIN_ICONV_ENCODING_INVALID ||
        output_encoding == RIN_ICONV_ENCODING_INVALID) {
        errno = EINVAL;
        return (iconv_t)(uintptr_t)-1;
    }
    for (;;) {
        RinIconvDescriptor* descriptor;
        RinIconvDescriptor* candidate_chunk = NULL;
        uintptr_t max_generation = UINTPTR_MAX >>
                                  RIN_ICONV_SLOT_INDEX_BITS;
        uint32_t chunk_index;
        uint32_t available;

        rin_iconv_table_lock();
        descriptor = g_iconv_free_list;
        if (descriptor) {
            g_iconv_free_list = descriptor->free_next;
            descriptor->free_next = NULL;
            if (descriptor->generation >= max_generation) {
                descriptor->retired = 1u;
                rin_iconv_table_unlock();
                continue;
            }
            ++descriptor->generation;
            descriptor->token = rin_iconv_token_for(
                descriptor->generation, descriptor->slot_index - 1u);
            if (descriptor->token == UINTPTR_MAX) {
                descriptor->retired = 1u;
                rin_iconv_table_unlock();
                continue;
            }
            descriptor->input_encoding = (uint32_t)input_encoding;
            descriptor->output_encoding = (uint32_t)output_encoding;
            descriptor->input_byte_order = 0u;
            descriptor->input_started = 0u;
            descriptor->output_started = 0u;
            descriptor->pins = 0u;
            descriptor->descriptor_lock = 0u;
            descriptor->closing = 0u;
            descriptor->used = 1u;
            rin_iconv_table_unlock();
            return (iconv_t)descriptor->token;
        }
        if (g_iconv_allocated_slots >= RIN_ICONV_SLOT_MAX) {
            rin_iconv_table_unlock();
            errno = EMFILE;
            return (iconv_t)(uintptr_t)-1;
        }
        chunk_index = g_iconv_allocated_slots >>
                      RIN_ICONV_SLOT_CHUNK_SHIFT;
        if (chunk_index >= RIN_ICONV_SLOT_CHUNK_COUNT) {
            rin_iconv_table_unlock();
            errno = EMFILE;
            return (iconv_t)(uintptr_t)-1;
        }
        available = RIN_ICONV_SLOT_MAX - g_iconv_allocated_slots;
        if (available > RIN_ICONV_SLOT_CHUNK_SIZE)
            available = RIN_ICONV_SLOT_CHUNK_SIZE;
        rin_iconv_table_unlock();

        /* Allocate outside the table lock. Another opener can win the same
         * growth race; the loser frees its private candidate and retries. */
        candidate_chunk = (RinIconvDescriptor*)malloc(
            (size_t)available * sizeof(*candidate_chunk));
        if (!candidate_chunk) {
            rin_iconv_table_lock();
            if (g_iconv_free_list ||
                g_iconv_allocated_slots >= RIN_ICONV_SLOT_MAX ||
                g_iconv_allocated_slots !=
                    chunk_index * RIN_ICONV_SLOT_CHUNK_SIZE) {
                rin_iconv_table_unlock();
                continue;
            }
            rin_iconv_table_unlock();
            errno = ENOMEM;
            return (iconv_t)(uintptr_t)-1;
        }
        __builtin_memset(candidate_chunk, 0,
                         (size_t)available * sizeof(*candidate_chunk));

        rin_iconv_table_lock();
        if (g_iconv_free_list ||
            g_iconv_allocated_slots !=
                chunk_index * RIN_ICONV_SLOT_CHUNK_SIZE) {
            rin_iconv_table_unlock();
            free(candidate_chunk);
            continue;
        }
        g_iconv_chunks[chunk_index] = candidate_chunk;
        for (uint32_t offset = 0u; offset < available; ++offset) {
            RinIconvDescriptor* added = &candidate_chunk[offset];
            added->slot_index = g_iconv_allocated_slots + offset + 1u;
            added->free_next = g_iconv_free_list;
            g_iconv_free_list = added;
        }
        g_iconv_allocated_slots += available;
        rin_iconv_table_unlock();
    }
}

typedef enum {
    RIN_ICONV_DECODE_INVALID = -1,
    RIN_ICONV_DECODE_INCOMPLETE = 0,
    RIN_ICONV_DECODE_SCALAR = 1,
    RIN_ICONV_DECODE_SKIP = 2
} RinIconvDecodeStatus;

static uint16_t rin_iconv_read_u16(const char* input, uint32_t byte_order) {
    uint16_t first = (uint8_t)input[0];
    uint16_t second = (uint8_t)input[1];
    return byte_order == 1u ? (uint16_t)(first | (uint16_t)(second << 8u))
                            : (uint16_t)((uint16_t)(first << 8u) | second);
}

static uint32_t rin_iconv_read_u32(const char* input, uint32_t byte_order) {
    uint32_t first = (uint8_t)input[0];
    uint32_t second = (uint8_t)input[1];
    uint32_t third = (uint8_t)input[2];
    uint32_t fourth = (uint8_t)input[3];
    if (byte_order == 1u)
        return first | (second << 8u) | (third << 16u) | (fourth << 24u);
    return (first << 24u) | (second << 16u) | (third << 8u) | fourth;
}

static int rin_iconv_jis_lead_index(uint8_t byte, uint32_t* index_out) {
    if (!index_out) return 0;
    if (byte >= 0x81u && byte <= 0x9fu) {
        *index_out = (uint32_t)(byte - 0x81u);
        return 1;
    }
    if (byte >= 0xe0u && byte <= 0xfcu) {
        *index_out = 31u + (uint32_t)(byte - 0xe0u);
        return 1;
    }
    return 0;
}

static int rin_iconv_jis_trail_index(uint8_t byte, uint32_t* index_out) {
    if (!index_out) return 0;
    if (byte >= 0x40u && byte <= 0x7eu) {
        *index_out = (uint32_t)(byte - 0x40u);
        return 1;
    }
    if (byte >= 0x80u && byte <= 0xfcu) {
        *index_out = 63u + (uint32_t)(byte - 0x80u);
        return 1;
    }
    return 0;
}

static RinIconvDecodeStatus rin_iconv_decode_japanese(
    uint32_t encoding, const char* input, size_t input_left,
    uint32_t* codepoint_out, size_t* consumed_out) {
    uint8_t first = (uint8_t)input[0];
    uint32_t codepoint;
    if (encoding == RIN_ICONV_ENCODING_SHIFT_JIS ||
        encoding == RIN_ICONV_ENCODING_CP932) {
        uint32_t lead_index;
        uint32_t trail_index;
        const uint16_t* table;
        if (first < 0x80u) {
            codepoint = first;
            *consumed_out = 1u;
        } else if (encoding == RIN_ICONV_ENCODING_CP932 && first == 0x80u) {
            codepoint = 0x0080u;
            *consumed_out = 1u;
        } else if (encoding == RIN_ICONV_ENCODING_CP932 &&
                   (first == 0xa0u || first >= 0xfdu)) {
            codepoint = 0xf8f0u +
                (first == 0xa0u ? 0u : (uint32_t)first - 0xfcu);
            *consumed_out = 1u;
        } else if (first >= 0xa1u && first <= 0xdfu) {
            codepoint = 0xff61u + (uint32_t)(first - 0xa1u);
            *consumed_out = 1u;
        } else {
            if (!rin_iconv_jis_lead_index(first, &lead_index))
                return RIN_ICONV_DECODE_INVALID;
            if (input_left < 2u) return RIN_ICONV_DECODE_INCOMPLETE;
            if (!rin_iconv_jis_trail_index((uint8_t)input[1], &trail_index))
                return RIN_ICONV_DECODE_INVALID;
            table = encoding == RIN_ICONV_ENCODING_CP932
                ? g_rin_iconv_cp932_pairs : g_rin_iconv_shift_jis_pairs;
            codepoint = table[lead_index * RIN_ICONV_JIS_TRAIL_COUNT +
                              trail_index];
            if (codepoint == 0u) return RIN_ICONV_DECODE_INVALID;
            *consumed_out = 2u;
        }
        *codepoint_out = codepoint;
        return RIN_ICONV_DECODE_SCALAR;
    }

    if (first < 0x80u) {
        codepoint = first;
        *consumed_out = 1u;
    } else if (first == 0x8eu) {
        uint8_t second;
        if (input_left < 2u) return RIN_ICONV_DECODE_INCOMPLETE;
        second = (uint8_t)input[1];
        if (second < 0xa1u || second > 0xdfu)
            return RIN_ICONV_DECODE_INVALID;
        codepoint = 0xff61u + (uint32_t)(second - 0xa1u);
        *consumed_out = 2u;
    } else if (first == 0x8fu) {
        uint8_t row;
        uint8_t cell;
        if (input_left < 2u) return RIN_ICONV_DECODE_INCOMPLETE;
        row = (uint8_t)input[1];
        if (row < 0xa1u || row > 0xfeu)
            return RIN_ICONV_DECODE_INVALID;
        if (input_left < 3u) return RIN_ICONV_DECODE_INCOMPLETE;
        cell = (uint8_t)input[2];
        if (cell < 0xa1u || cell > 0xfeu)
            return RIN_ICONV_DECODE_INVALID;
        codepoint = g_rin_iconv_euc_jp_plane2[
            (uint32_t)(row - 0xa1u) * 94u + (uint32_t)(cell - 0xa1u)];
        if (codepoint == 0u) return RIN_ICONV_DECODE_INVALID;
        *consumed_out = 3u;
    } else if (first >= 0xa1u && first <= 0xfeu) {
        uint8_t second;
        if (input_left < 2u) return RIN_ICONV_DECODE_INCOMPLETE;
        second = (uint8_t)input[1];
        if (second < 0xa1u || second > 0xfeu)
            return RIN_ICONV_DECODE_INVALID;
        codepoint = g_rin_iconv_euc_jp_plane1[
            (uint32_t)(first - 0xa1u) * 94u +
            (uint32_t)(second - 0xa1u)];
        if (codepoint == 0u) return RIN_ICONV_DECODE_INVALID;
        *consumed_out = 2u;
    } else {
        return RIN_ICONV_DECODE_INVALID;
    }
    *codepoint_out = codepoint;
    return RIN_ICONV_DECODE_SCALAR;
}

static int rin_iconv_find_japanese_encoding(const uint64_t* table,
                                             uint32_t count,
                                             uint32_t codepoint,
                                             char bytes[8],
                                             size_t* length_out) {
    uint32_t low = 0u;
    uint32_t high = count;
    while (low < high) {
        uint32_t middle = low + (high - low) / 2u;
        uint64_t entry = table[middle];
        uint32_t scalar = (uint32_t)(entry >> 32u);
        if (scalar < codepoint) {
            low = middle + 1u;
        } else if (scalar > codepoint) {
            high = middle;
        } else {
            uint32_t packed = (uint32_t)entry;
            size_t length = (packed >> 24u) & 0xffu;
            for (size_t index = 0u; index < length; ++index)
                bytes[index] = (char)(uint8_t)(packed >>
                    ((length - index - 1u) * 8u));
            *length_out = length;
            return 1;
        }
    }
    return 0;
}

static int rin_iconv_encode_japanese(uint32_t encoding, uint32_t codepoint,
                                     char bytes[8], size_t* length_out) {
    if (codepoint < 0x80u) {
        bytes[0] = (char)codepoint;
        *length_out = 1u;
        return 1;
    }
    if (encoding == RIN_ICONV_ENCODING_CP932) {
        if (codepoint == 0x0080u) {
            bytes[0] = (char)0x80;
            *length_out = 1u;
            return 1;
        }
        if (codepoint >= 0xf8f0u && codepoint <= 0xf8f3u) {
            bytes[0] = (char)(codepoint == 0xf8f0u
                ? 0xa0u : 0xfcu + (codepoint - 0xf8f0u));
            *length_out = 1u;
            return 1;
        }
    }
    if ((encoding == RIN_ICONV_ENCODING_SHIFT_JIS ||
         encoding == RIN_ICONV_ENCODING_CP932) &&
        codepoint >= 0xff61u && codepoint <= 0xff9fu) {
        bytes[0] = (char)(0xa1u + codepoint - 0xff61u);
        *length_out = 1u;
        return 1;
    }
    if (encoding == RIN_ICONV_ENCODING_EUC_JP &&
        codepoint >= 0xff61u && codepoint <= 0xff9fu) {
        bytes[0] = (char)0x8e;
        bytes[1] = (char)(0xa1u + codepoint - 0xff61u);
        *length_out = 2u;
        return 1;
    }
    if (encoding == RIN_ICONV_ENCODING_CP932)
        return rin_iconv_find_japanese_encoding(
            g_rin_iconv_cp932_encode, RIN_ICONV_CP932_ENCODE_COUNT,
            codepoint, bytes, length_out);
    if (encoding == RIN_ICONV_ENCODING_SHIFT_JIS)
        return rin_iconv_find_japanese_encoding(
            g_rin_iconv_shift_jis_encode, RIN_ICONV_SHIFT_JIS_ENCODE_COUNT,
            codepoint, bytes, length_out);
    return rin_iconv_find_japanese_encoding(
        g_rin_iconv_euc_jp_encode, RIN_ICONV_EUC_JP_ENCODE_COUNT,
        codepoint, bytes, length_out);
}

static RinIconvDecodeStatus rin_iconv_decode(
    RinIconvDescriptor* descriptor, const char* input, size_t input_left,
    uint32_t* codepoint_out, size_t* consumed_out) {
    uint32_t encoding = descriptor->input_encoding;
    uint32_t codepoint = 0u;
    size_t consumed = 0u;
    if (input_left == 0u) return RIN_ICONV_DECODE_INCOMPLETE;
    if (encoding == RIN_ICONV_ENCODING_ASCII ||
        encoding == RIN_ICONV_ENCODING_LATIN1) {
        unsigned char value = (unsigned char)input[0];
        if (encoding == RIN_ICONV_ENCODING_ASCII && value > 0x7fu)
            return RIN_ICONV_DECODE_INVALID;
        codepoint = value;
        consumed = 1u;
    } else if (encoding == RIN_ICONV_ENCODING_UTF8) {
        int status = rin_unicode_decode_utf8(input, input_left, &codepoint,
                                             &consumed);
        if (status == RIN_UNICODE_INCOMPLETE)
            return RIN_ICONV_DECODE_INCOMPLETE;
        if (status != RIN_UNICODE_OK) return RIN_ICONV_DECODE_INVALID;
    } else if (encoding == RIN_ICONV_ENCODING_SHIFT_JIS ||
               encoding == RIN_ICONV_ENCODING_CP932 ||
               encoding == RIN_ICONV_ENCODING_EUC_JP) {
        return rin_iconv_decode_japanese(encoding, input, input_left,
                                         codepoint_out, consumed_out);
    } else if (encoding == RIN_ICONV_ENCODING_UTF32 ||
               encoding == RIN_ICONV_ENCODING_UTF32BE ||
               encoding == RIN_ICONV_ENCODING_UTF32LE) {
        uint32_t byte_order;
        if (input_left < 4u) return RIN_ICONV_DECODE_INCOMPLETE;
        if (encoding == RIN_ICONV_ENCODING_UTF32LE) {
            byte_order = 1u;
        } else if (encoding == RIN_ICONV_ENCODING_UTF32BE) {
            byte_order = 2u;
        } else if (!descriptor->input_started) {
            if ((uint8_t)input[0] == 0x00u &&
                (uint8_t)input[1] == 0x00u &&
                (uint8_t)input[2] == 0xfeu &&
                (uint8_t)input[3] == 0xffu) {
                descriptor->input_byte_order = 2u;
                descriptor->input_started = 1u;
                *consumed_out = 4u;
                return RIN_ICONV_DECODE_SKIP;
            }
            if ((uint8_t)input[0] == 0xffu &&
                (uint8_t)input[1] == 0xfeu &&
                (uint8_t)input[2] == 0x00u &&
                (uint8_t)input[3] == 0x00u) {
                descriptor->input_byte_order = 1u;
                descriptor->input_started = 1u;
                *consumed_out = 4u;
                return RIN_ICONV_DECODE_SKIP;
            }
            byte_order = 2u;
            descriptor->input_byte_order = byte_order;
            descriptor->input_started = 1u;
        } else {
            byte_order = descriptor->input_byte_order;
        }
        codepoint = rin_iconv_read_u32(input, byte_order);
        if (codepoint > 0x10ffffu ||
            (codepoint >= 0xd800u && codepoint <= 0xdfffu))
            return RIN_ICONV_DECODE_INVALID;
        consumed = 4u;
    } else {
        uint32_t byte_order;
        uint16_t first;
        if (input_left < 2u) return RIN_ICONV_DECODE_INCOMPLETE;
        if (encoding == RIN_ICONV_ENCODING_UTF16LE) {
            byte_order = 1u;
        } else if (encoding == RIN_ICONV_ENCODING_UTF16BE) {
            byte_order = 2u;
        } else if (!descriptor->input_started) {
            if ((uint8_t)input[0] == 0xfeu &&
                (uint8_t)input[1] == 0xffu) {
                descriptor->input_byte_order = 2u;
                descriptor->input_started = 1u;
                *consumed_out = 2u;
                return RIN_ICONV_DECODE_SKIP;
            }
            if ((uint8_t)input[0] == 0xffu &&
                (uint8_t)input[1] == 0xfeu) {
                descriptor->input_byte_order = 1u;
                descriptor->input_started = 1u;
                *consumed_out = 2u;
                return RIN_ICONV_DECODE_SKIP;
            }
            byte_order = 2u;
            descriptor->input_byte_order = byte_order;
            descriptor->input_started = 1u;
        } else {
            byte_order = descriptor->input_byte_order;
        }
        first = rin_iconv_read_u16(input, byte_order);
        if (first >= 0xdc00u && first <= 0xdfffu)
            return RIN_ICONV_DECODE_INVALID;
        if (first >= 0xd800u && first <= 0xdbffu) {
            uint16_t second;
            if (input_left < 4u) return RIN_ICONV_DECODE_INCOMPLETE;
            second = rin_iconv_read_u16(input + 2u, byte_order);
            if (second < 0xdc00u || second > 0xdfffu)
                return RIN_ICONV_DECODE_INVALID;
            codepoint = 0x10000u +
                ((((uint32_t)first - 0xd800u) << 10u) |
                 ((uint32_t)second - 0xdc00u));
            consumed = 4u;
        } else {
            codepoint = first;
            consumed = 2u;
        }
    }
    *codepoint_out = codepoint;
    *consumed_out = consumed;
    return RIN_ICONV_DECODE_SCALAR;
}

static int rin_iconv_encode(uint32_t encoding, uint32_t codepoint,
                            char bytes[8], size_t* length_out) {
    size_t length = 0u;
    if (encoding == RIN_ICONV_ENCODING_ASCII) {
        if (codepoint > 0x7fu) return 0;
        bytes[0] = (char)codepoint;
        length = 1u;
    } else if (encoding == RIN_ICONV_ENCODING_LATIN1) {
        if (codepoint > 0xffu) return 0;
        bytes[0] = (char)(uint8_t)codepoint;
        length = 1u;
    } else if (encoding == RIN_ICONV_ENCODING_UTF8) {
        if (rin_unicode_encode_utf8(bytes, 8u, codepoint, &length) !=
            RIN_UNICODE_OK)
            return 0;
    } else if (encoding == RIN_ICONV_ENCODING_SHIFT_JIS ||
               encoding == RIN_ICONV_ENCODING_CP932 ||
               encoding == RIN_ICONV_ENCODING_EUC_JP) {
        return rin_iconv_encode_japanese(encoding, codepoint, bytes,
                                         length_out);
    } else if (encoding == RIN_ICONV_ENCODING_UTF32 ||
               encoding == RIN_ICONV_ENCODING_UTF32BE ||
               encoding == RIN_ICONV_ENCODING_UTF32LE) {
        uint32_t byte_order = encoding == RIN_ICONV_ENCODING_UTF32LE
            ? 1u : 2u;
        if (codepoint > 0x10ffffu ||
            (codepoint >= 0xd800u && codepoint <= 0xdfffu))
            return 0;
        if (byte_order == 1u) {
            bytes[0] = (char)(uint8_t)codepoint;
            bytes[1] = (char)(uint8_t)(codepoint >> 8u);
            bytes[2] = (char)(uint8_t)(codepoint >> 16u);
            bytes[3] = (char)(uint8_t)(codepoint >> 24u);
        } else {
            bytes[0] = (char)(uint8_t)(codepoint >> 24u);
            bytes[1] = (char)(uint8_t)(codepoint >> 16u);
            bytes[2] = (char)(uint8_t)(codepoint >> 8u);
            bytes[3] = (char)(uint8_t)codepoint;
        }
        length = 4u;
    } else {
        uint16_t units[2];
        size_t unit_count = 0u;
        uint32_t byte_order = encoding == RIN_ICONV_ENCODING_UTF16LE
            ? 1u : 2u;
        if (rin_unicode_encode_utf16(units, 2u, codepoint, &unit_count) !=
            RIN_UNICODE_OK)
            return 0;
        for (size_t index = 0u; index < unit_count; ++index) {
            uint16_t unit = units[index];
            if (byte_order == 1u) {
                bytes[length++] = (char)(uint8_t)unit;
                bytes[length++] = (char)(uint8_t)(unit >> 8u);
            } else {
                bytes[length++] = (char)(uint8_t)(unit >> 8u);
                bytes[length++] = (char)(uint8_t)unit;
            }
        }
    }
    *length_out = length;
    return 1;
}

static void rin_iconv_reset_state(RinIconvDescriptor* descriptor) {
    descriptor->input_byte_order = 0u;
    descriptor->input_started = 0u;
    descriptor->output_started = 0u;
}

static size_t rin_iconv_fail(RinIconvDescriptor* descriptor,
                             int error_number, char** input,
                             size_t* input_left, char** output,
                             size_t* output_left, const char* input_cursor,
                             size_t remaining_input, char* output_cursor,
                             size_t remaining_output) {
    if (input) *input = (char*)input_cursor;
    if (input_left) *input_left = remaining_input;
    if (output) *output = output_cursor;
    if (output_left) *output_left = remaining_output;
    rin_iconv_descriptor_unlock(descriptor);
    rin_iconv_unpin(descriptor);
    errno = error_number;
    return (size_t)-1;
}

size_t iconv(iconv_t opaque, char** input, size_t* input_left,
             char** output, size_t* output_left) {
    RinIconvDescriptor* descriptor = rin_iconv_pin(opaque);
    char* input_cursor;
    size_t remaining_input;
    char* output_cursor = NULL;
    size_t remaining_output = 0u;
    int reset_request;
    if (!descriptor) return (size_t)-1;
    if (rin_iconv_descriptor_lock(descriptor) != 0) {
        int saved_errno = errno;
        rin_iconv_unpin(descriptor);
        errno = saved_errno;
        return (size_t)-1;
    }
    reset_request = input == NULL || *input == NULL;
    if (reset_request && output && !output_left)
        return rin_iconv_fail(descriptor, EINVAL, NULL, NULL, output, NULL,
                              NULL, 0u, NULL, 0u);
    if (!reset_request && (!input_left || !output || !output_left ||
                           (!*output && *output_left != 0u)))
        return rin_iconv_fail(descriptor, EINVAL, input, input_left, output,
                              output_left, input ? *input : NULL,
                              input_left ? *input_left : 0u,
                              output ? *output : NULL,
                              output_left ? *output_left : 0u);
    if (output && output_left) {
        output_cursor = *output;
        remaining_output = *output_left;
    }
    if (reset_request) {
        rin_iconv_reset_state(descriptor);
        if (output && output_left) {
            *output = output_cursor;
            *output_left = remaining_output;
        }
        rin_iconv_descriptor_unlock(descriptor);
        rin_iconv_unpin(descriptor);
        return 0u;
    }
    input_cursor = *input;
    remaining_input = *input_left;
    while (remaining_input != 0u) {
        uint32_t codepoint = 0u;
        size_t consumed = 0u;
        size_t encoded_length = 0u;
        size_t prefix_length = 0u;
        char encoded[8];
        char prefix[4];
        RinIconvDecodeStatus decode_status = rin_iconv_decode(
            descriptor, input_cursor, remaining_input, &codepoint, &consumed);
        if (decode_status == RIN_ICONV_DECODE_INCOMPLETE)
            return rin_iconv_fail(descriptor, EINVAL, input, input_left,
                                  output, output_left, input_cursor,
                                  remaining_input, output_cursor,
                                  remaining_output);
        if (decode_status == RIN_ICONV_DECODE_INVALID)
            return rin_iconv_fail(descriptor, EILSEQ, input, input_left,
                                  output, output_left, input_cursor,
                                  remaining_input, output_cursor,
                                  remaining_output);
        if (decode_status == RIN_ICONV_DECODE_SKIP) {
            input_cursor += consumed;
            remaining_input -= consumed;
            continue;
        }
        if (!rin_iconv_encode(descriptor->output_encoding, codepoint, encoded,
                              &encoded_length))
            return rin_iconv_fail(descriptor, EILSEQ, input, input_left,
                                  output, output_left, input_cursor,
                                  remaining_input, output_cursor,
                                  remaining_output);
        if (!descriptor->output_started) {
            if (descriptor->output_encoding == RIN_ICONV_ENCODING_UTF16) {
                prefix[0] = (char)0xfe;
                prefix[1] = (char)0xff;
                prefix_length = 2u;
            } else if (descriptor->output_encoding ==
                       RIN_ICONV_ENCODING_UTF32) {
                prefix[0] = 0;
                prefix[1] = 0;
                prefix[2] = (char)0xfe;
                prefix[3] = (char)0xff;
                prefix_length = 4u;
            }
        }
        if (!output_cursor ||
            remaining_output < prefix_length + encoded_length)
            return rin_iconv_fail(descriptor, E2BIG, input, input_left,
                                  output, output_left, input_cursor,
                                  remaining_input, output_cursor,
                                  remaining_output);
        for (size_t index = 0u; index < prefix_length; ++index)
            output_cursor[index] = prefix[index];
        output_cursor += prefix_length;
        remaining_output -= prefix_length;
        for (size_t index = 0u; index < encoded_length; ++index)
            output_cursor[index] = encoded[index];
        output_cursor += encoded_length;
        remaining_output -= encoded_length;
        if (prefix_length != 0u) descriptor->output_started = 1u;
        input_cursor += consumed;
        remaining_input -= consumed;
    }
    *input = input_cursor;
    *input_left = remaining_input;
    *output = output_cursor;
    *output_left = remaining_output;
    rin_iconv_descriptor_unlock(descriptor);
    rin_iconv_unpin(descriptor);
    /* Every supported conversion is exact. POSIX counts only nonreversible
     * substitutions here, not the number of converted characters. */
    return 0u;
}

int iconv_close(iconv_t opaque) {
    uintptr_t token = (uintptr_t)opaque;
    RinIconvDescriptor* descriptor;
    rin_iconv_table_lock();
    descriptor = rin_iconv_lookup_locked(token, 0);
    if (!descriptor) {
        rin_iconv_table_unlock();
        errno = EBADF;
        return -1;
    }
    descriptor->closing = 1u;
    for (;;) {
        if (descriptor->pins == 0u) {
            descriptor->used = 0u;
            descriptor->closing = 0u;
            descriptor->token = 0u;
            descriptor->input_encoding = RIN_ICONV_ENCODING_INVALID;
            descriptor->output_encoding = RIN_ICONV_ENCODING_INVALID;
            descriptor->input_byte_order = 0u;
            descriptor->input_started = 0u;
            descriptor->output_started = 0u;
            descriptor->descriptor_lock = 0u;
            if (descriptor->generation >=
                    (UINTPTR_MAX >> RIN_ICONV_SLOT_INDEX_BITS) ||
                rin_iconv_token_for(descriptor->generation,
                                    descriptor->slot_index - 1u) ==
                    UINTPTR_MAX) {
                descriptor->retired = 1u;
                descriptor->free_next = NULL;
            } else {
                descriptor->free_next = g_iconv_free_list;
                g_iconv_free_list = descriptor;
            }
            rin_iconv_table_unlock();
            return 0;
        }
        uint32_t expected = __atomic_load_n(
            &g_iconv_table_wait_sequence, __ATOMIC_RELAXED);
        rin_iconv_table_unlock();
        if (rin_iconv_runtime_test_wait(&g_iconv_table_wait_sequence,
                                        expected) != 0) {
            int saved_errno = errno;
            rin_iconv_table_lock();
            if (descriptor->used && descriptor->token == token &&
                descriptor->closing)
                descriptor->closing = 0u;
            rin_iconv_table_unlock();
            errno = saved_errno;
            return -1;
        }
        rin_iconv_table_lock();
    }
}
