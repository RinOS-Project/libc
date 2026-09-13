#ifndef _INTTYPES_H
#define _INTTYPES_H

#include "stdint.h"
#include "stdlib.h"  /* For strtoll, strtoull */
#include "wchar.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * Integer division type and functions
 * ═══════════════════════════════════════════════════════════════*/

typedef struct {
    intmax_t quot;  /* Quotient */
    intmax_t rem;   /* Remainder */
} imaxdiv_t;

static inline intmax_t imaxabs(intmax_t j) {
    return j < 0 ? -j : j;
}

static inline imaxdiv_t imaxdiv(intmax_t numer, intmax_t denom) {
    imaxdiv_t result;
    result.quot = numer / denom;
    result.rem = numer % denom;
    return result;
}

/* ═══════════════════════════════════════════════════════════════
 * Format macros for printf/scanf
 * ═══════════════════════════════════════════════════════════════*/

// Format macros for printf/scanf
#define PRId8   "d"
#define PRId16  "d"
#define PRId32  "d"
#define PRId64  "lld"

#define PRIi8   "i"
#define PRIi16  "i"
#define PRIi32  "i"
#define PRIi64  "lli"

#define PRIu8   "u"
#define PRIu16  "u"
#define PRIu32  "u"
#define PRIu64  "llu"

#define PRIx8   "x"
#define PRIx16  "x"
#define PRIx32  "x"
#define PRIx64  "llx"

#define PRIX8   "X"
#define PRIX16  "X"
#define PRIX32  "X"
#define PRIX64  "llX"

#define PRIo8   "o"
#define PRIo16  "o"
#define PRIo32  "o"
#define PRIo64  "llo"

// Pointer formats
#define PRIdPTR "ld"
#define PRIiPTR "li"
#define PRIuPTR "lu"
#define PRIxPTR "lx"
#define PRIXPTR "lX"

// Scanf formats
#define SCNd8   "hhd"
#define SCNd16  "hd"
#define SCNd32  "d"
#define SCNd64  "lld"

#define SCNi8   "hhi"
#define SCNi16  "hi"
#define SCNi32  "i"
#define SCNi64  "lli"

#define SCNu8   "hhu"
#define SCNu16  "hu"
#define SCNu32  "u"
#define SCNu64  "llu"

#define SCNx8   "hhx"
#define SCNx16  "hx"
#define SCNx32  "x"
#define SCNx64  "llx"

// Integer conversion functions
static inline intmax_t strtoimax(const char* nptr, char** endptr, int base) {
#if defined(__cplusplus) && defined(RINCXX_CSTDLIB_H)
    return (intmax_t)std::strtoll(nptr, endptr, base);
#else
    return (intmax_t)strtoll(nptr, endptr, base);
#endif
}

static inline uintmax_t strtoumax(const char* nptr, char** endptr, int base) {
#if defined(__cplusplus) && defined(RINCXX_CSTDLIB_H)
    return (uintmax_t)std::strtoull(nptr, endptr, base);
#else
    return (uintmax_t)strtoull(nptr, endptr, base);
#endif
}

/* ═══════════════════════════════════════════════════════════════
 * Wide character conversion (C99)
 * ═══════════════════════════════════════════════════════════════*/

static inline intmax_t wcstoimax(const wchar_t* nptr, wchar_t** endptr, int base) {
    return (intmax_t)wcstoll(nptr, endptr, base);
}

static inline uintmax_t wcstoumax(const wchar_t* nptr, wchar_t** endptr, int base) {
    return (uintmax_t)wcstoull(nptr, endptr, base);
}

#ifdef __cplusplus
}
#endif

#endif // _INTTYPES_H
