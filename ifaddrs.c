/* SPDX-License-Identifier: MIT */
/* POSIX-shaped ifaddrs view over one generation-checked kernel snapshot. */
#include "ifaddrs.h"

#include "errno.h"
#include "net/if.h"
#include "netinet/in.h"
#include "stdlib.h"
#include "string.h"
#include "rin_net_interface_snapshot.h"

#define RIN_IFADDRS_MAX_ENTRIES \
    (RIN_NET_IFADDRS_SNAPSHOT_MAX_INTERFACES_V2 * \
     (RIN_NET_IFADDRS_SNAPSHOT_MAX_IPV6_ADDRESSES + 1u))

typedef struct RinIfaddrsStorage {
    struct ifaddrs entries[RIN_IFADDRS_MAX_ENTRIES];
    char names[RIN_NET_IFADDRS_SNAPSHOT_MAX_INTERFACES_V2][IF_NAMESIZE];
    struct sockaddr_in addresses4[RIN_NET_IFADDRS_SNAPSHOT_MAX_INTERFACES_V2];
    struct sockaddr_in netmasks4[RIN_NET_IFADDRS_SNAPSHOT_MAX_INTERFACES_V2];
    struct sockaddr_in broadcast4[RIN_NET_IFADDRS_SNAPSHOT_MAX_INTERFACES_V2];
    struct sockaddr_in peer4[RIN_NET_IFADDRS_SNAPSHOT_MAX_INTERFACES_V2];
    struct sockaddr_in6 addresses6[RIN_NET_IFADDRS_SNAPSHOT_MAX_INTERFACES_V2]
                                  [RIN_NET_IFADDRS_SNAPSHOT_MAX_IPV6_ADDRESSES];
    struct sockaddr_in6 netmasks6[RIN_NET_IFADDRS_SNAPSHOT_MAX_INTERFACES_V2]
                                  [RIN_NET_IFADDRS_SNAPSHOT_MAX_IPV6_ADDRESSES];
} RinIfaddrsStorage;

static unsigned int rin_ifaddrs_interface_flags(
    const RinNetIfaddrsInterfaceV2* interface)
{
    unsigned int flags = 0u;
    if ((interface->flags & RIN_NET_IFADDRS_V2_FLAG_ADMIN_UP) != 0u)
        flags |= IFF_UP;
    if ((interface->capabilities & RIN_NET_IFADDRS_V2_CAP_BROADCAST) != 0u)
        flags |= IFF_BROADCAST;
    if ((interface->capabilities & RIN_NET_IFADDRS_V2_CAP_MULTICAST) != 0u)
        flags |= IFF_MULTICAST;
    if ((interface->flags & RIN_NET_IFADDRS_V2_FLAG_LINK_UP) != 0u)
        flags |= IFF_RUNNING;
    if (interface->interface_type == RIN_NET_IFADDRS_V2_INTERFACE_LOOPBACK)
        flags |= IFF_LOOPBACK;
    if (interface->interface_type ==
        RIN_NET_IFADDRS_V2_INTERFACE_POINT_TO_POINT)
        flags |= IFF_POINTOPOINT;
    return flags;
}

static unsigned int rin_ifaddrs_ipv4_prefix_length(const uint8_t mask[4])
{
    unsigned int prefix = 0u;
    unsigned int index;
    for (index = 0u; index < 4u; ++index) {
        uint8_t bit;
        for (bit = 0x80u; bit != 0u; bit >>= 1u) {
            if ((mask[index] & bit) == 0u) return prefix;
            ++prefix;
        }
    }
    return prefix;
}

static void rin_ifaddrs_set_netmask6(struct sockaddr_in6* mask,
                                     uint8_t prefix_length)
{
    unsigned int bit;
    mask->sin6_family = AF_INET6;
    for (bit = 0u; bit < prefix_length; ++bit)
        mask->sin6_addr.s6_addr[bit / 8u] |=
            (uint8_t)(0x80u >> (bit % 8u));
}

