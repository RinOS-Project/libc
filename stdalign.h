#ifndef _STDALIGN_H
#define _STDALIGN_H

#if defined(__cplusplus)
#define alignas alignas
#define alignof alignof
#define __alignas_is_defined 1
#define __alignof_is_defined 1
#else
#define alignas _Alignas
#define alignof _Alignof
#define __alignas_is_defined 1
#define __alignof_is_defined 1
#endif

#endif /* _STDALIGN_H */
