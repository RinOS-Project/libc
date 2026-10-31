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
#include "sched.h"

/* ═══════════════════════════════════════════════════════════════
 * スレッドローカル errno
 * ═══════════════════════════════════════════════════════════════*/

#if !defined(RINSDK_SPLIT_BUILD)
#ifdef RIN_LIBC_THREAD_LOCAL
static RIN_LIBC_THREAD_LOCAL int _errno_val = 0;
#else
static __thread int _errno_val = 0;
#endif

int* __errno_location(void) {
    return &_errno_val;
}
#endif

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
#define RIN_PTHREAD_KEY_STATE_SHIFT 29u
#define RIN_PTHREAD_KEY_STATE_MASK UINT32_C(0xe0000000)
#define RIN_PTHREAD_KEY_DISPATCH_MASK UINT32_C(0x1fffffff)
#define RIN_PTHREAD_KEY_FREE (UINT32_C(0) << RIN_PTHREAD_KEY_STATE_SHIFT)
#define RIN_PTHREAD_KEY_INITIALIZING \
    (UINT32_C(1) << RIN_PTHREAD_KEY_STATE_SHIFT)
#define RIN_PTHREAD_KEY_LIVE (UINT32_C(2) << RIN_PTHREAD_KEY_STATE_SHIFT)
#define RIN_PTHREAD_KEY_DELETING \
    (UINT32_C(3) << RIN_PTHREAD_KEY_STATE_SHIFT)
#define RIN_PTHREAD_KEY_FINALIZING \
    (UINT32_C(4) << RIN_PTHREAD_KEY_STATE_SHIFT)
#define RIN_PTHREAD_KEY_CLOSING \
    (UINT32_C(5) << RIN_PTHREAD_KEY_STATE_SHIFT)

typedef struct _PthreadKeyDispatchFrame {
    unsigned int key;
    struct _PthreadKeyDispatchFrame* previous;
} _PthreadKeyDispatchFrame;

#ifdef RIN_LIBC_THREAD_LOCAL
static RIN_LIBC_THREAD_LOCAL _PthreadKeyDispatchFrame*
    _pthread_tls_current_dispatch;
#else
static __thread _PthreadKeyDispatchFrame* _pthread_tls_current_dispatch;
#endif

static volatile unsigned int _pthread_tls_key_state[PTHREAD_KEYS_MAX];
static volatile unsigned int _pthread_tls_generation[PTHREAD_KEYS_MAX];
static void (*volatile _pthread_tls_destructors[PTHREAD_KEYS_MAX])(void*);

