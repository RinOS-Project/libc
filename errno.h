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

#include <rin/errno_abi.h>

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

/* In a freestanding RinOS build these values define the target ABI even when
 * a host compiler's standard headers were force-included for a hosted test. */
#undef EADDRINUSE
#undef EADDRNOTAVAIL
#undef EAFNOSUPPORT
#undef EALREADY
#undef EBADMSG
#undef ECANCELED
#undef ECONNABORTED
#undef ECONNREFUSED
#undef ECONNRESET
#undef EDEADLK
#undef EDESTADDRREQ
#undef EHOSTUNREACH
#undef EIDRM
#undef EILSEQ
#undef EINPROGRESS
#undef EISCONN
#undef ELOOP
#undef EMSGSIZE
#undef ENAMETOOLONG
#undef ENETDOWN
#undef ENETRESET
#undef ENETUNREACH
#undef ENOBUFS
#undef ENODATA
#undef ENOLCK
#undef ENOLINK
#undef ENOMSG
#undef ENOPROTOOPT
#undef ENOSR
#undef ENOSTR
#undef ENOSYS
#undef ENOTCONN
#undef ENOTEMPTY
#undef ENOTRECOVERABLE
#undef ENOTSOCK
#undef ENOTSUP
#undef EOPNOTSUPP
#undef EOVERFLOW
#undef EOWNERDEAD
#undef EPROTO
#undef EPROTONOSUPPORT
#undef EPROTOTYPE
#undef ETIME
#undef ETIMEDOUT
#undef ETXTBSY
#undef EWOULDBLOCK

/* 操作/権限エラー */
#define EPERM           RIN_EPERM   /* Operation not permitted */
#define ENOENT          RIN_ENOENT   /* No such file or directory */
#define ESRCH           RIN_ESRCH   /* No such process */
#define EINTR           RIN_EINTR   /* Interrupted system call */
#define EIO             RIN_EIO   /* I/O error */
#define ENXIO           RIN_ENXIO   /* No such device or address */
#define E2BIG           RIN_E2BIG   /* Argument list too long */
#define ENOEXEC         RIN_ENOEXEC   /* Exec format error */
#define EBADF           RIN_EBADF   /* Bad file descriptor */
#define ECHILD          RIN_ECHILD  /* No child processes */
#define EAGAIN          RIN_EAGAIN  /* Try again / Resource temporarily unavailable */
#define ENOMEM          RIN_ENOMEM  /* Out of memory */
#define EACCES          RIN_EACCES  /* Permission denied */
#define EFAULT          RIN_EFAULT  /* Bad address */
#define ENOTBLK         RIN_ENOTBLK  /* Block device required */
#define EBUSY           RIN_EBUSY  /* Device or resource busy */
#define EEXIST          RIN_EEXIST  /* File exists */
#define EXDEV           RIN_EXDEV  /* Cross-device link */
#define ENODEV          RIN_ENODEV  /* No such device */
#define ENOTDIR         RIN_ENOTDIR  /* Not a directory */
#define EISDIR          RIN_EISDIR  /* Is a directory */
#define EINVAL          RIN_EINVAL  /* Invalid argument */
#define ENFILE          RIN_ENFILE  /* File table overflow */
#define EMFILE          RIN_EMFILE  /* Too many open files */
#define ENOTTY          RIN_ENOTTY  /* Not a typewriter */
#define ETXTBSY         RIN_ETXTBSY  /* Text file busy */
#define EFBIG           RIN_EFBIG  /* File too large */
#define ENOSPC          RIN_ENOSPC  /* No space left on device */
#define ESPIPE          RIN_ESPIPE  /* Illegal seek */
#define EROFS           RIN_EROFS  /* Read-only file system */
#define EMLINK          RIN_EMLINK  /* Too many links */
#define EPIPE           RIN_EPIPE  /* Broken pipe */

/* 数学エラー */
#define EDOM            RIN_EDOM  /* Math argument out of domain */
#define ERANGE          RIN_ERANGE  /* Math result not representable */

