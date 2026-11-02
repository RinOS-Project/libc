/* SPDX-License-Identifier: MIT */
#include "nl_types.h"

#include "errno.h"
#include "fcntl.h"
#include "limits.h"
#include "locale.h"
#include "stdatomic.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"

#define RIN_CAT_SLOT_COUNT 128u
#define RIN_CAT_SLOT_BITS 8u
#define RIN_CAT_SLOT_MASK ((uintptr_t)0xffu)
#define RIN_CAT_MAX_GENERATION (UINTPTR_MAX >> RIN_CAT_SLOT_BITS)
#define RIN_CAT_MAX_FILE_BYTES (16u * 1024u * 1024u)
#define RIN_CAT_MAX_MESSAGES 65536u
#define RIN_CAT_PATH_BYTES 4096u

typedef struct RinCatalogMessage {
    uint32_t set_id;
    uint32_t message_id;
    char* text;
} RinCatalogMessage;

typedef struct RinCatalog {
    char* bytes;
    RinCatalogMessage* messages;
    size_t message_count;
} RinCatalog;

typedef struct RinCatalogSlot {
    RinCatalog* catalog;
    uintptr_t generation;
    unsigned char state; /* 0 free, 1 open, 2 permanently retired */
} RinCatalogSlot;

static RinCatalogSlot g_catalog_slots[RIN_CAT_SLOT_COUNT];
static atomic_flag g_catalog_lock = ATOMIC_FLAG_INIT;

static void rin_catalog_lock(void) {
    while (atomic_flag_test_and_set_explicit(&g_catalog_lock,
                                             memory_order_acquire)) {
    }
}

static void rin_catalog_unlock(void) {
    atomic_flag_clear_explicit(&g_catalog_lock, memory_order_release);
}

static size_t rin_cat_strlen(const char* text) {
    size_t length = 0u;
    if (!text) return 0u;
    while (text[length] != '\0') ++length;
    return length;
}

static void rin_catalog_destroy(RinCatalog* catalog) {
    if (!catalog) return;
    free(catalog->messages);
    free(catalog->bytes);
    free(catalog);
}

static int rin_catalog_parse_unsigned(const char** cursor, const char* end,
                                      uint32_t* result) {
    const char* text = *cursor;
    uint32_t value = 0u;
    if (text >= end || *text < '0' || *text > '9') return 0;
    do {
        unsigned int digit = (unsigned int)(*text - '0');
        if (value > (UINT32_MAX - digit) / 10u) return 0;
        value = value * 10u + digit;
        if (value > (uint32_t)INT_MAX) return 0;
        ++text;
    } while (text < end && *text >= '0' && *text <= '9');
    if (value == 0u) return 0;
    *cursor = text;
    *result = value;
    return 1;
}

static int rin_catalog_hex(unsigned char value) {
    if (value >= (unsigned char)'0' && value <= (unsigned char)'9')
        return (int)(value - (unsigned char)'0');
    if (value >= (unsigned char)'a' && value <= (unsigned char)'f')
        return 10 + (int)(value - (unsigned char)'a');
    if (value >= (unsigned char)'A' && value <= (unsigned char)'F')
        return 10 + (int)(value - (unsigned char)'A');
    return -1;
}

static int rin_catalog_decode_message(char* begin, char* end) {
    char* input = begin;
    char* output = begin;
    while (input < end) {
        unsigned char value = (unsigned char)*input++;
        if (value == (unsigned char)'\\') {
            int high;
            int low;
            if (input >= end) return 0;
            value = (unsigned char)*input++;
            switch (value) {
                case (unsigned char)'\\': value = (unsigned char)'\\'; break;
                case (unsigned char)'n': value = (unsigned char)'\n'; break;
                case (unsigned char)'r': value = (unsigned char)'\r'; break;
                case (unsigned char)'t': value = (unsigned char)'\t'; break;
                case (unsigned char)'x':
                    if ((size_t)(end - input) < 2u) return 0;
                    high = rin_catalog_hex((unsigned char)input[0]);
                    low = rin_catalog_hex((unsigned char)input[1]);
                    if (high < 0 || low < 0) return 0;
                    value = (unsigned char)((high << 4) | low);
                    input += 2;
                    break;
                default: return 0;
            }
        }
        if (value == 0u) return 0;
        *output++ = (char)value;
    }
    *output = '\0';
    return 1;
}

