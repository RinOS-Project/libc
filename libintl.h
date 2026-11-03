/* SPDX-License-Identifier: MIT */
/* GNU gettext-compatible message translation API. */

#ifndef _LIBINTL_H
#define _LIBINTL_H

#include "features.h"

#if defined(_RIN_USE_GNU_GETTEXT) || defined(_RIN_USE_BSD_GETTEXT) || \
    defined(__USE_GNU) || defined(__USE_MISC) || \
    (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 202405L) || \
    (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 800) || \
    defined(__USE_XOPEN2K24) || defined(__USE_XOPEN2K24XSI) || \
    (!defined(__STRICT_ANSI__) && !defined(_POSIX_C_SOURCE) && \
     !defined(_XOPEN_SOURCE)) || \
    defined(RIN_GETTEXT_RUNTIME_IMPLEMENTATION)
#define _RIN_LIBINTL_API_VISIBLE 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_RIN_LIBINTL_API_VISIBLE)
char* gettext(const char* msgid);
char* dgettext(const char* domainname, const char* msgid);
char* dcgettext(const char* domainname, const char* msgid, int category);

char* ngettext(const char* msgid, const char* msgid_plural,
               unsigned long int n);
char* dngettext(const char* domainname, const char* msgid,
                const char* msgid_plural, unsigned long int n);
char* dcngettext(const char* domainname, const char* msgid,
                 const char* msgid_plural, unsigned long int n,
                 int category);

char* textdomain(const char* domainname);
char* bindtextdomain(const char* domainname, const char* dirname);
char* bind_textdomain_codeset(const char* domainname, const char* codeset);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _LIBINTL_H */
