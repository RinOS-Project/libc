/* SPDX-License-Identifier: MIT */
#include "locale.h"
#include "stdlib.h"
#include "../libunicode/rin_unicode.h"

#ifndef RIN_LOCALE_OBJECT_ALLOC
#define RIN_LOCALE_OBJECT_ALLOC(size) malloc(size)
#endif

#ifndef RIN_LOCALE_OBJECT_FREE
#define RIN_LOCALE_OBJECT_FREE(pointer) free(pointer)
#endif

#define RIN_LOCALE_OBJECT_MAGIC UINT32_C(0x434f4c52)
#define RIN_LOCALE_OBJECT_VERSION UINT32_C(0x00010000)

#define RIN_LOCALE_KIND_C 1u
#define RIN_LOCALE_KIND_EN_US_UTF8 2u
#define RIN_LOCALE_KIND_EN_US 3u
#define RIN_LOCALE_KIND_JA_JP_UTF8 4u
#define RIN_LOCALE_KIND_JA_JP 5u
#define RIN_LOCALE_KIND_DE_DE_UTF8 6u
#define RIN_LOCALE_KIND_DE_DE 7u
#define RIN_LOCALE_KIND_FR_FR_UTF8 8u
#define RIN_LOCALE_KIND_FR_FR 9u
#define RIN_LOCALE_KIND_ES_ES_UTF8 10u
#define RIN_LOCALE_KIND_ES_ES 11u
#define RIN_LOCALE_KIND_IT_IT_UTF8 12u
#define RIN_LOCALE_KIND_IT_IT 13u
#define RIN_LOCALE_KIND_PT_BR_UTF8 14u
#define RIN_LOCALE_KIND_PT_BR 15u
#define RIN_LOCALE_KIND_ES_MX_UTF8 16u
#define RIN_LOCALE_KIND_ES_MX 17u
#define RIN_LOCALE_KIND_PT_PT_UTF8 18u
#define RIN_LOCALE_KIND_PT_PT 19u

struct RinLocaleObject {
    uint32_t magic;
    uint32_t version;
    uint8_t categories[6];
    uint8_t reserved[10];
};

_Static_assert(sizeof(struct RinLocaleObject) == 24u,
               "locale object layout drift");

static __thread locale_t locale_thread_current = LC_GLOBAL_LOCALE;

static void locale_zero(void* pointer, size_t size) {
    volatile uint8_t* output = (volatile uint8_t*)pointer;
    while (size-- != 0u) *output++ = 0u;
}

static void locale_copy(void* destination, const void* source, size_t size) {
    uint8_t* output = (uint8_t*)destination;
    const uint8_t* input = (const uint8_t*)source;
    while (size-- != 0u) *output++ = *input++;
}

static int locale_equal(const char* left, const char* right) {
    if (!left || !right) return 0;
    while (*left != '\0' && *right != '\0') {
        if (*left++ != *right++) return 0;
    }
    return *left == *right;
}

static int locale_kind_from_name(const char* name, uint8_t* kind) {
    if (!name || !kind) return 0;
    if (locale_equal(name, "C") || locale_equal(name, "POSIX"))
        *kind = RIN_LOCALE_KIND_C;
    else if (locale_equal(name, "en_US.UTF-8"))
        *kind = RIN_LOCALE_KIND_EN_US_UTF8;
    else if (locale_equal(name, "en_US"))
        *kind = RIN_LOCALE_KIND_EN_US;
    else if (locale_equal(name, "ja_JP.UTF-8"))
        *kind = RIN_LOCALE_KIND_JA_JP_UTF8;
    else if (locale_equal(name, "ja_JP"))
        *kind = RIN_LOCALE_KIND_JA_JP;
    else if (locale_equal(name, "de_DE.UTF-8"))
        *kind = RIN_LOCALE_KIND_DE_DE_UTF8;
    else if (locale_equal(name, "de_DE"))
        *kind = RIN_LOCALE_KIND_DE_DE;
    else if (locale_equal(name, "fr_FR.UTF-8"))
        *kind = RIN_LOCALE_KIND_FR_FR_UTF8;
    else if (locale_equal(name, "fr_FR"))
        *kind = RIN_LOCALE_KIND_FR_FR;
    else if (locale_equal(name, "es_ES.UTF-8"))
        *kind = RIN_LOCALE_KIND_ES_ES_UTF8;
    else if (locale_equal(name, "es_ES"))
        *kind = RIN_LOCALE_KIND_ES_ES;
    else if (locale_equal(name, "it_IT.UTF-8"))
        *kind = RIN_LOCALE_KIND_IT_IT_UTF8;
    else if (locale_equal(name, "it_IT"))
        *kind = RIN_LOCALE_KIND_IT_IT;
    else if (locale_equal(name, "pt_BR.UTF-8"))
        *kind = RIN_LOCALE_KIND_PT_BR_UTF8;
    else if (locale_equal(name, "pt_BR"))
        *kind = RIN_LOCALE_KIND_PT_BR;
    else if (locale_equal(name, "es_MX.UTF-8"))
        *kind = RIN_LOCALE_KIND_ES_MX_UTF8;
    else if (locale_equal(name, "es_MX"))
        *kind = RIN_LOCALE_KIND_ES_MX;
    else if (locale_equal(name, "pt_PT.UTF-8"))
        *kind = RIN_LOCALE_KIND_PT_PT_UTF8;
    else if (locale_equal(name, "pt_PT"))
        *kind = RIN_LOCALE_KIND_PT_PT;
    else
        return 0;
    return 1;
}

