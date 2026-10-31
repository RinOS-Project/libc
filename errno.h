/*
 * RinOS libc - errno.h
 * エラーコード定義
 */

#ifndef RIN_LIBC_ERRNO_H
#define RIN_LIBC_ERRNO_H

#if !defined(RIN_FREESTANDING) && \
    defined(__STDC_HOSTED__) && \
    __STDC_HOSTED__ && (defined(__clang__) || defined(__GNUC__)) && \
    (!defined(_SYS_TYPES_H) || defined(_INC_ERRNO)) && \
    (defined(_INC_ERRNO) || defined(RIN_LIBC_FORCE_HOSTED_ERRNO_OWNER))
/* Hosted C tests use Rin's mockable native errno ABI.  C++ wrapper headers may
 * instead share their runtime's errno namespace, but only before a Rin public
 * type header has supplied target ABI declarations: on MinGW, including the
 * host errno header after `sys/types.h` would redeclare `ssize_t` and
 * `wchar_t` with a conflicting host ABI.  Do not infer host ownership merely
 * from stdint/stddef include order: with `-Ilibs/libc`, `include_next` can
 * resolve this header again and suppress every Rin errno definition. */
#include_next <errno.h>
#define RIN_LIBC_HOSTED_ERRNO_OWNER 1
#endif

#ifndef RIN_LIBC_HOSTED_ERRNO_OWNER

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * errno変数
 * ═══════════════════════════════════════════════════════════════*/

/* __errno_location関数でスレッドローカルなerrnoへのポインタを返す */
extern int* __errno_location(void);
#ifndef errno
#define errno (*__errno_location())
#endif

/* ═══════════════════════════════════════════════════════════════
 * エラーコード定義 (POSIX準拠)
 * ═══════════════════════════════════════════════════════════════*/

/* 操作/権限エラー */
#define EPERM           1   /* Operation not permitted */
#define ENOENT          2   /* No such file or directory */
#define ESRCH           3   /* No such process */
#define EINTR           4   /* Interrupted system call */
#define EIO             5   /* I/O error */
#define ENXIO           6   /* No such device or address */
#define E2BIG           7   /* Argument list too long */
#define ENOEXEC         8   /* Exec format error */
#define EBADF           9   /* Bad file descriptor */
#define ECHILD          10  /* No child processes */
#define EAGAIN          11  /* Try again / Resource temporarily unavailable */
#define ENOMEM          12  /* Out of memory */
#define EACCES          13  /* Permission denied */
#define EFAULT          14  /* Bad address */
#define ENOTBLK         15  /* Block device required */
#define EBUSY           16  /* Device or resource busy */
#define EEXIST          17  /* File exists */
#define EXDEV           18  /* Cross-device link */
#define ENODEV          19  /* No such device */
#define ENOTDIR         20  /* Not a directory */
#define EISDIR          21  /* Is a directory */
#define EINVAL          22  /* Invalid argument */
#define ENFILE          23  /* File table overflow */
#define EMFILE          24  /* Too many open files */
#define ENOTTY          25  /* Not a typewriter */
#define ETXTBSY         26  /* Text file busy */
#define EFBIG           27  /* File too large */
#define ENOSPC          28  /* No space left on device */
#define ESPIPE          29  /* Illegal seek */
#define EROFS           30  /* Read-only file system */
#define EMLINK          31  /* Too many links */
#define EPIPE           32  /* Broken pipe */

/* 数学エラー */
#define EDOM            33  /* Math argument out of domain */
#define ERANGE          34  /* Math result not representable */

/* リソースエラー */
#define EDEADLK         35  /* Resource deadlock would occur */
#define ENAMETOOLONG    36  /* File name too long */
#define ENOLCK          37  /* No record locks available */
#define ENOSYS          38  /* Function not implemented */
#define ENOTEMPTY       39  /* Directory not empty */
#define ELOOP           40  /* Too many symbolic links */
#define EWOULDBLOCK     EAGAIN  /* Operation would block */
#define ENOMSG          42  /* No message of desired type */
#define EIDRM           43  /* Identifier removed */
#define ECHRNG          44  /* Channel number out of range */
#define EL2NSYNC        45  /* Level 2 not synchronized */
#define EL3HLT          46  /* Level 3 halted */
#define EL3RST          47  /* Level 3 reset */
#define ELNRNG          48  /* Link number out of range */
#define EUNATCH         49  /* Protocol driver not attached */
#define ENOCSI          50  /* No CSI structure available */
#define EL2HLT          51  /* Level 2 halted */
#define EBADE           52  /* Invalid exchange */
#define EBADR           53  /* Invalid request descriptor */
#define EXFULL          54  /* Exchange full */
#define ENOANO          55  /* No anode */
#define EBADRQC         56  /* Invalid request code */
#define EBADSLT         57  /* Invalid slot */

#define EDEADLOCK       EDEADLK

