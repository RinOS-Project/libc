/*
 * RinOS libc - sys/select.h
 * select関数 (poll.hから再エクスポート)
 */

#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H

#include "types.h"
#include "../time.h"
#include "../poll.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * fd_set と FD_* マクロは sys/types.h で定義済み
 * select 関数は poll.h で定義済み
 * ═══════════════════════════════════════════════════════════════*/

/* 追加の定数 */
#ifndef FD_SETSIZE
#define FD_SETSIZE 1024
#endif

#ifdef __cplusplus
}
#endif

#endif /* _SYS_SELECT_H */