static int locale_object_valid(locale_t locale) {
    uint32_t index;
    uint8_t combined = 0u;
    if (!locale || locale == LC_GLOBAL_LOCALE ||
        locale->magic != RIN_LOCALE_OBJECT_MAGIC ||
        locale->version != RIN_LOCALE_OBJECT_VERSION) {
        return 0;
    }
    for (index = 0u; index < 6u; ++index) {
        if (locale->categories[index] < RIN_LOCALE_KIND_C ||
            locale->categories[index] > RIN_LOCALE_KIND_PT_PT) {
            return 0;
        }
    }
    for (index = 0u; index < sizeof(locale->reserved); ++index)
        combined |= locale->reserved[index];
    return combined == 0u;
}

static void locale_object_c(struct RinLocaleObject* locale) {
    uint32_t index;
    locale_zero(locale, sizeof(*locale));
    locale->magic = RIN_LOCALE_OBJECT_MAGIC;
    locale->version = RIN_LOCALE_OBJECT_VERSION;
    for (index = 0u; index < 6u; ++index)
        locale->categories[index] = RIN_LOCALE_KIND_C;
}

static int locale_apply_name(struct RinLocaleObject* locale,
                             int category_mask,
                             const char* name) {
    uint32_t index;
    uint8_t kind = 0u;
    if (name[0] != '\0' && !locale_kind_from_name(name, &kind)) return 0;
    for (index = 0u; index < 6u; ++index) {
        if ((category_mask & (1 << index)) == 0) continue;
        if (name[0] == '\0') {
            const char* environment_name =
                rin_locale_environment_name((int)index);
            if (!locale_kind_from_name(environment_name, &kind)) return 0;
        }
        locale->categories[index] = kind;
    }
    return 1;
}

locale_t newlocale(int category_mask, const char* name, locale_t base) {
    struct RinLocaleObject result;
    struct RinLocaleObject* allocated;
    if ((category_mask & ~LC_ALL_MASK) != 0 || !name ||
        base == LC_GLOBAL_LOCALE) {
        errno = EINVAL;
        return NULL;
    }
    if (base) {
        if (!locale_object_valid(base)) {
            errno = EINVAL;
            return NULL;
        }
        locale_copy(&result, base, sizeof(result));
    } else {
        locale_object_c(&result);
    }
    if (!locale_apply_name(&result, category_mask, name)) {
        errno = ENOENT;
        return NULL;
    }
    /* POSIX newlocale() is copy-on-write: even with a live base object,
     * publish a distinct locale object and leave the caller's base untouched.
     * The candidate has already resolved every selected category, so an
     * allocation failure cannot expose a partial category update. */
    allocated = (struct RinLocaleObject*)RIN_LOCALE_OBJECT_ALLOC(
        sizeof(*allocated));
    if (!allocated) {
        errno = ENOMEM;
        return NULL;
    }
    locale_copy(allocated, &result, sizeof(result));
    return allocated;
}

locale_t duplocale(locale_t locale) {
    struct RinLocaleObject result;
    struct RinLocaleObject* duplicate;
    uint32_t index;
    if (!locale) {
        errno = EINVAL;
        return NULL;
    }
    if (locale == LC_GLOBAL_LOCALE) {
        locale_object_c(&result);
        for (index = 0u; index < 6u; ++index) {
            if (!locale_kind_from_name(rin_locale_name((int)index),
                                       &result.categories[index])) {
                errno = ENOENT;
                return NULL;
            }
        }
    } else {
        if (!locale_object_valid(locale)) {
            errno = EINVAL;
            return NULL;
        }
        locale_copy(&result, locale, sizeof(result));
    }
    duplicate = (struct RinLocaleObject*)RIN_LOCALE_OBJECT_ALLOC(
        sizeof(*duplicate));
    if (!duplicate) {
        errno = ENOMEM;
        return NULL;
    }
    locale_copy(duplicate, &result, sizeof(result));
    return duplicate;
}

void freelocale(locale_t locale) {
    if (!locale_object_valid(locale)) return;
    locale_zero(locale, sizeof(*locale));
    RIN_LOCALE_OBJECT_FREE(locale);
}

