/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - sys/auxv.h
 * POSIX-style auxiliary vector access
 *
 * This header provides the getauxval() function to query the ELF
 * auxiliary vector passed by the kernel to the process at startup.
 */

#ifndef _SYS_AUXV_H
#define _SYS_AUXV_H

#include "../linux/auxvec.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * getauxval - retrieve a value from the auxiliary vector
 *
 * @type: The auxiliary vector entry type (AT_* constant)
 * @return: The value associated with the type, or 0 if not found
 *
 * The auxiliary vector is a mechanism for the kernel to pass
 * information to user space during program startup. Common uses:
 *   - AT_PAGESZ: Get system page size
 *   - AT_HWCAP/AT_HWCAP2: Query CPU capabilities
 *   - AT_CLKTCK: Get clock ticks per second
 *   - AT_RANDOM: Get address of 16 random bytes
 *
 * RinOS reports architecture-wide constants and obtains UID/GID values from
 * the versioned process credential snapshot. Loader-specific entries still
 * require a verified native auxiliary vector; unknown types fail with ENOENT.
 */

/* getauxval is already defined inline in linux/auxvec.h */

/*
 * For compatibility with glibc, also provide:
 */

/* errno disambiguates valid zero values (for example AT_HWCAP2) from errors. */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_AUXV_H */
