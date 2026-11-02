// SPDX-License-Identifier: MIT
#include <errno.h>
#include <net/if.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>

#include <rin/net/netif_abi.h>
#include "rin_net_interface_snapshot.h"

int rin_net_get_interface_statistics(RinNetInterfaceStatisticsV1* out)
{
    if (!out) {
        errno = EINVAL;
        return -1;
    }
    if (syscall(SYS_NET_STATISTICS, out) != 0)
        return -1;
    if (out->version != RIN_NET_STATISTICS_VERSION ||
        out->struct_size != sizeof(*out) || out->device_generation == 0u) {
        errno = EPROTO;
        return -1;
    }
    return 0;
}

int rin_net_get_ip_global_statistics(
    uint32_t address_family, RinNetIpGlobalStatisticsV1* out)
{
    if (!out) {
        errno = EINVAL;
        return -1;
    }
    if (address_family != RIN_NET_IP_GLOBAL_STATISTICS_AF_IPV4 &&
        address_family != RIN_NET_IP_GLOBAL_STATISTICS_AF_IPV6) {
        errno = EAFNOSUPPORT;
        return -1;
    }
    if (syscall(SYS_NET_IP_GLOBAL_STATISTICS, out, address_family) != 0)
        return -1;
    if (out->version != RIN_NET_IP_GLOBAL_STATISTICS_VERSION ||
        out->struct_size != sizeof(*out) ||
        out->device_generation == 0u ||
        out->address_family != address_family ||
        (out->supported_flags & ~RIN_NET_IP_GLOBAL_STATISTICS_KNOWN_FLAGS) != 0u) {
        errno = EPROTO;
        return -1;
    }
    return 0;
}

int rin_net_get_udp_global_statistics(
    uint32_t address_family, RinNetUdpGlobalStatisticsV1* out)
{
    if (!out) {
        errno = EINVAL;
        return -1;
    }
    if (address_family != RIN_NET_UDP_GLOBAL_STATISTICS_AF_IPV4 &&
        address_family != RIN_NET_UDP_GLOBAL_STATISTICS_AF_IPV6) {
        errno = EAFNOSUPPORT;
        return -1;
    }
    if (syscall(SYS_NET_UDP_GLOBAL_STATISTICS, out, address_family) != 0)
        return -1;
    if (out->version != RIN_NET_UDP_GLOBAL_STATISTICS_VERSION ||
        out->struct_size != sizeof(*out) ||
        out->device_generation == 0u ||
        out->address_family != address_family ||
        (out->supported_flags & ~RIN_NET_UDP_GLOBAL_STATISTICS_KNOWN_FLAGS) != 0u) {
        errno = EPROTO;
        return -1;
    }
    return 0;
}

int rin_net_get_tcp_global_statistics(
    uint32_t address_family, RinNetTcpGlobalStatisticsV1* out)
{
    if (!out) {
        errno = EINVAL;
        return -1;
    }
    if (address_family != RIN_NET_TCP_GLOBAL_STATISTICS_AF_IPV4 &&
        address_family != RIN_NET_TCP_GLOBAL_STATISTICS_AF_IPV6) {
        errno = EAFNOSUPPORT;
        return -1;
    }
    if (syscall(SYS_NET_TCP_GLOBAL_STATISTICS, out, address_family) != 0)
        return -1;
    if (out->version != RIN_NET_TCP_GLOBAL_STATISTICS_VERSION ||
        out->struct_size != sizeof(*out) ||
        out->device_generation == 0u ||
        out->address_family != address_family ||
        (out->supported_flags & ~RIN_NET_TCP_GLOBAL_STATISTICS_KNOWN_FLAGS) != 0u) {
        errno = EPROTO;
        return -1;
    }
    return 0;
}

int rin_net_get_icmp_global_statistics(
    uint32_t address_family, RinNetIcmpGlobalStatisticsV1* out)
{
    if (!out) {
        errno = EINVAL;
        return -1;
    }
    if (address_family != RIN_NET_ICMP_GLOBAL_STATISTICS_AF_IPV4 &&
        address_family != RIN_NET_ICMP_GLOBAL_STATISTICS_AF_IPV6) {
        errno = EAFNOSUPPORT;
        return -1;
    }
    if (syscall(SYS_NET_ICMP_GLOBAL_STATISTICS, out, address_family) != 0)
        return -1;
    if (out->version != RIN_NET_ICMP_GLOBAL_STATISTICS_VERSION ||
        out->struct_size != sizeof(*out) ||
        out->device_generation == 0u ||
        out->address_family != address_family ||
        (out->supported_flags & ~RIN_NET_ICMP_GLOBAL_STATISTICS_KNOWN_FLAGS) != 0u) {
        errno = EPROTO;
        return -1;
    }
    return 0;
}

int rin_net_get_icmp_global_statistics_v2(
    uint32_t address_family, RinNetIcmpGlobalStatisticsV2* out)
{
    if (!out) {
        errno = EINVAL;
        return -1;
    }
    if (address_family != RIN_NET_ICMP_GLOBAL_STATISTICS_AF_IPV4 &&
        address_family != RIN_NET_ICMP_GLOBAL_STATISTICS_AF_IPV6) {
        errno = EAFNOSUPPORT;
        return -1;
    }
    if (syscall(SYS_NET_ICMP_GLOBAL_STATISTICS_V2, out, address_family) != 0)
        return -1;
    if (out->version != RIN_NET_ICMP_GLOBAL_STATISTICS_V2_VERSION ||
        out->struct_size != sizeof(*out) ||
        out->device_generation == 0u ||
        out->address_family != address_family ||
        (out->supported_flags & ~RIN_NET_ICMP_GLOBAL_STATISTICS_V2_KNOWN_FLAGS) != 0u) {
        errno = EPROTO;
        return -1;
    }
    return 0;
}