locale_t uselocale(locale_t locale) {
    locale_t previous = locale_thread_current;
    if (!locale) return previous;
    if (locale != LC_GLOBAL_LOCALE && !locale_object_valid(locale)) {
        errno = EINVAL;
        return NULL;
    }
    locale_thread_current = locale;
    return previous;
}

static int locale_ascii_test(int character, uint32_t operation) {
    unsigned int value;
    int alpha;
    int digit;
    if (character < 0 || character > 255) return 0;
    value = (unsigned int)character;
    alpha = (value >= (unsigned int)'A' && value <= (unsigned int)'Z') ||
            (value >= (unsigned int)'a' && value <= (unsigned int)'z');
    digit = value >= (unsigned int)'0' && value <= (unsigned int)'9';
    switch (operation) {
        case RIN_LOCALE_CTYPE_ALNUM: return alpha || digit;
        case RIN_LOCALE_CTYPE_ALPHA: return alpha;
        case RIN_LOCALE_CTYPE_DIGIT: return digit;
        case RIN_LOCALE_CTYPE_XDIGIT:
            return digit ||
                   (value >= (unsigned int)'A' && value <= (unsigned int)'F') ||
                   (value >= (unsigned int)'a' && value <= (unsigned int)'f');
        case RIN_LOCALE_CTYPE_LOWER:
            return value >= (unsigned int)'a' && value <= (unsigned int)'z';
        case RIN_LOCALE_CTYPE_UPPER:
            return value >= (unsigned int)'A' && value <= (unsigned int)'Z';
        case RIN_LOCALE_CTYPE_SPACE:
            return value == (unsigned int)' ' || value == (unsigned int)'\t' ||
                   value == (unsigned int)'\n' || value == (unsigned int)'\v' ||
                   value == (unsigned int)'\f' || value == (unsigned int)'\r';
        case RIN_LOCALE_CTYPE_BLANK:
            return value == (unsigned int)' ' || value == (unsigned int)'\t';
        case RIN_LOCALE_CTYPE_CNTRL: return value < 32u || value == 127u;
        case RIN_LOCALE_CTYPE_PRINT: return value >= 32u && value <= 126u;
        case RIN_LOCALE_CTYPE_GRAPH: return value >= 33u && value <= 126u;
        case RIN_LOCALE_CTYPE_PUNCT:
            return value >= 33u && value <= 126u && !alpha && !digit;
        default: return 0;
    }
}

static int locale_global_test(int character, uint32_t operation) {
    switch (operation) {
        case RIN_LOCALE_CTYPE_ALNUM: return rin_isalnum(character);
        case RIN_LOCALE_CTYPE_ALPHA: return rin_isalpha(character);
        case RIN_LOCALE_CTYPE_DIGIT: return rin_isdigit(character);
        case RIN_LOCALE_CTYPE_XDIGIT: return rin_isxdigit(character);
        case RIN_LOCALE_CTYPE_LOWER: return rin_islower(character);
        case RIN_LOCALE_CTYPE_UPPER: return rin_isupper(character);
        case RIN_LOCALE_CTYPE_SPACE: return rin_isspace(character);
        case RIN_LOCALE_CTYPE_BLANK: return rin_isblank(character);
        case RIN_LOCALE_CTYPE_CNTRL: return rin_iscntrl(character);
        case RIN_LOCALE_CTYPE_PRINT: return rin_isprint(character);
        case RIN_LOCALE_CTYPE_GRAPH: return rin_isgraph(character);
        case RIN_LOCALE_CTYPE_PUNCT: return rin_ispunct(character);
        default: return 0;
    }
}

static int locale_kind_is_utf8(uint8_t kind) {
    return kind == RIN_LOCALE_KIND_EN_US_UTF8 ||
           kind == RIN_LOCALE_KIND_JA_JP_UTF8 ||
           kind == RIN_LOCALE_KIND_DE_DE_UTF8 ||
           kind == RIN_LOCALE_KIND_FR_FR_UTF8 ||
           kind == RIN_LOCALE_KIND_ES_ES_UTF8 ||
           kind == RIN_LOCALE_KIND_IT_IT_UTF8 ||
           kind == RIN_LOCALE_KIND_PT_BR_UTF8 ||
           kind == RIN_LOCALE_KIND_ES_MX_UTF8 ||
           kind == RIN_LOCALE_KIND_PT_PT_UTF8;
}

static int locale_global_narrow_ascii_only(void) {
    const char* codeset = rin_locale_codeset();
    return !codeset || locale_equal(codeset, "ASCII") ||
           locale_equal(codeset, "US-ASCII") ||
           locale_equal(codeset, "UTF-8") || locale_equal(codeset, "UTF8");
}

static int locale_narrow_ascii_only(locale_t locale) {
    if (locale == LC_GLOBAL_LOCALE) return locale_global_narrow_ascii_only();
    if (!locale_object_valid(locale)) return 1;
    return locale->categories[LC_CTYPE] == RIN_LOCALE_KIND_C ||
           locale_kind_is_utf8(locale->categories[LC_CTYPE]);
}

