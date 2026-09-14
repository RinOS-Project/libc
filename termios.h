/*
 * RinOS libc - termios.h
 * 端末I/O制御
 */

#ifndef _TERMIOS_H
#define _TERMIOS_H

/* `termios.h` is a public hosted-C++ entry point.  Its ABI types include
 * target-owned stdint/syscall headers, so an earlier host <stdint.h> would
 * otherwise make errno.h select the Rin namespace instead of the C++ runtime
 * namespace.  Ask errno.h to claim the hosted owner before those types are
 * included; the force is scoped to this include only. */
#if defined(__cplusplus) && !defined(RIN_FREESTANDING) && \
    !defined(RIN_LIBC_FORCE_HOSTED_ERRNO_OWNER)
#define RIN_LIBC_TERMIOS_SET_HOSTED_ERRNO_OWNER 1
#define RIN_LIBC_FORCE_HOSTED_ERRNO_OWNER 1
#endif
#include "errno.h"
#ifdef RIN_LIBC_TERMIOS_SET_HOSTED_ERRNO_OWNER
#undef RIN_LIBC_FORCE_HOSTED_ERRNO_OWNER
#undef RIN_LIBC_TERMIOS_SET_HOSTED_ERRNO_OWNER
#endif
#include "stdint.h"
#include "sys/ioctl.h"
#include "sys/types.h"
#include <rin/tty/abi.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════
 * 型定義
 * ═══════════════════════════════════════════════════════════════*/

typedef unsigned char  cc_t;
typedef unsigned int   speed_t;
typedef unsigned int   tcflag_t;

/* ═══════════════════════════════════════════════════════════════
 * 制御文字インデックス
 * ═══════════════════════════════════════════════════════════════*/

#define NCCS 32

#define VINTR     0   /* SIGINT文字 (^C) */
#define VQUIT     1   /* SIGQUIT文字 (^\) */
#define VERASE    2   /* 1文字削除 (^H, DEL) */
#define VKILL     3   /* 行削除 (^U) */
#define VEOF      4   /* EOF文字 (^D) */
#define VTIME     5   /* 非正規モードタイムアウト (0.1秒単位) */
#define VMIN      6   /* 非正規モード最小文字数 */
#define VSWTC     7   /* スイッチ文字 */
#define VSTART    8   /* XON文字 (^Q) */
#define VSTOP     9   /* XOFF文字 (^S) */
#define VSUSP     10  /* SIGTSTP文字 (^Z) */
#define VEOL      11  /* 追加EOL文字 */
#define VREPRINT  12  /* 行再表示 (^R) */
#define VDISCARD  13  /* 出力破棄トグル (^O) */
#define VWERASE   14  /* ワード削除 (^W) */
#define VLNEXT    15  /* リテラル次文字 (^V) */
#define VEOL2     16  /* 追加EOL文字2 */

/* ═══════════════════════════════════════════════════════════════
 * 入力モードフラグ (c_iflag)
 * ═══════════════════════════════════════════════════════════════*/

#define IGNBRK   0x00001  /* BREAKを無視 */
#define BRKINT   0x00002  /* BREAK時にSIGINT */
#define IGNPAR   0x00004  /* パリティエラーを無視 */
#define PARMRK   0x00008  /* パリティエラーをマーク */
#define INPCK    0x00010  /* パリティチェック有効 */
#define ISTRIP   0x00020  /* 8ビット目を除去 */
#define INLCR    0x00040  /* NLをCRに変換 */
#define IGNCR    0x00080  /* CRを無視 */
#define ICRNL    0x00100  /* CRをNLに変換 */
#define IUCLC    0x00200  /* 大文字を小文字に変換 */
#define IXON     0x00400  /* 出力XON/XOFF制御有効 */
#define IXANY    0x00800  /* 任意の文字でXON */
#define IXOFF    0x01000  /* 入力XON/XOFF制御有効 */
#define IMAXBEL  0x02000  /* 入力バッファフルでベル */
#define IUTF8    0x04000  /* UTF-8入力 */

/* ═══════════════════════════════════════════════════════════════
 * 出力モードフラグ (c_oflag)
 * ═══════════════════════════════════════════════════════════════*/

