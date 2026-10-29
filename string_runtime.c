/* RinOS libc string runtime state shared by all translation units. */

#define RIN_STRING_IMPLEMENTATION 1
#define RIN_WCHAR_IMPLEMENTATION 1
#include "string.h"
#include "wchar.h"
#include "stdlib.h"
#include "../libunicode/rin_unicode.h"

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
    return rin_unicode_compare_utf8(left ? left : "", right ? right : "");
}

size_t strxfrm(char* destination, const char* source, size_t capacity) {
    return rin_unicode_transform_utf8(destination, capacity,
                                      source ? source : "");
}

size_t strlcpy(char* destination, const char* source, size_t size) {
    size_t source_length = 0u;
    size_t copied = 0u;
    while (source[source_length] != '\0') ++source_length;
    if (size != 0u) {
        while (copied < size - 1u && source[copied] != '\0') {
            destination[copied] = source[copied];
            ++copied;
        }
        destination[copied] = '\0';
    }
    return source_length;
}

size_t strlcat(char* destination, const char* source, size_t size) {
    size_t destination_length = 0u;
    size_t source_length = 0u;
    size_t copied = 0u;
    while (destination_length < size &&
           destination[destination_length] != '\0')
        ++destination_length;
    while (source[source_length] != '\0') ++source_length;
    if (destination_length == size) return size + source_length;
    while (copied < size - destination_length - 1u &&
           source[copied] != '\0') {
        destination[destination_length + copied] = source[copied];
        ++copied;
    }
    destination[destination_length + copied] = '\0';
    return destination_length + source_length;
}

size_t wcsnlen(const wchar_t* string, size_t maxlen) {
    size_t length = 0u;
    while (length < maxlen && string[length] != L'\0') ++length;
    return length;
}

wchar_t* wcsdup(const wchar_t* string) {
    const size_t max_elements = SIZE_MAX / sizeof(wchar_t);
    size_t length = 0u;
    wchar_t* copy;
    while (string[length] != L'\0') {
        if (length >= max_elements - 1u) {
            errno = ENOMEM;
            return NULL;
        }
        ++length;
    }
    copy = (wchar_t*)malloc((length + 1u) * sizeof(wchar_t));
    if (copy == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    for (size_t index = 0u; index <= length; ++index)
        copy[index] = string[index];
    return copy;
}

size_t wcslcpy(wchar_t* destination, const wchar_t* source, size_t size) {
    size_t source_length = 0u;
    size_t copied = 0u;
    while (source[source_length] != L'\0') ++source_length;
    if (size != 0u) {
        while (copied < size - 1u && source[copied] != L'\0') {
            destination[copied] = source[copied];
            ++copied;
        }
        destination[copied] = L'\0';
    }
    return source_length;
}

size_t wcslcat(wchar_t* destination, const wchar_t* source, size_t size) {
    size_t destination_length = 0u;
    size_t source_length = 0u;
    size_t copied = 0u;
    while (destination_length < size &&
           destination[destination_length] != L'\0')
        ++destination_length;
    while (source[source_length] != L'\0') ++source_length;
    if (destination_length == size) return size + source_length;
    while (copied < size - destination_length - 1u &&
           source[copied] != L'\0') {
        destination[destination_length + copied] = source[copied];
        ++copied;
    }
    destination[destination_length + copied] = L'\0';
    return destination_length + source_length;
}