static int locale_ctype_is_c(locale_t locale) {
    const char* name;
    if (locale == LC_GLOBAL_LOCALE) {
        name = rin_locale_name(LC_CTYPE);
        return !name || locale_equal(name, "C") || locale_equal(name, "POSIX");
    }
    return !locale_object_valid(locale) ||
           locale->categories[LC_CTYPE] == RIN_LOCALE_KIND_C;
}

static int locale_unicode_wide_test(uint32_t character,
                                    uint32_t operation) {
    switch (operation) {
        case RIN_LOCALE_CTYPE_ALNUM: return rin_unicode_isalnum(character);
        case RIN_LOCALE_CTYPE_ALPHA: return rin_unicode_isalpha(character);
        case RIN_LOCALE_CTYPE_DIGIT: return rin_unicode_isdigit(character);
        case RIN_LOCALE_CTYPE_XDIGIT: return rin_unicode_isxdigit(character);
        case RIN_LOCALE_CTYPE_LOWER: return rin_unicode_islower(character);
        case RIN_LOCALE_CTYPE_UPPER: return rin_unicode_isupper(character);
        case RIN_LOCALE_CTYPE_SPACE: return rin_unicode_isspace(character);
        case RIN_LOCALE_CTYPE_BLANK: return rin_unicode_isblank(character);
        case RIN_LOCALE_CTYPE_CNTRL: return rin_unicode_iscntrl(character);
        case RIN_LOCALE_CTYPE_PRINT: return rin_unicode_isprint(character);
        case RIN_LOCALE_CTYPE_GRAPH: return rin_unicode_isgraph(character);
        case RIN_LOCALE_CTYPE_PUNCT: return rin_unicode_ispunct(character);
        default: return 0;
    }
}

static uint32_t locale_unicode_wide_map(uint32_t character,
                                       uint32_t operation) {
    if (operation == RIN_LOCALE_CTYPE_TOLOWER)
        return rin_unicode_tolower(character);
    if (operation == RIN_LOCALE_CTYPE_TOUPPER)
        return rin_unicode_toupper(character);
    return character;
}

static int locale_ascii_map(int character, uint32_t operation) {
    if (operation == RIN_LOCALE_CTYPE_TOLOWER &&
        character >= 'A' && character <= 'Z')
        return character + ('a' - 'A');
    if (operation == RIN_LOCALE_CTYPE_TOUPPER &&
        character >= 'a' && character <= 'z')
        return character - ('a' - 'A');
    return character;
}

int rin_locale_ctype_test(locale_t locale, int character,
                          uint32_t operation) {
    if (character < 0 || character > 255) return 0;
    if (locale == LC_GLOBAL_LOCALE) {
        if (locale_global_narrow_ascii_only())
            return locale_ascii_test(character, operation);
        return locale_global_test(character, operation);
    }
    if (!locale_object_valid(locale)) return 0;
    if (locale_narrow_ascii_only(locale))
        return locale_ascii_test(character, operation);
    return locale_global_test(character, operation);
}

int rin_locale_current_ctype_test(int character, uint32_t operation) {
    return rin_locale_ctype_test(locale_thread_current, character, operation);
}

int rin_locale_wide_ctype_test(locale_t locale, uint32_t character,
                               uint32_t operation) {
    if (locale != LC_GLOBAL_LOCALE && !locale_object_valid(locale)) return 0;
    if (!rin_unicode_is_valid_scalar(character)) return 0;
    if (locale_ctype_is_c(locale))
        return character <= 0x7fu
            ? locale_ascii_test((int)character, operation) : 0;
    return locale_unicode_wide_test(character, operation);
}

int rin_locale_current_wide_ctype_test(uint32_t character,
                                       uint32_t operation) {
    return rin_locale_wide_ctype_test(locale_thread_current, character,
                                      operation);
}

int rin_locale_wide_ctype_iswctype(locale_t locale, uint32_t character,
                                   unsigned long descriptor) {
    if (locale != LC_GLOBAL_LOCALE && !locale_object_valid(locale)) return 0;
    if (!rin_unicode_is_valid_scalar(character)) return 0;
    if (locale_ctype_is_c(locale) && character > 0x7fu) return 0;
    return rin_unicode_iswctype(character, descriptor);
}

int rin_locale_current_wide_ctype_iswctype(uint32_t character,
                                           unsigned long descriptor) {
    return rin_locale_wide_ctype_iswctype(locale_thread_current, character,
                                          descriptor);
}

uint32_t rin_locale_wide_ctype_map(locale_t locale, uint32_t character,
                                   uint32_t operation) {
    if (locale != LC_GLOBAL_LOCALE && !locale_object_valid(locale))
        return character;
    if (!rin_unicode_is_valid_scalar(character)) return character;
    if (locale_ctype_is_c(locale)) {
        if (character > 0x7fu) return character;
        return (uint32_t)locale_ascii_map((int)character, operation);
    }
    return locale_unicode_wide_map(character, operation);
}

