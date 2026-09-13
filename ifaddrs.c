/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc ifaddrs adapter.
 *
 * RinOS intentionally exposes a bounded network inventory rather than a
 * host /proc or ioctl interface table.  Present that inventory as a stable
 * POSIX-shaped list so System.Native can use its existing interface parser.
 * The returned allocation is one block and is released by freeifaddrs().
 */
#include "ifaddrs.h"

#include "errno.h"
#include "net/if.h"
#include "netinet/in.h"
#include "stdlib.h"
#include "string.h"
#include "../../../src/shared/netif_abi.h"

extern int rin_net_get_primary_info(RinNetPrimaryInfo* out);
extern int rin_net_get_ipv6_info(RinNetIPv6Info* out);

typedef struct RinIfaddrsStorage {
    struct ifaddrs entries[2];
    char name[IF_NAMESIZE];
    struct sockaddr_in address4;
    struct sockaddr_in netmask4;
    struct sockaddr_in6 address6;
    struct sockaddr_in6 netmask6;
} RinIfaddrsStorage;

static unsigned int rin_ifaddrs_flags(const RinNetPrimaryInfo* primary) {
    unsigned int flags = 0u;
    if (!primary) return flags;
    if ((primary->flags & RIN_NETINFO_FLAG_DEVICE_READY) != 0u)
        flags |= IFF_UP | IFF_MULTICAST;
    if ((primary->flags & RIN_NETINFO_FLAG_LINK_UP) != 0u)
        flags |= IFF_RUNNING;
    if (strcmp(primary->ifname, "lo") == 0)
        flags |= IFF_LOOPBACK;
    return flags;
}

static void rin_ifaddrs_set_netmask4(struct sockaddr_in* mask,
                                     const uint8_t bytes[4]) {
    memset(mask, 0, sizeof(*mask));
    mask->sin_family = AF_INET;
    memcpy(&mask->sin_addr.s_addr, bytes, 4u);
}

static void rin_ifaddrs_set_netmask6(struct sockaddr_in6* mask) {
    memset(mask, 0, sizeof(*mask));
    mask->sin6_family = AF_INET6;
    memset(mask->sin6_addr.s6_addr, 0xff, 8u);
}

int getifaddrs(struct ifaddrs** ifap) {
    RinNetPrimaryInfo primary;
    RinNetIPv6Info ipv6;
    RinIfaddrsStorage* storage;
    unsigned int flags;
    unsigned int count = 0u;
    int have_ipv4;
    int have_ipv6;

    if (!ifap) {
        errno = EFAULT;
        return -1;
    }
    *ifap = (struct ifaddrs*)0;
    memset(&primary, 0, sizeof(primary));
    if (rin_net_get_primary_info(&primary) != 0) {
        errno = ENXIO;
        return -1;
    }
    if (primary.ifname[0] == '\0' || primary.device_generation == 0u) {
        errno = EPROTO;
        return -1;
    }

    have_ipv4 = (primary.flags & RIN_NETINFO_FLAG_IPV4_CONFIGURED) != 0u;
    memset(&ipv6, 0, sizeof(ipv6));
    have_ipv6 = rin_net_get_ipv6_info(&ipv6) == 0 &&
        ipv6.version == RIN_NET_IPV6_INFO_VERSION &&
        ipv6.struct_size >= sizeof(ipv6) &&
        ipv6.device_generation == primary.device_generation &&
        (ipv6.flags & RIN_NET_IPV6_INFO_FLAG_LINK_LOCAL) != 0u;
    if (have_ipv4) ++count;
    if (have_ipv6) ++count;
    if (count == 0u) count = 1u; /* publish the link-only interface */

    storage = (RinIfaddrsStorage*)calloc(1u, sizeof(*storage));
    if (!storage) {
        errno = ENOMEM;
        return -1;
    }
    memcpy(storage->name, primary.ifname, sizeof(storage->name));
    storage->name[IF_NAMESIZE - 1u] = '\0';
    flags = rin_ifaddrs_flags(&primary);

    count = 0u;
    if (have_ipv4) {
        struct ifaddrs* entry = &storage->entries[count++];
        entry->ifa_name = storage->name;
        entry->ifa_flags = flags;
        entry->ifa_addr = (struct sockaddr*)&storage->address4;
        entry->ifa_netmask = (struct sockaddr*)&storage->netmask4;
        storage->address4.sin_family = AF_INET;
        memcpy(&storage->address4.sin_addr.s_addr, primary.ip, 4u);
        rin_ifaddrs_set_netmask4(&storage->netmask4, primary.netmask);
    }
    if (have_ipv6) {
        struct ifaddrs* entry = &storage->entries[count++];
        entry->ifa_name = storage->name;
        entry->ifa_flags = flags;
        entry->ifa_addr = (struct sockaddr*)&storage->address6;
        entry->ifa_netmask = (struct sockaddr*)&storage->netmask6;
        storage->address6.sin6_family = AF_INET6;
        storage->address6.sin6_scope_id = primary.device_generation;
        memcpy(storage->address6.sin6_addr.s6_addr, ipv6.link_local, 16u);
        rin_ifaddrs_set_netmask6(&storage->netmask6);
    }
    if (count == 0u) {
        storage->entries[0].ifa_name = storage->name;
        storage->entries[0].ifa_flags = flags;
        count = 1u;
    }
    for (unsigned int index = 0u; index + 1u < count; ++index)
        storage->entries[index].ifa_next = &storage->entries[index + 1u];
    *ifap = &storage->entries[0];
    return 0;
}

void freeifaddrs(struct ifaddrs* ifap) {
    if (ifap) free(ifap);
}
