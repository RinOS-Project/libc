/* SPDX-License-Identifier: MIT */
#ifndef RIN_INTERNAL_ENVIRONMENT_RUNTIME_H
#define RIN_INTERNAL_ENVIRONMENT_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

/* 成功時はsnapshotの参照leaseを取得する。参照値はlease中のみ有効で、
 * 最後の参照後に必ず対応するread_endを呼ぶこと。 */
char** __rin_env_read_begin(unsigned int* epoch_out);
void __rin_env_read_end(unsigned int epoch);
unsigned int __rin_env_generation(void);
/* 呼出側がsnapshotを保護するread leaseを保持していること。返す値も
 * lease終了までしか使えない。 */
char* __rin_env_get_from_snapshot(char** snapshot, const char* name);
int __rin_env_is_secure(void);
char* __rin_env_get_secure_from_snapshot(char** snapshot, const char* name);

#ifdef __cplusplus
}
#endif

#endif /* RIN_INTERNAL_ENVIRONMENT_RUNTIME_H */
