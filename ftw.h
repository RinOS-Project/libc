/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - ftw.h
 * ファイル木走査
 */

#ifndef _FTW_H
#define _FTW_H

#include "sys/stat.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FTW_F   0
#define FTW_D   1
#define FTW_DNR 2
#define FTW_NS  3
#define FTW_SL  4
#define FTW_DP  5
#define FTW_SLN 6

#define FTW_PHYS  0x01
#define FTW_MOUNT 0x02
#define FTW_CHDIR 0x04
#define FTW_DEPTH 0x08

struct FTW {
    int base;
    int level;
};

int ftw(const char* path,
        int (*fn)(const char*, const struct stat*, int), int fd_limit);
int nftw(const char* path,
         int (*fn)(const char*, const struct stat*, int, struct FTW*),
         int fd_limit, int flags);

#ifdef __cplusplus
}
#endif

#endif /* _FTW_H */