/* ストリーム/メッセージエラー */
#define EBFONT          59  /* Bad font file format */
#define ENOSTR          60  /* Device not a stream */
#define ENODATA         61  /* No data available */
#define ETIME           62  /* Timer expired */
#define ENOSR           63  /* Out of streams resources */
#define ENONET          64  /* Machine is not on the network */
#define ENOPKG          65  /* Package not installed */
#define EREMOTE         66  /* Object is remote */
#define ENOLINK         67  /* Link has been severed */
#define EADV            68  /* Advertise error */
#define ESRMNT          69  /* Srmount error */
#define ECOMM           70  /* Communication error on send */
#define EPROTO          71  /* Protocol error */
#define EMULTIHOP       74  /* Multihop attempted */
#define EDOTDOT         76  /* RFS specific error */
#define EBADMSG         77  /* Bad message */
#define EOVERFLOW       78  /* Value too large for defined data type */
#define ENOTUNIQ        80  /* Name not unique on network */
#define EBADFD          81  /* File descriptor in bad state */
#define EREMCHG         82  /* Remote address changed */
#define ELIBACC         83  /* Can not access a needed shared library */
#define ELIBBAD         84  /* Accessing a corrupted shared library */
#define ELIBSCN         85  /* .lib section in a.out corrupted */
#define ELIBMAX         86  /* Attempting to link in too many shared libraries */
#define ELIBEXEC        87  /* Cannot exec a shared library directly */
#define EILSEQ          88  /* Illegal byte sequence */
#define ERESTART        89  /* Interrupted system call should be restarted */
#define ESTRPIPE        90  /* Streams pipe error */
#define EUSERS          91  /* Too many users */

/* ソケットエラー (200番台で衝突回避) */
#define ENOTSOCK        200 /* Socket operation on non-socket */
#define EDESTADDRREQ    201 /* Destination address required */
#define EMSGSIZE        202 /* Message too long */
#define EPROTOTYPE      203 /* Protocol wrong type for socket */
#define ENOPROTOOPT     204 /* Protocol not available */
#define EPROTONOSUPPORT 205 /* Protocol not supported */
#define ESOCKTNOSUPPORT 206 /* Socket type not supported */
#define EOPNOTSUPP      207 /* Operation not supported */
#define EPFNOSUPPORT    208 /* Protocol family not supported */
#define EAFNOSUPPORT    209 /* Address family not supported by protocol */
#define EADDRINUSE      210 /* Address already in use */
#define EADDRNOTAVAIL   211 /* Cannot assign requested address */
#define ENETDOWN        212 /* Network is down */
#define ENETUNREACH     213 /* Network is unreachable */
#define ENETRESET       214 /* Network dropped connection on reset */
#define ECONNABORTED    215 /* Software caused connection abort */
#define ECONNRESET      216 /* Connection reset by peer */
#define ENOBUFS         217 /* No buffer space available */
#define EISCONN         218 /* Transport endpoint is already connected */
#define ENOTCONN        219 /* Transport endpoint is not connected */
#define ESHUTDOWN       220 /* Cannot send after transport endpoint shutdown */
#define ETOOMANYREFS    221 /* Too many references: cannot splice */
#define ETIMEDOUT       222 /* Connection timed out */
#define ECONNREFUSED    223 /* Connection refused */
#define EHOSTDOWN       224 /* Host is down */
#define EHOSTUNREACH    225 /* No route to host */
#define EALREADY        226 /* Operation already in progress */
#define EINPROGRESS     227 /* Operation now in progress */
#define ESTALE          116 /* Stale file handle */
#define EUCLEAN         117 /* Structure needs cleaning */
#define ENOTNAM         118 /* Not a XENIX named type file */
#define ENAVAIL         119 /* No XENIX semaphores available */
#define EISNAM          120 /* Is a named type file */
#define EREMOTEIO       121 /* Remote I/O error */
#define EDQUOT          122 /* Quota exceeded */
#define ENOMEDIUM       123 /* No medium found */
#define EMEDIUMTYPE     124 /* Wrong medium type */
#define ECANCELED       125 /* Operation canceled */
#define ENOKEY          126 /* Required key not available */
#define EKEYEXPIRED     127 /* Key has expired */
#define EKEYREVOKED     128 /* Key has been revoked */
#define EKEYREJECTED    129 /* Key was rejected by service */
#define EOWNERDEAD      130 /* Owner died */
#define ENOTRECOVERABLE 131 /* State not recoverable */

/* POSIX.1-2001互換。Hosted C runtime が既に独自の値を所有する場合は
 * それを再定義せず、RinOS native build だけで POSIX alias を公開する。 */
#ifndef ENOTSUP
#define ENOTSUP         EOPNOTSUPP
#endif

/* strerror is defined in string.h */

#ifdef __cplusplus
}
#endif

#endif /* !RIN_LIBC_HOSTED_ERRNO_OWNER */
#undef RIN_LIBC_HOSTED_ERRNO_OWNER

#endif /* RIN_LIBC_ERRNO_H */
