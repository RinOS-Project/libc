// SPDX-License-Identifier: MIT
#ifndef _NET_IF_H
#define _NET_IF_H

#include <stdint.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IF_NAMESIZE 16
#define IFNAMSIZ IF_NAMESIZE

struct if_nameindex {
    unsigned int if_index;
    char* if_name;
};

struct ifmap {
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8
    uint64_t mem_start;
    uint64_t mem_end;
#else
    uint32_t mem_start;
    uint32_t mem_end;
#endif
    unsigned short base_addr;
    unsigned char irq;
    unsigned char dma;
    unsigned char port;
};

/* Linux x86 wire layout.  The union size follows the active pointer width,
 * giving 32-byte IA-32/compat32 and 40-byte x86_64 native ifreq records. */
struct ifreq {
    union {
        char ifrn_name[IFNAMSIZ];
    } ifr_ifrn;
    union {
        struct sockaddr ifru_addr;
        struct sockaddr ifru_dstaddr;
        struct sockaddr ifru_broadaddr;
        struct sockaddr ifru_netmask;
        struct sockaddr ifru_hwaddr;
        short ifru_flags;
        int ifru_ivalue;
        int ifru_mtu;
        struct ifmap ifru_map;
        char ifru_slave[IFNAMSIZ];
        char ifru_newname[IFNAMSIZ];
        void* ifru_data;
    } ifr_ifru;
};

struct ifconf {
    int ifc_len;
    union {
        char* ifcu_buf;
        struct ifreq* ifcu_req;
    } ifc_ifcu;
};

#define ifr_name      ifr_ifrn.ifrn_name
#define ifr_addr      ifr_ifru.ifru_addr
#define ifr_dstaddr   ifr_ifru.ifru_dstaddr
#define ifr_broadaddr ifr_ifru.ifru_broadaddr
#define ifr_netmask   ifr_ifru.ifru_netmask
#define ifr_hwaddr    ifr_ifru.ifru_hwaddr
#define ifr_flags     ifr_ifru.ifru_flags
#define ifr_metric    ifr_ifru.ifru_ivalue
#define ifr_ifindex   ifr_ifru.ifru_ivalue
#define ifr_mtu       ifr_ifru.ifru_mtu
#define ifr_map       ifr_ifru.ifru_map
#define ifr_slave     ifr_ifru.ifru_slave
#define ifr_newname   ifr_ifru.ifru_newname
#define ifr_data      ifr_ifru.ifru_data
#define ifc_buf       ifc_ifcu.ifcu_buf
#define ifc_req       ifc_ifcu.ifcu_req

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#ifdef __cplusplus
static_assert(sizeof(struct ifreq) == 32u, "IA-32 ifreq ABI drift");
static_assert(sizeof(struct ifconf) == 8u, "IA-32 ifconf ABI drift");
#else
_Static_assert(sizeof(struct ifreq) == 32u, "IA-32 ifreq ABI drift");
_Static_assert(sizeof(struct ifconf) == 8u, "IA-32 ifconf ABI drift");
#endif
#elif defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8
#ifdef __cplusplus
static_assert(sizeof(struct ifreq) == 40u, "x86_64 ifreq ABI drift");
static_assert(sizeof(struct ifconf) == 16u, "x86_64 ifconf ABI drift");
#else
_Static_assert(sizeof(struct ifreq) == 40u, "x86_64 ifreq ABI drift");
_Static_assert(sizeof(struct ifconf) == 16u, "x86_64 ifconf ABI drift");
#endif
#endif

/* Product network interface flags used by the POSIX-shaped ifaddrs ABI.
 * Keep these values stable because System.Native consumes them without a
 * second product-specific translation layer. */
#ifndef IFF_UP
#define IFF_UP        0x0001u
#define IFF_BROADCAST 0x0002u
#define IFF_POINTOPOINT 0x0010u
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