static int rin_catalog_message_compare(const void* left, const void* right) {
    const RinCatalogMessage* a = (const RinCatalogMessage*)left;
    const RinCatalogMessage* b = (const RinCatalogMessage*)right;
    if (a->set_id < b->set_id) return -1;
    if (a->set_id > b->set_id) return 1;
    if (a->message_id < b->message_id) return -1;
    if (a->message_id > b->message_id) return 1;
    return 0;
}

static int rin_catalog_append_message(RinCatalog* catalog, size_t* capacity,
                                      uint32_t set_id, uint32_t message_id,
                                      char* text) {
    RinCatalogMessage* grown;
    size_t next_capacity;
    if (catalog->message_count >= RIN_CAT_MAX_MESSAGES) return 0;
    if (catalog->message_count == *capacity) {
        next_capacity = *capacity ? *capacity * 2u : 16u;
        if (next_capacity > RIN_CAT_MAX_MESSAGES)
            next_capacity = RIN_CAT_MAX_MESSAGES;
        grown = (RinCatalogMessage*)realloc(
            catalog->messages, next_capacity * sizeof(*grown));
        if (!grown) {
            errno = ENOMEM;
            return -1;
        }
        catalog->messages = grown;
        *capacity = next_capacity;
    }
    catalog->messages[catalog->message_count].set_id = set_id;
    catalog->messages[catalog->message_count].message_id = message_id;
    catalog->messages[catalog->message_count].text = text;
    ++catalog->message_count;
    return 1;
}

static int rin_catalog_parse(RinCatalog* catalog, size_t length) {
    static const char signature[] = "RINCAT1\n";
    size_t position = sizeof(signature) - 1u;
    size_t capacity = 0u;
    if (length < sizeof(signature) - 1u ||
        memcmp(catalog->bytes, signature, sizeof(signature) - 1u) != 0)
        return 0;
    while (position < length) {
        size_t line_start = position;
        size_t line_end;
        char* cursor;
        char* text_start;
        uint32_t set_id;
        uint32_t message_id;
        int appended;
        while (position < length && catalog->bytes[position] != '\n')
            ++position;
        line_end = position;
        if (position < length) ++position;
        if (line_end > line_start && catalog->bytes[line_end - 1u] == '\r')
            --line_end;
        catalog->bytes[line_end] = '\0';
        cursor = catalog->bytes + line_start;
        while (cursor < catalog->bytes + line_end &&
               (*cursor == ' ' || *cursor == '\t'))
            ++cursor;
        if (cursor == catalog->bytes + line_end || *cursor == '#') continue;
        {
            const char* number_cursor = cursor;
            const char* number_end = catalog->bytes + line_end;
            if (!rin_catalog_parse_unsigned(&number_cursor, number_end,
                                            &set_id) ||
                number_cursor >= number_end || *number_cursor != ' ')
                return 0;
            cursor = (char*)number_cursor;
        }
        while (cursor < catalog->bytes + line_end && *cursor == ' ') ++cursor;
        {
            const char* number_cursor = cursor;
            const char* number_end = catalog->bytes + line_end;
            if (!rin_catalog_parse_unsigned(&number_cursor, number_end,
                                            &message_id) ||
                number_cursor >= number_end || *number_cursor != '\t')
                return 0;
            text_start = (char*)number_cursor + 1;
        }
        if (!rin_catalog_decode_message(text_start,
                                       catalog->bytes + line_end))
            return 0;
        appended = rin_catalog_append_message(catalog, &capacity, set_id,
                                               message_id, text_start);
        if (appended <= 0) return appended;
    }
    if (catalog->message_count != 0u) {
        size_t index;
        qsort(catalog->messages, catalog->message_count,
              sizeof(*catalog->messages), rin_catalog_message_compare);
        for (index = 1u; index < catalog->message_count; ++index) {
            if (rin_catalog_message_compare(&catalog->messages[index - 1u],
                                            &catalog->messages[index]) == 0)
                return 0;
        }
    }
    return 1;
}

