/*
 * RinOS libc - sys/param.h
 * System parameters and configuration
 */

#ifndef _SYS_PARAM_H
#define _SYS_PARAM_H

#include "types.h"
#include "../limits.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * System Version and Identity
 * ═══════════════════════════════════════════════════════════════*/

#define BSD         199506      /* BSD version */
#define BSD4_3      1
#define BSD4_4      1

/* RinOS version */
#define __RinOS__       1
#define __RinOS_major__ 1
#define __RinOS_minor__ 0

/* ═══════════════════════════════════════════════════════════════
 * Memory Page Configuration
 * ═══════════════════════════════════════════════════════════════*/

#ifndef PAGE_SIZE
#define PAGE_SIZE       4096        /* Default page size (4KB) */
#endif

#ifndef PAGESIZE
#define PAGESIZE        PAGE_SIZE
#endif

#ifndef PAGE_SHIFT
#define PAGE_SHIFT      12          /* log2(PAGE_SIZE) */
#endif

#ifndef PAGE_MASK
#define PAGE_MASK       (~(PAGE_SIZE - 1))
#endif

/* Large page support */
#define LARGE_PAGE_SIZE (4 * 1024 * 1024)   /* 4MB large page (PSE) */
#define HUGE_PAGE_SIZE  (2 * 1024 * 1024)   /* 2MB huge page */

/* ═══════════════════════════════════════════════════════════════
 * Path and Name Limits
 * ═══════════════════════════════════════════════════════════════*/

#ifndef MAXPATHLEN
#define MAXPATHLEN      4096        /* Maximum pathname length */
#endif

#ifndef PATH_MAX
#define PATH_MAX        MAXPATHLEN
#endif

#ifndef MAXNAMLEN
#define MAXNAMLEN       255         /* Maximum filename length */
#endif

#ifndef NAME_MAX
#define NAME_MAX        MAXNAMLEN
#endif

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN  256         /* Maximum hostname length */
#endif

#ifndef MAXSYMLINKS
#define MAXSYMLINKS     20          /* Maximum symbolic link chain */
#endif

#ifndef MAXLOGNAME
#define MAXLOGNAME      32          /* Maximum login name length */
#endif

#ifndef MAXCOMLEN
#define MAXCOMLEN       16          /* Maximum command name length */
#endif

#ifndef MAXINTERP
#define MAXINTERP       64          /* Maximum interpreter path */
#endif

/* ═══════════════════════════════════════════════════════════════
 * Process and Thread Limits
 * ═══════════════════════════════════════════════════════════════*/

#ifndef NGROUPS
#define NGROUPS         8           /* Kernel credential ABI supplementary-group capacity */
#endif

#ifndef NGROUPS_MAX
#define NGROUPS_MAX     NGROUPS
#endif

#ifndef MAXUPRC
#define MAXUPRC         256         /* Maximum processes per user */
#endif

#ifndef CHILD_MAX
#define CHILD_MAX       256         /* Maximum child processes */
#endif

#ifndef OPEN_MAX
#define OPEN_MAX        256         /* Maximum open files */
#endif

#ifndef ARG_MAX
#define ARG_MAX         (256 * 1024) /* Maximum argument bytes */
#endif

#ifndef NCARGS
#define NCARGS          ARG_MAX
#endif

/* ═══════════════════════════════════════════════════════════════
 * Disk and I/O Configuration
 * ═══════════════════════════════════════════════════════════════*/

#ifndef DEV_BSIZE
#define DEV_BSIZE       512         /* Physical disk block size */
#endif

#ifndef DEV_BSHIFT
#define DEV_BSHIFT      9           /* log2(DEV_BSIZE) */
#endif

#ifndef BLKDEV_IOSIZE
#define BLKDEV_IOSIZE   2048        /* Block device I/O size */
#endif

#ifndef MAXBSIZE
#define MAXBSIZE        65536       /* Maximum filesystem block size */
#endif

#ifndef MAXFRAG
#define MAXFRAG         8           /* Maximum fragments per block */
#endif

#ifndef MAXPHYS
#define MAXPHYS         (128 * 1024) /* Maximum physical I/O size */
#endif

/* ═══════════════════════════════════════════════════════════════
 * Buffer Cache Configuration
 * ═══════════════════════════════════════════════════════════════*/

#ifndef DFLTPHYS
#define DFLTPHYS        (64 * 1024) /* Default physical I/O size */
#endif

/* ═══════════════════════════════════════════════════════════════
 * Network Configuration
 * ═══════════════════════════════════════════════════════════════*/

