/* SPDX-License-Identifier: MIT */
/* Process-wide C rand/srand owner.  This deterministic generator is not
 * suitable for keys, nonces, tokens, or any other cryptographic purpose. */

#define RIN_LIBC_RAND_MAX 0x7fffffffU

#ifdef __cplusplus
extern "C" {
#endif

static unsigned int rin_libc_rand_state = 1U;

static unsigned int rin_libc_rand_next(unsigned int state)
{
    return state * 1664525U + 1013904223U;
}

int rand(void)
{
    unsigned int observed = __atomic_load_n(
        &rin_libc_rand_state, __ATOMIC_RELAXED);
    unsigned int next;

    do {
        next = rin_libc_rand_next(observed);
    } while (!__atomic_compare_exchange_n(
        &rin_libc_rand_state, &observed, next, 1,
        __ATOMIC_RELAXED, __ATOMIC_RELAXED));
    return (int)(next & RIN_LIBC_RAND_MAX);
}

void srand(unsigned int seed)
{
    __atomic_store_n(&rin_libc_rand_state, seed, __ATOMIC_RELAXED);
}

/* POSIX random(3) uses the same non-cryptographic process-wide generator
 * for this freestanding libc.  Keep the aliases in one state owner so code
 * that mixes rand() and random() observes deterministic, coherent progress. */
long random(void)
{
    return (long)rand();
}

void srandom(unsigned int seed)
{
    srand(seed);
}

#ifdef __cplusplus
}
#endif
