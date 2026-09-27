// SPDX-License-Identifier: MIT
#include <errno.h>
#include <net/if.h>
#include <sys/syscall.h>

#include <rin/net/netif_abi.h>

extern int rin_net_get_primary_info(RinNetPrimaryInfo* out);

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

static int netif_snapshot(RinNetPrimaryInfo* info)
{
    int status;
    unsigned int length = 0u;

    if (!info) {
        errno = EINVAL;
        return -1;
    }
    status = rin_net_get_primary_info(info);
    if (status != 0) {
        errno = status < 0 ? -status : EIO;
        return -1;
    }
    if ((info->flags & ~RIN_NETINFO_KNOWN_FLAGS) != 0u) {
        errno = EPROTO;
        return -1;
    }
    if ((info->flags & RIN_NETINFO_FLAG_DEVICE_READY) == 0u) {
        if (info->flags != 0u || info->device_generation != 0u ||
            info->ifname[0] != '\0') {
            errno = EPROTO;
            return -1;
        }
        errno = ENXIO;
        return -1;
    }
    if (info->device_generation == 0u) {
        errno = EPROTO;
        return -1;
    }
    while (length < RIN_NETINFO_IFNAME_MAX && info->ifname[length] != '\0')
        ++length;
    if (length == 0u || length == RIN_NETINFO_IFNAME_MAX) {
        errno = EPROTO;
        return -1;
    }
    return (int)length;
}

unsigned int if_nametoindex(const char* ifname)
{
    RinNetPrimaryInfo info;
    int length;
    unsigned int index;

    if (!ifname) {
        errno = EINVAL;
        return 0u;
    }
    length = netif_snapshot(&info);
    if (length < 0)
        return 0u;
    for (index = 0u; index < (unsigned int)length; ++index) {
        if (ifname[index] == '\0' || ifname[index] != info.ifname[index]) {
            errno = ENXIO;
            return 0u;
        }
    }
    if (ifname[index] != '\0') {
        errno = ENXIO;
        return 0u;
    }
    return (unsigned int)info.device_generation;
}

char* if_indextoname(unsigned int ifindex, char* ifname)
{
    RinNetPrimaryInfo info;
    int length;
    int index;

    if (!ifname || ifindex == 0u) {
        errno = EINVAL;
        return (char*)0;
    }
    length = netif_snapshot(&info);
    if (length < 0)
        return (char*)0;
    if (ifindex != (unsigned int)info.device_generation) {
        errno = ENXIO;
        return (char*)0;
    }
    for (index = 0; index <= length; ++index)
        ifname[index] = info.ifname[index];
    return ifname;
}
