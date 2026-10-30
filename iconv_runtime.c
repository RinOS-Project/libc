/* SPDX-License-Identifier: MIT */
/* Bounded POSIX iconv runtime for the explicitly supported codesets. */

#include "iconv.h"

#include "errno.h"
#include "stdint.h"
#include "../libunicode/rin_unicode.h"

#define RIN_ICONV_SLOT_COUNT 31u
#define RIN_ICONV_SLOT_MASK  63u

typedef enum {
    RIN_ICONV_ENCODING_INVALID = 0,
    RIN_ICONV_ENCODING_ASCII,
    RIN_ICONV_ENCODING_UTF8,
    RIN_ICONV_ENCODING_LATIN1,
    RIN_ICONV_ENCODING_UTF16,
    RIN_ICONV_ENCODING_UTF16BE,
    RIN_ICONV_ENCODING_UTF16LE
} RinIconvEncoding;

typedef struct {
    uintptr_t token;
    uintptr_t generation;
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
} RinIconvDescriptor;

static RinIconvDescriptor g_iconv_descriptors[RIN_ICONV_SLOT_COUNT];
static uint32_t g_iconv_table_lock;

static void rin_iconv_table_lock(void) {
    while (__sync_lock_test_and_set(&g_iconv_table_lock, 1u) != 0u) {
    }
}

static void rin_iconv_table_unlock(void) {
    __sync_lock_release(&g_iconv_table_lock);
}

static void rin_iconv_descriptor_lock(RinIconvDescriptor* descriptor) {
    while (__sync_lock_test_and_set(&descriptor->descriptor_lock, 1u) != 0u) {
    }
}

static void rin_iconv_descriptor_unlock(RinIconvDescriptor* descriptor) {
    __sync_lock_release(&descriptor->descriptor_lock);
}

static int rin_iconv_name_normalize(const char* name, char* output,
                                    size_t capacity) {
    size_t length = 0u;
    if (!name || !output || capacity < 2u) return 0;
    while (*name != '\0') {
        unsigned char value = (unsigned char)*name++;
        if (value == '-' || value == '_' || value == '.' ||
            value == ' ' || value == '\t')
            continue;
        if (value >= 'a' && value <= 'z') value -= (unsigned char)('a' - 'A');
        if (!((value >= 'A' && value <= 'Z') ||
              (value >= '0' && value <= '9')) || length + 1u >= capacity)
            return 0;
        output[length++] = (char)value;
    }
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
    return RIN_ICONV_ENCODING_INVALID;
}

static uintptr_t rin_iconv_token_for(uintptr_t generation, uint32_t index) {
    return (generation << 6u) | ((uintptr_t)index + 1u);
}

static RinIconvDescriptor* rin_iconv_lookup_locked(uintptr_t token,
                                                   int allow_closing) {
    uintptr_t slot = token & RIN_ICONV_SLOT_MASK;
    uint32_t index;
    RinIconvDescriptor* descriptor;
    if (slot == 0u || slot > RIN_ICONV_SLOT_COUNT || token == 0u)
        return NULL;
    index = (uint32_t)(slot - 1u);
    descriptor = &g_iconv_descriptors[index];
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
    rin_iconv_table_lock();
    if (descriptor->pins != 0u) --descriptor->pins;
    rin_iconv_table_unlock();
}

iconv_t iconv_open(const char* tocode, const char* fromcode) {
    RinIconvEncoding output_encoding =
        rin_iconv_encoding_from_name(tocode);
    RinIconvEncoding input_encoding =
        rin_iconv_encoding_from_name(fromcode);
    uint32_t index;
    if (input_encoding == RIN_ICONV_ENCODING_INVALID ||
        output_encoding == RIN_ICONV_ENCODING_INVALID) {
        errno = EINVAL;
        return (iconv_t)(uintptr_t)-1;
    }
    rin_iconv_table_lock();
    for (index = 0u; index < RIN_ICONV_SLOT_COUNT; ++index) {
        RinIconvDescriptor* descriptor = &g_iconv_descriptors[index];
        uintptr_t max_generation = ((uintptr_t)~(uintptr_t)0) >> 6u;
        if (descriptor->used || descriptor->retired) continue;
        if (descriptor->generation >= max_generation) {
            descriptor->retired = 1u;
            continue;
        }
        ++descriptor->generation;
        descriptor->token = rin_iconv_token_for(descriptor->generation,
                                                index);
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
    rin_iconv_table_unlock();
    errno = EMFILE;
    return (iconv_t)(uintptr_t)-1;
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
    rin_iconv_descriptor_lock(descriptor);
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
        char encoded[8];
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
        if (descriptor->output_encoding == RIN_ICONV_ENCODING_UTF16 &&
            !descriptor->output_started) {
            if (!output_cursor || remaining_output < 2u)
                return rin_iconv_fail(descriptor, E2BIG, input, input_left,
                                      output, output_left, input_cursor,
                                      remaining_input, output_cursor,
                                      remaining_output);
            output_cursor[0] = (char)0xfe;
            output_cursor[1] = (char)0xff;
            output_cursor += 2u;
            remaining_output -= 2u;
            descriptor->output_started = 1u;
        }
        if (remaining_output < encoded_length)
            return rin_iconv_fail(descriptor, E2BIG, input, input_left,
                                  output, output_left, input_cursor,
                                  remaining_input, output_cursor,
                                  remaining_output);
        for (size_t index = 0u; index < encoded_length; ++index)
            output_cursor[index] = encoded[index];
        output_cursor += encoded_length;
        remaining_output -= encoded_length;
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
    rin_iconv_table_unlock();
    for (;;) {
        rin_iconv_table_lock();
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
            rin_iconv_table_unlock();
            return 0;
        }
        rin_iconv_table_unlock();
    }
}
