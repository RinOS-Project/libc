/* SPDX-License-Identifier: MIT */
#ifndef RIN_LIBC_SYS_RIN_PERF_EVENT_H
#define RIN_LIBC_SYS_RIN_PERF_EVENT_H

#include <stdint.h>
#include <errno.h>
#include <rin/perf_event.h>
#include <sys/syscall.h>

static inline long rin_perf_event_control(RinPerfEventControlV1* request) {
    return syscall(RIN_SYS_PERF_EVENT_CONTROL_V1, request);
}

static inline long rin_perf_event_open(uint32_t event_id, uint64_t* handle_out) {
    RinPerfEventControlV1 request = {
        sizeof(RinPerfEventControlV1), RIN_PERF_EVENT_CONTROL_VERSION,
        RIN_PERF_EVENT_CONTROL_OPEN, event_id, 0u, 0u, 0u, 0u
    };
    long result;
    if (handle_out == 0) {
        errno = EINVAL;
        return -1;
    }
    result = rin_perf_event_control(&request);
    if (result == 0) *handle_out = request.handle;
    return result;
}

static inline long rin_perf_event_read(uint64_t handle, uint64_t* value_out) {
    RinPerfEventControlV1 request = {
        sizeof(RinPerfEventControlV1), RIN_PERF_EVENT_CONTROL_VERSION,
        RIN_PERF_EVENT_CONTROL_READ, 0u, 0u, 0u, handle, 0u
    };
    long result;
    if (value_out == 0) {
        errno = EINVAL;
        return -1;
    }
    result = rin_perf_event_control(&request);
    if (result == 0) *value_out = request.value;
    return result;
}

static inline long rin_perf_event_reset(uint64_t handle) {
    RinPerfEventControlV1 request = {
        sizeof(RinPerfEventControlV1), RIN_PERF_EVENT_CONTROL_VERSION,
        RIN_PERF_EVENT_CONTROL_RESET, 0u, 0u, 0u, handle, 0u
    };
    return rin_perf_event_control(&request);
}

static inline long rin_perf_event_enable(uint64_t handle) {
    RinPerfEventControlV1 request = {
        sizeof(RinPerfEventControlV1), RIN_PERF_EVENT_CONTROL_VERSION,
        RIN_PERF_EVENT_CONTROL_ENABLE, 0u, 0u, 0u, handle, 0u
    };
    return rin_perf_event_control(&request);
}

static inline long rin_perf_event_disable(uint64_t handle) {
    RinPerfEventControlV1 request = {
        sizeof(RinPerfEventControlV1), RIN_PERF_EVENT_CONTROL_VERSION,
        RIN_PERF_EVENT_CONTROL_DISABLE, 0u, 0u, 0u, handle, 0u
    };
    return rin_perf_event_control(&request);
}

static inline long rin_perf_event_close(uint64_t handle) {
    RinPerfEventControlV1 request = {
        sizeof(RinPerfEventControlV1), RIN_PERF_EVENT_CONTROL_VERSION,
        RIN_PERF_EVENT_CONTROL_CLOSE, 0u, 0u, 0u, handle, 0u
    };
    return rin_perf_event_control(&request);
}

#endif
