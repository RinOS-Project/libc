/*
 * RinOS features.h
 * Feature test macros for libc compatibility
 */

#ifndef _FEATURES_H
#define _FEATURES_H

/* RinOS freestanding environment */
#define __RINOS__ 1

/* POSIX version compatibility (minimal) */
#define _POSIX_VERSION 200809L
#define _POSIX_MESSAGE_PASSING 200809L

/* GNU extensions disabled */
#undef _GNU_SOURCE
#undef __USE_GNU

#endif /* _FEATURES_H */
