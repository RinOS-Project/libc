/*
 * RinOS libc - pthread_tls.c
 * POSIX スレッドローカルストレージ (pthread_key) + errno
 *
 * 責務: per-thread キーバリューストア（POSIX pthread_key_* API）
 *       スレッドローカル errno
 *
 * __thread 変数はコンパイラが .tbss/.tdata に配置し、
 * カーネルの CLONE ハンドラが新スレッドごとに独立 TLS ページを
 * 割り当てるため、各スレッドで自動的に独立した値を持つ。
 */

#include "pthread.h"

/* ═══════════════════════════════════════════════════════════════
 * スレッドローカル errno
 * ═══════════════════════════════════════════════════════════════*/

#ifdef RIN_LIBC_THREAD_LOCAL
static RIN_LIBC_THREAD_LOCAL int _errno_val = 0;
#else
static __thread int _errno_val = 0;
#endif

int* __errno_location(void) {
    return &_errno_val;
}

/* ═══════════════════════════════════════════════════════════════
 * Per-thread キーバリューストア
 * ═══════════════════════════════════════════════════════════════
 *
 * _pthread_tls[]           : __thread — スレッドごとに独立した値配列
 * _pthread_tls_key_state[] : static  — プロセス共有、キーの公開状態
 * _pthread_tls_generation : static  — プロセス共有、キー再利用世代
 * _pthread_tls_destructors : static  — プロセス共有、デストラクタ関数ポインタ
 * _pthread_tls_key_generation : __thread — 各スレッドが保持する世代
 */

#ifdef RIN_LIBC_THREAD_LOCAL
RIN_LIBC_THREAD_LOCAL void* _pthread_tls[PTHREAD_KEYS_MAX];
RIN_LIBC_THREAD_LOCAL unsigned int
    _pthread_tls_key_generation[PTHREAD_KEYS_MAX];
#else
__thread void* _pthread_tls[PTHREAD_KEYS_MAX];
__thread unsigned int _pthread_tls_key_generation[PTHREAD_KEYS_MAX];
#endif

/* A key must not become visible until its destructor and generation have been
 * published.  The intermediate state also prevents a concurrent creator from
 * reusing a slot while delete is still clearing its metadata. */
#define RIN_PTHREAD_KEY_FREE UINT32_C(0)
#define RIN_PTHREAD_KEY_INITIALIZING UINT32_C(1)
#define RIN_PTHREAD_KEY_LIVE UINT32_C(2)
#define RIN_PTHREAD_KEY_DELETING UINT32_C(3)

static volatile unsigned int _pthread_tls_key_state[PTHREAD_KEYS_MAX];
static volatile unsigned int _pthread_tls_generation[PTHREAD_KEYS_MAX];
static void (*volatile _pthread_tls_destructors[PTHREAD_KEYS_MAX])(void*);

static unsigned int pthread_key_state_load(unsigned int key)
{
    return __sync_fetch_and_add(&_pthread_tls_key_state[key], 0u);
}

static unsigned int pthread_key_generation_load(unsigned int key)
{
    return __sync_fetch_and_add(&_pthread_tls_generation[key], 0u);
}

static unsigned int pthread_key_generation_next(unsigned int key)
{
    unsigned int generation = __sync_add_and_fetch(
        &_pthread_tls_generation[key], 1u);
    /* Zero is reserved for an uninitialized per-thread value. */
    if (generation == 0u) {
        generation = __sync_add_and_fetch(
            &_pthread_tls_generation[key], 1u);
    }
    return generation;
}

/* ═══════════════════════════════════════════════════════════════
 * pthread_key_create — 新しいキーをアトミックに確保
 * ═══════════════════════════════════════════════════════════════*/

int pthread_key_create(pthread_key_t* key, void (*destructor)(void*)) {
    if (!key) return EINVAL;

    for (unsigned int i = 0; i < PTHREAD_KEYS_MAX; i++) {
        if (__sync_bool_compare_and_swap(&_pthread_tls_key_state[i],
                                         RIN_PTHREAD_KEY_FREE,
                                         RIN_PTHREAD_KEY_INITIALIZING)) {
            (void)pthread_key_generation_next(i);
            _pthread_tls_destructors[i] = destructor;
            __sync_synchronize();
            __sync_lock_test_and_set(&_pthread_tls_key_state[i],
                                     RIN_PTHREAD_KEY_LIVE);
            *key = i;
            return 0;
        }
    }
    return EAGAIN;  /* 全スロット使用中 */
}