#ifndef NMBCLUSTERS
#define NMBCLUSTERS     512         /* Network mbuf clusters */
#endif

#ifndef MSIZE
#define MSIZE           256         /* mbuf size */
#endif

#ifndef MCLBYTES
#define MCLBYTES        2048        /* mbuf cluster size */
#endif

/* ═══════════════════════════════════════════════════════════════
 * Bit Manipulation
 * ═══════════════════════════════════════════════════════════════*/

#ifndef NBBY
#define NBBY            8           /* Bits per byte */
#endif

/* Bit field operations */
#define setbit(a, i)    ((a)[(i) / NBBY] |= (1 << ((i) % NBBY)))
#define clrbit(a, i)    ((a)[(i) / NBBY] &= ~(1 << ((i) % NBBY)))
#define isset(a, i)     ((a)[(i) / NBBY] & (1 << ((i) % NBBY)))
#define isclr(a, i)     (((a)[(i) / NBBY] & (1 << ((i) % NBBY))) == 0)

/* ═══════════════════════════════════════════════════════════════
 * BSD-style MIN/MAX Macros
 * ═══════════════════════════════════════════════════════════════*/

#ifndef MIN
#define MIN(a, b)       ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b)       ((a) > (b) ? (a) : (b))
#endif

/* ═══════════════════════════════════════════════════════════════
 * Alignment and Rounding Macros
 * ═══════════════════════════════════════════════════════════════*/

/* Calculate how many y-sized units fit in x (round up) */
#define howmany(x, y)   (((x) + ((y) - 1)) / (y))

/* Round x up to the next multiple of y */
#define roundup(x, y)   ((((x) + ((y) - 1)) / (y)) * (y))

/* Round x up to the next power of 2 boundary y */
#define roundup2(x, y)  (((x) + ((y) - 1)) & ~((y) - 1))

/* Round x down to the previous multiple of y */
#define rounddown(x, y) (((x) / (y)) * (y))

/* Round x down to the previous power of 2 boundary y */
#define rounddown2(x, y) ((x) & ~((y) - 1))

/* Check if x is a power of 2 */
#define powerof2(x)     ((((x) - 1) & (x)) == 0)

/* Align pointer to specified boundary */
#define P2ALIGN(x, align)       ((x) & -(align))
#define P2PHASE(x, align)       ((x) & ((align) - 1))
#define P2NPHASE(x, align)      (-(x) & ((align) - 1))
#define P2ROUNDUP(x, align)     (-(-(x) & -(align)))
#define P2END(x, align)         (-(~(x) & -(align)))

/* ═══════════════════════════════════════════════════════════════
 * Byte Ordering Macros
 * ═══════════════════════════════════════════════════════════════*/

#ifndef BYTE_ORDER
#define LITTLE_ENDIAN   1234
#define BIG_ENDIAN      4321
#define PDP_ENDIAN      3412
#define BYTE_ORDER      LITTLE_ENDIAN   /* x86 is little endian */
#endif

/* ═══════════════════════════════════════════════════════════════
 * Miscellaneous Constants
 * ═══════════════════════════════════════════════════════════════*/

#ifndef NOFILE
#define NOFILE          256         /* Default max open files */
#endif

#ifndef CANBSIZ
#define CANBSIZ         256         /* Terminal canonical buffer size */
#endif

#ifndef NOGROUP
#define NOGROUP         65534       /* No group */
#endif

#ifndef NODEV
#define NODEV           ((dev_t)-1) /* Non-existent device */
#endif

/* Clock ticks per second */
#ifndef HZ
#define HZ              100
#endif

#ifndef CLK_TCK
#define CLK_TCK         HZ
#endif

/* ═══════════════════════════════════════════════════════════════
 * Utility Macros
 * ═══════════════════════════════════════════════════════════════*/

/* Number of elements in array */
#ifndef nitems
#define nitems(x)       (sizeof((x)) / sizeof((x)[0]))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x)   nitems(x)
#endif

/* Container of - get parent structure from member pointer */
#ifndef __containerof
#define __containerof(ptr, type, member) \
    ((type *)((char *)(ptr) - __builtin_offsetof(type, member)))
#endif

/* Compile-time assertion */
#ifndef _Static_assert
#define _Static_assert(x, s)    extern int __static_assert[(x) ? 1 : -1]
#endif

#ifndef CTASSERT
#define CTASSERT(x)     _Static_assert(x, "compile-time assertion failed")
#endif

#ifdef __cplusplus
}
#endif

#endif /* _SYS_PARAM_H */
