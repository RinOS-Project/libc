/* SPDX-License-Identifier: MIT */
#ifndef RIN_ENVIRONMENT_H
#define RIN_ENVIRONMENT_H

/* Stable read lease for consumers that must resolve several environment
 * names from one process-wide environment generation.  The storage is owned
 * by libc and remains valid only until the matching end call. */
typedef struct RinEnvironmentSnapshotV1 {
    char** entries;
    unsigned int epoch;
    unsigned int active;
} RinEnvironmentSnapshotV1;

#ifdef __cplusplus
extern "C" {
#endif

/* These owner functions predate this public contract.  Keep their ABI and
 * names stable while allowing other library submodules to use the lease. */
char** __rin_env_read_begin(unsigned int* epoch_out);
void __rin_env_read_end(unsigned int epoch);
char* __rin_env_get_from_snapshot(char** snapshot, const char* name);

#ifdef __cplusplus
}
#endif

static inline void rin_environment_snapshot_begin_v1(
    RinEnvironmentSnapshotV1* snapshot) {
    if (!snapshot) return;
    snapshot->entries = __rin_env_read_begin(&snapshot->epoch);
    snapshot->active = 1u;
}

static inline char const* rin_environment_snapshot_get_v1(
    RinEnvironmentSnapshotV1 const* snapshot, char const* name) {
    if (!snapshot || !snapshot->active) return (char const*)0;
    return __rin_env_get_from_snapshot(snapshot->entries, name);
}

static inline void rin_environment_snapshot_end_v1(
    RinEnvironmentSnapshotV1* snapshot) {
    if (!snapshot || !snapshot->active) return;
    __rin_env_read_end(snapshot->epoch);
    snapshot->entries = (char**)0;
    snapshot->epoch = 0u;
    snapshot->active = 0u;
}

#endif /* RIN_ENVIRONMENT_H */
