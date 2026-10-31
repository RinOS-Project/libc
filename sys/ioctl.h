/*
 * RinOS libc - sys/ioctl.h
 * デバイス制御
 */

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__ && \
    (!defined(RIN_FREESTANDING) || !(RIN_FREESTANDING)) && \
    (defined(__clang__) || defined(__GNUC__)) && \
    !defined(_WIN32) && \
    !defined(_RIN_IOCTL_SYSCALL3)
/* The host header below already provides struct termio; tell termio.h to
 * skip the RinOS definition so hosted termios.h consumers do not see a
 * redefinition. */
#define RIN_LIBC_SYS_IOCTL_HOST_DELEGATED 1
#include_next <sys/ioctl.h>
#else

#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H

#include "types.h"
#include "../stdarg.h"
#include "../errno.h"
#include "socket.h"
#include "syscall.h"
#include <rin/tty/abi.h>

#ifndef _RIN_IOCTL_SYSCALL3
#define _RIN_IOCTL_SYSCALL3(number, argument1, argument2, argument3) \
    _syscall3((uintptr_t)(number), (uintptr_t)(argument1), \
              (uintptr_t)(argument2), (uintptr_t)(argument3))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * ioctl リクエストマクロ
 * ═══════════════════════════════════════════════════════════════*/

#define _IOC_NRBITS    8
#define _IOC_TYPEBITS  8
#define _IOC_SIZEBITS  14
#define _IOC_DIRBITS   2

#define _IOC_NRMASK    ((1 << _IOC_NRBITS) - 1)
#define _IOC_TYPEMASK  ((1 << _IOC_TYPEBITS) - 1)
#define _IOC_SIZEMASK  ((1 << _IOC_SIZEBITS) - 1)
#define _IOC_DIRMASK   ((1 << _IOC_DIRBITS) - 1)

#define _IOC_NRSHIFT   0
#define _IOC_TYPESHIFT (_IOC_NRSHIFT + _IOC_NRBITS)
#define _IOC_SIZESHIFT (_IOC_TYPESHIFT + _IOC_TYPEBITS)
#define _IOC_DIRSHIFT  (_IOC_SIZESHIFT + _IOC_SIZEBITS)

#define _IOC_NONE      0U
#define _IOC_WRITE     1U
#define _IOC_READ      2U

#define _IOC(dir, type, nr, size) \
    (((dir) << _IOC_DIRSHIFT) | \
     ((type) << _IOC_TYPESHIFT) | \
     ((nr) << _IOC_NRSHIFT) | \
     ((size) << _IOC_SIZESHIFT))

#define _IO(type, nr)        _IOC(_IOC_NONE, (type), (nr), 0)
#define _IOR(type, nr, sz)   _IOC(_IOC_READ, (type), (nr), sizeof(sz))
#define _IOW(type, nr, sz)   _IOC(_IOC_WRITE, (type), (nr), sizeof(sz))
#define _IOWR(type, nr, sz)  _IOC(_IOC_READ|_IOC_WRITE, (type), (nr), sizeof(sz))

/* デコードマクロ */
#define _IOC_DIR(nr)   (((nr) >> _IOC_DIRSHIFT) & _IOC_DIRMASK)
#define _IOC_TYPE(nr)  (((nr) >> _IOC_TYPESHIFT) & _IOC_TYPEMASK)
#define _IOC_NR(nr)    (((nr) >> _IOC_NRSHIFT) & _IOC_NRMASK)
#define _IOC_SIZE(nr)  (((nr) >> _IOC_SIZESHIFT) & _IOC_SIZEMASK)

/* ═══════════════════════════════════════════════════════════════
 * 端末ioctl
 * ═══════════════════════════════════════════════════════════════*/

#define TCGETS      0x5401  /* tcgetattr */
#define TCSETS      0x5402  /* tcsetattr (TCSANOW) */
#define TCSETSW     0x5403  /* tcsetattr (TCSADRAIN) */
#define TCSETSF     0x5404  /* tcsetattr (TCSAFLUSH) */
#define TCGETA      0x5405  /* old tcgetattr */
#define TCSETA      0x5406  /* old tcsetattr */
#define TCSETAW     0x5407
#define TCSETAF     0x5408
#define TCSBRK      0x5409
#define TCXONC      0x540A
#define TCFLSH      0x540B

/* PTY master allocation controls use the public kernel TTY ABI. */
#ifndef TIOCGPTN
#define TIOCGPTN    RIN_TTY_IOCTL_TIOCGPTN
#endif
#ifndef TIOCSPTLCK
#define TIOCSPTLCK  RIN_TTY_IOCTL_TIOCSPTLCK
#endif