uint32_t rin_locale_current_wide_ctype_map(uint32_t character,
                                           uint32_t operation) {
    return rin_locale_wide_ctype_map(locale_thread_current, character,
                                     operation);
}

uint32_t rin_locale_wide_ctype_towctrans(locale_t locale, uint32_t character,
                                         unsigned long descriptor) {
    if (locale != LC_GLOBAL_LOCALE && !locale_object_valid(locale))
        return character;
    if (!rin_unicode_is_valid_scalar(character)) return character;
    if (locale_ctype_is_c(locale) && character > 0x7fu) return character;
    return rin_unicode_towctrans(character, descriptor);
}

uint32_t rin_locale_current_wide_ctype_towctrans(
    uint32_t character, unsigned long descriptor) {
    return rin_locale_wide_ctype_towctrans(locale_thread_current, character,
                                          descriptor);
}

int rin_locale_current_wide_print_test(uint32_t character) {
    return rin_locale_current_wide_ctype_test(
        character, RIN_LOCALE_CTYPE_PRINT);
}

int rin_locale_ctype_map(locale_t locale, int character,
                         uint32_t operation) {
    int mapped;
    if (character < 0 || character > 255) return character;
    if (locale != LC_GLOBAL_LOCALE && !locale_object_valid(locale))
        return character;
    if (locale_narrow_ascii_only(locale))
        return locale_ascii_map(character, operation);
    mapped = operation == RIN_LOCALE_CTYPE_TOLOWER
        ? rin_tolower(character)
        : operation == RIN_LOCALE_CTYPE_TOUPPER
            ? rin_toupper(character) : character;
    return mapped >= 0 && mapped <= 255 ? mapped : character;
}

int rin_locale_current_ctype_map(int character, uint32_t operation) {
    return rin_locale_ctype_map(locale_thread_current, character, operation);
}

static __thread rin_unicode_mbstate_t locale_mbrtowc_private_state = {
    0u, 0u
};
static __thread rin_unicode_mbstate_t locale_mbrlen_private_state = {
    0u, 0u
};
static __thread rin_unicode_mbstate_t locale_wcrtomb_private_state = {
    0u, 0u
};
static __thread rin_unicode_mbstate_t locale_mbsrtowcs_private_state = {
    0u, 0u
};
static __thread rin_unicode_mbstate_t locale_wcsrtombs_private_state = {
    0u, 0u
};
static __thread rin_unicode_mbstate_t locale_mblen_private_state = {
    0u, 0u
};
static __thread rin_unicode_mbstate_t locale_mbtowc_private_state = {
    0u, 0u
};
static __thread rin_unicode_mbstate_t locale_mbstowcs_private_state = {
    0u, 0u
};
static __thread rin_unicode_mbstate_t locale_wcstombs_private_state = {
    0u, 0u
};
static __thread rin_unicode_mbstate_t locale_wctomb_private_state = {
    0u, 0u
};

static void locale_mbstate_reset(rin_unicode_mbstate_t* state) {
    if (!state) return;
    state->state = 0u;
    state->codepoint = 0u;
}

static int locale_mbstate_valid(locale_t locale) {
    return locale == LC_GLOBAL_LOCALE || locale_object_valid(locale);
}

static int locale_codeset_is_utf8(locale_t locale) {
    const char* codeset;
    if (locale == LC_GLOBAL_LOCALE) {
        codeset = rin_locale_codeset();
        return codeset && (locale_equal(codeset, "UTF-8") ||
                           locale_equal(codeset, "UTF8"));
    }
    return locale_object_valid(locale) &&
           locale_kind_is_utf8(locale->categories[LC_CTYPE]);
}

static int locale_codeset_is_ascii(locale_t locale) {
    const char* codeset;
    if (locale == LC_GLOBAL_LOCALE) {
        codeset = rin_locale_codeset();
        return !codeset || locale_equal(codeset, "ASCII") ||
               locale_equal(codeset, "US-ASCII");
    }
    return !locale_object_valid(locale) ||
           locale->categories[LC_CTYPE] == RIN_LOCALE_KIND_C;
}

static int locale_conversion_state(locale_t locale,
                                   rin_unicode_mbstate_t** state,
                                   rin_unicode_mbstate_t* private_state) {
    if (!locale_mbstate_valid(locale) || !state) {
        errno = EINVAL;
        return 0;
    }
    if (!*state) *state = private_state;
    return 1;
}

int rin_locale_current_mb_cur_max(void) {
    return locale_codeset_is_utf8(locale_thread_current) ? 4 : 1;
}

