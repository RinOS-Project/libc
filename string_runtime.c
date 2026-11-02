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

typedef struct RinStrerrorNpEntry {
    int value;
    const char* name;
    const char* description;
} RinStrerrorNpEntry;

#define RIN_STRERROR_NP_ENTRY(error, text) { error, #error, text }
static const RinStrerrorNpEntry g_rin_strerror_np_entries[] = {
    { 0, "0", "Success" },
    RIN_STRERROR_NP_ENTRY(EPERM, "Operation not permitted"),
    RIN_STRERROR_NP_ENTRY(ENOENT, "No such file or directory"),
    RIN_STRERROR_NP_ENTRY(ESRCH, "No such process"),
    RIN_STRERROR_NP_ENTRY(EINTR, "Interrupted system call"),
    RIN_STRERROR_NP_ENTRY(EIO, "I/O error"),
    RIN_STRERROR_NP_ENTRY(ENXIO, "No such device or address"),
    RIN_STRERROR_NP_ENTRY(E2BIG, "Argument list too long"),
    RIN_STRERROR_NP_ENTRY(ENOEXEC, "Exec format error"),
    RIN_STRERROR_NP_ENTRY(EBADF, "Bad file descriptor"),
    RIN_STRERROR_NP_ENTRY(ECHILD, "No child processes"),
    RIN_STRERROR_NP_ENTRY(EAGAIN, "Resource temporarily unavailable"),
    RIN_STRERROR_NP_ENTRY(ENOMEM, "Out of memory"),
    RIN_STRERROR_NP_ENTRY(EACCES, "Permission denied"),
    RIN_STRERROR_NP_ENTRY(EFAULT, "Bad address"),
    RIN_STRERROR_NP_ENTRY(ENOTBLK, "Block device required"),
    RIN_STRERROR_NP_ENTRY(EBUSY, "Device or resource busy"),
    RIN_STRERROR_NP_ENTRY(EEXIST, "File exists"),
    RIN_STRERROR_NP_ENTRY(EXDEV, "Cross-device link"),
    RIN_STRERROR_NP_ENTRY(ENODEV, "No such device"),
    RIN_STRERROR_NP_ENTRY(ENOTDIR, "Not a directory"),
    RIN_STRERROR_NP_ENTRY(EISDIR, "Is a directory"),
    RIN_STRERROR_NP_ENTRY(EINVAL, "Invalid argument"),
    RIN_STRERROR_NP_ENTRY(ENFILE, "File table overflow"),
    RIN_STRERROR_NP_ENTRY(EMFILE, "Too many open files"),
    RIN_STRERROR_NP_ENTRY(ENOTTY, "Inappropriate ioctl for device"),
    RIN_STRERROR_NP_ENTRY(ETXTBSY, "Text file busy"),
    RIN_STRERROR_NP_ENTRY(EFBIG, "File too large"),
    RIN_STRERROR_NP_ENTRY(ENOSPC, "No space left on device"),
    RIN_STRERROR_NP_ENTRY(ESPIPE, "Illegal seek"),
    RIN_STRERROR_NP_ENTRY(EROFS, "Read-only file system"),
    RIN_STRERROR_NP_ENTRY(EMLINK, "Too many links"),
    RIN_STRERROR_NP_ENTRY(EPIPE, "Broken pipe"),
    RIN_STRERROR_NP_ENTRY(EDOM, "Mathematics argument out of domain of function"),
    RIN_STRERROR_NP_ENTRY(ERANGE, "Numerical result out of range"),
    RIN_STRERROR_NP_ENTRY(EDEADLK, "Resource deadlock would occur"),
    RIN_STRERROR_NP_ENTRY(ENAMETOOLONG, "File name too long"),
    RIN_STRERROR_NP_ENTRY(ENOLCK, "No locks available"),
    RIN_STRERROR_NP_ENTRY(ENOSYS, "Function not implemented"),
    RIN_STRERROR_NP_ENTRY(ENOTEMPTY, "Directory not empty"),
    RIN_STRERROR_NP_ENTRY(ELOOP, "Too many levels of symbolic links"),
    RIN_STRERROR_NP_ENTRY(ENOMSG, "No message of desired type"),
    RIN_STRERROR_NP_ENTRY(EIDRM, "Identifier removed"),
    RIN_STRERROR_NP_ENTRY(ECHRNG, "Channel number out of range"),
    RIN_STRERROR_NP_ENTRY(EL2NSYNC, "Level 2 not synchronized"),
    RIN_STRERROR_NP_ENTRY(EL3HLT, "Level 3 halted"),
    RIN_STRERROR_NP_ENTRY(EL3RST, "Level 3 reset"),
    RIN_STRERROR_NP_ENTRY(ELNRNG, "Link number out of range"),
    RIN_STRERROR_NP_ENTRY(EUNATCH, "Protocol driver not attached"),
    RIN_STRERROR_NP_ENTRY(ENOCSI, "No CSI structure available"),
    RIN_STRERROR_NP_ENTRY(EL2HLT, "Level 2 halted"),
    RIN_STRERROR_NP_ENTRY(EBADE, "Invalid exchange"),
    RIN_STRERROR_NP_ENTRY(EBADR, "Invalid request descriptor"),
    RIN_STRERROR_NP_ENTRY(EXFULL, "Exchange full"),
    RIN_STRERROR_NP_ENTRY(ENOANO, "No anode"),
    RIN_STRERROR_NP_ENTRY(EBADRQC, "Invalid request code"),
    RIN_STRERROR_NP_ENTRY(EBADSLT, "Invalid slot"),
    RIN_STRERROR_NP_ENTRY(EBFONT, "Bad font file format"),
    RIN_STRERROR_NP_ENTRY(ENOSTR, "Device not a stream"),
    RIN_STRERROR_NP_ENTRY(ENODATA, "No data available"),
    RIN_STRERROR_NP_ENTRY(ETIME, "Timer expired"),
    RIN_STRERROR_NP_ENTRY(ENOSR, "Out of streams resources"),
    RIN_STRERROR_NP_ENTRY(ENONET, "Machine is not on the network"),
    RIN_STRERROR_NP_ENTRY(ENOPKG, "Package not installed"),
    RIN_STRERROR_NP_ENTRY(EREMOTE, "Object is remote"),
    RIN_STRERROR_NP_ENTRY(ENOLINK, "Link has been severed"),
    RIN_STRERROR_NP_ENTRY(EADV, "Advertise error"),
    RIN_STRERROR_NP_ENTRY(ESRMNT, "Srmount error"),
    RIN_STRERROR_NP_ENTRY(ECOMM, "Communication error on send"),
    RIN_STRERROR_NP_ENTRY(EPROTO, "Protocol error"),
    RIN_STRERROR_NP_ENTRY(EMULTIHOP, "Multihop attempted"),
    RIN_STRERROR_NP_ENTRY(EDOTDOT, "RFS specific error"),
    RIN_STRERROR_NP_ENTRY(EBADMSG, "Bad message"),
    RIN_STRERROR_NP_ENTRY(EOVERFLOW, "Value too large for defined data type"),
    RIN_STRERROR_NP_ENTRY(ENOTUNIQ, "Name not unique on network"),
    RIN_STRERROR_NP_ENTRY(EBADFD, "File descriptor in bad state"),
    RIN_STRERROR_NP_ENTRY(EREMCHG, "Remote address changed"),
    RIN_STRERROR_NP_ENTRY(ELIBACC, "Can not access a needed shared library"),
    RIN_STRERROR_NP_ENTRY(ELIBBAD, "Accessing a corrupted shared library"),
    RIN_STRERROR_NP_ENTRY(ELIBSCN, ".lib section in a.out corrupted"),
    RIN_STRERROR_NP_ENTRY(ELIBMAX, "Attempting to link in too many shared libraries"),
    RIN_STRERROR_NP_ENTRY(ELIBEXEC, "Cannot exec a shared library directly"),
    RIN_STRERROR_NP_ENTRY(EILSEQ, "Illegal byte sequence"),
#ifdef ERESTART
    RIN_STRERROR_NP_ENTRY(ERESTART, "Interrupted system call should be restarted"),
#endif
    RIN_STRERROR_NP_ENTRY(ESTRPIPE, "Streams pipe error"),
    RIN_STRERROR_NP_ENTRY(EUSERS, "Too many users"),
    RIN_STRERROR_NP_ENTRY(ESTALE, "Stale file handle"),
    RIN_STRERROR_NP_ENTRY(EUCLEAN, "Structure needs cleaning"),
    RIN_STRERROR_NP_ENTRY(ENOTNAM, "Not a XENIX named type file"),
    RIN_STRERROR_NP_ENTRY(ENAVAIL, "No XENIX semaphores available"),
    RIN_STRERROR_NP_ENTRY(EISNAM, "Is a named type file"),
    RIN_STRERROR_NP_ENTRY(EREMOTEIO, "Remote I/O error"),
    RIN_STRERROR_NP_ENTRY(EDQUOT, "Quota exceeded"),
    RIN_STRERROR_NP_ENTRY(ENOMEDIUM, "No medium found"),
    RIN_STRERROR_NP_ENTRY(EMEDIUMTYPE, "Wrong medium type"),
    RIN_STRERROR_NP_ENTRY(ECANCELED, "Operation canceled"),
    RIN_STRERROR_NP_ENTRY(ENOKEY, "Required key not available"),
    RIN_STRERROR_NP_ENTRY(EKEYEXPIRED, "Key has expired"),
    RIN_STRERROR_NP_ENTRY(EKEYREVOKED, "Key has been revoked"),
    RIN_STRERROR_NP_ENTRY(EKEYREJECTED, "Key was rejected by service"),
    RIN_STRERROR_NP_ENTRY(EOWNERDEAD, "Owner died"),
    RIN_STRERROR_NP_ENTRY(ENOTRECOVERABLE, "State not recoverable"),
    RIN_STRERROR_NP_ENTRY(ENOTSOCK, "Socket operation on non-socket"),
    RIN_STRERROR_NP_ENTRY(EDESTADDRREQ, "Destination address required"),
    RIN_STRERROR_NP_ENTRY(EMSGSIZE, "Message too long"),
    RIN_STRERROR_NP_ENTRY(EPROTOTYPE, "Protocol wrong type for socket"),
    RIN_STRERROR_NP_ENTRY(ENOPROTOOPT, "Protocol not available"),
    RIN_STRERROR_NP_ENTRY(EPROTONOSUPPORT, "Protocol not supported"),
    RIN_STRERROR_NP_ENTRY(ESOCKTNOSUPPORT, "Socket type not supported"),
    RIN_STRERROR_NP_ENTRY(EOPNOTSUPP, "Operation not supported"),
    RIN_STRERROR_NP_ENTRY(EPFNOSUPPORT, "Protocol family not supported"),
    RIN_STRERROR_NP_ENTRY(EAFNOSUPPORT, "Address family not supported by protocol"),
    RIN_STRERROR_NP_ENTRY(EADDRINUSE, "Address already in use"),
    RIN_STRERROR_NP_ENTRY(EADDRNOTAVAIL, "Cannot assign requested address"),
    RIN_STRERROR_NP_ENTRY(ENETDOWN, "Network is down"),
    RIN_STRERROR_NP_ENTRY(ENETUNREACH, "Network is unreachable"),
    RIN_STRERROR_NP_ENTRY(ENETRESET, "Network dropped connection on reset"),
    RIN_STRERROR_NP_ENTRY(ECONNABORTED, "Software caused connection abort"),
    RIN_STRERROR_NP_ENTRY(ECONNRESET, "Connection reset by peer"),
    RIN_STRERROR_NP_ENTRY(ENOBUFS, "No buffer space available"),
    RIN_STRERROR_NP_ENTRY(EISCONN, "Transport endpoint is already connected"),
    RIN_STRERROR_NP_ENTRY(ENOTCONN, "Transport endpoint is not connected"),
    RIN_STRERROR_NP_ENTRY(ESHUTDOWN, "Cannot send after transport endpoint shutdown"),
    RIN_STRERROR_NP_ENTRY(ETOOMANYREFS, "Too many references"),
    RIN_STRERROR_NP_ENTRY(ETIMEDOUT, "Connection timed out"),
    RIN_STRERROR_NP_ENTRY(ECONNREFUSED, "Connection refused"),
    RIN_STRERROR_NP_ENTRY(EHOSTDOWN, "Host is down"),
    RIN_STRERROR_NP_ENTRY(EHOSTUNREACH, "No route to host"),
    RIN_STRERROR_NP_ENTRY(EALREADY, "Operation already in progress"),
    RIN_STRERROR_NP_ENTRY(EINPROGRESS, "Operation now in progress")
};
#undef RIN_STRERROR_NP_ENTRY

static const RinStrerrorNpEntry* rin_strerror_np_find(int errnum)
{
    for (size_t index = 0u;
         index < sizeof(g_rin_strerror_np_entries) /
                     sizeof(g_rin_strerror_np_entries[0]);
         ++index) {
        if (g_rin_strerror_np_entries[index].value == errnum)
            return &g_rin_strerror_np_entries[index];
    }
    return NULL;
}

const char* strerrorname_np(int errnum)
{
    const RinStrerrorNpEntry* entry = rin_strerror_np_find(errnum);
    return entry != NULL ? entry->name : NULL;
}

const char* strerrordesc_np(int errnum)
{
    const RinStrerrorNpEntry* entry = rin_strerror_np_find(errnum);
    return entry != NULL ? entry->description : NULL;
}