static int rin_catalog_read_file(const char* path, RinCatalog** output) {
    RinCatalog* catalog;
    char* bytes = NULL;
    size_t capacity = 0u;
    size_t length = 0u;
    int fd;
    int saved_errno = 0;
    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return 0;
    for (;;) {
        ssize_t amount;
        if (length == capacity) {
            size_t next_capacity;
            char* grown;
            if (capacity == RIN_CAT_MAX_FILE_BYTES) {
                char probe;
                do {
                    amount = read(fd, &probe, 1u);
                } while (amount < 0 && errno == EINTR);
                if (amount < 0) goto fail;
                if (amount > 0) {
                    errno = EFBIG;
                    goto fail;
                }
                break;
            }
            next_capacity = capacity ? capacity * 2u : 4096u;
            if (next_capacity > RIN_CAT_MAX_FILE_BYTES)
                next_capacity = RIN_CAT_MAX_FILE_BYTES;
            grown = (char*)realloc(bytes, next_capacity + 1u);
            if (!grown) {
                errno = ENOMEM;
                goto fail;
            }
            bytes = grown;
            capacity = next_capacity;
        }
        do {
            amount = read(fd, bytes + length, capacity - length);
        } while (amount < 0 && errno == EINTR);
        if (amount < 0) goto fail;
        if (amount == 0) break;
        length += (size_t)amount;
    }
    (void)close(fd);
    fd = -1;
    if (!bytes) {
        bytes = (char*)malloc(1u);
        if (!bytes) {
            errno = ENOMEM;
            return 0;
        }
    }
    bytes[length] = '\0';
    catalog = (RinCatalog*)calloc(1u, sizeof(*catalog));
    if (!catalog) {
        free(bytes);
        errno = ENOMEM;
        return 0;
    }
    catalog->bytes = bytes;
    errno = 0;
    if (!rin_catalog_parse(catalog, length)) {
        saved_errno = errno;
        if (saved_errno == 0) saved_errno = EINVAL;
        rin_catalog_destroy(catalog);
        errno = saved_errno;
        return 0;
    }
    *output = catalog;
    return 1;

fail:
    saved_errno = errno;
    if (fd >= 0) (void)close(fd);
    free(bytes);
    errno = saved_errno;
    return 0;
}

static size_t rin_catalog_name_length(const char* text, size_t maximum) {
    size_t length = 0u;
    if (!text) return 0u;
    while (length < maximum && text[length] != '\0') ++length;
    return length;
}

