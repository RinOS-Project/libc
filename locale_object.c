/* SPDX-License-Identifier: MIT */
#include "locale.h"
#include "stdlib.h"

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

int rin_locale_ctype_test(locale_t locale, int character,
                          uint32_t operation) {
    if (locale == LC_GLOBAL_LOCALE)
        return locale_global_test(character, operation);
    if (!locale_object_valid(locale)) return 0;
    return locale_ascii_test(character, operation);
}

int rin_locale_current_ctype_test(int character, uint32_t operation) {
    return rin_locale_ctype_test(locale_thread_current, character, operation);
}

int rin_locale_ctype_map(locale_t locale, int character,
                         uint32_t operation) {
    if (locale == LC_GLOBAL_LOCALE) {
        if (operation == RIN_LOCALE_CTYPE_TOLOWER) return rin_tolower(character);
        if (operation == RIN_LOCALE_CTYPE_TOUPPER) return rin_toupper(character);
        return character;
    }
    if (!locale_object_valid(locale)) return character;
    if (operation == RIN_LOCALE_CTYPE_TOLOWER &&
        character >= 'A' && character <= 'Z')
        return character + ('a' - 'A');
    if (operation == RIN_LOCALE_CTYPE_TOUPPER &&
        character >= 'a' && character <= 'z')
        return character - ('a' - 'A');
    return character;
}

int rin_locale_current_ctype_map(int character, uint32_t operation) {
    return rin_locale_ctype_map(locale_thread_current, character, operation);
}