/* ═══════════════════════════════════════════════════════════════
 * pthread_key_delete — キーを解放（各スレッドの値は触らない）
 *
 * POSIX: delete 後の getspecific/setspecific は未定義動作。
 *        値は残るが、キー番号は再利用可能になる。
 * ═══════════════════════════════════════════════════════════════*/

int pthread_key_delete(pthread_key_t key) {
    if (key >= PTHREAD_KEYS_MAX) return EINVAL;
    if (!__sync_bool_compare_and_swap(&_pthread_tls_key_state[key],
                                      RIN_PTHREAD_KEY_LIVE,
                                      RIN_PTHREAD_KEY_DELETING)) {
        return EINVAL;
    }

    /* Invalidate every thread's value before the slot can be reused.  A
     * thread that still has the old pointer observes a generation mismatch,
     * rather than passing it to a destructor belonging to the new key. */
    (void)pthread_key_generation_next(key);
    _pthread_tls_destructors[key] = NULL;
    __sync_synchronize();
    __sync_lock_test_and_set(&_pthread_tls_key_state[key],
                             RIN_PTHREAD_KEY_FREE);

    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * pthread_getspecific / pthread_setspecific
 * ═══════════════════════════════════════════════════════════════*/

void* pthread_getspecific(pthread_key_t key) {
    unsigned int generation;
    if (key >= PTHREAD_KEYS_MAX ||
        pthread_key_state_load(key) != RIN_PTHREAD_KEY_LIVE) return NULL;
    generation = pthread_key_generation_load(key);
    if (_pthread_tls_key_generation[key] != generation) return NULL;
    return _pthread_tls[key];
}

int pthread_setspecific(pthread_key_t key, const void* value) {
    unsigned int generation;
    if (key >= PTHREAD_KEYS_MAX ||
        pthread_key_state_load(key) != RIN_PTHREAD_KEY_LIVE) return EINVAL;
    generation = pthread_key_generation_load(key);
    _pthread_tls[key] = (void*)value;
    _pthread_tls_key_generation[key] = generation;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * __pthread_run_destructors — スレッド終了時のデストラクタ反復
 *
 * POSIX §pthread_key_create:
 *   スレッド終了時に、非 NULL な値を持つキーについて
 *   デストラクタを最大 PTHREAD_DESTRUCTOR_ITERATIONS 回反復して呼ぶ。
 *   各反復で 1 つでもデストラクタを呼んだら、もう 1 周する。
 *
 * 競合防御:
 *   - key state/generationを読む → delete/reuse後の値を無視
 *   - dtorをローカルにコピー → deleteとの競合でNULLを呼ばない
 *   - valを世代とともに確認 → 新しいkeyのdtorへ古い値を渡さない
 *   - _pthread_tls[k] = NULL をdtor呼び出し前に行う
 * ═══════════════════════════════════════════════════════════════*/

void __pthread_run_destructors(void) {
    for (int iter = 0; iter < PTHREAD_DESTRUCTOR_ITERATIONS; iter++) {
        int called = 0;

        for (unsigned int k = 0; k < PTHREAD_KEYS_MAX; k++) {
            unsigned int generation;
            unsigned int value_generation;
            void* val;
            void (*dtor)(void*);

            if (pthread_key_state_load(k) != RIN_PTHREAD_KEY_LIVE) continue;
            generation = pthread_key_generation_load(k);
            dtor = _pthread_tls_destructors[k];
            if (!dtor) continue;

            value_generation = _pthread_tls_key_generation[k];
            val = _pthread_tls[k];
            if (!val || value_generation != generation) continue;

            /* Re-check before consuming the value.  This closes the normal
             * delete/recreate window without requiring a global lock around
             * user destructors. */
            if (pthread_key_state_load(k) != RIN_PTHREAD_KEY_LIVE ||
                pthread_key_generation_load(k) != generation) continue;

            _pthread_tls[k] = NULL;
            _pthread_tls_key_generation[k] = 0u;
            dtor(val);
            called = 1;
        }

        if (!called) break;
    }
}
