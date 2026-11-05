/* Public UDP-specific socket options. */
#ifndef _NETINET_UDP_H
#define _NETINET_UDP_H 1

#include <netinet/in.h>

#ifndef UDP_SEGMENT
#define UDP_SEGMENT 103
#endif

#ifndef UDP_GRO
#define UDP_GRO 104
#endif

#endif /* _NETINET_UDP_H */
