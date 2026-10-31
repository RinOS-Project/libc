/* SPDX-License-Identifier: MIT */

#include "stdlib.h"
#include "sys/sysinfo.h"

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