#define OPOST    0x00001  /* 出力処理有効 */
#define OLCUC    0x00002  /* 小文字を大文字に変換 */
#define ONLCR    0x00004  /* NLをCR-NLに変換 */
#define OCRNL    0x00008  /* CRをNLに変換 */
#define ONOCR    0x00010  /* 行頭でCRを出力しない */
#define ONLRET   0x00020  /* NL時にCRを実行 */
#define OFILL    0x00040  /* 遅延にフィル文字を使用 */
#define OFDEL    0x00080  /* フィル文字はDEL (なければNUL) */
#define NLDLY    0x00100  /* NL遅延マスク */
#define   NL0    0x00000
#define   NL1    0x00100
#define CRDLY    0x00600  /* CR遅延マスク */
#define   CR0    0x00000
#define   CR1    0x00200
#define   CR2    0x00400
#define   CR3    0x00600
#define TABDLY   0x01800  /* TAB遅延マスク */
#define   TAB0   0x00000
#define   TAB1   0x00800
#define   TAB2   0x01000
#define   TAB3   0x01800
#define   XTABS  TAB3
#define BSDLY    0x02000  /* BS遅延マスク */
#define   BS0    0x00000
#define   BS1    0x02000
#define VTDLY    0x04000  /* VT遅延マスク */
#define   VT0    0x00000
#define   VT1    0x04000
#define FFDLY    0x08000  /* FF遅延マスク */
#define   FF0    0x00000
#define   FF1    0x08000

/* ═══════════════════════════════════════════════════════════════
 * 制御モードフラグ (c_cflag)
 * ═══════════════════════════════════════════════════════════════*/

#define CBAUD    0x0000F  /* ボーレートマスク */
#define  B0      0x00000  /* 回線切断 */
#define  B50     0x00001
#define  B75     0x00002
#define  B110    0x00003
#define  B134    0x00004
#define  B150    0x00005
#define  B200    0x00006
#define  B300    0x00007
#define  B600    0x00008
#define  B1200   0x00009
#define  B1800   0x0000A
#define  B2400   0x0000B
#define  B4800   0x0000C
#define  B9600   0x0000D
#define  B19200  0x0000E
#define  B38400  0x0000F
#define EXTA     B19200
#define EXTB     B38400
#define CSIZE    0x00030  /* 文字サイズマスク */
#define  CS5     0x00000  /* 5ビット */
#define  CS6     0x00010  /* 6ビット */
#define  CS7     0x00020  /* 7ビット */
#define  CS8     0x00030  /* 8ビット */
#define CSTOPB   0x00040  /* ストップビット2 */
#define CREAD    0x00080  /* 受信有効 */
#define PARENB   0x00100  /* パリティ有効 */
#define PARODD   0x00200  /* 奇数パリティ */
#define HUPCL    0x00400  /* 回線切断時にDTRをドロップ */
#define CLOCAL   0x00800  /* モデム制御を無視 */
#define CBAUDEX  0x01000  /* 拡張ボーレート */
#define  B57600  0x01001
#define  B115200 0x01002
#define  B230400 0x01003
#define  B460800 0x01004
#define  B500000 0x01005
#define  B576000 0x01006
#define  B921600 0x01007
#define  B1000000 0x01008
#define  B1152000 0x01009
#define  B1500000 0x0100A
#define  B2000000 0x0100B
#define  B2500000 0x0100C
#define  B3000000 0x0100D
#define  B3500000 0x0100E
#define  B4000000 0x0100F
#define CIBAUD   0x100F0000  /* 入力ボーレートマスク */
#define CRTSCTS  0x80000000  /* RTS/CTSフロー制御 */

/* ═══════════════════════════════════════════════════════════════
 * ローカルモードフラグ (c_lflag)
 * ═══════════════════════════════════════════════════════════════*/