static unsigned int pthread_key_state_load(unsigned int key)
{
    return __sync_fetch_and_add(&_pthread_tls_key_state[key], 0u) &
           RIN_PTHREAD_KEY_STATE_MASK;
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

static unsigned int pthread_key_state_word_load(unsigned int key)
{
    return __sync_fetch_and_add(&_pthread_tls_key_state[key], 0u);
}

static unsigned int pthread_key_dispatch_local_depth(unsigned int key)
{
    _PthreadKeyDispatchFrame* frame = _pthread_tls_current_dispatch;
    unsigned int depth = 0u;
    while (frame) {
        if (frame->key == key) ++depth;
        frame = frame->previous;
    }
    return depth;
}

static void pthread_key_finalize_delete(unsigned int key,
                                       unsigned int deleting_word)
{
    if (deleting_word != RIN_PTHREAD_KEY_DELETING) return;
    if (!__sync_bool_compare_and_swap(
            &_pthread_tls_key_state[key], deleting_word,
            RIN_PTHREAD_KEY_FINALIZING))
        return;
    _pthread_tls_destructors[key] = NULL;
    __sync_synchronize();
    __sync_lock_test_and_set(&_pthread_tls_key_state[key],
                             RIN_PTHREAD_KEY_FREE);
}

static void pthread_key_dispatch_release(unsigned int key)
{
    unsigned int word = pthread_key_state_word_load(key);
    for (;;) {
        unsigned int state = word & RIN_PTHREAD_KEY_STATE_MASK;
        unsigned int dispatches = word & RIN_PTHREAD_KEY_DISPATCH_MASK;
        unsigned int next;
        if ((state != RIN_PTHREAD_KEY_LIVE &&
             state != RIN_PTHREAD_KEY_CLOSING &&
             state != RIN_PTHREAD_KEY_DELETING) || dispatches == 0u)
            return;
        next = word - 1u;
        if (__sync_bool_compare_and_swap(
                &_pthread_tls_key_state[key], word, next)) {
            if (state == RIN_PTHREAD_KEY_DELETING &&
                (next & RIN_PTHREAD_KEY_DISPATCH_MASK) == 0u)
                pthread_key_finalize_delete(key, next);
            return;
        }
        word = pthread_key_state_word_load(key);
    }
}

static int pthread_key_dispatch_acquire(unsigned int key,
                                        unsigned int generation)
{
    unsigned int word = pthread_key_state_word_load(key);
    for (;;) {
        unsigned int dispatches = word & RIN_PTHREAD_KEY_DISPATCH_MASK;
        unsigned int current_word;
        if ((word & RIN_PTHREAD_KEY_STATE_MASK) != RIN_PTHREAD_KEY_LIVE ||
            dispatches == RIN_PTHREAD_KEY_DISPATCH_MASK)
            return 0;
        if (!__sync_bool_compare_and_swap(
                &_pthread_tls_key_state[key], word, word + 1u)) {
            word = pthread_key_state_word_load(key);
            continue;
        }
        current_word = pthread_key_state_word_load(key);
        if ((current_word & RIN_PTHREAD_KEY_STATE_MASK) ==
                RIN_PTHREAD_KEY_LIVE &&
            pthread_key_generation_load(key) == generation)
            return 1;
        pthread_key_dispatch_release(key);
        return 0;
    }
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
    unsigned int word;
    unsigned int local_depth;
    if (key >= PTHREAD_KEYS_MAX) return EINVAL;
    word = pthread_key_state_word_load(key);
    for (;;) {
        unsigned int deleting_word;
        if ((word & RIN_PTHREAD_KEY_STATE_MASK) != RIN_PTHREAD_KEY_LIVE)
            return EINVAL;
        deleting_word = RIN_PTHREAD_KEY_CLOSING |
                        (word & RIN_PTHREAD_KEY_DISPATCH_MASK);
        if (__sync_bool_compare_and_swap(
                &_pthread_tls_key_state[key], word, deleting_word))
            break;
        word = pthread_key_state_word_load(key);
    }

    /* Closing the state gate prevents new destructor dispatch claims.  The
     * generation change invalidates every thread's old TLS value. */
    (void)pthread_key_generation_next(key);
    __sync_synchronize();
    word = pthread_key_state_word_load(key);
    for (;;) {
        unsigned int deleting_word;
        if ((word & RIN_PTHREAD_KEY_STATE_MASK) != RIN_PTHREAD_KEY_CLOSING)
            return 0;
        deleting_word = RIN_PTHREAD_KEY_DELETING |
                        (word & RIN_PTHREAD_KEY_DISPATCH_MASK);
        if (__sync_bool_compare_and_swap(
                &_pthread_tls_key_state[key], word, deleting_word))
            break;
        word = pthread_key_state_word_load(key);
    }
    local_depth = pthread_key_dispatch_local_depth(key);
    for (;;) {
        unsigned int state;
        unsigned int dispatches;
        word = pthread_key_state_word_load(key);
        state = word & RIN_PTHREAD_KEY_STATE_MASK;
        dispatches = word & RIN_PTHREAD_KEY_DISPATCH_MASK;
        if (state == RIN_PTHREAD_KEY_DELETING) {
            if (dispatches == 0u) {
                pthread_key_finalize_delete(key, word);
                continue;
            }
            /* A destructor may delete its own key. Wait for callbacks on
             * other threads, but retain this key until our frame exits. */
            if (local_depth != 0u && dispatches <= local_depth) return 0;
        } else if (state != RIN_PTHREAD_KEY_FINALIZING) {
            /* Another finalizer completed the old generation. The slot may
             * already belong to a new key, which this delete must not touch. */
            return 0;
        }
        (void)sched_yield();
    }

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
            value_generation = _pthread_tls_key_generation[k];
            val = _pthread_tls[k];
            if (!val || value_generation != generation) continue;

            /* Claim this dispatch atomically with key deletion.  The active
             * count keeps the function pointer valid until the callback ends. */
            if (!pthread_key_dispatch_acquire(k, generation)) continue;
            dtor = _pthread_tls_destructors[k];
            if (!dtor || pthread_key_state_load(k) != RIN_PTHREAD_KEY_LIVE ||
                pthread_key_generation_load(k) != generation ||
                _pthread_tls_key_generation[k] != generation ||
                _pthread_tls[k] != val) {
                pthread_key_dispatch_release(k);
                continue;
            }

            _pthread_tls[k] = NULL;
            _pthread_tls_key_generation[k] = 0u;
            _PthreadKeyDispatchFrame frame;
            frame.key = k;
            frame.previous = _pthread_tls_current_dispatch;
            _pthread_tls_current_dispatch = &frame;
            dtor(val);
            _pthread_tls_current_dispatch = frame.previous;
            pthread_key_dispatch_release(k);
            called = 1;
        }

        if (!called) break;
    }
}