static int rin_catalog_environment_copy(const char* name, char* output,
                                        size_t capacity) {
    const int saved_errno = errno;
    char* owned;
    size_t length;
    if (!output || capacity == 0u) {
        errno = EINVAL;
        return -1;
    }
#if defined(RIN_FREESTANDING) && defined(RIN_USERSPACE)
    owned = rin_getenv_copy(name);
    if (!owned) {
        if (errno == ENOENT) {
            errno = saved_errno;
            return 0;
        }
        return -1;
    }
#else
    {
        const char* borrowed = getenv(name);
        if (!borrowed) return 0;
        length = rin_catalog_name_length(borrowed, capacity);
        if (length == capacity) {
            errno = ENAMETOOLONG;
            return -1;
        }
        owned = (char*)malloc(length + 1u);
        if (!owned) {
            errno = ENOMEM;
            return -1;
        }
        memcpy(owned, borrowed, length + 1u);
    }
#endif
    length = rin_catalog_name_length(owned, capacity);
    if (length == capacity) {
        free(owned);
        errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(output, owned, length + 1u);
    free(owned);
    errno = saved_errno;
    return 1;
}

static int rin_catalog_append(char* output, size_t* length,
                              const char* text, size_t count) {
    if (count > RIN_CAT_PATH_BYTES - 1u - *length) return 0;
    if (count != 0u) memcpy(output + *length, text, count);
    *length += count;
    output[*length] = '\0';
    return 1;
}

static void rin_catalog_locale_parts(const char* locale, const char** language,
                                    size_t* language_length,
                                    const char** territory,
                                    size_t* territory_length,
                                    const char** codeset,
                                    size_t* codeset_length) {
    const char* end = locale + rin_cat_strlen(locale);
    const char* modifier = locale;
    const char* underscore = NULL;
    const char* dot = NULL;
    const char* cursor;
    for (cursor = locale; cursor < end; ++cursor) {
        if (*cursor == '@') { modifier = cursor; break; }
        if (*cursor == '_' && !underscore) underscore = cursor;
        if (*cursor == '.' && !dot) dot = cursor;
    }
    *language = locale;
    *language_length = (size_t)((underscore ? underscore :
                                  (dot ? dot : modifier)) - locale);
    *territory = "";
    *territory_length = 0u;
    if (underscore) {
        const char* territory_end = dot && dot > underscore ? dot : modifier;
        *territory = underscore + 1;
        *territory_length = (size_t)(territory_end - *territory);
    }
    *codeset = "";
    *codeset_length = 0u;
    if (dot && dot < modifier) {
        *codeset = dot + 1;
        *codeset_length = (size_t)(modifier - *codeset);
    }
}

static int rin_catalog_expand_path(const char* pattern, size_t pattern_length,
                                   const char* name, const char* locale,
                                   char* output) {
    const char* language;
    const char* territory;
    const char* codeset;
    size_t language_length;
    size_t territory_length;
    size_t codeset_length;
    size_t length = 0u;
    size_t index;
    rin_catalog_locale_parts(locale, &language, &language_length, &territory,
                             &territory_length, &codeset, &codeset_length);
    output[0] = '\0';
    for (index = 0u; index < pattern_length; ++index) {
        const char* replacement = NULL;
        size_t replacement_length = 0u;
        if (pattern[index] == '%' && index + 1u < pattern_length) {
            switch (pattern[++index]) {
                case '%': replacement = "%"; replacement_length = 1u; break;
                case 'N': replacement = name; replacement_length = rin_cat_strlen(name); break;
                case 'L': replacement = locale; replacement_length = rin_cat_strlen(locale); break;
                case 'l': replacement = language; replacement_length = language_length; break;
                case 't': replacement = territory; replacement_length = territory_length; break;
                case 'c': replacement = codeset; replacement_length = codeset_length; break;
                default: return 0;
            }
        } else {
            replacement = pattern + index;
            replacement_length = 1u;
        }
        if (!rin_catalog_append(output, &length, replacement,
                                replacement_length))
            return 0;
    }
    return length != 0u;
}

static int rin_catalog_try_path(const char* path, RinCatalog** catalog,
                                int* error_out) {
    if (rin_catalog_read_file(path, catalog)) return 1;
    if (errno == EINVAL || errno == ENOMEM || errno == EFBIG)
        *error_out = errno;
    else if (*error_out == 0 && errno != ENOENT)
        *error_out = errno;
    return 0;
}

static int rin_catalog_try_template(const char* pattern, size_t length,
                                    const char* name, const char* locale,
                                    RinCatalog** catalog, int* error_out) {
    char path[RIN_CAT_PATH_BYTES];
    if (!rin_catalog_expand_path(pattern, length, name, locale, path)) {
        *error_out = ENAMETOOLONG;
        return 0;
    }
    return rin_catalog_try_path(path, catalog, error_out);
}

static int rin_catalog_search_nlspath(const char* nlspath, const char* name,
                                      const char* locale, RinCatalog** catalog,
                                      int* error_out) {
    const char* cursor = nlspath;
    while (1) {
        const char* end = cursor;
        size_t length;
        while (*end != '\0' && *end != ':') ++end;
        length = (size_t)(end - cursor);
        if (length == 0u) {
            if (rin_catalog_try_template("%N", 2u, name, locale,
                                         catalog, error_out))
                return 1;
        } else if (rin_catalog_try_template(cursor, length, name, locale,
                                            catalog, error_out)) {
            return 1;
        }
        if (*end == '\0') break;
        cursor = end + 1;
    }
    return 0;
}

static uintptr_t rin_catalog_make_handle(size_t slot, uintptr_t generation) {
    return (generation << RIN_CAT_SLOT_BITS) | (uintptr_t)(slot + 1u);
}

nl_catd catopen(const char* name, int oflag) {
    const char* locale;
    const char* nlspath;
    char locale_storage[RIN_CAT_PATH_BYTES];
    char nlspath_storage[RIN_CAT_PATH_BYTES];
    size_t name_length;
    int last_error = 0;
    RinCatalog* catalog = NULL;
    size_t slot;
    if (!name || (name_length = rin_catalog_name_length(name,
            RIN_CAT_PATH_BYTES)) == 0u) {
        errno = ENOENT;
        return (nl_catd)-1;
    }
    if (name_length >= RIN_CAT_PATH_BYTES ||
        (oflag != 0 && oflag != NL_CAT_LOCALE)) {
        errno = oflag != 0 && oflag != NL_CAT_LOCALE ? EINVAL : ENAMETOOLONG;
        return (nl_catd)-1;
    }
    if (oflag == NL_CAT_LOCALE) {
        locale_t current = uselocale(NULL);
        locale = current ? rin_locale_l_name(current, LC_MESSAGES) : NULL;
        if (!locale) {
            errno = EINVAL;
            return (nl_catd)-1;
        }
    } else {
        int environment_result = rin_catalog_environment_copy(
            "LANG", locale_storage, sizeof(locale_storage));
        if (environment_result < 0) return (nl_catd)-1;
        locale = environment_result > 0 && locale_storage[0] != '\0'
                     ? locale_storage
                     : "C";
    }
    if (strchr(name, '/') != NULL) {
        if (!rin_catalog_try_path(name, &catalog, &last_error)) {
            errno = last_error ? last_error : ENOENT;
            return (nl_catd)-1;
        }
    } else {
        int environment_result = rin_catalog_environment_copy(
            "NLSPATH", nlspath_storage, sizeof(nlspath_storage));
        if (environment_result < 0) return (nl_catd)-1;
        nlspath = environment_result > 0 ? nlspath_storage : NULL;
        if (nlspath && nlspath[0] != '\0') {
            size_t nlspath_length = rin_catalog_name_length(
                nlspath, RIN_CAT_PATH_BYTES);
            if (nlspath_length == RIN_CAT_PATH_BYTES) {
                errno = ENAMETOOLONG;
                return (nl_catd)-1;
            }
            if (!rin_catalog_search_nlspath(nlspath, name, locale, &catalog,
                                            &last_error)) {
                static const char fallback[] =
                    "/usr/share/locale/%L/LC_MESSAGES/%N";
                (void)rin_catalog_try_template(
                    fallback, sizeof(fallback) - 1u, name, locale,
                    &catalog, &last_error);
            }
        } else {
            static const char fallback[] =
                "/usr/share/locale/%L/LC_MESSAGES/%N";
            (void)rin_catalog_try_template(
                fallback, sizeof(fallback) - 1u, name, locale,
                &catalog, &last_error);
        }
        if (!catalog) {
            errno = last_error ? last_error : ENOENT;
            return (nl_catd)-1;
        }
    }

    rin_catalog_lock();
    for (slot = 0u; slot < RIN_CAT_SLOT_COUNT; ++slot) {
        RinCatalogSlot* entry = &g_catalog_slots[slot];
        if (entry->state != 0u) continue;
        if (entry->generation == 0u) entry->generation = 1u;
        entry->catalog = catalog;
        entry->state = 1u;
        {
            uintptr_t handle = rin_catalog_make_handle(slot,
                                                        entry->generation);
            rin_catalog_unlock();
            return (nl_catd)handle;
        }
    }
    rin_catalog_unlock();
    rin_catalog_destroy(catalog);
    errno = EMFILE;
    return (nl_catd)-1;
}

static RinCatalogSlot* rin_catalog_resolve_locked(nl_catd descriptor) {
    uintptr_t handle = (uintptr_t)descriptor;
    uintptr_t encoded_slot = handle & RIN_CAT_SLOT_MASK;
    uintptr_t generation = handle >> RIN_CAT_SLOT_BITS;
    RinCatalogSlot* slot;
    if (descriptor == (nl_catd)-1 || encoded_slot == 0u ||
        encoded_slot > RIN_CAT_SLOT_COUNT || generation == 0u)
        return NULL;
    slot = &g_catalog_slots[encoded_slot - 1u];
    if (slot->state != 1u || slot->generation != generation ||
        !slot->catalog)
        return NULL;
    return slot;
}

char* catgets(nl_catd descriptor, int set_id, int message_id,
              const char* default_string) {
    RinCatalogSlot* slot;
    RinCatalogMessage search;
    RinCatalogMessage* found;
    char* result = (char*)default_string;
    if (set_id <= 0 || message_id <= 0) return result;
    search.set_id = (uint32_t)set_id;
    search.message_id = (uint32_t)message_id;
    search.text = NULL;
    rin_catalog_lock();
    slot = rin_catalog_resolve_locked(descriptor);
    if (slot) {
        if (slot->catalog->message_count != 0u) {
            found = (RinCatalogMessage*)bsearch(
                &search, slot->catalog->messages,
                slot->catalog->message_count,
                sizeof(*slot->catalog->messages),
                rin_catalog_message_compare);
        } else {
            found = NULL;
        }
        if (found) result = found->text;
    }
    rin_catalog_unlock();
    return result;
}

int catclose(nl_catd descriptor) {
    RinCatalogSlot* slot;
    RinCatalog* catalog;
    rin_catalog_lock();
    slot = rin_catalog_resolve_locked(descriptor);
    if (!slot) {
        rin_catalog_unlock();
        errno = EBADF;
        return -1;
    }
    catalog = slot->catalog;
    slot->catalog = NULL;
    if (slot->generation >= RIN_CAT_MAX_GENERATION) {
        slot->state = 2u;
    } else {
        ++slot->generation;
        slot->state = 0u;
    }
    rin_catalog_unlock();
    rin_catalog_destroy(catalog);
    return 0;
}
