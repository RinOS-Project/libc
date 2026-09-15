/* SPDX-License-Identifier: MIT */
/* RinOS libc - netinet/tcp.h */
#ifndef _NETINET_TCP_H
#define _NETINET_TCP_H

/* The product socket contract owns the TCP level and option constants. */
#include "../sys/socket.h"

#ifndef IPPROTO_TCP
#define IPPROTO_TCP 6
#endif
#ifndef TCP_NODELAY
#define TCP_NODELAY 1
#endif
#ifndef TCP_MAXSEG
#define TCP_MAXSEG 2
#endif
#ifndef TCP_KEEPIDLE
#define TCP_KEEPIDLE 4
#endif
#ifndef TCP_KEEPINTVL
#define TCP_KEEPINTVL 5
#endif
#ifndef TCP_KEEPCNT
#define TCP_KEEPCNT 6
#endif

#endif /* _NETINET_TCP_H */
