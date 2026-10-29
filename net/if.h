// SPDX-License-Identifier: MIT
#ifndef _NET_IF_H
#define _NET_IF_H

#ifdef __cplusplus
extern "C" {
#endif

#define IF_NAMESIZE 16
#define IFNAMSIZ IF_NAMESIZE

struct if_nameindex {
    unsigned int if_index;
    char* if_name;
};

/* Product network interface flags used by the POSIX-shaped ifaddrs ABI.
 * Keep these values stable because System.Native consumes them without a
 * second product-specific translation layer. */
#ifndef IFF_UP
#define IFF_UP        0x0001u
#define IFF_BROADCAST 0x0002u
#define IFF_LOOPBACK  0x0008u
#define IFF_RUNNING   0x0040u
#define IFF_MULTICAST 0x1000u
#define IFF_ALLMULTI  0x0200u
#endif

unsigned int if_nametoindex(const char* ifname);
char* if_indextoname(unsigned int ifindex, char* ifname);
struct if_nameindex* if_nameindex(void);
void if_freenameindex(struct if_nameindex* list);

#ifdef __cplusplus
}
#endif

#endif
