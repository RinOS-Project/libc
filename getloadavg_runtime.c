/* SPDX-License-Identifier: MIT */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include "stdlib.h"
#include "limits.h"
#include "sys/sysinfo.h"
#include "unistd.h"

static int rin_get_nprocs_sysconf(int name) {
    long count = sysconf(name);
    if (count < 0) return -1;
    if (count > INT_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    return (int)count;
}

int get_nprocs(void) {
    return rin_get_nprocs_sysconf(_SC_NPROCESSORS_ONLN);
}

int get_nprocs_conf(void) {
    return rin_get_nprocs_sysconf(_SC_NPROCESSORS_CONF);
}

long get_phys_pages(void) {
    return sysconf(_SC_PHYS_PAGES);
}

long get_avphys_pages(void) {
    return sysconf(_SC_AVPHYS_PAGES);
}

int getloadavg(double loadavg[], int nelem) {
    struct sysinfo information;
    int count;
    int index;
    if (nelem <= 0) return 0;
    if (loadavg == NULL) {
        errno = EFAULT;
        return -1;
    }
    if (sysinfo(&information) != 0) return -1;
    count = nelem < 3 ? nelem : 3;
    for (index = 0; index < count; ++index)
        loadavg[index] = (double)information.loads[index] / 65536.0;
    return count;
}