static size_t locale_mbrtowc32(locale_t locale, uint32_t* character,
                               const char* source, size_t length,
                               rin_unicode_mbstate_t* state) {
    unsigned char byte;
    if (!locale_mbstate_valid(locale)) {
        errno = EINVAL;
        return (size_t)-1;
    }
    if (!state) {
        errno = EINVAL;
        return (size_t)-1;
    }
    if (!source) {
        locale_mbstate_reset(state);
        return 0u;
    }
    if (length == 0u) return (size_t)-2;
    if (locale_codeset_is_utf8(locale)) {
        size_t result = rin_unicode_mbrtowc32(character, source, length,
                                              state);
        if (result == (size_t)-1) errno = EILSEQ;
        return result;
    }
    if (!rin_unicode_mbsinit(state)) {
        locale_mbstate_reset(state);
        errno = EILSEQ;
        return (size_t)-1;
    }
    byte = (unsigned char)source[0];
    if (byte == 0u) {
        if (character) *character = 0u;
        return 0u;
    }
    if (locale_codeset_is_ascii(locale) && byte >= 0x80u) {
        errno = EILSEQ;
        return (size_t)-1;
    }
    if (character) *character = (uint32_t)byte;
    return 1u;
}

static size_t locale_wcrtomb32(locale_t locale, char* destination,
                               uint32_t character,
                               rin_unicode_mbstate_t* state) {
    char reset_buffer[4];
    if (!locale_mbstate_valid(locale)) {
        errno = EINVAL;
        return (size_t)-1;
    }
    if (!state) {
        errno = EINVAL;
        return (size_t)-1;
    }
    if (!destination) {
        locale_mbstate_reset(state);
        destination = reset_buffer;
        character = 0u;
    }
    if (locale_codeset_is_utf8(locale)) {
        size_t result = rin_unicode_wcrtomb32(destination, character, state);
        if (result == (size_t)-1) errno = EILSEQ;
        return result;
    }
    if (!rin_unicode_is_valid_scalar(character) || character > 0xffu ||
        (locale_codeset_is_ascii(locale) && character > 0x7fu)) {
        locale_mbstate_reset(state);
        errno = EILSEQ;
        return (size_t)-1;
    }
    destination[0] = (char)(unsigned char)character;
    locale_mbstate_reset(state);
    return 1u;
}

size_t rin_locale_current_mbrlen(const char* source, size_t length,
                                 struct rin_unicode_mbstate* state) {
    rin_unicode_mbstate_t* active = (rin_unicode_mbstate_t*)state;
    if (!locale_conversion_state(locale_thread_current, &active,
                                 &locale_mbrlen_private_state))
        return (size_t)-1;
    return locale_mbrtowc32(locale_thread_current, (uint32_t*)0, source,
                            length, active);
}

size_t rin_locale_current_mbrtowc32(
    uint32_t* character, const char* source, size_t length,
    struct rin_unicode_mbstate* state) {
    rin_unicode_mbstate_t* active = (rin_unicode_mbstate_t*)state;
    if (!locale_conversion_state(locale_thread_current, &active,
                                 &locale_mbrtowc_private_state))
        return (size_t)-1;
    return locale_mbrtowc32(locale_thread_current, character, source, length,
                            active);
}

size_t rin_locale_current_wcrtomb32(
    char* destination, uint32_t character,
    struct rin_unicode_mbstate* state) {
    rin_unicode_mbstate_t* active = (rin_unicode_mbstate_t*)state;
    if (!locale_conversion_state(locale_thread_current, &active,
                                 &locale_wcrtomb_private_state))
        return (size_t)-1;
    return locale_wcrtomb32(locale_thread_current, destination, character,
                            active);
}

static size_t locale_multibyte_available(locale_t locale,
                                         const char* source) {
    size_t maximum = locale_codeset_is_utf8(locale) ? 4u : 1u;
    size_t available = 1u;
    while (available < maximum && source[available] != '\0') ++available;
    return available;
}

size_t rin_locale_current_mbsrtowcs32(
    uint32_t* destination, const char** source, size_t length,
    struct rin_unicode_mbstate* state) {
    rin_unicode_mbstate_t* active = (rin_unicode_mbstate_t*)state;
    const char* cursor;
    size_t converted = 0u;
    if (!source) {
        errno = EINVAL;
        return (size_t)-1;
    }
    if (!locale_conversion_state(locale_thread_current, &active,
                                 &locale_mbsrtowcs_private_state))
        return (size_t)-1;
    cursor = *source;
    if (!cursor) return 0u;
    while (*cursor != '\0' && (!destination || converted < length)) {
        uint32_t character = 0u;
        size_t available = locale_multibyte_available(locale_thread_current,
                                                       cursor);
        size_t consumed = locale_mbrtowc32(locale_thread_current, &character,
                                           cursor, available, active);
        if (consumed == (size_t)-1 || consumed == (size_t)-2) {
            if (consumed == (size_t)-2) errno = EILSEQ;
            *source = cursor;
            return (size_t)-1;
        }
        if (consumed == 0u) break;
        if (converted == (size_t)-1) {
            errno = EOVERFLOW;
            return (size_t)-1;
        }
        if (destination) destination[converted] = character;
        ++converted;
        cursor += consumed;
    }
    if (*cursor == '\0') {
        if (!destination || converted < length) {
            if (destination) destination[converted] = 0u;
            *source = (const char*)0;
        } else {
            *source = cursor;
        }
    } else {
        *source = cursor;
    }
    return converted;
}