#define ISIG     0x00001  /* シグナル文字有効 */
#define ICANON   0x00002  /* 正規モード */
#define XCASE    0x00004  /* ICANON時に大文字/小文字変換 */
#define ECHO     0x00008  /* エコー有効 */
#define ECHOE    0x00010  /* ERASEをecho */
#define ECHOK    0x00020  /* KILL後にNLをecho */
#define ECHONL   0x00040  /* NLをecho (ECHO無効時も) */
#define NOFLSH   0x00080  /* 割り込み時にフラッシュしない */
#define TOSTOP   0x00100  /* バックグラウンド出力でSIGTTOU */
#define ECHOCTL  0x00200  /* 制御文字を^Xでecho */
#define ECHOPRT  0x00400  /* ERASEした文字を表示 */
#define ECHOKE   0x00800  /* KILL時に行を消去 */
#define FLUSHO   0x01000  /* 出力フラッシュ中 */
#define PENDIN   0x04000  /* 入力保留中 */
#define IEXTEN   0x08000  /* 実装依存の入力処理 */
#define EXTPROC  0x10000  /* 外部処理 */

/* ═══════════════════════════════════════════════════════════════
 * termios構造体
 * ═══════════════════════════════════════════════════════════════*/

struct termios {
    tcflag_t c_iflag;    /* 入力モード */
    tcflag_t c_oflag;    /* 出力モード */
    tcflag_t c_cflag;    /* 制御モード */
    tcflag_t c_lflag;    /* ローカルモード */
    cc_t     c_line;     /* 回線規約 */
    cc_t     c_cc[NCCS]; /* 制御文字 */
    speed_t  c_ispeed;   /* 入力ボーレート */
    speed_t  c_ospeed;   /* 出力ボーレート */
};

#if defined(__cplusplus)
static_assert(sizeof(struct termios) == sizeof(RinTtyTermiosV1),
              "RinPort termios ABI drift");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(struct termios) == sizeof(RinTtyTermiosV1),
               "RinPort termios ABI drift");
#endif

/* ═══════════════════════════════════════════════════════════════
 * tcsetattr オプション
 * ═══════════════════════════════════════════════════════════════*/

#define TCSANOW   0  /* 即座に変更 */
#define TCSADRAIN 1  /* 出力を排出してから変更 */
#define TCSAFLUSH 2  /* 出力を排出し、入力を破棄してから変更 */

/* ═══════════════════════════════════════════════════════════════
 * tcflush キュー選択
 * ═══════════════════════════════════════════════════════════════*/

#define TCIFLUSH  0  /* 入力キューをフラッシュ */
#define TCOFLUSH  1  /* 出力キューをフラッシュ */
#define TCIOFLUSH 2  /* 両方をフラッシュ */

/* ═══════════════════════════════════════════════════════════════
 * tcflow アクション
 * ═══════════════════════════════════════════════════════════════*/

#define TCOOFF 0  /* 出力を停止 */
#define TCOON  1  /* 出力を再開 */
#define TCIOFF 2  /* 入力を停止 (XOFFを送信) */
#define TCION  3  /* 入力を再開 (XONを送信) */

/* ═══════════════════════════════════════════════════════════════
 * 端末制御関数
 * ═══════════════════════════════════════════════════════════════*/

#ifndef _RIN_TERMIOS_IOCTL
#define _RIN_TERMIOS_IOCTL(fd, request, argument) \
    ioctl((fd), (request), (argument))
#endif

