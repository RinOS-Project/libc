/* SPDX-License-Identifier: MIT */
/* POSIX message catalog interface. */

#ifndef _RIN_NL_TYPES_H
#define _RIN_NL_TYPES_H

#include "features.h"
#include <stddef.h>
#include <stdint.h>

#if defined(__USE_XOPEN_EXTENDED) || \
    defined(__USE_GNU) || defined(__DARWIN_C_SOURCE) || \
    (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200809L) || \
    (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 500) || \
    (defined(__XSI_VISIBLE) && __XSI_VISIBLE)

#ifdef __cplusplus
extern "C" {
#endif

/* 世代番号を含む不透明handle。(nl_catd)-1はcatopenの失敗を表す。 */
typedef uintptr_t nl_catd;
typedef int nl_item;

#define NL_CAT_LOCALE 1
#define NL_SETD 1

/* RinOS catalog file format v1はUTF-8互換のbyte列を保持するテキスト形式:
 *   RINCAT1\n
 *   <set-id> <message-id>\t<escaped bytes>\n
 * escapeは\\、\n、\r、\t、\xHHを使う。行頭が#のrecordはcomment。 */
nl_catd catopen(const char* name, int oflag);
char* catgets(nl_catd catalog, int set_id, int message_id,
              const char* default_string);
int catclose(nl_catd catalog);

#ifdef __cplusplus
}
#endif

#endif /* POSIX message catalog interfaces */

#endif
