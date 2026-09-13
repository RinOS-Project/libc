/*
 * RinOS libc - shared file open flags
 */

#ifndef _FCNTL_FLAGS_H
#define _FCNTL_FLAGS_H

#ifndef O_RDONLY
#define O_RDONLY    0x0001
#endif
#ifndef O_WRONLY
#define O_WRONLY    0x0002
#endif
#ifndef O_RDWR
#define O_RDWR      0x0003
#endif
#ifndef O_ACCMODE
#define O_ACCMODE   0x0003
#endif

#ifndef O_CREAT
#define O_CREAT     0x0004
#endif
#ifndef O_EXCL
#define O_EXCL      0x0008
#endif
#ifndef O_TRUNC
#define O_TRUNC     0x0010
#endif
#ifndef O_APPEND
#define O_APPEND    0x0020
#endif

#ifndef O_NONBLOCK
#define O_NONBLOCK  0x0040
#endif
#ifndef O_NDELAY
#define O_NDELAY    O_NONBLOCK
#endif
#ifndef O_SYNC
#define O_SYNC      0x0080
#endif
#ifndef O_DSYNC
#define O_DSYNC     O_SYNC
#endif
#ifndef O_RSYNC
#define O_RSYNC     O_SYNC
#endif

#ifndef O_NOCTTY
#define O_NOCTTY    0x0100
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC   0x0200
#endif
#ifndef O_DIRECTORY
#define O_DIRECTORY 0x0400
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW  0x0800
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE 0x1000
#endif
#ifndef O_PATH
#define O_PATH      0x2000
#endif
#ifndef O_TMPFILE
#define O_TMPFILE   0x4000
#endif

#ifndef O_CREATE
#define O_CREATE    O_CREAT
#endif

#endif /* _FCNTL_FLAGS_H */