/* リソースエラー */
#define EDEADLK         RIN_EDEADLK  /* Resource deadlock would occur */
#define ENAMETOOLONG    RIN_ENAMETOOLONG  /* File name too long */
#define ENOLCK          RIN_ENOLCK  /* No record locks available */
#define ENOSYS          RIN_ENOSYS  /* Function not implemented */
#define ENOTEMPTY       RIN_ENOTEMPTY  /* Directory not empty */
#define ELOOP           RIN_ELOOP  /* Too many symbolic links */
#define EWOULDBLOCK     RIN_EWOULDBLOCK  /* Operation would block */
#define ENOMSG          RIN_ENOMSG  /* No message of desired type */
#define EIDRM           RIN_EIDRM  /* Identifier removed */
#define ECHRNG          RIN_ECHRNG  /* Channel number out of range */
#define EL2NSYNC        RIN_EL2NSYNC  /* Level 2 not synchronized */
#define EL3HLT          RIN_EL3HLT  /* Level 3 halted */
#define EL3RST          RIN_EL3RST  /* Level 3 reset */
#define ELNRNG          RIN_ELNRNG  /* Link number out of range */
#define EUNATCH         RIN_EUNATCH  /* Protocol driver not attached */
#define ENOCSI          RIN_ENOCSI  /* No CSI structure available */
#define EL2HLT          RIN_EL2HLT  /* Level 2 halted */
#define EBADE           RIN_EBADE  /* Invalid exchange */
#define EBADR           RIN_EBADR  /* Invalid request descriptor */
#define EXFULL          RIN_EXFULL  /* Exchange full */
#define ENOANO          RIN_ENOANO  /* No anode */
#define EBADRQC         RIN_EBADRQC  /* Invalid request code */
#define EBADSLT         RIN_EBADSLT  /* Invalid slot */

#define EDEADLOCK       RIN_EDEADLOCK

/* ストリーム/メッセージエラー */
#define EBFONT          RIN_EBFONT  /* Bad font file format */
#define ENOSTR          RIN_ENOSTR  /* Device not a stream */
#define ENODATA         RIN_ENODATA  /* No data available */
#define ETIME           RIN_ETIME  /* Timer expired */
#define ENOSR           RIN_ENOSR  /* Out of streams resources */
#define ENONET          RIN_ENONET  /* Machine is not on the network */
#define ENOPKG          RIN_ENOPKG  /* Package not installed */
#define EREMOTE         RIN_EREMOTE  /* Object is remote */
#define ENOLINK         RIN_ENOLINK  /* Link has been severed */
#define EADV            RIN_EADV  /* Advertise error */
#define ESRMNT          RIN_ESRMNT  /* Srmount error */
#define ECOMM           RIN_ECOMM  /* Communication error on send */
#define EPROTO          RIN_EPROTO  /* Protocol error */
#define EMULTIHOP       RIN_EMULTIHOP  /* Multihop attempted */
#define EDOTDOT         RIN_EDOTDOT  /* RFS specific error */
#define EBADMSG         RIN_EBADMSG  /* Bad message */
#define EOVERFLOW       RIN_EOVERFLOW  /* Value too large for defined data type */
#define ENOTUNIQ        RIN_ENOTUNIQ  /* Name not unique on network */
#define EBADFD          RIN_EBADFD  /* File descriptor in bad state */
#define EREMCHG         RIN_EREMCHG  /* Remote address changed */
#define ELIBACC         RIN_ELIBACC  /* Can not access a needed shared library */
#define ELIBBAD         RIN_ELIBBAD  /* Accessing a corrupted shared library */
#define ELIBSCN         RIN_ELIBSCN  /* .lib section in a.out corrupted */
#define ELIBMAX         RIN_ELIBMAX  /* Attempting to link in too many shared libraries */
#define ELIBEXEC        RIN_ELIBEXEC  /* Cannot exec a shared library directly */
#define EILSEQ          RIN_EILSEQ  /* Illegal byte sequence */
#define ERESTART        RIN_ERESTART  /* Interrupted system call should be restarted */
#define ESTRPIPE        RIN_ESTRPIPE  /* Streams pipe error */
#define EUSERS          RIN_EUSERS  /* Too many users */

