/*
 * RinOS libc - termio.h
 * 旧端末I/O構造体
 */

#ifndef _RIN_TERMINAL_TERMIO_H
#define _RIN_TERMINAL_TERMIO_H

#include <rin/tty/termio_abi.h>

#ifndef NCC
#define NCC RIN_TTY_TERMIO_NCC
#endif

struct termio {
    unsigned short c_iflag;
    unsigned short c_oflag;
    unsigned short c_cflag;
    unsigned short c_lflag;
    unsigned char c_line;
    unsigned char c_cc[NCC];
};

#if defined(__cplusplus)
static_assert(sizeof(struct termio) == sizeof(RinTtyTermioV1),
              "RinPort termio ABI drift");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(struct termio) == sizeof(RinTtyTermioV1),
               "RinPort termio ABI drift");
#endif

#endif /* _RIN_TERMINAL_TERMIO_H */
