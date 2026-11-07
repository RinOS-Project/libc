/* SPDX-License-Identifier: MIT */
/* Process-wide C rand/srand and BSD random-state owners.  These deterministic
 * generators are not suitable for keys, nonces, tokens, or other
 * cryptographic purposes. */

#define _RIN_STDLIB_RANDOM_STATE_OWNER 1
#include "stdlib.h"
#include "sys/random.h"

#define RIN_LIBC_RAND_MAX 0x7fffffffU

#ifdef __cplusplus
extern "C" {
#endif

static unsigned int rin_libc_rand_state = 1U;

enum {
    RIN_RANDOM_TYPE_0 = 0,
    RIN_RANDOM_TYPE_1 = 1,
    RIN_RANDOM_TYPE_2 = 2,
    RIN_RANDOM_TYPE_3 = 3,
    RIN_RANDOM_TYPE_4 = 4,
    RIN_RANDOM_STATE_HEADER_WORDS = 1
};

typedef struct RinRandomShape {
    int degree;
    int separation;
} RinRandomShape;

static struct random_data rin_libc_random_data;
static int32_t rin_libc_random_default_state[RIN_RANDOM_STATE_HEADER_WORDS + 31];
static volatile uint32_t rin_libc_random_lock;
static int rin_libc_random_initialized;

static void rin_libc_random_lock_acquire(void)
{
    while (__atomic_exchange_n(&rin_libc_random_lock, 1u, __ATOMIC_ACQUIRE))
        __atomic_signal_fence(__ATOMIC_ACQUIRE);
}

static void rin_libc_random_lock_release(void)
{
    __atomic_store_n(&rin_libc_random_lock, 0u, __ATOMIC_RELEASE);
}

static int rin_libc_random_shape(int type, RinRandomShape* shape)
{
    static const RinRandomShape shapes[] = {
        {0, 0}, {7, 3}, {15, 1}, {31, 3}, {63, 1}
    };
    if (shape == NULL || type < RIN_RANDOM_TYPE_0 ||
        type > RIN_RANDOM_TYPE_4)
        return 0;
    *shape = shapes[type];
    return 1;
}

static int rin_libc_random_data_valid(const struct random_data* buffer)
{
    RinRandomShape shape;
    uintptr_t state_address;
    uintptr_t end_address;
    uintptr_t fptr_address;
    uintptr_t rptr_address;
    uintptr_t state_bytes;
    if (buffer == NULL || buffer->state == NULL ||
        !rin_libc_random_shape(buffer->rand_type, &shape) ||
        buffer->rand_deg != shape.degree ||
        buffer->rand_sep != shape.separation)
        return 0;
    state_address = (uintptr_t)buffer->state;
    state_bytes = (uintptr_t)shape.degree * sizeof(int32_t);
    if (state_address > UINTPTR_MAX - state_bytes) return 0;
    end_address = state_address + state_bytes;
    if ((uintptr_t)buffer->end_ptr != end_address) return 0;
    fptr_address = (uintptr_t)buffer->fptr;
    rptr_address = (uintptr_t)buffer->rptr;
    if (shape.degree == 0)
        return fptr_address == state_address && rptr_address == state_address;
    return fptr_address >= state_address && fptr_address < end_address &&
           rptr_address >= state_address && rptr_address < end_address &&
           ((fptr_address - state_address) % sizeof(int32_t)) == 0u &&
           ((rptr_address - state_address) % sizeof(int32_t)) == 0u;
}

static void rin_libc_random_store_header(struct random_data* buffer)
{
    uintptr_t rear_index = 0u;
    if (buffer->rand_deg != 0)
        rear_index = ((uintptr_t)buffer->rptr -
                      (uintptr_t)buffer->state) / sizeof(int32_t);
    buffer->state[-1] = (int32_t)(((uint32_t)buffer->rand_type << 16) |
                                  (uint32_t)rear_index);
}

static int rin_libc_random_type_for_size(size_t size, int* type_out)
{
    if (type_out == NULL || size < 8u) return 0;
    if (size >= 256u) *type_out = RIN_RANDOM_TYPE_4;
    else if (size >= 128u) *type_out = RIN_RANDOM_TYPE_3;
    else if (size >= 64u) *type_out = RIN_RANDOM_TYPE_2;
    else if (size >= 32u) *type_out = RIN_RANDOM_TYPE_1;
    else *type_out = RIN_RANDOM_TYPE_0;
    return 1;
}

static int rin_libc_random_state_buffer_valid(const char* state, size_t size,
                                             int* type_out,
                                             RinRandomShape* shape_out)
{
    int type;
    RinRandomShape shape;
    if (state == NULL ||
        ((uintptr_t)state % sizeof(int32_t)) != 0u ||
        !rin_libc_random_type_for_size(size, &type) ||
        !rin_libc_random_shape(type, &shape))
        return 0;
    if (type_out != NULL) *type_out = type;
    if (shape_out != NULL) *shape_out = shape;
    return 1;
}

static int rin_libc_random_step(struct random_data* buffer, int32_t* result)
{
    uint32_t value;
    if (buffer->rand_type == RIN_RANDOM_TYPE_0) {
        value = ((uint32_t)buffer->state[0] * 1103515245u + 12345u) &
                RIN_LIBC_RAND_MAX;
        buffer->state[0] = (int32_t)value;
    } else {
        value = (uint32_t)*buffer->fptr + (uint32_t)*buffer->rptr;
        *buffer->fptr = (int32_t)value;
        value = (value >> 1) & RIN_LIBC_RAND_MAX;
        ++buffer->fptr;
        if (buffer->fptr >= buffer->end_ptr) buffer->fptr = buffer->state;
        ++buffer->rptr;
        if (buffer->rptr >= buffer->end_ptr) buffer->rptr = buffer->state;
    }
    rin_libc_random_store_header(buffer);
    if (result != NULL) *result = (int32_t)value;
    return 0;
}

static int rin_libc_random_seed(unsigned int seed,
                                struct random_data* buffer)
{
    int index;
    int64_t word;
    int32_t discard;
    if (!rin_libc_random_data_valid(buffer)) return -1;
    if (seed == 0u) seed = 1u;
    buffer->state[0] = (int32_t)seed;
    word = (int64_t)seed;
    for (index = 1; index < buffer->rand_deg; ++index) {
        int64_t high = word / 127773;
        int64_t low = word % 127773;
        word = 16807 * low - 2836 * high;
        if (word < 0) word += 2147483647;
        buffer->state[index] = (int32_t)word;
    }
    buffer->fptr = buffer->state + buffer->rand_sep;
    buffer->rptr = buffer->state;
    rin_libc_random_store_header(buffer);
    for (index = 0; index < 10 * buffer->rand_deg; ++index)
        (void)rin_libc_random_step(buffer, &discard);
    return 0;
}

static int rin_libc_random_ensure_initialized(void)
{
    if (rin_libc_random_initialized) return 0;
    if (initstate_r(1u, (char*)rin_libc_random_default_state,
                    sizeof(rin_libc_random_default_state),
                    &rin_libc_random_data) != 0)
        return -1;
    rin_libc_random_initialized = 1;
    return 0;
}

static unsigned int rin_libc_rand_next_state(unsigned int state)
{
    /* xorshift32 has a full 2^32-1 cycle for every nonzero state. */
    if (state == 0U) state = 0x6D2B79F5U;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

static unsigned int rin_libc_rand_output(unsigned int state)
{
    /* Avalanche high state bits into rand()'s exposed low bits. */
    state ^= state >> 16;
    state *= 0x7FEB352DU;
    state ^= state >> 15;
    state *= 0x846CA68BU;
    state ^= state >> 16;
    return state;
}

int rand(void)
{
    unsigned int observed = __atomic_load_n(
        &rin_libc_rand_state, __ATOMIC_RELAXED);
    unsigned int next;

    do {
        next = rin_libc_rand_next_state(observed);
    } while (!__atomic_compare_exchange_n(
        &rin_libc_rand_state, &observed, next, 1,
        __ATOMIC_RELAXED, __ATOMIC_RELAXED));
    return (int)(rin_libc_rand_output(next) & RIN_LIBC_RAND_MAX);
}

void srand(unsigned int seed)
{
    __atomic_store_n(&rin_libc_rand_state, seed, __ATOMIC_RELAXED);
}

int rand_r(unsigned int* seed)
{
    unsigned int next;
    if (!seed) {
        errno = EINVAL;
        return 0;
    }
    next = rin_libc_rand_next_state(*seed);
    *seed = next;
    return (int)(rin_libc_rand_output(next) & RIN_LIBC_RAND_MAX);
}

void srandom(unsigned int seed)
{
    rin_libc_random_lock_acquire();
    if (rin_libc_random_ensure_initialized() == 0)
        (void)rin_libc_random_seed(seed, &rin_libc_random_data);
    rin_libc_random_lock_release();
}

int random_r(struct random_data* buffer, int32_t* result)
{
    if (result == NULL || !rin_libc_random_data_valid(buffer)) {
        errno = EINVAL;
        return -1;
    }
    (void)rin_libc_random_step(buffer, result);
    return 0;
}

int srandom_r(unsigned int seed, struct random_data* buffer)
{
    if (!rin_libc_random_data_valid(buffer)) {
        errno = EINVAL;
        return -1;
    }
    if (rin_libc_random_seed(seed, buffer) != 0) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int initstate_r(unsigned int seed, char* state, size_t state_size,
                struct random_data* buffer)
{
    RinRandomShape shape;
    int type;
    int32_t* words;
    if (buffer == NULL ||
        !rin_libc_random_state_buffer_valid(state, state_size, &type,
                                            &shape)) {
        errno = EINVAL;
        return -1;
    }
    words = (int32_t*)state;
    words[0] = (int32_t)((uint32_t)type << 16);
    buffer->state = words + RIN_RANDOM_STATE_HEADER_WORDS;
    buffer->rand_type = type;
    buffer->rand_deg = shape.degree;
    buffer->rand_sep = shape.separation;
    buffer->end_ptr = buffer->state + shape.degree;
    buffer->fptr = buffer->state + shape.separation;
    buffer->rptr = buffer->state;
    if (rin_libc_random_seed(seed, buffer) != 0) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int setstate_r(char* state, struct random_data* buffer)
{
    RinRandomShape shape;
    uint32_t state_header;
    uint32_t rear_index;
    int type;
    int32_t* words;
    int32_t* new_state;
    if (buffer == NULL || state == NULL ||
        ((uintptr_t)state % sizeof(int32_t)) != 0u ||
        !rin_libc_random_data_valid(buffer)) {
        errno = EINVAL;
        return -1;
    }
    words = (int32_t*)state;
    state_header = (uint32_t)words[0];
    type = (int)(state_header >> 16);
    rear_index = state_header & 0xffffu;
    if (!rin_libc_random_shape(type, &shape) ||
        (shape.degree == 0 ? rear_index != 0u
                           : rear_index >= (uint32_t)shape.degree)) {
        errno = EINVAL;
        return -1;
    }

    rin_libc_random_store_header(buffer);
    new_state = words + RIN_RANDOM_STATE_HEADER_WORDS;
    buffer->state = new_state;
    buffer->rand_type = type;
    buffer->rand_deg = shape.degree;
    buffer->rand_sep = shape.separation;
    buffer->end_ptr = new_state + shape.degree;
    if (shape.degree == 0) {
        buffer->fptr = new_state;
        buffer->rptr = new_state;
    } else {
        buffer->rptr = new_state + rear_index;
        buffer->fptr = buffer->rptr + shape.separation;
        if (buffer->fptr >= buffer->end_ptr)
            buffer->fptr -= shape.degree;
    }
    return 0;
}

char* initstate(unsigned int seed, char* state, size_t state_size)
{
    RinRandomShape shape;
    int type;
    char* previous;
    if (!rin_libc_random_state_buffer_valid(state, state_size, &type,
                                            &shape)) {
        errno = EINVAL;
        return NULL;
    }
    (void)type;
    (void)shape;
    rin_libc_random_lock_acquire();
    if (rin_libc_random_ensure_initialized() != 0) {
        rin_libc_random_lock_release();
        errno = EINVAL;
        return NULL;
    }
    previous = (char*)(rin_libc_random_data.state -
                       RIN_RANDOM_STATE_HEADER_WORDS);
    rin_libc_random_store_header(&rin_libc_random_data);
    if (initstate_r(seed, state, state_size,
                    &rin_libc_random_data) != 0) {
        rin_libc_random_lock_release();
        return NULL;
    }
    rin_libc_random_lock_release();
    return previous;
}

char* setstate(char* state)
{
    char* previous;
    rin_libc_random_lock_acquire();
    if (rin_libc_random_ensure_initialized() != 0) {
        rin_libc_random_lock_release();
        errno = EINVAL;
        return NULL;
    }
    previous = (char*)(rin_libc_random_data.state -
                       RIN_RANDOM_STATE_HEADER_WORDS);
    if (setstate_r(state, &rin_libc_random_data) != 0) {
        rin_libc_random_lock_release();
        return NULL;
    }
    rin_libc_random_lock_release();
    return previous;
}

long random(void)
{
    int32_t result = 0;
    rin_libc_random_lock_acquire();
    if (rin_libc_random_ensure_initialized() != 0 ||
        !rin_libc_random_data_valid(&rin_libc_random_data)) {
        rin_libc_random_lock_release();
        errno = EINVAL;
        return 0;
    }
    (void)rin_libc_random_step(&rin_libc_random_data, &result);
    rin_libc_random_lock_release();
    return (long)result;
}

static int rin_libc_arc4random_fill(void* buffer, size_t length)
{
#if defined(RIN_SYS_RANDOM_HOSTED_UNAVAILABLE)
    (void)buffer;
    (void)length;
    errno = ENOSYS;
    return -1;
#else
    unsigned char* bytes = (unsigned char*)buffer;
    size_t offset = 0u;
    while (offset < length) {
        size_t request = length - offset;
        ssize_t received;
        /* Keep each request bounded for providers with narrower syscall
         * length types; getrandom may still complete it in short reads. */
        if (request > 1048576u) request = 1048576u;
        received = getrandom(bytes + offset, request, 0u);
        if (received > 0) {
            offset += (size_t)received;
            continue;
        }
        if (received < 0 && errno == EINTR) continue;
        if (received == 0) errno = EIO;
        return -1;
    }
    return 0;
#endif
}

void arc4random_buf(void* buffer, size_t length)
{
    /* The API has no error return: treat an unavailable or failed secure
     * provider as fatal rather than exposing deterministic fallback bytes. */
    if (length != 0u && rin_libc_arc4random_fill(buffer, length) != 0)
        abort();
}

uint32_t arc4random(void)
{
    uint32_t value;
    arc4random_buf(&value, sizeof(value));
    return value;
}

uint32_t arc4random_uniform(uint32_t upper_bound)
{
    uint32_t threshold;
    uint32_t value;
    if (upper_bound < 2u) return 0u;
    threshold = (uint32_t)(0u - upper_bound) % upper_bound;
    do {
        value = arc4random();
    } while (value < threshold);
    return value % upper_bound;
}

#ifdef __cplusplus
}
#endif