#define TIOCEXCL    0x540C  /* 排他モード設定 */
#define TIOCNXCL    0x540D  /* 排他モード解除 */
#define TIOCSCTTY   0x540E  /* 制御端末設定 */
#define TIOCGPGRP   0x540F  /* プロセスグループ取得 */
#define TIOCSPGRP   0x5410  /* プロセスグループ設定 (tcsetpgrp) */
#define TIOCOUTQ    0x5411  /* 出力キューサイズ */
#define TIOCSTI     0x5412  /* 文字を挿入 */
#define TIOCGWINSZ  0x5413  /* ウィンドウサイズ取得 */
#define TIOCSWINSZ  0x5414  /* ウィンドウサイズ設定 */
#define TIOCMGET    0x5415  /* モデム状態取得 */
#define TIOCMBIS    0x5416  /* モデムビット設定 */
#define TIOCMBIC    0x5417  /* モデムビットクリア */
#define TIOCMSET    0x5418  /* モデム状態設定 */
/* TIOCMGET/TIOCMSET status bits (Linux-compatible values). */
#define TIOCM_LE     0x0001
#define TIOCM_DTR    0x0002
#define TIOCM_RTS    0x0004
#define TIOCM_ST     0x0008
#define TIOCM_SR     0x0010
#define TIOCM_CTS    0x0020
#define TIOCM_CAR    0x0040
#define TIOCM_RNG    0x0080
#define TIOCM_DSR    0x0100
#define TIOCGSOFTCAR 0x5419
#define TIOCSSOFTCAR 0x541A
#define FIONREAD    0x541B  /* 読み取り可能バイト数 */
#define TIOCINQ     FIONREAD
#define TIOCLINUX   0x541C
#define TIOCCONS    0x541D
#ifndef TIOCPKT
#define TIOCPKT     0x5420
#endif
#ifndef TIOCPKT_DATA
#define TIOCPKT_DATA 0x00
#endif
#ifndef TIOCPKT_FLUSHREAD
#define TIOCPKT_FLUSHREAD 0x01
#endif
#ifndef TIOCPKT_FLUSHWRITE
#define TIOCPKT_FLUSHWRITE 0x02
#endif
#ifndef TIOCPKT_STOP
#define TIOCPKT_STOP 0x04
#endif
#ifndef TIOCPKT_START
#define TIOCPKT_START 0x08
#endif
#ifndef TIOCPKT_NOSTOP
#define TIOCPKT_NOSTOP 0x10
#endif
#ifndef TIOCPKT_DOSTOP
#define TIOCPKT_DOSTOP 0x20
#endif
#ifndef TIOCPKT_IOCTL
#define TIOCPKT_IOCTL 0x40
#endif
#ifndef TIOCPKT_WINDOW
#define TIOCPKT_WINDOW 0x80
#endif
#define FIONBIO     0x5421  /* ノンブロッキング設定 */
#define TIOCNOTTY   0x5422  /* 制御端末解除 */
#define TIOCSETD    0x5423
#define TIOCGETD    0x5424
#define TCSBRKP     0x5425
#define TIOCSBRK    0x5427
#define TIOCCBRK    0x5428
#define TIOCGSID    0x5429  /* セッションID取得 */

/* TIOCGWINSZ/TIOCSWINSZ用 */
struct winsize {
    unsigned short ws_row;    /* 行数 */
    unsigned short ws_col;    /* 列数 */
    unsigned short ws_xpixel; /* ピクセル幅 */
    unsigned short ws_ypixel; /* ピクセル高さ */
};

/* ═══════════════════════════════════════════════════════════════
 * ファイルioctl
 * ═══════════════════════════════════════════════════════════════*/

#define FIOCLEX     0x5451  /* FD_CLOEXECを設定 */
#define FIONCLEX    0x5450  /* FD_CLOEXECをクリア */
#define FIOASYNC    0x5452  /* 非同期I/O設定 */
#define FIOSETOWN   0x8901
#define SIOCSPGRP   0x8902
#define FIOGETOWN   0x8903
#define SIOCGPGRP   0x8904
/* sockatmark() query.  The kernel TCP urgent-pointer owner is tracked
 * separately; until it exists the ioctl reports the request as unhandled. */
#define SIOCATMARK  0x8905

/* ネットワークインターフェースioctlはkernel handler未実装のため公開しない。
 * 列挙にはgetifaddrs()/if_nameindex()を使用する。 */

/* ═══════════════════════════════════════════════════════════════
 * ioctl関数
 * ═══════════════════════════════════════════════════════════════*/

/* A kernel ioctl is a POSIX status operation: only an exact zero is success.
 * Keep the raw target-width result until the errno contract is decided;
 * converting through host long/int would truncate on LLP64 and could turn an
 * unknown negative or malformed positive into a false success. */
