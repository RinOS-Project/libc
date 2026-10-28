/* SPDX-License-Identifier: MIT */
#ifndef RIN_DLERROR_RUNTIME_H
#define RIN_DLERROR_RUNTIME_H

#define RIN_DLERROR_BUFFER_CAPACITY 256u

typedef struct RinDlerrorState {
    char buffer[RIN_DLERROR_BUFFER_CAPACITY];
    char kernel_buffer[RIN_DLERROR_BUFFER_CAPACITY];
    int pending;
} RinDlerrorState;

#ifdef __cplusplus
extern "C" {
#endif
RinDlerrorState* __rin_dlerror_state(void);
#ifdef __cplusplus
}
#endif

#endif /* RIN_DLERROR_RUNTIME_H */
