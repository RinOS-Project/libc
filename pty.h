/* SPDX-License-Identifier: MIT */

#ifndef _PTY_H
#define _PTY_H

#include "sys/types.h"
#include "sys/ioctl.h"
#include "termios.h"

#ifdef __cplusplus
extern "C" {
#endif

/* BSD libutil互換のPTY API。nameを指定する場合はPATH_MAX byte以上の
 * 書込み可能領域を渡すこと。 */
int openpty(int* master_fd_out, int* slave_fd_out, char* name,
            const struct termios* terminal_attributes,
            const struct winsize* window_size);
int login_tty(int fd);
pid_t forkpty(int* master_fd_out, char* name,
              const struct termios* terminal_attributes,
              const struct winsize* window_size);

#ifdef __cplusplus
}
#endif

#endif /* _PTY_H */
