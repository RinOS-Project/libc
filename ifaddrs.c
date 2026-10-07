/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc ifaddrs adapter.
 *
 * RinOS exposes a bounded primary-interface snapshot.  This adapter keeps
 * that boundary while returning every configured IPv6 address and its
 * prefix/scope metadata in the POSIX-shaped form used by System.Native.
 */
#include "ifaddrs.h"

#include "errno.h"
#include "net/if.h"
#include "netinet/in.h"
#include "stdlib.h"
#include "string.h"
#include <rin/net/netif_abi.h>

extern int rin_net_get_primary_info(RinNetPrimaryInfo* out);
extern int rin_net_get_ipv6_interface_addresses(
    RinNetIPv6InterfaceAddressListV1* out);

#define RIN_IFADDRS_MAX_ENTRIES \
    (RIN_NET_IPV6_INTERFACE_ADDRESS_CAPACITY + 1u)

typedef struct RinIfaddrsStorage {
    struct ifaddrs entries[RIN_IFADDRS_MAX_ENTRIES];
    char name[IF_NAMESIZE];
    struct sockaddr_in address4;
    struct sockaddr_in netmask4;
    struct sockaddr_in6 addresses6[RIN_NET_IPV6_INTERFACE_ADDRESS_CAPACITY];
    struct sockaddr_in6 netmasks6[RIN_NET_IPV6_INTERFACE_ADDRESS_CAPACITY];
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

static void rin_ifaddrs_set_netmask6(struct sockaddr_in6* mask,
                                     uint8_t prefix_length) {
    unsigned int bit;
    memset(mask, 0, sizeof(*mask));
    mask->sin6_family = AF_INET6;
    for (bit = 0u; bit < prefix_length; ++bit)
        mask->sin6_addr.s6_addr[bit / 8u] |=
            (uint8_t)(0x80u >> (bit % 8u));
}

static int rin_ifaddrs_ipv6_address_valid(const uint8_t address[16]) {
    unsigned int index;
    unsigned int nonzero = 0u;
    for (index = 0u; index < 16u; ++index)
        if (address[index] != 0u) nonzero = 1u;
    return nonzero && address[0] != 0xffu;
}

static int rin_ifaddrs_ipv6_link_local(const uint8_t address[16]) {
    return address[0] == 0xfeu && (address[1] & 0xc0u) == 0x80u;
}

static int rin_ifaddrs_all_zero(const void* memory, size_t size) {
    const uint8_t* bytes = (const uint8_t*)memory;
    size_t index;
    for (index = 0u; index < size; ++index)
        if (bytes[index] != 0u) return 0;
    return 1;
}

static int rin_ifaddrs_ipv6_list_valid(
    const RinNetIPv6InterfaceAddressListV1* list,
    uint16_t expected_device_generation) {
    unsigned int index;
    if (list == NULL ||
        list->version != RIN_NET_IPV6_INTERFACE_ADDRESS_LIST_VERSION ||
        list->struct_size != sizeof(*list) || list->ndp_generation == 0u ||
        list->device_generation != expected_device_generation ||
        list->address_count > RIN_NET_IPV6_INTERFACE_ADDRESS_CAPACITY)
        return 0;
    for (index = 0u; index < list->address_count; ++index) {
        const RinNetIPv6InterfaceAddressV1* entry = &list->addresses[index];
        unsigned int prior;
        if (entry->prefix_length > 128u ||
            !rin_ifaddrs_all_zero(entry->reserved, sizeof(entry->reserved)) ||
            !rin_ifaddrs_ipv6_address_valid(entry->address))
            return 0;
        for (prior = 0u; prior < index; ++prior)
            if (memcmp(entry->address, list->addresses[prior].address,
                       sizeof(entry->address)) == 0)
                return 0;
    }
    for (index = list->address_count;
         index < RIN_NET_IPV6_INTERFACE_ADDRESS_CAPACITY; ++index)
        if (!rin_ifaddrs_all_zero(&list->addresses[index],
                                  sizeof(list->addresses[index])))
            return 0;
    return 1;
}

int getifaddrs(struct ifaddrs** ifap) {
    RinNetPrimaryInfo primary;
    RinNetIPv6InterfaceAddressListV1 ipv6;
    RinIfaddrsStorage* storage;
    unsigned int name_length = 0u;
    unsigned int flags;
    unsigned int count = 0u;
    unsigned int interface_index = 0u;
    unsigned int index;
    int have_ipv4;

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
    while (name_length < sizeof(primary.ifname) &&
           primary.ifname[name_length] != '\0')
        ++name_length;
    if (name_length == 0u || name_length >= sizeof(primary.ifname) ||
        primary.device_generation == 0u ||
        (primary.flags & RIN_NETINFO_FLAG_DEVICE_READY) == 0u ||
        (primary.flags & ~RIN_NETINFO_KNOWN_FLAGS) != 0u) {
        errno = EPROTO;
        return -1;
    }

    memset(&ipv6, 0, sizeof(ipv6));
    if (rin_net_get_ipv6_interface_addresses(&ipv6) != 0 ||
        !rin_ifaddrs_ipv6_list_valid(&ipv6,
                                     primary.device_generation)) {
        errno = EAGAIN;
        return -1;
    }
    have_ipv4 = (primary.flags & RIN_NETINFO_FLAG_IPV4_CONFIGURED) != 0u;
    if (have_ipv4) ++count;
    count += ipv6.address_count;
    if (count == 0u) count = 1u; /* publish the link-only interface */

    for (index = 0u; index < ipv6.address_count; ++index) {
        if (rin_ifaddrs_ipv6_link_local(ipv6.addresses[index].address)) {
            interface_index = if_nametoindex(primary.ifname);
            if (interface_index == 0u) {
                errno = ENXIO;
                return -1;
            }
            break;
        }
    }

    storage = (RinIfaddrsStorage*)calloc(1u, sizeof(*storage));
    if (!storage) {
        errno = ENOMEM;
        return -1;
    }
    memcpy(storage->name, primary.ifname, name_length);
    storage->name[name_length] = '\0';
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
    for (index = 0u; index < ipv6.address_count; ++index) {
        const RinNetIPv6InterfaceAddressV1* source = &ipv6.addresses[index];
        struct ifaddrs* entry = &storage->entries[count];
        struct sockaddr_in6* address = &storage->addresses6[index];
        struct sockaddr_in6* netmask = &storage->netmasks6[index];
        entry->ifa_name = storage->name;
        entry->ifa_flags = flags;
        entry->ifa_addr = (struct sockaddr*)address;
        entry->ifa_netmask = (struct sockaddr*)netmask;
        address->sin6_family = AF_INET6;
        address->sin6_scope_id = rin_ifaddrs_ipv6_link_local(source->address)
            ? interface_index : 0u;
        memcpy(address->sin6_addr.s6_addr, source->address,
               sizeof(source->address));
        rin_ifaddrs_set_netmask6(netmask, source->prefix_length);
        ++count;
    }
    if (count == 0u) {
        storage->entries[0].ifa_name = storage->name;
        storage->entries[0].ifa_flags = flags;
        count = 1u;
    }
    for (index = 0u; index + 1u < count; ++index)
        storage->entries[index].ifa_next = &storage->entries[index + 1u];
    *ifap = &storage->entries[0];
    return 0;
}

void freeifaddrs(struct ifaddrs* ifap) {
    if (ifap) free(ifap);
}
