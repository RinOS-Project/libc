/* RinOS libc string runtime state shared by all translation units. */

#include "string.h"
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