static inline int _rin_ioctl_result_status_zero(intptr_t raw_result) {
    if (raw_result < 0) {
        if (raw_result >= (intptr_t)-4095) {
            errno = (int)-raw_result;
        } else {
            errno = EIO;
        }
        return -1;
    }
    if (raw_result != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

/*
 * ioctl 実装: すべて kernel SYS_IOCTL に委譲する。
 * userspace 側の no-op/スタブは禁止 (feedback_no_stubs.md)。
 * kernel 側 (platform_bare64.c case 49) が以下を処理する:
 *   FIONREAD   : socket / pipe → 残量を *arg に書き戻す。その他は ENOTTY。
 *   FIONBIO    : socket → SO_NONBLOCK、pipe → pipe_set_nonblock、
 *                regular VFS_FILE/DIR/STDIO → 成功 (意味的に no-op)。
 *                admitted legacy CHAR1 VFS_FILE は driver ioctl callbackへ渡す。
 *   TCGETS / TCSETS / TCSETSW / TCSETSF / TCSBRK / TCSBRKP / TCXONC /
 *   TCFLSH / TIOCGSID / TIOCGPGRP / TIOCSPGRP / TIOCSCTTY / TIOCNOTTY
 *              : COM1 serial TTY owner、または admitted legacy CHAR1 callbackへ委譲。
 *                legacy CHAR1は旧TTY requestのwire sizeをkernelが補う。
 *                line-discipline、出力suspend、flow-control、breakなど
 *                physical backend未接続のrequestは EOPNOTSUPP を返すが、
 *                session attach/detach、TIOCGSID、同一session限定の
 *                TIOCGPGRP / TIOCSPGRPはTTY ownerへ実装済み。
 *   TIOCGWINSZ / TIOCSWINSZ : x86_64 の admitted stdio descriptor は
 *              TTY owner の 8-byte winsize を取得・更新する。32-bit ABI、
 *              virtual-terminal descriptor、未admit fd は ENOTTY。
 *   未対応 req : EINVAL。
 */
typedef enum _rin_ioctl_argument_kind {
    _RIN_IOCTL_ARGUMENT_NONE,
    _RIN_IOCTL_ARGUMENT_SCALAR_INT,
    _RIN_IOCTL_ARGUMENT_POINTER
} _rin_ioctl_argument_kind;

static inline _rin_ioctl_argument_kind
_rin_ioctl_get_argument_kind(unsigned long request) {
    switch (request) {
        /* Requests whose ABI has no third argument. */
        case TIOCEXCL:
        case TIOCNXCL:
        case TIOCNOTTY:
        case TIOCSBRK:
        case TIOCCBRK:
        case FIOCLEX:
        case FIONCLEX:
        case FIOGETOWN:
        case SIOCGPGRP:
            return _RIN_IOCTL_ARGUMENT_NONE;

        /* These legacy terminal requests carry an int value by value. */
        case TCSBRK:
        case TCXONC:
        case TCFLSH:
        case TCSBRKP:
        case TIOCSCTTY:
        case FIOSETOWN:
        case SIOCSPGRP:
            return _RIN_IOCTL_ARGUMENT_SCALAR_INT;

        /* Legacy pointer requests have no _IOC size/direction metadata. */
        case TCGETS:
        case TCSETS:
        case TCSETSW:
        case TCSETSF:
        case TCGETA:
        case TCSETA:
        case TCSETAW:
        case TCSETAF:
        case TIOCGPGRP:
        case TIOCSPGRP:
        case TIOCOUTQ:
        case TIOCSTI:
        case TIOCGWINSZ:
        case TIOCSWINSZ:
        case TIOCMGET:
        case TIOCMBIS:
        case TIOCMBIC:
        case TIOCMSET:
        case TIOCGSOFTCAR:
        case TIOCSSOFTCAR:
        case TIOCPKT:
        case FIONREAD:
        case FIONBIO:
        case TIOCSETD:
        case TIOCGETD:
        case TIOCGSID:
        case FIOASYNC:
        case SIOCATMARK: /* sockatmark(3) out-param */
            return _RIN_IOCTL_ARGUMENT_POINTER;

        default:
            /* Encoded _IO requests declare no payload. Legacy pointer
             * requests are classified explicitly above because their
             * direction/size bits do not describe their argument. */
            if (_IOC_DIR(request) == _IOC_NONE && _IOC_SIZE(request) == 0u)
                return _RIN_IOCTL_ARGUMENT_NONE;
            return _RIN_IOCTL_ARGUMENT_POINTER;
    }
}

static inline int ioctl(int fd, unsigned long request, ...) {
    va_list ap;
    uintptr_t arg_word = 0u;
    intptr_t raw_result;
    _rin_ioctl_argument_kind argument_kind =
        _rin_ioctl_get_argument_kind(request);

    if (argument_kind != _RIN_IOCTL_ARGUMENT_NONE) {
        va_start(ap, request);
        if (argument_kind == _RIN_IOCTL_ARGUMENT_SCALAR_INT) {
            /* Default argument promotions pass these legacy scalar values
             * as int. Zero-extend to the fixed-width ioctl syscall word. */
            arg_word = (uintptr_t)(uint32_t)va_arg(ap, int);
        } else {
            arg_word = (uintptr_t)va_arg(ap, void*);
        }
        va_end(ap);
    }
    raw_result = _RIN_IOCTL_SYSCALL3(
        SYS_IOCTL, (uintptr_t)fd, (uintptr_t)request, arg_word);
    return _rin_ioctl_result_status_zero(raw_result);
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_IOCTL_H */

#endif /* hosted ioctl owner */