size_t rin_locale_current_wcsrtombs32(
    char* destination, const uint32_t** source, size_t length,
    struct rin_unicode_mbstate* state) {
    rin_unicode_mbstate_t* active = (rin_unicode_mbstate_t*)state;
    const uint32_t* cursor;
    size_t converted = 0u;
    if (!source) {
        errno = EINVAL;
        return (size_t)-1;
    }
    if (!locale_conversion_state(locale_thread_current, &active,
                                 &locale_wcsrtombs_private_state))
        return (size_t)-1;
    cursor = *source;
    if (!cursor) return 0u;
    while (*cursor != 0u) {
        char encoded[4];
        rin_unicode_mbstate_t candidate_state = *active;
        size_t written = locale_wcrtomb32(locale_thread_current, encoded,
                                          *cursor, &candidate_state);
        if (written == (size_t)-1) {
            *source = cursor;
            return (size_t)-1;
        }
        if (destination && (converted > length ||
                            written > length - converted))
            break;
        if (destination) {
            size_t index;
            for (index = 0u; index < written; ++index)
                destination[converted + index] = encoded[index];
        }
        if (converted > (size_t)-1 - written) {
            errno = EOVERFLOW;
            *source = cursor;
            return (size_t)-1;
        }
        converted += written;
        *active = candidate_state;
        ++cursor;
    }
    if (*cursor == 0u) {
        if (!destination || converted < length) {
            if (destination) destination[converted] = '\0';
            *source = (const uint32_t*)0;
        } else {
            *source = cursor;
        }
    } else {
        *source = cursor;
    }
    return converted;
}

int rin_locale_current_mblen(const char* source, size_t length) {
    size_t result;
    locale_mbstate_reset(&locale_mblen_private_state);
    if (!source) return 0;
    result = locale_mbrtowc32(locale_thread_current, (uint32_t*)0, source,
                              length, &locale_mblen_private_state);
    if (result == (size_t)-1 || result == (size_t)-2) {
        if (result == (size_t)-2) errno = EILSEQ;
        locale_mbstate_reset(&locale_mblen_private_state);
        return -1;
    }
    return (int)result;
}

int rin_locale_current_mbtowc32(uint32_t* character, const char* source,
                                size_t length) {
    size_t result;
    locale_mbstate_reset(&locale_mbtowc_private_state);
    if (!source) return 0;
    result = locale_mbrtowc32(locale_thread_current, character, source,
                              length, &locale_mbtowc_private_state);
    if (result == (size_t)-1 || result == (size_t)-2) {
        if (result == (size_t)-2) errno = EILSEQ;
        locale_mbstate_reset(&locale_mbtowc_private_state);
        return -1;
    }
    return (int)result;
}

int rin_locale_current_wctomb32(char* destination, uint32_t character) {
    size_t result;
    if (!destination) {
        locale_mbstate_reset(&locale_wctomb_private_state);
        return 0;
    }
    result = locale_wcrtomb32(locale_thread_current, destination, character,
                              &locale_wctomb_private_state);
    return result == (size_t)-1 ? -1 : (int)result;
}

size_t rin_locale_current_mbstowcs32(uint32_t* destination,
                                     const char* source, size_t length) {
    const char* cursor = source;
    size_t converted = 0u;
    if (!source) {
        errno = EINVAL;
        return (size_t)-1;
    }
    locale_mbstate_reset(&locale_mbstowcs_private_state);
    while (*cursor != '\0' && (!destination || converted < length)) {
        uint32_t character = 0u;
        size_t available = locale_multibyte_available(locale_thread_current,
                                                       cursor);
        size_t consumed = locale_mbrtowc32(
            locale_thread_current, &character, cursor, available,
            &locale_mbstowcs_private_state);
        if (consumed == (size_t)-1 || consumed == (size_t)-2) {
            if (consumed == (size_t)-2) errno = EILSEQ;
            return (size_t)-1;
        }
        if (converted == (size_t)-1) {
            errno = EOVERFLOW;
            return (size_t)-1;
        }
        if (destination) destination[converted] = character;
        ++converted;
        cursor += consumed;
    }
    if (*cursor == '\0' && destination && converted < length)
        destination[converted] = 0u;
    return converted;
}

