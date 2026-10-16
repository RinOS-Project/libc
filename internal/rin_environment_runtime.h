/* SPDX-License-Identifier: MIT */
#ifndef RIN_INTERNAL_ENVIRONMENT_RUNTIME_H
#define RIN_INTERNAL_ENVIRONMENT_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

char** __rin_env_read_begin(unsigned int* epoch_out);
void __rin_env_read_end(unsigned int epoch);
/* The caller must hold a read epoch that protects snapshot. */
char* __rin_env_get_from_snapshot(char** snapshot, const char* name);
int __rin_env_is_secure(void);
char* __rin_env_get_secure_from_snapshot(char** snapshot, const char* name);

#ifdef __cplusplus
}
#endif

#endif /* RIN_INTERNAL_ENVIRONMENT_RUNTIME_H */
