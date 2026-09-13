/*
 * RinOS libc - sys/ioctl.h
 * デバイス制御
 */

#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H

#include "types.h"
#include "../stdarg.h"
#include "../errno.h"
#include "socket.h"
#include "syscall.h"

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

#define TIOCEXCL    0x540C  /* 排他モード設定 */
#define TIOCNXCL    0x540D  /* 排他モード解除 */
#define TIOCSCTTY   0x540E  /* 制御端末設定 */
#define TIOCGPGRP   0x540F  /* プロセスグループ取得 */
#define TIOCSPGRP   0x5410  /* プロセスグループ設定 */
#define TIOCOUTQ    0x5411  /* 出力キューサイズ */
#define TIOCSTI     0x5412  /* 文字を挿入 */
#define TIOCGWINSZ  0x5413  /* ウィンドウサイズ取得 */
#define TIOCSWINSZ  0x5414  /* ウィンドウサイズ設定 */
#define TIOCMGET    0x5415  /* モデム状態取得 */
#define TIOCMBIS    0x5416  /* モデムビット設定 */
#define TIOCMBIC    0x5417  /* モデムビットクリア */
#define TIOCMSET    0x5418  /* モデム状態設定 */
#define TIOCGSOFTCAR 0x5419
#define TIOCSSOFTCAR 0x541A
#define FIONREAD    0x541B  /* 読み取り可能バイト数 */
#define TIOCINQ     FIONREAD
#define TIOCLINUX   0x541C
#define TIOCCONS    0x541D
#define TIOCGSERIAL 0x541E
#define TIOCSSERIAL 0x541F
#define TIOCPKT     0x5420
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

/* ═══════════════════════════════════════════════════════════════
 * ソケットioctl
 * ═══════════════════════════════════════════════════════════════*/

#define SIOCGIFNAME    0x8910  /* インターフェース名取得 */
#define SIOCSIFLINK    0x8911
#define SIOCGIFCONF    0x8912  /* インターフェース設定一覧 */
#define SIOCGIFFLAGS   0x8913  /* インターフェースフラグ取得 */
#define SIOCSIFFLAGS   0x8914  /* インターフェースフラグ設定 */
#define SIOCGIFADDR    0x8915  /* IPアドレス取得 */
#define SIOCSIFADDR    0x8916  /* IPアドレス設定 */
#define SIOCGIFDSTADDR 0x8917  /* 宛先アドレス取得 */
#define SIOCSIFDSTADDR 0x8918  /* 宛先アドレス設定 */
#define SIOCGIFBRDADDR 0x8919  /* ブロードキャストアドレス取得 */
#define SIOCSIFBRDADDR 0x891A  /* ブロードキャストアドレス設定 */
#define SIOCGIFNETMASK 0x891B  /* ネットマスク取得 */
#define SIOCSIFNETMASK 0x891C  /* ネットマスク設定 */
#define SIOCGIFMETRIC  0x891D
#define SIOCSIFMETRIC  0x891E
#define SIOCGIFMEM     0x891F
#define SIOCSIFMEM     0x8920
#define SIOCGIFMTU     0x8921  /* MTU取得 */
#define SIOCSIFMTU     0x8922  /* MTU設定 */
#define SIOCSIFHWADDR  0x8924  /* MACアドレス設定 */
#define SIOCGIFENCAP   0x8925
#define SIOCSIFENCAP   0x8926
#define SIOCGIFHWADDR  0x8927  /* MACアドレス取得 */
#define SIOCGIFSLAVE   0x8929
#define SIOCSIFSLAVE   0x8930
#define SIOCADDMULTI   0x8931  /* マルチキャスト追加 */
#define SIOCDELMULTI   0x8932  /* マルチキャスト削除 */
#define SIOCGIFINDEX   0x8933  /* インターフェースインデックス取得 */

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
 *   TCFLSH / TIOCGSID / TIOCSCTTY / TIOCNOTTY
 *              : COM1 serial TTY owner、または admitted legacy CHAR1 callbackへ委譲。
 *                legacy CHAR1は旧TTY requestのwire sizeをkernelが補う。
 *                line-discipline、出力suspend、flow-control、breakなど
 *                physical backend未接続のrequestは EOPNOTSUPP を返すが、
 *                session attach/detachとTIOCGSIDはTTY ownerへ実装済み。
 *   TIOCGWINSZ / TIOCSWINSZ : x86_64 の admitted stdio descriptor は
 *              TTY owner の 8-byte winsize を取得・更新する。32-bit ABI、
 *              virtual-terminal descriptor、未admit fd は ENOTTY。
 *   未対応 req : EINVAL。
 */
static inline int ioctl(int fd, unsigned long request, ...) {
    va_list ap;
    void* arg_ptr;
    intptr_t raw_result;
    va_start(ap, request);
    arg_ptr = va_arg(ap, void*);
    va_end(ap);
    raw_result = _RIN_IOCTL_SYSCALL3(
        SYS_IOCTL, (uintptr_t)fd, (uintptr_t)request, (uintptr_t)arg_ptr);
    return _rin_ioctl_result_status_zero(raw_result);
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_IOCTL_H */
