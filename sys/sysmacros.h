/*
 * RinOS libc - sys/sysmacros.h
 * Device number manipulation macros
 *
 * These macros handle the encoding and decoding of device numbers
 * (dev_t) which combine major and minor device numbers.
 */

#ifndef _SYS_SYSMACROS_H
#define _SYS_SYSMACROS_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * Device Number Layout (Linux-compatible)
 *
 * Traditional Unix used 8 bits each for major and minor.
 * Linux extended this to support more devices:
 *   - Major: 12 bits (0-4095)
 *   - Minor: 20 bits (0-1048575)
 *
 * 32-bit dev_t layout:
 *   bits 0-7:   minor low 8 bits
 *   bits 8-19:  major 12 bits
 *   bits 20-31: minor high 12 bits
 * ═══════════════════════════════════════════════════════════════*/

/* Extract major device number from dev_t */
#define major(dev)  ((unsigned int)(((dev) >> 8) & 0xFFF))

/* Extract minor device number from dev_t */
#define minor(dev)  ((unsigned int)(((dev) & 0xFF) | (((dev) >> 12) & 0xFFF00)))

/* Combine major and minor into dev_t */
#define makedev(maj, min) \
    ((dev_t)(((min) & 0xFF) | (((maj) & 0xFFF) << 8) | (((min) & 0xFFF00) << 12)))

/* ═══════════════════════════════════════════════════════════════
 * GNU-style function versions (for compatibility)
 * ═══════════════════════════════════════════════════════════════*/

#ifndef __SYSMACROS_DEPRECATED_INCLUSION
#define __SYSMACROS_DEPRECATED_INCLUSION

static inline unsigned int gnu_dev_major(dev_t dev) {
    return major(dev);
}

static inline unsigned int gnu_dev_minor(dev_t dev) {
    return minor(dev);
}

static inline dev_t gnu_dev_makedev(unsigned int maj, unsigned int min) {
    return makedev(maj, min);
}

#endif /* __SYSMACROS_DEPRECATED_INCLUSION */

/* ═══════════════════════════════════════════════════════════════
 * Legacy/Compatibility Definitions
 * ═══════════════════════════════════════════════════════════════*/

/* Some systems define these as the number of bits */
#define MAJOR_BITS      12
#define MINOR_BITS      20

/* Maximum values */
#define MAJOR_MAX       ((1U << MAJOR_BITS) - 1)    /* 4095 */
#define MINOR_MAX       ((1U << MINOR_BITS) - 1)    /* 1048575 */

/* ═══════════════════════════════════════════════════════════════
 * Common Device Numbers (for reference)
 * ═══════════════════════════════════════════════════════════════*/

/* Character devices */
#define MEM_MAJOR       1       /* /dev/mem, /dev/null, /dev/zero, etc. */
#define PTY_MASTER_MAJOR 2      /* PTY master */
#define PTY_SLAVE_MAJOR 3       /* PTY slave */
#define TTY_MAJOR       4       /* TTY devices */
#define TTYAUX_MAJOR    5       /* /dev/tty, /dev/console, /dev/ptmx */
#define LP_MAJOR        6       /* Parallel port */
#define VCS_MAJOR       7       /* Virtual console screen */
#define LOOP_MAJOR      7       /* Loop devices (block) */
#define SCSI_DISK_MAJOR 8       /* SCSI disk (block) */
#define MISC_MAJOR      10      /* Miscellaneous devices */
#define INPUT_MAJOR     13      /* Input devices */
#define SOUND_MAJOR     14      /* Sound devices */
#define USB_MAJOR       180     /* USB devices */
#define FB_MAJOR        29      /* Framebuffer */

/* Block devices */
#define IDE0_MAJOR      3       /* First IDE disk */
#define IDE1_MAJOR      22      /* Second IDE disk */
#define FLOPPY_MAJOR    2       /* Floppy disk */
#define RAMDISK_MAJOR   1       /* RAM disk */

/* Minor numbers for /dev/mem major */
#define MEM_MINOR       1       /* /dev/mem */
#define KMEM_MINOR      2       /* /dev/kmem */
#define NULL_MINOR      3       /* /dev/null */
#define PORT_MINOR      4       /* /dev/port */
#define ZERO_MINOR      5       /* /dev/zero */
#define FULL_MINOR      7       /* /dev/full */
#define RANDOM_MINOR    8       /* /dev/random */
#define URANDOM_MINOR   9       /* /dev/urandom */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_SYSMACROS_H */