/* ソケットエラー (200番台で衝突回避) */
#define ENOTSOCK        RIN_ENOTSOCK /* Socket operation on non-socket */
#define EDESTADDRREQ    RIN_EDESTADDRREQ /* Destination address required */
#define EMSGSIZE        RIN_EMSGSIZE /* Message too long */
#define EPROTOTYPE      RIN_EPROTOTYPE /* Protocol wrong type for socket */
#define ENOPROTOOPT     RIN_ENOPROTOOPT /* Protocol not available */
#define EPROTONOSUPPORT RIN_EPROTONOSUPPORT /* Protocol not supported */
#define ESOCKTNOSUPPORT RIN_ESOCKTNOSUPPORT /* Socket type not supported */
#define EOPNOTSUPP      RIN_EOPNOTSUPP /* Operation not supported */
#define EPFNOSUPPORT    RIN_EPFNOSUPPORT /* Protocol family not supported */
#define EAFNOSUPPORT    RIN_EAFNOSUPPORT /* Address family not supported by protocol */
#define EADDRINUSE      RIN_EADDRINUSE /* Address already in use */
#define EADDRNOTAVAIL   RIN_EADDRNOTAVAIL /* Cannot assign requested address */
#define ENETDOWN        RIN_ENETDOWN /* Network is down */
#define ENETUNREACH     RIN_ENETUNREACH /* Network is unreachable */
#define ENETRESET       RIN_ENETRESET /* Network dropped connection on reset */
#define ECONNABORTED    RIN_ECONNABORTED /* Software caused connection abort */
#define ECONNRESET      RIN_ECONNRESET /* Connection reset by peer */
#define ENOBUFS         RIN_ENOBUFS /* No buffer space available */
#define EISCONN         RIN_EISCONN /* Transport endpoint is already connected */
#define ENOTCONN        RIN_ENOTCONN /* Transport endpoint is not connected */
#define ESHUTDOWN       RIN_ESHUTDOWN /* Cannot send after transport endpoint shutdown */
#define ETOOMANYREFS    RIN_ETOOMANYREFS /* Too many references: cannot splice */
#define ETIMEDOUT       RIN_ETIMEDOUT /* Connection timed out */
#define ECONNREFUSED    RIN_ECONNREFUSED /* Connection refused */
#define EHOSTDOWN       RIN_EHOSTDOWN /* Host is down */
#define EHOSTUNREACH    RIN_EHOSTUNREACH /* No route to host */
#define EALREADY        RIN_EALREADY /* Operation already in progress */
#define EINPROGRESS     RIN_EINPROGRESS /* Operation now in progress */
#define ESTALE          RIN_ESTALE /* Stale file handle */
#define EUCLEAN         RIN_EUCLEAN /* Structure needs cleaning */
#define ENOTNAM         RIN_ENOTNAM /* Not a XENIX named type file */
#define ENAVAIL         RIN_ENAVAIL /* No XENIX semaphores available */
#define EISNAM          RIN_EISNAM /* Is a named type file */
#define EREMOTEIO       RIN_EREMOTEIO /* Remote I/O error */
#define EDQUOT          RIN_EDQUOT /* Quota exceeded */
#define ENOMEDIUM       RIN_ENOMEDIUM /* No medium found */
#define EMEDIUMTYPE     RIN_EMEDIUMTYPE /* Wrong medium type */
#define ECANCELED       RIN_ECANCELED /* Operation canceled */
#define ENOKEY          RIN_ENOKEY /* Required key not available */
#define EKEYEXPIRED     RIN_EKEYEXPIRED /* Key has expired */
#define EKEYREVOKED     RIN_EKEYREVOKED /* Key has been revoked */
#define EKEYREJECTED    RIN_EKEYREJECTED /* Key was rejected by service */
#define EOWNERDEAD      RIN_EOWNERDEAD /* Owner died */
#define ENOTRECOVERABLE RIN_ENOTRECOVERABLE /* State not recoverable */

/* POSIX.1-2001互換。Hosted C runtime が既に独自の値を所有する場合は
 * それを再定義せず、RinOS native build だけで POSIX alias を公開する。 */
#ifndef ENOTSUP
#define ENOTSUP         RIN_ENOTSUP
#endif

/* strerror is defined in string.h */

#ifdef __cplusplus
}
#endif

#endif /* !RIN_LIBC_HOSTED_ERRNO_OWNER */
#undef RIN_LIBC_HOSTED_ERRNO_OWNER

#endif /* RIN_LIBC_ERRNO_H */
