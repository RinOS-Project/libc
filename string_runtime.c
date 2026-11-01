/* RinOS libc string runtime state shared by all translation units. */

#define RIN_STRING_IMPLEMENTATION 1
#define RIN_STRING_RUNTIME_IMPLEMENTATION 1
#define RIN_WCHAR_IMPLEMENTATION 1
#include "string.h"
#include "stdlib.h"
#include "locale.h"
#include "wchar.h"
#include "stdlib.h"
#include "../libunicode/rin_unicode.h"

#define RIN_STRERROR_UNKNOWN_CAPACITY \
    (sizeof("不明なエラー ") + 1u + sizeof(unsigned int) * CHAR_BIT)

static _Thread_local char rin_strerror_unknown_buffer[
    RIN_STRERROR_UNKNOWN_CAPACITY];

static char* rin_strerror_format_unknown(int errnum, const char* prefix) {
    char reverse_digits[sizeof(unsigned int) * CHAR_BIT];
    unsigned int magnitude;
    size_t output = 0u;
    size_t digits = 0u;
    size_t index;
    while (prefix[output] != '\0') ++output;
    for (index = 0u; index < output; ++index)
        rin_strerror_unknown_buffer[index] = prefix[index];
    if (errnum < 0) {
        rin_strerror_unknown_buffer[output++] = '-';
        magnitude = 0u - (unsigned int)errnum;
    } else {
        magnitude = (unsigned int)errnum;
    }
    do {
        reverse_digits[digits++] = (char)('0' + magnitude % 10u);
        magnitude /= 10u;
    } while (magnitude != 0u);
    while (digits != 0u)
        rin_strerror_unknown_buffer[output++] = reverse_digits[--digits];
    rin_strerror_unknown_buffer[output] = '\0';
    return rin_strerror_unknown_buffer;
}

#if defined(RIN_USERSPACE) && !defined(_MSVCRT_COMPAT)
char* strerror_l(int errnum, locale_t locale) {
    int saved_errno = errno;
    const char* message = rin_locale_l_error_message(locale, errnum);
    if (!message) message = _rin_strerror_known_message(errnum);
    if (message) {
        errno = saved_errno;
        return (char*)message;
    }
    message = rin_locale_l_error_unknown_prefix(locale);
    if (!message) message = "Error ";
    {
        char* result = rin_strerror_format_unknown(errnum, message);
        errno = saved_errno;
        return result;
    }
}
#endif

char* strerror(int errnum) {
    int saved_errno = errno;
#if defined(RIN_USERSPACE) && !defined(_MSVCRT_COMPAT)
    char* result = strerror_l(errnum, uselocale(NULL));
#else
    const char* message = _rin_strerror_known_message(errnum);
    char* result = message ? (char*)message
                           : rin_strerror_format_unknown(errnum, "Error ");
#endif
    errno = saved_errno;
    return result;
}

int strerror_r(int errnum, char* buffer, size_t buffer_length) {
    int saved_errno = errno;
    char* message;
    size_t length = 0u;
    size_t copied;
    int result;
    if (!buffer) {
        errno = saved_errno;
        return EINVAL;
    }
    message = strerror(errnum);
    while (message[length] != '\0') ++length;
    if (buffer_length == 0u) {
        errno = saved_errno;
        return ERANGE;
    }
    copied = length < buffer_length - 1u ? length : buffer_length - 1u;
    for (size_t index = 0u; index < copied; ++index)
        buffer[index] = message[index];
    buffer[copied] = '\0';
    result = copied == length ? 0 : ERANGE;
    errno = saved_errno;
    return result;
}

void* memccpy(void* destination, const void* source, int stop_byte,
              size_t size) {
    unsigned char* output = (unsigned char*)destination;
    const unsigned char* input = (const unsigned char*)source;
    const unsigned char stop = (unsigned char)stop_byte;
    while (size != 0u) {
        unsigned char value = *input++;
        *output++ = value;
        --size;
        if (value == stop) return output;
    }
    return NULL;
}

void* memset_explicit(void* memory, int value, size_t size) {
    volatile unsigned char* output = (volatile unsigned char*)memory;
    const unsigned char byte = (unsigned char)value;
    void* result = memory;
    while (size != 0u) {
        *output++ = byte;
        --size;
    }
    return result;
}

char* strdup(const char* string) {
    size_t length = 0u;
    char* copy;
    if (!string) return NULL;
    while (string[length] != '\0') {
        if (length == SIZE_MAX - 1u) {
            errno = ENOMEM;
            return NULL;
        }
        ++length;
    }
    copy = (char*)malloc(length + 1u);
    if (!copy) {
        errno = ENOMEM;
        return NULL;
    }
    for (size_t index = 0u; index <= length; ++index)
        copy[index] = string[index];
    return copy;
}

char* strndup(const char* string, size_t size) {
    size_t length = 0u;
    char* copy;
    if (!string) return NULL;
    while (length < size && string[length] != '\0') {
        if (length == SIZE_MAX - 1u) {
            errno = ENOMEM;
            return NULL;
        }
        ++length;
    }
    copy = (char*)malloc(length + 1u);
    if (!copy) {
        errno = ENOMEM;
        return NULL;
    }
    for (size_t index = 0u; index < length; ++index)
        copy[index] = string[index];
    copy[length] = '\0';
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