int getifaddrs(struct ifaddrs** ifap)
{
    RinNetIfaddrsSnapshotListV2 snapshot;
    RinIfaddrsStorage* storage;
    unsigned int count = 0u;
    unsigned int interface_index;

    if (ifap == NULL) {
        errno = EFAULT;
        return -1;
    }
    *ifap = (struct ifaddrs*)0;
    if (rin_net_get_interface_ifaddrs_snapshot(&snapshot) != 0)
        return -1;
    if (snapshot.count == 0u)
        return 0;

    storage = (RinIfaddrsStorage*)calloc(1u, sizeof(*storage));
    if (storage == NULL) {
        errno = ENOMEM;
        return -1;
    }
    for (interface_index = 0u; interface_index < snapshot.count;
         ++interface_index) {
        const RinNetIfaddrsInterfaceV2* source =
            &snapshot.interfaces[interface_index];
        const RinNetIfaddrsInterfaceV1* addresses = &source->addresses;
        const unsigned int flags = rin_ifaddrs_interface_flags(source);
        unsigned int address_index;
        unsigned int interface_entries = 0u;
        memcpy(storage->names[interface_index], addresses->ifname, IF_NAMESIZE);

        if ((addresses->flags &
             RIN_NET_IFADDRS_INTERFACE_FLAG_IPV4_CONFIGURED) != 0u) {
            struct ifaddrs* entry = &storage->entries[count++];
            struct sockaddr_in* address =
                &storage->addresses4[interface_index];
            struct sockaddr_in* netmask =
                &storage->netmasks4[interface_index];
            entry->ifa_name = storage->names[interface_index];
            entry->ifa_flags = flags;
            entry->ifa_addr = (struct sockaddr*)address;
            entry->ifa_netmask = (struct sockaddr*)netmask;
            address->sin_family = AF_INET;
            memcpy(&address->sin_addr.s_addr, addresses->ipv4_address, 4u);
            netmask->sin_family = AF_INET;
            memcpy(&netmask->sin_addr.s_addr, addresses->ipv4_netmask, 4u);
            if ((source->capabilities &
                 RIN_NET_IFADDRS_V2_CAP_POINT_TO_POINT) != 0u) {
                struct sockaddr_in* peer = &storage->peer4[interface_index];
                peer->sin_family = AF_INET;
                memcpy(&peer->sin_addr.s_addr, source->ipv4_peer, 4u);
                entry->ifa_dstaddr = (struct sockaddr*)peer;
            } else if ((source->capabilities &
                        RIN_NET_IFADDRS_V2_CAP_BROADCAST) != 0u &&
                       rin_ifaddrs_ipv4_prefix_length(
                           addresses->ipv4_netmask) <= 30u) {
                struct sockaddr_in* broadcast =
                    &storage->broadcast4[interface_index];
                broadcast->sin_family = AF_INET;
                for (unsigned int byte = 0u; byte < 4u; ++byte)
                    ((uint8_t*)&broadcast->sin_addr.s_addr)[byte] =
                        addresses->ipv4_address[byte] |
                        (uint8_t)~addresses->ipv4_netmask[byte];
                entry->ifa_broadaddr = (struct sockaddr*)broadcast;
            }
            ++interface_entries;
        }
        for (address_index = 0u; address_index < addresses->address_count;
             ++address_index) {
            const uint8_t* source_address =
                addresses->addresses6[address_index];
            struct ifaddrs* entry = &storage->entries[count++];
            struct sockaddr_in6* address =
                &storage->addresses6[interface_index][address_index];
            struct sockaddr_in6* netmask =
                &storage->netmasks6[interface_index][address_index];
            entry->ifa_name = storage->names[interface_index];
            entry->ifa_flags = flags;
            entry->ifa_addr = (struct sockaddr*)address;
            entry->ifa_netmask = (struct sockaddr*)netmask;
            address->sin6_family = AF_INET6;
            if (addresses->address_flags6[address_index] ==
                RIN_NET_IFADDRS_ADDRESS_FLAG_LINK_LOCAL)
                address->sin6_scope_id =
                    (uint32_t)addresses->interface_id;
            memcpy(address->sin6_addr.s6_addr, source_address, 16u);
            rin_ifaddrs_set_netmask6(
                netmask, addresses->prefix_lengths6[address_index]);
            ++interface_entries;
        }
        if (interface_entries == 0u) {
            struct ifaddrs* entry = &storage->entries[count++];
            entry->ifa_name = storage->names[interface_index];
            entry->ifa_flags = flags;
        }
    }
    for (interface_index = 0u; interface_index < count; ++interface_index)
        storage->entries[interface_index].ifa_next =
            interface_index + 1u < count
                ? &storage->entries[interface_index + 1u]
                : (struct ifaddrs*)0;
    *ifap = count != 0u ? &storage->entries[0] : (struct ifaddrs*)0;
    return 0;
}

void freeifaddrs(struct ifaddrs* ifap)
{
    if (ifap != NULL)
        free(ifap);
}
