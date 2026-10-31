/* SPDX-License-Identifier: MIT */
#ifndef RIN_LIBC_DIRENT_GNU_RUNTIME_H
#define RIN_LIBC_DIRENT_GNU_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

/* GNU directory scan and version-sort functions are implemented in the
 * libc runtime archive so split-header consumers do not depend on inlining. */
int scandirat(int dirfd, const char* directory,
              struct dirent*** namelist,
              int (*selector)(const struct dirent*),
              int (*compar)(const struct dirent**,
                            const struct dirent**));
int scandirat64(int dirfd, const char* directory,
                struct dirent64*** namelist,
                int (*selector)(const struct dirent64*),
                int (*compar)(const struct dirent64**,
                              const struct dirent64**));
int versionsort(const struct dirent** left,
                const struct dirent** right);
int versionsort64(const struct dirent64** left,
                  const struct dirent64** right);
int strverscmp(const char* left, const char* right);

#ifdef __cplusplus
}
#endif

#endif /* RIN_LIBC_DIRENT_GNU_RUNTIME_H */
