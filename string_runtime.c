/* RinOS libc string runtime state shared by all translation units. */

#define RIN_STRING_RUNTIME_IMPLEMENTATION 1
#include "string.h"
#include "stdlib.h"
#include "locale.h"
#include "../libunicode/rin_unicode.h"

char* strdup(const char* string) {
    size_t length = 0u;
    char* copy;
    if (!string) return NULL;
    while (string[length] != 0) {
        if (length == SIZE_MAX - 1u) { errno = ENOMEM; return NULL; }
        ++length;
    }
    copy = (char*)malloc(length + 1u);
    if (!copy) { errno = ENOMEM; return NULL; }
    for (size_t index = 0u; index <= length; ++index)
        copy[index] = string[index];
    return copy;
}

char* strndup(const char* string, size_t size) {
    size_t length = 0u;
    char* copy;
    if (!string) return NULL;
    while (length < size && string[length] != 0) {
        if (length == SIZE_MAX - 1u) { errno = ENOMEM; return NULL; }
        ++length;
    }
    copy = (char*)malloc(length + 1u);
    if (!copy) { errno = ENOMEM; return NULL; }
    for (size_t index = 0u; index < length; ++index)
        copy[index] = string[index];
    copy[length] = 0;
    return copy;
}

static _Thread_local char* g_rin_strtok_last;

char* strtok(char* input, const char* delimiters) {
    char* cursor = input ? input : g_rin_strtok_last;
    char* token;

    if (!cursor) return NULL;
    while (*cursor) {
        const char* delimiter = delimiters;
        int matched = 0;
        while (*delimiter) {
            if (*cursor == *delimiter++) {
                matched = 1;
                break;
            }
        }
        if (!matched) break;
        ++cursor;
    }
    if (!*cursor) {
        g_rin_strtok_last = NULL;
        return NULL;
    }

    token = cursor;
    while (*cursor) {
        const char* delimiter = delimiters;
        while (*delimiter) {
            if (*cursor == *delimiter++) {
                *cursor = '\0';
                g_rin_strtok_last = cursor + 1;
                return token;
            }
        }
        ++cursor;
    }
    g_rin_strtok_last = NULL;
    return token;
}

int strcoll(const char* left, const char* right) {
    return rin_locale_current_strcoll(left, right);
}

size_t strxfrm(char* destination, const char* source, size_t capacity) {
    return rin_locale_current_strxfrm(destination, source, capacity);
}

int strcoll_l(const char* left, const char* right, locale_t locale) {
    return rin_locale_strcoll(locale, left, right);
}

size_t strxfrm_l(char* destination, const char* source, size_t capacity,
                 locale_t locale) {
    return rin_locale_strxfrm(locale, destination, source, capacity);
}
