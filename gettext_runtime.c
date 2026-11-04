/* SPDX-License-Identifier: MIT */
/* Bounded GNU MO catalog reader and gettext runtime. */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 202405L
#endif
#define RIN_GETTEXT_RUNTIME_IMPLEMENTATION 1
#include "libintl.h"

#include "errno.h"
#include "fcntl.h"
#include "iconv.h"
#include "limits.h"
#include "locale.h"
#include "stdatomic.h"
#include "stdlib.h"
#include "stdint.h"
#include "string.h"
#include "unistd.h"

#define RIN_INTL_MAX_CATALOG_BYTES (16u * 1024u * 1024u)
#define RIN_INTL_MAX_EXPANDED_BYTES (64u * 1024u * 1024u)
#define RIN_INTL_MAX_MESSAGES 65536u
#define RIN_INTL_MAX_CACHED_CATALOGS 64u
#define RIN_INTL_MAX_CACHE_BYTES (128u * 1024u * 1024u)
#define RIN_INTL_MAX_DOMAIN 256u
#define RIN_INTL_MAX_LOCALE 128u
#define RIN_INTL_MAX_CODESET 128u
#define RIN_INTL_MAX_PATH 4096u
#define RIN_INTL_MAX_PLURAL_EXPRESSION 1024u
#define RIN_INTL_MAX_PLURAL_FORMS 16u

typedef struct RinIntlEntry {
    const char* original;
    size_t original_length;
    char* translation;
    size_t translation_length;
} RinIntlEntry;

typedef struct RinIntlCatalog {
    char* bytes;
    RinIntlEntry* entries;
    size_t entry_count;
    size_t memory_cost;
    char* path;
    char* output_codeset;
    char* plural_expression;
    unsigned int plural_forms;
    struct RinIntlCatalog* next;
} RinIntlCatalog;

typedef struct RinIntlBinding {
    char* domain;
    char* directory;
    char* codeset;
    struct RinIntlBinding* next;
} RinIntlBinding;

static atomic_flag g_rin_intl_lock = ATOMIC_FLAG_INIT;
static RinIntlBinding* g_rin_intl_bindings;
static RinIntlCatalog* g_rin_intl_catalogs;
static size_t g_rin_intl_cached_catalog_count;
static size_t g_rin_intl_cached_bytes;
static char g_rin_intl_default_domain[] = "messages";
static char* g_rin_intl_current_domain = g_rin_intl_default_domain;
static const char g_rin_intl_default_directory[] = "/usr/share/locale";
static const char g_rin_intl_default_plural[] = "n != 1";

static void rin_intl_lock(void)
{
    while (atomic_flag_test_and_set_explicit(&g_rin_intl_lock,
                                             memory_order_acquire)) {
    }
}

static void rin_intl_unlock(void)
{
    atomic_flag_clear_explicit(&g_rin_intl_lock, memory_order_release);
}

static size_t rin_intl_strlen(const char* text)
{
    size_t length = 0u;
    if (!text) return 0u;
    while (text[length] != '\0') ++length;
    return length;
}

static int rin_intl_equal(const char* left, const char* right)
{
    size_t index = 0u;
    if (!left || !right) return left == right;
    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) return 0;
        ++index;
    }
    return left[index] == right[index];
}

static unsigned char rin_intl_ascii_lower(unsigned char value)
{
    if (value >= (unsigned char)'A' && value <= (unsigned char)'Z')
        value = (unsigned char)(value + ('a' - 'A'));
    return value;
}

static int rin_intl_ascii_equal_nocase(const char* left, const char* right,
                                       size_t length)
{
    size_t index;
    for (index = 0u; index < length; ++index) {
        if (rin_intl_ascii_lower((unsigned char)left[index]) !=
            rin_intl_ascii_lower((unsigned char)right[index]))
            return 0;
    }
    return 1;
}

static char* rin_intl_copy_string(const char* text, size_t maximum)
{
    size_t length = rin_intl_strlen(text);
    char* copy;
    if (!text || length > maximum) {
        errno = text ? ENAMETOOLONG : EINVAL;
        return NULL;
    }
    copy = (char*)malloc(length + 1u);
    if (!copy) {
        errno = ENOMEM;
        return NULL;
    }
    memcpy(copy, text, length + 1u);
    return copy;
}

static int rin_intl_valid_domain(const char* domain)
{
    size_t index;
    size_t length = rin_intl_strlen(domain);
    if (!domain || length == 0u || length > RIN_INTL_MAX_DOMAIN ||
        (length == 1u && domain[0] == '.') ||
        (length == 2u && domain[0] == '.' && domain[1] == '.'))
        return 0;
    for (index = 0u; index < length; ++index) {
        unsigned char value = (unsigned char)domain[index];
        if (!((value >= 'A' && value <= 'Z') ||
              (value >= 'a' && value <= 'z') ||
              (value >= '0' && value <= '9') || value == '_' ||
              value == '-' || value == '.' || value == '+'))
            return 0;
    }
    return 1;
}

static RinIntlBinding* rin_intl_find_binding_locked(const char* domain)
{
    RinIntlBinding* binding;
    for (binding = g_rin_intl_bindings; binding; binding = binding->next) {
        if (rin_intl_equal(binding->domain, domain)) return binding;
    }
    return NULL;
}

static void rin_intl_catalog_destroy(RinIntlCatalog* catalog)
{
    size_t index;
    if (!catalog) return;
    if (catalog->entries) {
        for (index = 0u; index < catalog->entry_count; ++index)
            free(catalog->entries[index].translation);
    }
    free(catalog->entries);
    free(catalog->plural_expression);
    free(catalog->output_codeset);
    free(catalog->path);
    free(catalog->bytes);
    free(catalog);
}

static int rin_intl_append(char* output, size_t capacity, size_t* length,
                           const char* text, size_t text_length)
{
    if (text_length > capacity - 1u - *length) return 0;
    if (text_length != 0u) memcpy(output + *length, text, text_length);
    *length += text_length;
    output[*length] = '\0';
    return 1;
}

static int rin_intl_path_component_valid(const char* text)
{
    size_t index;
    size_t length = rin_intl_strlen(text);
    if (!text || length == 0u || length > RIN_INTL_MAX_LOCALE) return 0;
    for (index = 0u; index < length; ++index) {
        unsigned char value = (unsigned char)text[index];
        if (!((value >= 'A' && value <= 'Z') ||
              (value >= 'a' && value <= 'z') ||
              (value >= '0' && value <= '9') || value == '_' ||
              value == '-' || value == '.' || value == '@'))
            return 0;
    }
    return 1;
}