static int netif_snapshot(RinNetIfaddrsSnapshotListV2* snapshot)
{
    if (snapshot == NULL) {
        errno = EINVAL;
        return -1;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    if (syscall(RIN_SYS_NETWORK_INTERFACE_IFADDRS_SNAPSHOT_V2,
                snapshot) != 0)
        return -1;
    if (!rin_net_ifaddrs_snapshot_list_v2_valid(snapshot)) {
        errno = EPROTO;
        return -1;
    }
    return 0;
}

int rin_net_get_interface_ifaddrs_snapshot(
    RinNetIfaddrsSnapshotListV2* output)
{
    return netif_snapshot(output);
}

unsigned int if_nametoindex(const char* ifname)
{
    RinNetIfaddrsSnapshotListV2 snapshot;
    unsigned int index;

    if (!ifname) {
        errno = EINVAL;
        return 0u;
    }
    if (ifname[0] == '\0') {
        errno = ENXIO;
        return 0u;
    }
    if (netif_snapshot(&snapshot) != 0)
        return 0u;
    for (index = 0u; index < snapshot.count; ++index) {
        const RinNetIfaddrsInterfaceV2* entry = &snapshot.interfaces[index];
        if (strcmp(ifname, entry->addresses.ifname) == 0)
            return (unsigned int)entry->addresses.interface_id;
    }
    errno = ENXIO;
    return 0u;
}

char* if_indextoname(unsigned int ifindex, char* ifname)
{
    RinNetIfaddrsSnapshotListV2 snapshot;
    unsigned int index;

    if (!ifname || ifindex == 0u) {
        errno = EINVAL;
        return (char*)0;
    }
    if (netif_snapshot(&snapshot) != 0)
        return (char*)0;
    for (index = 0u; index < snapshot.count; ++index) {
        const RinNetIfaddrsInterfaceV2* entry = &snapshot.interfaces[index];
        if (entry->addresses.interface_id == (uint64_t)ifindex) {
            memcpy(ifname, entry->addresses.ifname, IF_NAMESIZE);
            return ifname;
        }
    }
    errno = ENXIO;
    return (char*)0;
}

struct if_nameindex* if_nameindex(void)
{
    RinNetIfaddrsSnapshotListV2 snapshot;
    struct if_nameindex* result;
    char* next_name;
    size_t names_size = 0u;
    size_t entries_size;
    unsigned int index;

    if (netif_snapshot(&snapshot) != 0)
        return (struct if_nameindex*)0;
    for (index = 0u; index < snapshot.count; ++index) {
        size_t length = strlen(snapshot.interfaces[index].addresses.ifname);
        if (length == 0u || length >= IF_NAMESIZE ||
            names_size > SIZE_MAX - length - 1u) {
            errno = EPROTO;
            return (struct if_nameindex*)0;
        }
        names_size += length + 1u;
    }
    entries_size = ((size_t)snapshot.count + 1u) * sizeof(*result);
    if (entries_size > SIZE_MAX - names_size) {
        errno = ENOMEM;
        return (struct if_nameindex*)0;
    }
    result = (struct if_nameindex*)calloc(1u, entries_size + names_size);
    if (result == NULL) {
        errno = ENOMEM;
        return (struct if_nameindex*)0;
    }
    next_name = (char*)result + entries_size;
    for (index = 0u; index < snapshot.count; ++index) {
        const RinNetIfaddrsInterfaceV2* entry = &snapshot.interfaces[index];
        size_t length = strlen(entry->addresses.ifname) + 1u;
        result[index].if_index = (unsigned int)entry->addresses.interface_id;
        result[index].if_name = next_name;
        memcpy(next_name, entry->addresses.ifname, length);
        next_name += length;
    }
    result[snapshot.count].if_index = 0u;
    result[snapshot.count].if_name = (char*)0;
    return result;
}

void if_freenameindex(struct if_nameindex* list)
{
    free(list);
}

int rin_net_addrconfig_snapshot(int* ipv4_configured,
                                int* ipv6_configured)
{
    RinNetIfaddrsSnapshotListV2 snapshot;
    unsigned int index;
    if (ipv4_configured == NULL || ipv6_configured == NULL) {
        errno = EINVAL;
        return -1;
    }
    *ipv4_configured = 0;
    *ipv6_configured = 0;
    if (netif_snapshot(&snapshot) != 0)
        return -1;
    for (index = 0u; index < snapshot.count; ++index) {
        const RinNetIfaddrsInterfaceV2* interface =
            &snapshot.interfaces[index];
        const uint16_t flags = interface->addresses.flags;
        if ((flags & RIN_NET_IFADDRS_INTERFACE_FLAG_IPV4_CONFIGURED) != 0u &&
            interface->addresses.ipv4_address[0] != 127u)
            *ipv4_configured = 1;
        if (interface->interface_type !=
                RIN_NET_IFADDRS_V2_INTERFACE_LOOPBACK &&
            (flags & RIN_NET_IFADDRS_INTERFACE_FLAG_IPV6_CONFIGURED) != 0u)
            *ipv6_configured = 1;
    }
    return 0;
}
