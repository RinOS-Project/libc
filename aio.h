/* SPDX-License-Identifier: MIT */
/* RinOS libc - POSIX非同期I/O */

#ifndef _AIO_H
#define _AIO_H

#include "stddef.h"
#include "limits.h"
#include "sys/types.h"
#include "signal.h"
#include "time.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AIO_CANCELED       0
#define AIO_NOTCANCELED    1
#define AIO_ALLDONE        2

#define LIO_NOP            0
#define LIO_READ           1
#define LIO_WRITE          2

#define LIO_WAIT           0
#define LIO_NOWAIT         1

struct aiocb {
    int aio_fildes;
    off_t aio_offset;
    volatile void* aio_buf;
    size_t aio_nbytes;
    int aio_reqprio;
    struct sigevent aio_sigevent;
    int aio_lio_opcode;
    void* __rin_private[8];
};

int aio_read(struct aiocb* request);
int aio_write(struct aiocb* request);
int aio_fsync(int operation, struct aiocb* request);
int aio_error(const struct aiocb* request);
ssize_t aio_return(struct aiocb* request);
int aio_cancel(int fd, struct aiocb* request);
int aio_suspend(const struct aiocb* const requests[], int count,
                const struct timespec* timeout);
int lio_listio(int mode, struct aiocb* const requests[], int count,
               const struct sigevent* notification);

#ifdef __cplusplus
}
#endif

#endif /* _AIO_H */