size_t rin_locale_current_wcstombs32(char* destination,
                                     const uint32_t* source, size_t length) {
    const uint32_t* cursor = source;
    size_t converted = 0u;
    if (!source) {
        errno = EINVAL;
        return (size_t)-1;
    }
    locale_mbstate_reset(&locale_wcstombs_private_state);
    while (*cursor != 0u) {
        char encoded[4];
        rin_unicode_mbstate_t candidate_state = locale_wcstombs_private_state;
        size_t written = locale_wcrtomb32(locale_thread_current, encoded,
                                          *cursor, &candidate_state);
        if (written == (size_t)-1) return (size_t)-1;
        if (destination && (converted > length ||
                            written > length - converted))
            break;
        if (destination) {
            size_t index;
            for (index = 0u; index < written; ++index)
                destination[converted + index] = encoded[index];
        }
        if (converted > (size_t)-1 - written) {
            errno = EOVERFLOW;
            return (size_t)-1;
        }
        converted += written;
        locale_wcstombs_private_state = candidate_state;
        ++cursor;
    }
    if (*cursor == 0u && destination && converted < length)
        destination[converted] = '\0';
    return converted;
}

static int locale_collate_is_c(locale_t locale) {
    if (locale == LC_GLOBAL_LOCALE) {
        const char* name = rin_locale_name(LC_COLLATE);
        return !name || locale_equal(name, "C") || locale_equal(name, "POSIX");
    }
    if (!locale_object_valid(locale)) return 1;
    return locale->categories[LC_COLLATE] == RIN_LOCALE_KIND_C;
}

static int locale_compare_bytes(const char* left, const char* right) {
    const unsigned char* a = (const unsigned char*)(left ? left : "");
    const unsigned char* b = (const unsigned char*)(right ? right : "");
    while (*a != 0u && *a == *b) {
        ++a;
        ++b;
    }
    return *a < *b ? -1 : (*a > *b ? 1 : 0);
}

static size_t locale_transform_bytes(char* destination, const char* source,
                                     size_t capacity) {
    size_t length = 0u;
    size_t copied = 0u;
    const char* input = source ? source : "";
    while (input[length] != '\0') {
        if (length == SIZE_MAX) return (size_t)-1;
        ++length;
    }
    if (destination && capacity != 0u) {
        copied = length < capacity - 1u ? length : capacity - 1u;
        for (size_t index = 0u; index < copied; ++index)
            destination[index] = input[index];
        destination[copied] = '\0';
    }
    return length;
}

static int locale_compare_wide(const uint32_t* left, const uint32_t* right) {
    static const uint32_t empty[1] = {0u};
    const uint32_t* a = left ? left : empty;
    const uint32_t* b = right ? right : empty;
    while (*a != 0u && *a == *b) {
        ++a;
        ++b;
    }
    return *a < *b ? -1 : (*a > *b ? 1 : 0);
}

static size_t locale_transform_wide(uint32_t* destination,
                                    const uint32_t* source,
                                    size_t capacity) {
    static const uint32_t empty[1] = {0u};
    const uint32_t* input = source ? source : empty;
    size_t length = 0u;
    if (source) {
        while (input[length] != 0u) {
            if (length == SIZE_MAX) return (size_t)-1;
            ++length;
        }
    }
    if (destination && capacity != 0u) {
        size_t copied = length < capacity - 1u ? length : capacity - 1u;
        for (size_t index = 0u; index < copied; ++index)
            destination[index] = input[index];
        destination[copied] = 0u;
    }
    return length;
}

int rin_locale_strcoll(locale_t locale, const char* left, const char* right) {
    if (locale_collate_is_c(locale)) return locale_compare_bytes(left, right);
    return rin_unicode_compare_utf8(left ? left : "", right ? right : "");
}

size_t rin_locale_strxfrm(locale_t locale, char* destination,
                          const char* source, size_t capacity) {
    if (locale_collate_is_c(locale))
        return locale_transform_bytes(destination, source, capacity);
    return rin_unicode_transform_utf8(destination, capacity,
                                      source ? source : "");
}

int rin_locale_wcscoll32(locale_t locale, const uint32_t* left,
                         const uint32_t* right) {
    if (locale_collate_is_c(locale)) return locale_compare_wide(left, right);
    return rin_unicode_compare_utf32(left, right);
}

size_t rin_locale_wcsxfrm32(locale_t locale, uint32_t* destination,
                            const uint32_t* source, size_t capacity) {
    if (locale_collate_is_c(locale))
        return locale_transform_wide(destination, source, capacity);
    return rin_unicode_transform_utf32(destination, capacity, source);
}

int rin_locale_current_strcoll(const char* left, const char* right) {
    return rin_locale_strcoll(locale_thread_current, left, right);
}

size_t rin_locale_current_strxfrm(char* destination, const char* source,
                                  size_t capacity) {
    return rin_locale_strxfrm(locale_thread_current, destination, source,
                              capacity);
}

int rin_locale_current_wcscoll32(const uint32_t* left,
                                 const uint32_t* right) {
    return rin_locale_wcscoll32(locale_thread_current, left, right);
}

size_t rin_locale_current_wcsxfrm32(uint32_t* destination,
                                    const uint32_t* source,
                                    size_t capacity) {
    return rin_locale_wcsxfrm32(locale_thread_current, destination, source,
                                capacity);
}