static int rin_intl_build_path(const char* directory, const char* locale,
                               const char* domain, char* output,
                               size_t capacity)
{
    size_t length = 0u;
    size_t directory_length = rin_intl_strlen(directory);
    size_t locale_length = rin_intl_strlen(locale);
    size_t domain_length = rin_intl_strlen(domain);
    if (!directory || directory_length == 0u ||
        directory_length >= capacity ||
        !rin_intl_path_component_valid(locale) ||
        !rin_intl_valid_domain(domain) ||
        domain_length > capacity - 1u)
        return 0;
    output[0] = '\0';
    if (!rin_intl_append(output, capacity, &length, directory,
                         directory_length))
        return 0;
    if (output[length - 1u] != '/' &&
        !rin_intl_append(output, capacity, &length, "/", 1u))
        return 0;
    if (!rin_intl_append(output, capacity, &length, locale, locale_length) ||
        !rin_intl_append(output, capacity, &length, "/LC_MESSAGES/", 13u) ||
        !rin_intl_append(output, capacity, &length, domain, domain_length) ||
        !rin_intl_append(output, capacity, &length, ".mo", 3u))
        return 0;
    return 1;
}

static int rin_intl_load_bytes(const char* path, char** bytes_out,
                               size_t* length_out)
{
    char* bytes = NULL;
    size_t length = 0u;
    size_t capacity = 0u;
    int descriptor = open(path, O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) return 0;
    for (;;) {
        ssize_t amount;
        if (length == capacity) {
            size_t next_capacity;
            char* grown;
            if (capacity == RIN_INTL_MAX_CATALOG_BYTES) {
                char probe;
                do {
                    amount = read(descriptor, &probe, 1u);
                } while (amount < 0 && errno == EINTR);
                if (amount < 0) goto fail;
                if (amount > 0) {
                    errno = EFBIG;
                    goto fail;
                }
                break;
            }
            next_capacity = capacity ? capacity * 2u : 4096u;
            if (next_capacity > RIN_INTL_MAX_CATALOG_BYTES)
                next_capacity = RIN_INTL_MAX_CATALOG_BYTES;
            grown = (char*)realloc(bytes, next_capacity);
            if (!grown) {
                errno = ENOMEM;
                goto fail;
            }
            bytes = grown;
            capacity = next_capacity;
        }
        do {
            amount = read(descriptor, bytes + length, capacity - length);
        } while (amount < 0 && errno == EINTR);
        if (amount < 0) goto fail;
        if (amount == 0) break;
        length += (size_t)amount;
    }
    (void)close(descriptor);
    if (!bytes) {
        bytes = (char*)malloc(1u);
        if (!bytes) {
            errno = ENOMEM;
            return 0;
        }
    }
    *bytes_out = bytes;
    *length_out = length;
    return 1;

fail:
    {
        int saved_errno = errno;
        (void)close(descriptor);
        free(bytes);
        errno = saved_errno;
        return 0;
    }
}