static inline int _rin_termios_ioctl_status(int fd, unsigned long request,
                                            void* argument) {
    intptr_t result = __rin_syscall_posixize(
        (intptr_t)_RIN_TERMIOS_IOCTL(fd, request, argument));
    if (result < 0) {
        if (result != -1) errno = EIO;
        return -1;
    }
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

/* The third ioctl word is scalar for these calls.  Do not narrow it through
 * `unsigned long`: that type is only 32 bits in the Win64 ABI used by RinOS.
 */
static inline void* _rin_termios_scalar_argument(uintptr_t value) {
    return (void*)value;
}

static inline int tcgetattr(int fd, struct termios* termios_p) {
    if (!termios_p) {
        errno = EFAULT;
        return -1;
    }
    return _rin_termios_ioctl_status(fd, TCGETS, termios_p);
}

static inline int tcsetattr(int fd, int optional_actions, const struct termios* termios_p) {
    unsigned long request;
    if (!termios_p) {
        errno = EFAULT;
        return -1;
    }
    switch (optional_actions) {
        case TCSANOW: request = TCSETS; break;
        case TCSADRAIN: request = TCSETSW; break;
        case TCSAFLUSH: request = TCSETSF; break;
        default:
            errno = EINVAL;
            return -1;
    }
    return _rin_termios_ioctl_status(fd, request, (void*)termios_p);
}

static inline int tcsendbreak(int fd, int duration) {
    if (duration < 0) {
        errno = EINVAL;
        return -1;
    }
    /* Linux's TCSBRK only distinguishes zero/non-zero (non-zero drains).
     * Preserve a requested positive duration through TCSBRKP instead. */
    return _rin_termios_ioctl_status(fd,
        duration == 0 ? TCSBRK : TCSBRKP,
        _rin_termios_scalar_argument((uintptr_t)duration));
}

static inline int tcdrain(int fd) {
    return _rin_termios_ioctl_status(fd, TCSBRK,
                                     _rin_termios_scalar_argument(UINT32_C(1)));
}

static inline int tcflush(int fd, int queue_selector) {
    if (queue_selector != TCIFLUSH && queue_selector != TCOFLUSH &&
        queue_selector != TCIOFLUSH) {
        errno = EINVAL;
        return -1;
    }
    return _rin_termios_ioctl_status(fd, TCFLSH,
        _rin_termios_scalar_argument((uintptr_t)queue_selector));
}

static inline int tcflow(int fd, int action) {
    if (action != TCOOFF && action != TCOON && action != TCIOFF &&
        action != TCION) {
        errno = EINVAL;
        return -1;
    }
    return _rin_termios_ioctl_status(fd, TCXONC,
        _rin_termios_scalar_argument((uintptr_t)action));
}

static inline pid_t tcgetsid(int fd) {
    pid_t result = (pid_t)-1;
    if (_rin_termios_ioctl_status(fd, TIOCGSID, &result) != 0)
        return (pid_t)-1;
    return result;
}

/* ═══════════════════════════════════════════════════════════════
 * ボーレート関数
 * ═══════════════════════════════════════════════════════════════*/

static inline speed_t cfgetospeed(const struct termios* termios_p) {
    if (!termios_p) {
        errno = EINVAL;
        return B0;
    }
    return termios_p->c_ospeed;
}

static inline speed_t cfgetispeed(const struct termios* termios_p) {
    if (!termios_p) {
        errno = EINVAL;
        return B0;
    }
    return termios_p->c_ispeed;
}

static inline int _rin_termios_valid_speed(speed_t speed) {
    if (speed <= B38400) return 1;
    return speed >= B57600 && speed <= B4000000;
}

static inline int cfsetospeed(struct termios* termios_p, speed_t speed) {
    if (!termios_p || !_rin_termios_valid_speed(speed)) {
        errno = EINVAL;
        return -1;
    }
    termios_p->c_ospeed = speed;
    termios_p->c_cflag = (termios_p->c_cflag & ~(CBAUD | CBAUDEX)) |
                         (speed & (CBAUD | CBAUDEX));
    return 0;
}

static inline int cfsetispeed(struct termios* termios_p, speed_t speed) {
    if (!termios_p || !_rin_termios_valid_speed(speed)) {
        errno = EINVAL;
        return -1;
    }
    if (speed == B0) speed = termios_p->c_ospeed;
    termios_p->c_ispeed = speed;
    return 0;
}

static inline int cfsetspeed(struct termios* termios_p, speed_t speed) {
    if (!termios_p || !_rin_termios_valid_speed(speed)) {
        errno = EINVAL;
        return -1;
    }
    termios_p->c_ispeed = speed;
    termios_p->c_ospeed = speed;
    termios_p->c_cflag = (termios_p->c_cflag & ~(CBAUD | CBAUDEX)) |
                         (speed & (CBAUD | CBAUDEX));
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * 便利関数
 * ═══════════════════════════════════════════════════════════════*/

static inline void cfmakeraw(struct termios* termios_p) {
    if (!termios_p) return;
    termios_p->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    termios_p->c_oflag &= ~OPOST;
    termios_p->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    termios_p->c_cflag &= ~(CSIZE | PARENB);
    termios_p->c_cflag |= CS8;
    termios_p->c_cc[VMIN] = 1;
    termios_p->c_cc[VTIME] = 0;
}

#ifdef __cplusplus
}
#endif

#endif /* _TERMIOS_H */
