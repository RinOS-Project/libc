/*
 * RinOS libc - sys/file.h
 * BSD-style whole-file locking mapped to the RinOS fcntl lock ABI.
 */

#ifndef _SYS_FILE_H
#define _SYS_FILE_H

#include <fcntl.h>

#ifndef LOCK_SH
#define LOCK_SH 0x01
#endif
#ifndef LOCK_EX
#define LOCK_EX 0x02
#endif
#ifndef LOCK_NB
#define LOCK_NB 0x04
#endif
#ifndef LOCK_UN
#define LOCK_UN 0x08
#endif

#ifndef MIDL_PASS
static inline int flock(int fd, int operation)
{
    struct flock lock;
    int command;
    int mode = operation & (LOCK_SH | LOCK_EX | LOCK_UN);

    if ((operation & ~(LOCK_SH | LOCK_EX | LOCK_NB | LOCK_UN)) != 0 ||
        (mode != LOCK_SH && mode != LOCK_EX && mode != LOCK_UN)) {
        errno = EINVAL;
        return -1;
    }

    lock.l_type = mode == LOCK_SH ? F_RDLCK :
                  mode == LOCK_EX ? F_WRLCK : F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_pid = 0;
    command = (operation & LOCK_NB) != 0 ? F_SETLK : F_SETLKW;
    return fcntl(fd, command, &lock);
}
#endif /* !MIDL_PASS */

#endif /* _SYS_FILE_H */