static uint32_t rin_intl_u32(const unsigned char* bytes, int little_endian)
{
    if (little_endian) {
        return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
               ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
    }
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

static int rin_intl_descriptor(const unsigned char* bytes, size_t file_length,
                               uint32_t table_offset, uint32_t count,
                               int little_endian, uint32_t index,
                               uint32_t* string_length,
                               uint32_t* string_offset)
{
    size_t entry_offset;
    uint32_t length;
    uint32_t offset;
    if (index >= count || table_offset > file_length ||
        (size_t)count > (file_length - (size_t)table_offset) / 8u)
        return 0;
    entry_offset = (size_t)table_offset + (size_t)index * 8u;
    length = rin_intl_u32(bytes + entry_offset, little_endian);
    offset = rin_intl_u32(bytes + entry_offset + 4u, little_endian);
    if ((size_t)offset >= file_length ||
        (size_t)length > file_length - (size_t)offset - 1u ||
        bytes[(size_t)offset + (size_t)length] != 0u)
        return 0;
    *string_length = length;
    *string_offset = offset;
    return 1;
}

static int rin_intl_codeset_equal(const char* left, const char* right)
{
    size_t left_index = 0u;
    size_t right_index = 0u;
    for (;;) {
        unsigned char a;
        unsigned char b;
        while (left[left_index] == '-' || left[left_index] == '_' ||
               left[left_index] == '.' || left[left_index] == ' ')
            ++left_index;
        while (right[right_index] == '-' || right[right_index] == '_' ||
               right[right_index] == '.' || right[right_index] == ' ')
            ++right_index;
        a = rin_intl_ascii_lower((unsigned char)left[left_index]);
        b = rin_intl_ascii_lower((unsigned char)right[right_index]);
        if (a != b) return 0;
        if (a == 0u) return 1;
        ++left_index;
        ++right_index;
    }
}

static char* rin_intl_convert(const char* input, size_t input_length,
                              const char* source_codeset,
                              const char* output_codeset,
                              size_t* output_length,
                              size_t* expanded_total)
{
    iconv_t converter;
    char* result;
    size_t capacity;
    size_t produced;
    char* input_cursor = (char*)input;
    size_t input_left = input_length;
    char* output_cursor;
    size_t output_left;
    if (rin_intl_codeset_equal(source_codeset, output_codeset)) {
        result = (char*)malloc(input_length + 1u);
        if (!result) {
            errno = ENOMEM;
            return NULL;
        }
        if (input_length) memcpy(result, input, input_length);
        result[input_length] = '\0';
        *output_length = input_length;
        *expanded_total += input_length;
        return result;
    }
    if (input_length > RIN_INTL_MAX_EXPANDED_BYTES / 4u) {
        errno = EFBIG;
        return NULL;
    }
    capacity = input_length * 2u + 32u;
    if (capacity < 32u) capacity = 32u;
    if (capacity > RIN_INTL_MAX_EXPANDED_BYTES - *expanded_total)
        capacity = RIN_INTL_MAX_EXPANDED_BYTES - *expanded_total;
    converter = iconv_open(output_codeset, source_codeset);
    if (converter == (iconv_t)-1) return NULL;
    result = (char*)malloc(capacity + 1u);
    if (!result) {
        (void)iconv_close(converter);
        errno = ENOMEM;
        return NULL;
    }
    output_cursor = result;
    output_left = capacity;
    while (input_left != 0u) {
        size_t converted = iconv(converter, &input_cursor, &input_left,
                                 &output_cursor, &output_left);
        if (converted != (size_t)-1) continue;
        if (errno != E2BIG) goto fail;
        produced = (size_t)(output_cursor - result);
        if (capacity >= RIN_INTL_MAX_EXPANDED_BYTES - *expanded_total)
            goto fail_large;
        {
            size_t next = capacity * 2u;
            char* grown;
            if (next < capacity ||
                next > RIN_INTL_MAX_EXPANDED_BYTES - *expanded_total)
                next = RIN_INTL_MAX_EXPANDED_BYTES - *expanded_total;
            grown = (char*)realloc(result, next + 1u);
            if (!grown) {
                errno = ENOMEM;
                goto fail;
            }
            result = grown;
            capacity = next;
            output_cursor = result + produced;
            output_left = capacity - produced;
        }
    }
    for (;;) {
        size_t converted = iconv(converter, NULL, NULL,
                                 &output_cursor, &output_left);
        if (converted != (size_t)-1) break;
        if (errno != E2BIG) goto fail;
        produced = (size_t)(output_cursor - result);
        if (capacity >= RIN_INTL_MAX_EXPANDED_BYTES - *expanded_total)
            goto fail_large;
        {
            size_t next = capacity * 2u;
            char* grown;
            if (next < capacity ||
                next > RIN_INTL_MAX_EXPANDED_BYTES - *expanded_total)
                next = RIN_INTL_MAX_EXPANDED_BYTES - *expanded_total;
            grown = (char*)realloc(result, next + 1u);
            if (!grown) {
                errno = ENOMEM;
                goto fail;
            }
            result = grown;
            capacity = next;
            output_cursor = result + produced;
            output_left = capacity - produced;
        }
    }
    produced = (size_t)(output_cursor - result);
    if (produced > RIN_INTL_MAX_EXPANDED_BYTES - *expanded_total)
        goto fail_large;
    result[produced] = '\0';
    (void)iconv_close(converter);
    *output_length = produced;
    *expanded_total += produced;
    return result;

fail_large:
    errno = EFBIG;
fail:
    {
        int saved_errno = errno;
        (void)iconv_close(converter);
        free(result);
        errno = saved_errno;
        return NULL;
    }
}

typedef struct RinIntlPluralParser {
    const char* cursor;
    unsigned long int n;
    unsigned int operations;
    unsigned int depth;
    int failed;
} RinIntlPluralParser;

static void rin_intl_plural_space(RinIntlPluralParser* parser)
{
    while (*parser->cursor == ' ' || *parser->cursor == '\t' ||
           *parser->cursor == '\r' || *parser->cursor == '\n')
        ++parser->cursor;
}

static int rin_intl_plural_take(RinIntlPluralParser* parser,
                                const char* token)
{
    size_t length = rin_intl_strlen(token);
    rin_intl_plural_space(parser);
    if (strncmp(parser->cursor, token, length) != 0) return 0;
    parser->cursor += length;
    if (++parser->operations > 512u) parser->failed = 1;
    return 1;
}

static unsigned long int rin_intl_plural_conditional(
    RinIntlPluralParser* parser, int evaluate);

static unsigned long int rin_intl_plural_primary(
    RinIntlPluralParser* parser, int evaluate)
{
    unsigned long int value = 0u;
    rin_intl_plural_space(parser);
    if (*parser->cursor == '(') {
        ++parser->cursor;
        if (++parser->depth > 64u) {
            parser->failed = 1;
            return 0u;
        }
        value = rin_intl_plural_conditional(parser, evaluate);
        if (!rin_intl_plural_take(parser, ")")) parser->failed = 1;
        if (parser->depth != 0u) --parser->depth;
        return value;
    }
    if (*parser->cursor == 'n') {
        ++parser->cursor;
        if ((*parser->cursor >= 'a' && *parser->cursor <= 'z') ||
            (*parser->cursor >= 'A' && *parser->cursor <= 'Z') ||
            (*parser->cursor >= '0' && *parser->cursor <= '9') ||
            *parser->cursor == '_')
            parser->failed = 1;
        return evaluate ? parser->n : 0u;
    }
    if (*parser->cursor < '0' || *parser->cursor > '9') {
        parser->failed = 1;
        return 0u;
    }
    do {
        unsigned int digit = (unsigned int)(*parser->cursor - '0');
        if (value > (ULONG_MAX - digit) / 10u) parser->failed = 1;
        else value = value * 10u + digit;
        ++parser->cursor;
    } while (*parser->cursor >= '0' && *parser->cursor <= '9');
    return evaluate ? value : 0u;
}

static unsigned long int rin_intl_plural_unary(
    RinIntlPluralParser* parser, int evaluate)
{
    unsigned long int value;
    if (rin_intl_plural_take(parser, "!")) {
        value = rin_intl_plural_unary(parser, evaluate);
        return evaluate ? (value == 0u) : 0u;
    }
    if (rin_intl_plural_take(parser, "+"))
        return rin_intl_plural_unary(parser, evaluate);
    if (rin_intl_plural_take(parser, "-")) {
        parser->failed = 1;
        (void)rin_intl_plural_unary(parser, 0);
        return 0u;
    }
    return rin_intl_plural_primary(parser, evaluate);
}

static unsigned long int rin_intl_plural_multiply(
    RinIntlPluralParser* parser, int evaluate)
{
    unsigned long int value = rin_intl_plural_unary(parser, evaluate);
    for (;;) {
        if (rin_intl_plural_take(parser, "*")) {
            unsigned long int right = rin_intl_plural_unary(parser, evaluate);
            if (evaluate) value *= right;
        } else if (rin_intl_plural_take(parser, "/")) {
            unsigned long int right = rin_intl_plural_unary(parser, evaluate);
            if (evaluate) {
                if (right == 0u) parser->failed = 1;
                else value /= right;
            }
        } else if (rin_intl_plural_take(parser, "%")) {
            unsigned long int right = rin_intl_plural_unary(parser, evaluate);
            if (evaluate) {
                if (right == 0u) parser->failed = 1;
                else value %= right;
            }
        } else break;
    }
    return value;
}

static unsigned long int rin_intl_plural_add(
    RinIntlPluralParser* parser, int evaluate)
{
    unsigned long int value = rin_intl_plural_multiply(parser, evaluate);
    for (;;) {
        if (rin_intl_plural_take(parser, "+")) {
            unsigned long int right = rin_intl_plural_multiply(parser, evaluate);
            if (evaluate) value += right;
        } else if (rin_intl_plural_take(parser, "-")) {
            unsigned long int right = rin_intl_plural_multiply(parser, evaluate);
            if (evaluate) value -= right;
        } else break;
    }
    return value;
}

static unsigned long int rin_intl_plural_shift(
    RinIntlPluralParser* parser, int evaluate)
{
    unsigned long int value = rin_intl_plural_add(parser, evaluate);
    for (;;) {
        if (rin_intl_plural_take(parser, "<<")) {
            unsigned long int right = rin_intl_plural_add(parser, evaluate);
            if (evaluate) {
                if (right >= sizeof(value) * CHAR_BIT) parser->failed = 1;
                else value <<= right;
            }
        } else if (rin_intl_plural_take(parser, ">>")) {
            unsigned long int right = rin_intl_plural_add(parser, evaluate);
            if (evaluate) {
                if (right >= sizeof(value) * CHAR_BIT) parser->failed = 1;
                else value >>= right;
            }
        } else break;
    }
    return value;
}

static unsigned long int rin_intl_plural_relational(
    RinIntlPluralParser* parser, int evaluate)
{
    unsigned long int value = rin_intl_plural_shift(parser, evaluate);
    for (;;) {
        if (rin_intl_plural_take(parser, "<=")) {
            unsigned long int right = rin_intl_plural_shift(parser, evaluate);
            if (evaluate) value = value <= right;
        } else if (rin_intl_plural_take(parser, ">=")) {
            unsigned long int right = rin_intl_plural_shift(parser, evaluate);
            if (evaluate) value = value >= right;
        } else if (rin_intl_plural_take(parser, "<")) {
            unsigned long int right = rin_intl_plural_shift(parser, evaluate);
            if (evaluate) value = value < right;
        } else if (rin_intl_plural_take(parser, ">")) {
            unsigned long int right = rin_intl_plural_shift(parser, evaluate);
            if (evaluate) value = value > right;
        } else break;
    }
    return value;
}

static unsigned long int rin_intl_plural_equality(
    RinIntlPluralParser* parser, int evaluate)
{
    unsigned long int value = rin_intl_plural_relational(parser, evaluate);
    for (;;) {
        if (rin_intl_plural_take(parser, "==")) {
            unsigned long int right = rin_intl_plural_relational(parser, evaluate);
            if (evaluate) value = value == right;
        } else if (rin_intl_plural_take(parser, "!=")) {
            unsigned long int right = rin_intl_plural_relational(parser, evaluate);
            if (evaluate) value = value != right;
        } else break;
    }
    return value;
}

static unsigned long int rin_intl_plural_bitand(
    RinIntlPluralParser* parser, int evaluate)
{
    unsigned long int value = rin_intl_plural_equality(parser, evaluate);
    for (;;) {
        rin_intl_plural_space(parser);
        if (*parser->cursor != '&' || parser->cursor[1] == '&') break;
        (void)rin_intl_plural_take(parser, "&");
        {
            unsigned long int right = rin_intl_plural_equality(parser, evaluate);
            if (evaluate) value &= right;
        }
    }
    return value;
}

static unsigned long int rin_intl_plural_bitxor(
    RinIntlPluralParser* parser, int evaluate)
{
    unsigned long int value = rin_intl_plural_bitand(parser, evaluate);
    while (rin_intl_plural_take(parser, "^")) {
        unsigned long int right = rin_intl_plural_bitand(parser, evaluate);
        if (evaluate) value ^= right;
    }
    return value;
}

static unsigned long int rin_intl_plural_bitor(
    RinIntlPluralParser* parser, int evaluate)
{
    unsigned long int value = rin_intl_plural_bitxor(parser, evaluate);
    for (;;) {
        rin_intl_plural_space(parser);
        if (*parser->cursor != '|' || parser->cursor[1] == '|') break;
        (void)rin_intl_plural_take(parser, "|");
        {
            unsigned long int right = rin_intl_plural_bitxor(parser, evaluate);
            if (evaluate) value |= right;
        }
    }
    return value;
}

static unsigned long int rin_intl_plural_and(
    RinIntlPluralParser* parser, int evaluate)
{
    unsigned long int value = rin_intl_plural_bitor(parser, evaluate);
    while (rin_intl_plural_take(parser, "&&")) {
        int left_true = value != 0u;
        unsigned long int right = rin_intl_plural_bitor(
            parser, evaluate && left_true);
        if (evaluate) value = left_true && right != 0u;
    }
    return value;
}

static unsigned long int rin_intl_plural_or(
    RinIntlPluralParser* parser, int evaluate)
{
    unsigned long int value = rin_intl_plural_and(parser, evaluate);
    while (rin_intl_plural_take(parser, "||")) {
        int left_true = value != 0u;
        unsigned long int right = rin_intl_plural_and(
            parser, evaluate && !left_true);
        if (evaluate) value = left_true || right != 0u;
    }
    return value;
}

static unsigned long int rin_intl_plural_conditional(
    RinIntlPluralParser* parser, int evaluate)
{
    unsigned long int condition = rin_intl_plural_or(parser, evaluate);
    rin_intl_plural_space(parser);
    if (*parser->cursor == '?') {
        unsigned long int when_true;
        unsigned long int when_false;
        ++parser->cursor;
        if (++parser->operations > 512u || ++parser->depth > 64u) {
            parser->failed = 1;
            return 0u;
        }
        when_true = rin_intl_plural_conditional(
            parser, evaluate && condition != 0u);
        if (!rin_intl_plural_take(parser, ":")) parser->failed = 1;
        when_false = rin_intl_plural_conditional(
            parser, evaluate && condition == 0u);
        if (parser->depth != 0u) --parser->depth;
        return evaluate ? (condition != 0u ? when_true : when_false) : 0u;
    }
    return condition;
}

static int rin_intl_plural_evaluate(const char* expression,
                                    unsigned long int n,
                                    unsigned long int* result)
{
    RinIntlPluralParser parser;
    parser.cursor = expression;
    parser.n = n;
    parser.operations = 0u;
    parser.depth = 0u;
    parser.failed = 0;
    *result = rin_intl_plural_conditional(&parser, 1);
    rin_intl_plural_space(&parser);
    return !parser.failed && *parser.cursor == '\0';
}

static const char* rin_intl_find_header_field(const char* header,
                                               size_t length,
                                               const char* field)
{
    size_t field_length = rin_intl_strlen(field);
    size_t position = 0u;
    while (position < length) {
        size_t end = position;
        while (end < length && header[end] != '\n' && header[end] != '\r')
            ++end;
        if (end - position >= field_length &&
            rin_intl_ascii_equal_nocase(header + position, field,
                                        field_length))
            return header + position + field_length;
        position = end;
        while (position < length &&
               (header[position] == '\n' || header[position] == '\r'))
            ++position;
    }
    return NULL;
}

static int rin_intl_parse_plural_header(RinIntlCatalog* catalog,
                                        const char* header, size_t length)
{
    const char* value = rin_intl_find_header_field(
        header, length, "Plural-Forms:");
    const char* end = header + length;
    unsigned int forms = 0u;
    char expression[RIN_INTL_MAX_PLURAL_EXPRESSION + 1u];
    size_t expression_length;
    size_t index;
    unsigned long int ignored;
    if (!value) return 1;
    while (value < end && (*value == ' ' || *value == '\t')) ++value;
    if ((size_t)(end - value) < 8u ||
        !rin_intl_ascii_equal_nocase(value, "nplurals", 8u))
        return 0;
    value += 8u;
    while (value < end && (*value == ' ' || *value == '\t')) ++value;
    if (value >= end || *value++ != '=') return 0;
    while (value < end && (*value == ' ' || *value == '\t')) ++value;
    if (value >= end || *value < '0' || *value > '9') return 0;
    do {
        unsigned int digit = (unsigned int)(*value++ - '0');
        if (forms > (RIN_INTL_MAX_PLURAL_FORMS - digit) / 10u) return 0;
        forms = forms * 10u + digit;
    } while (value < end && *value >= '0' && *value <= '9');
    while (value < end && (*value == ' ' || *value == '\t')) ++value;
    if (value >= end || *value++ != ';' || forms == 0u ||
        forms > RIN_INTL_MAX_PLURAL_FORMS)
        return 0;
    while (value < end && (*value == ' ' || *value == '\t')) ++value;
    if ((size_t)(end - value) < 6u ||
        !rin_intl_ascii_equal_nocase(value, "plural", 6u))
        return 0;
    value += 6u;
    while (value < end && (*value == ' ' || *value == '\t')) ++value;
    if (value >= end || *value++ != '=') return 0;
    while (value < end && (*value == ' ' || *value == '\t')) ++value;
    {
        const char* expression_end = value;
        while (expression_end < end && *expression_end != ';' &&
               *expression_end != '\n' && *expression_end != '\r')
            ++expression_end;
        while (expression_end > value &&
               (expression_end[-1] == ' ' || expression_end[-1] == '\t'))
            --expression_end;
        expression_length = (size_t)(expression_end - value);
    }
    if (expression_length == 0u ||
        expression_length > RIN_INTL_MAX_PLURAL_EXPRESSION)
        return 0;
    for (index = 0u; index < expression_length; ++index)
        expression[index] = value[index];
    expression[expression_length] = '\0';
    if (!rin_intl_plural_evaluate(expression, 1u, &ignored)) return 0;
    free(catalog->plural_expression);
    catalog->plural_expression = rin_intl_copy_string(
        expression, RIN_INTL_MAX_PLURAL_EXPRESSION);
    if (!catalog->plural_expression) return -1;
    catalog->plural_forms = forms;
    return 1;
}

static int rin_intl_parse_codeset(const char* header, size_t length,
                                  const char* fallback, char* codeset,
                                  size_t capacity)
{
    const char* value = rin_intl_find_header_field(
        header, length, "Content-Type:");
    const char* end = header + length;
    const char* cursor;
    size_t length_out = 0u;
    if (!value) value = fallback;
    else {
        while (value < end && *value != '\n' && *value != '\r') {
            if ((size_t)(end - value) >= 8u &&
                rin_intl_ascii_equal_nocase(value, "charset=", 8u)) {
                value += 8u;
                break;
            }
            ++value;
        }
        if (value >= end || *value == '\n' || *value == '\r')
            value = fallback;
    }
    if (!value || !*value) value = "UTF-8";
    cursor = value;
    if (*cursor == '"' || *cursor == '\'') {
        char quote = *cursor++;
        while (*cursor && *cursor != quote && *cursor != ';' &&
               *cursor != '\n' && *cursor != '\r') {
            if (length_out + 1u >= capacity) return 0;
            codeset[length_out++] = *cursor++;
        }
    } else {
        while (*cursor && *cursor != ';' && *cursor != ' ' &&
               *cursor != '\t' && *cursor != '\n' && *cursor != '\r') {
            if (length_out + 1u >= capacity) return 0;
            codeset[length_out++] = *cursor++;
        }
    }
    if (length_out == 0u) return 0;
    codeset[length_out] = '\0';
    return 1;
}

static size_t rin_intl_original_key_length(const RinIntlEntry* entry)
{
    size_t index;
    for (index = 0u; index < entry->original_length; ++index) {
        if (entry->original[index] == '\0') return index;
    }
    return entry->original_length;
}

static int rin_intl_entry_compare(const void* left, const void* right)
{
    const RinIntlEntry* a = (const RinIntlEntry*)left;
    const RinIntlEntry* b = (const RinIntlEntry*)right;
    size_t a_length = rin_intl_original_key_length(a);
    size_t b_length = rin_intl_original_key_length(b);
    size_t common = a_length < b_length ? a_length : b_length;
    int order = memcmp(a->original, b->original, common);
    if (order != 0) return order;
    if (a_length < b_length) return -1;
    if (a_length > b_length) return 1;
    return 0;
}

static RinIntlCatalog* rin_intl_parse_mo(char* bytes, size_t file_length,
                                         const char* path,
                                         const char* output_codeset)
{
    const unsigned char* raw = (const unsigned char*)bytes;
    uint32_t magic;
    uint32_t revision;
    uint32_t count;
    uint32_t original_table;
    uint32_t translation_table;
    uint32_t hash_count;
    uint32_t hash_offset;
    uint32_t header_index = UINT32_MAX;
    int little_endian;
    size_t expanded_total = 0u;
    size_t index;
    char source_codeset[RIN_INTL_MAX_CODESET];
    RinIntlCatalog* catalog;
    if (file_length < 28u) {
        errno = EINVAL;
        return NULL;
    }
    magic = rin_intl_u32(raw, 1);
    if (magic == 0x950412deu) little_endian = 1;
    else if (magic == 0xde120495u) little_endian = 0;
    else {
        errno = EINVAL;
        return NULL;
    }
    revision = rin_intl_u32(raw + 4u, little_endian);
    if ((revision >> 16) != 0u || (revision & 0xffffu) > 1u) {
        errno = ENOTSUP;
        return NULL;
    }
    count = rin_intl_u32(raw + 8u, little_endian);
    original_table = rin_intl_u32(raw + 12u, little_endian);
    translation_table = rin_intl_u32(raw + 16u, little_endian);
    hash_count = rin_intl_u32(raw + 20u, little_endian);
    hash_offset = rin_intl_u32(raw + 24u, little_endian);
    if (count == 0u || count > RIN_INTL_MAX_MESSAGES ||
        original_table > file_length || translation_table > file_length ||
        (size_t)count > (file_length - original_table) / 8u ||
        (size_t)count > (file_length - translation_table) / 8u ||
        (hash_count != 0u &&
         (hash_offset > file_length ||
          (size_t)hash_count > (file_length - hash_offset) / 4u))) {
        errno = EINVAL;
        return NULL;
    }
    for (index = 0u; index < count; ++index) {
        uint32_t original_length;
        uint32_t original_offset;
        if (!rin_intl_descriptor(raw, file_length, original_table, count,
                                 little_endian, (uint32_t)index,
                                 &original_length, &original_offset)) {
            errno = EINVAL;
            return NULL;
        }
        if (original_length == 0u) header_index = (uint32_t)index;
    }
    if (header_index == UINT32_MAX) {
        errno = EINVAL;
        return NULL;
    }
    {
        uint32_t header_length;
        uint32_t header_offset;
        const char* header;
        int plural_result;
        if (!rin_intl_descriptor(raw, file_length, translation_table, count,
                                 little_endian, header_index,
                                 &header_length, &header_offset) ||
            !rin_intl_parse_codeset(bytes + header_offset, header_length,
                                    output_codeset, source_codeset,
                                    sizeof(source_codeset))) {
            errno = EINVAL;
            return NULL;
        }
        header = bytes + header_offset;
        catalog = (RinIntlCatalog*)calloc(1u, sizeof(*catalog));
        if (!catalog) {
            errno = ENOMEM;
            return NULL;
        }
        catalog->plural_expression = (char*)malloc(
            sizeof(g_rin_intl_default_plural));
        if (!catalog->plural_expression) {
            free(catalog);
            errno = ENOMEM;
            return NULL;
        }
        memcpy(catalog->plural_expression, g_rin_intl_default_plural,
               sizeof(g_rin_intl_default_plural));
        catalog->plural_forms = 2u;
        plural_result = rin_intl_parse_plural_header(catalog, header,
                                                      header_length);
        if (plural_result <= 0) {
            int saved_errno = errno;
            rin_intl_catalog_destroy(catalog);
            errno = plural_result < 0 ? saved_errno : EINVAL;
            return NULL;
        }
    }
    catalog->entry_count = count;
    catalog->entries = (RinIntlEntry*)calloc(count, sizeof(*catalog->entries));
    catalog->path = rin_intl_copy_string(path, RIN_INTL_MAX_PATH - 1u);
    catalog->output_codeset = rin_intl_copy_string(
        output_codeset, RIN_INTL_MAX_CODESET - 1u);
    if (!catalog->entries || !catalog->path || !catalog->output_codeset) {
        int saved_errno = errno;
        rin_intl_catalog_destroy(catalog);
        errno = saved_errno ? saved_errno : ENOMEM;
        return NULL;
    }
    for (index = 0u; index < count; ++index) {
        uint32_t original_length;
        uint32_t original_offset;
        uint32_t translation_length;
        uint32_t translation_offset;
        RinIntlEntry* entry = &catalog->entries[index];
        if (!rin_intl_descriptor(raw, file_length, original_table, count,
                                 little_endian, (uint32_t)index,
                                 &original_length, &original_offset) ||
            !rin_intl_descriptor(raw, file_length, translation_table, count,
                                 little_endian, (uint32_t)index,
                                 &translation_length, &translation_offset)) {
            errno = EINVAL;
            rin_intl_catalog_destroy(catalog);
            return NULL;
        }
        entry->original = bytes + original_offset;
        entry->original_length = original_length;
        entry->translation = rin_intl_convert(
            bytes + translation_offset, translation_length, source_codeset,
            output_codeset, &entry->translation_length, &expanded_total);
        if (!entry->translation) {
            int saved_errno = errno;
            rin_intl_catalog_destroy(catalog);
            errno = saved_errno;
            return NULL;
        }
    }
    qsort(catalog->entries, catalog->entry_count,
          sizeof(*catalog->entries), rin_intl_entry_compare);
    for (index = 1u; index < catalog->entry_count; ++index) {
        if (rin_intl_entry_compare(&catalog->entries[index - 1u],
                                   &catalog->entries[index]) == 0) {
            rin_intl_catalog_destroy(catalog);
            errno = EINVAL;
            return NULL;
        }
    }
    catalog->memory_cost = sizeof(*catalog) + file_length + expanded_total +
        count * sizeof(*catalog->entries) +
        rin_intl_strlen(path) + 1u +
        rin_intl_strlen(output_codeset) + 1u +
        rin_intl_strlen(catalog->plural_expression) + 1u;
    if (catalog->memory_cost > RIN_INTL_MAX_CACHE_BYTES) {
        rin_intl_catalog_destroy(catalog);
        errno = EFBIG;
        return NULL;
    }
    catalog->bytes = bytes;
    return catalog;
}

static RinIntlCatalog* rin_intl_read_catalog(const char* path,
                                             const char* output_codeset)
{
    char* bytes;
    size_t length;
    RinIntlCatalog* catalog;
    if (!rin_intl_load_bytes(path, &bytes, &length)) return NULL;
    catalog = rin_intl_parse_mo(bytes, length, path, output_codeset);
    if (!catalog) free(bytes);
    return catalog;
}

static RinIntlCatalog* rin_intl_cached_catalog(const char* path,
                                                const char* codeset)
{
    RinIntlCatalog* catalog;
    rin_intl_lock();
    for (catalog = g_rin_intl_catalogs; catalog; catalog = catalog->next) {
        if (rin_intl_equal(catalog->path, path) &&
            rin_intl_equal(catalog->output_codeset, codeset))
            break;
    }
    rin_intl_unlock();
    return catalog;
}

static int rin_intl_locale_candidates(const char* locale,
                                      char candidates[3][RIN_INTL_MAX_LOCALE])
{
    size_t length = rin_intl_strlen(locale);
    size_t first_cut = length;
    size_t second_cut;
    size_t index;
    if (!rin_intl_path_component_valid(locale)) return 0;
    memcpy(candidates[0], locale, length + 1u);
    for (index = 0u; index < length; ++index) {
        if (locale[index] == '.' || locale[index] == '@') {
            first_cut = index;
            break;
        }
    }
    if (first_cut == length) {
        for (index = 0u; index < length; ++index) {
            if (locale[index] == '_') {
                memcpy(candidates[1], locale, index);
                candidates[1][index] = '\0';
                return index == 0u ? 1 : 2;
            }
        }
        return 1;
    }
    memcpy(candidates[1], locale, first_cut);
    candidates[1][first_cut] = '\0';
    if (first_cut == 0u || first_cut == length) return 1;
    second_cut = first_cut;
    for (index = 0u; index < first_cut; ++index) {
        if (locale[index] == '_') {
            second_cut = index;
            break;
        }
    }
    if (second_cut == first_cut || second_cut == 0u) return 2;
    memcpy(candidates[2], locale, second_cut);
    candidates[2][second_cut] = '\0';
    return 3;
}

static RinIntlCatalog* rin_intl_get_catalog(const char* domain,
                                             const char* locale,
                                             const char* codeset)
{
    char directory[RIN_INTL_MAX_PATH];
    char selected_codeset[RIN_INTL_MAX_CODESET];
    char candidates[3][RIN_INTL_MAX_LOCALE];
    size_t candidate_count;
    size_t index;
    RinIntlCatalog* catalog;
    RinIntlBinding* binding;
    if (!domain || !rin_intl_valid_domain(domain) || !locale || !codeset)
        return NULL;
    rin_intl_lock();
    binding = rin_intl_find_binding_locked(domain);
    if (binding && binding->directory) {
        size_t length = rin_intl_strlen(binding->directory);
        if (length >= sizeof(directory)) {
            rin_intl_unlock();
            return NULL;
        }
        memcpy(directory, binding->directory, length + 1u);
    } else {
        memcpy(directory, g_rin_intl_default_directory,
               sizeof(g_rin_intl_default_directory));
    }
    if (binding && binding->codeset) {
        size_t length = rin_intl_strlen(binding->codeset);
        if (length >= sizeof(selected_codeset)) {
            rin_intl_unlock();
            return NULL;
        }
        memcpy(selected_codeset, binding->codeset, length + 1u);
    } else {
        size_t length = rin_intl_strlen(codeset);
        if (length >= sizeof(selected_codeset)) {
            rin_intl_unlock();
            return NULL;
        }
        memcpy(selected_codeset, codeset, length + 1u);
    }
    rin_intl_unlock();
    if (rin_intl_equal(locale, "C") || rin_intl_equal(locale, "POSIX"))
        return NULL;
    candidate_count = (size_t)rin_intl_locale_candidates(locale, candidates);
    for (index = 0u; index < candidate_count; ++index) {
        char path[RIN_INTL_MAX_PATH];
        if (!rin_intl_build_path(directory, candidates[index], domain, path,
                                 sizeof(path)))
            continue;
        catalog = rin_intl_cached_catalog(path, selected_codeset);
        if (catalog) return catalog;
        catalog = rin_intl_read_catalog(path, selected_codeset);
        if (!catalog) continue;
        rin_intl_lock();
        {
            RinIntlCatalog* existing;
            for (existing = g_rin_intl_catalogs; existing;
                 existing = existing->next) {
                if (rin_intl_equal(existing->path, path) &&
                    rin_intl_equal(existing->output_codeset,
                                   selected_codeset))
                    break;
            }
            if (existing) {
                rin_intl_unlock();
                rin_intl_catalog_destroy(catalog);
                return existing;
            }
            if (g_rin_intl_cached_catalog_count >=
                    RIN_INTL_MAX_CACHED_CATALOGS ||
                catalog->memory_cost > RIN_INTL_MAX_CACHE_BYTES -
                    g_rin_intl_cached_bytes) {
                rin_intl_unlock();
                rin_intl_catalog_destroy(catalog);
                return NULL;
            }
            catalog->next = g_rin_intl_catalogs;
            g_rin_intl_catalogs = catalog;
            ++g_rin_intl_cached_catalog_count;
            g_rin_intl_cached_bytes += catalog->memory_cost;
        }
        rin_intl_unlock();
        return catalog;
    }
    return NULL;
}

static RinIntlEntry* rin_intl_find_entry(RinIntlCatalog* catalog,
                                          const char* msgid)
{
    size_t low = 0u;
    size_t high = catalog->entry_count;
    size_t key_length = rin_intl_strlen(msgid);
    while (low < high) {
        size_t middle = low + (high - low) / 2u;
        RinIntlEntry* entry = &catalog->entries[middle];
        size_t original_length = rin_intl_original_key_length(entry);
        size_t common = key_length < original_length
                            ? key_length : original_length;
        int order = memcmp(msgid, entry->original, common);
        if (order == 0) {
            if (key_length < original_length) order = -1;
            else if (key_length > original_length) order = 1;
        }
        if (order == 0) return entry;
        if (order < 0) high = middle;
        else low = middle + 1u;
    }
    return NULL;
}

static const char* rin_intl_plural_translation(const RinIntlEntry* entry,
                                                unsigned long int index)
{
    const char* cursor = entry->translation;
    size_t remaining = entry->translation_length;
    while (index != 0u) {
        const char* separator = (const char*)memchr(cursor, '\0', remaining);
        if (!separator) return NULL;
        remaining -= (size_t)(separator + 1 - cursor);
        cursor = separator + 1;
        --index;
    }
    if (remaining == 0u) return NULL;
    return cursor;
}

static char* rin_intl_translate(const char* domain, const char* singular,
                                const char* plural, unsigned long int n,
                                int category)
{
    locale_t locale_object = uselocale(NULL);
    const char* locale_name = rin_locale_l_name(locale_object, category);
    const char* output_codeset = rin_locale_l_codeset(locale_object);
    RinIntlCatalog* catalog;
    RinIntlEntry* entry;
    unsigned long int plural_index = n == 1u ? 0u : 1u;
    const char* translated;
    if (!singular) return NULL;
    if (!plural) plural = singular;
    if (!domain || domain[0] == '\0') {
        rin_intl_lock();
        domain = g_rin_intl_current_domain;
        rin_intl_unlock();
    }
    if (!locale_name || !output_codeset || !rin_intl_valid_domain(domain))
        return (char*)(n == 1u ? singular : plural);
    catalog = rin_intl_get_catalog(domain, locale_name, output_codeset);
    if (!catalog) return (char*)(n == 1u ? singular : plural);
    entry = rin_intl_find_entry(catalog, singular);
    if (!entry) return (char*)(n == 1u ? singular : plural);
    if (plural && !rin_intl_plural_evaluate(catalog->plural_expression, n,
                                            &plural_index))
        plural_index = n == 1u ? 0u : 1u;
    if (plural_index >= catalog->plural_forms)
        plural_index = n == 1u ? 0u : 1u;
    translated = rin_intl_plural_translation(entry, plural_index);
    if (!translated || translated[0] == '\0')
        return (char*)(n == 1u ? singular : plural);
    return (char*)translated;
}

char* dcgettext(const char* domainname, const char* msgid, int category)
{
    int saved_errno = errno;
    char* result;
    if (category < LC_CTYPE || category >= LC_ALL) category = LC_MESSAGES;
    result = rin_intl_translate(domainname, msgid, msgid, 1u, category);
    errno = saved_errno;
    return result;
}

char* dgettext(const char* domainname, const char* msgid)
{
    return dcgettext(domainname, msgid, LC_MESSAGES);
}

char* gettext(const char* msgid)
{
    return dgettext(NULL, msgid);
}

char* dcngettext(const char* domainname, const char* msgid,
                 const char* msgid_plural, unsigned long int n,
                 int category)
{
    int saved_errno = errno;
    char* result;
    if (category < LC_CTYPE || category >= LC_ALL) category = LC_MESSAGES;
    result = rin_intl_translate(domainname, msgid, msgid_plural, n, category);
    errno = saved_errno;
    return result;
}

char* dngettext(const char* domainname, const char* msgid,
                const char* msgid_plural, unsigned long int n)
{
    return dcngettext(domainname, msgid, msgid_plural, n, LC_MESSAGES);
}

char* ngettext(const char* msgid, const char* msgid_plural,
               unsigned long int n)
{
    return dngettext(NULL, msgid, msgid_plural, n);
}

char* textdomain(const char* domainname)
{
    int saved_errno = errno;
    char* replacement;
    char* result;
    if (!domainname) {
        rin_intl_lock();
        result = g_rin_intl_current_domain;
        rin_intl_unlock();
        errno = saved_errno;
        return result;
    }
    if (domainname[0] == '\0') domainname = g_rin_intl_default_domain;
    if (!rin_intl_valid_domain(domainname)) {
        errno = EINVAL;
        return NULL;
    }
    replacement = rin_intl_copy_string(domainname, RIN_INTL_MAX_DOMAIN);
    if (!replacement) return NULL;
    rin_intl_lock();
    g_rin_intl_current_domain = replacement;
    result = replacement;
    rin_intl_unlock();
    errno = saved_errno;
    return result;
}

char* bindtextdomain(const char* domainname, const char* dirname)
{
    int saved_errno = errno;
    RinIntlBinding* binding;
    RinIntlBinding* new_binding = NULL;
    char* directory_copy = NULL;
    char* domain_copy = NULL;
    char* result;
    if (!domainname || !rin_intl_valid_domain(domainname)) {
        errno = EINVAL;
        return NULL;
    }
    if (dirname) {
        if (dirname[0] == '\0') dirname = g_rin_intl_default_directory;
        directory_copy = rin_intl_copy_string(dirname, RIN_INTL_MAX_PATH - 1u);
        domain_copy = rin_intl_copy_string(domainname, RIN_INTL_MAX_DOMAIN);
        if (!directory_copy || !domain_copy) {
            free(directory_copy);
            free(domain_copy);
            return NULL;
        }
        new_binding = (RinIntlBinding*)calloc(1u, sizeof(*new_binding));
        if (!new_binding) {
            free(directory_copy);
            free(domain_copy);
            errno = ENOMEM;
            return NULL;
        }
    }
    rin_intl_lock();
    binding = rin_intl_find_binding_locked(domainname);
    if (!binding && dirname) {
        binding = new_binding;
        new_binding = NULL;
        binding->domain = domain_copy;
        domain_copy = NULL;
        binding->next = g_rin_intl_bindings;
        g_rin_intl_bindings = binding;
    }
    if (binding && dirname) binding->directory = directory_copy;
    directory_copy = NULL;
    result = binding && binding->directory
                 ? binding->directory : (char*)g_rin_intl_default_directory;
    rin_intl_unlock();
    free(new_binding);
    free(domain_copy);
    errno = saved_errno;
    return result;
}

char* bind_textdomain_codeset(const char* domainname, const char* codeset)
{
    int saved_errno = errno;
    RinIntlBinding* binding;
    RinIntlBinding* new_binding = NULL;
    char* codeset_copy = NULL;
    char* domain_copy = NULL;
    char* result;
    if (!domainname || !rin_intl_valid_domain(domainname)) {
        errno = EINVAL;
        return NULL;
    }
    if (codeset) {
        if (codeset[0] == '\0') {
            errno = EINVAL;
            return NULL;
        }
        codeset_copy = rin_intl_copy_string(codeset,
                                             RIN_INTL_MAX_CODESET - 1u);
        domain_copy = rin_intl_copy_string(domainname, RIN_INTL_MAX_DOMAIN);
        if (!codeset_copy || !domain_copy) {
            free(codeset_copy);
            free(domain_copy);
            return NULL;
        }
        new_binding = (RinIntlBinding*)calloc(1u, sizeof(*new_binding));
        if (!new_binding) {
            free(codeset_copy);
            free(domain_copy);
            errno = ENOMEM;
            return NULL;
        }
    }
    rin_intl_lock();
    binding = rin_intl_find_binding_locked(domainname);
    if (!binding && codeset) {
        binding = new_binding;
        new_binding = NULL;
        binding->domain = domain_copy;
        domain_copy = NULL;
        binding->next = g_rin_intl_bindings;
        g_rin_intl_bindings = binding;
    }
    if (binding && codeset) binding->codeset = codeset_copy;
    codeset_copy = NULL;
    result = binding ? binding->codeset : NULL;
    rin_intl_unlock();
    free(new_binding);
    free(domain_copy);
    errno = saved_errno;
    return result;
}
