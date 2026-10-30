/* SPDX-License-Identifier: MIT */
#ifndef RIN_LIBC_NET_INTERFACE_SNAPSHOT_H
#define RIN_LIBC_NET_INTERFACE_SNAPSHOT_H

#include "../../../src/api/network_interface_snapshot_v1.h"

int rin_net_get_interface_ifaddrs_snapshot(
    RinNetIfaddrsSnapshotListV2* output);
int rin_net_addrconfig_snapshot(int* ipv4_configured,
                                int* ipv6_configured);

#endif
