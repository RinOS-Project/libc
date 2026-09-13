/*
 * RinOS libc - linux/futex.h
 * Futex (fast userspace mutex) definitions
 */

#ifndef _LINUX_FUTEX_H
#define _LINUX_FUTEX_H

/* Futex operations */
#define FUTEX_WAIT              0
#define FUTEX_WAKE              1
#define FUTEX_FD                2
#define FUTEX_REQUEUE           3
#define FUTEX_CMP_REQUEUE       4
#define FUTEX_WAKE_OP           5
#define FUTEX_LOCK_PI           6
#define FUTEX_UNLOCK_PI         7
#define FUTEX_TRYLOCK_PI        8
#define FUTEX_WAIT_BITSET       9
#define FUTEX_WAKE_BITSET       10
#define FUTEX_WAIT_REQUEUE_PI   11
#define FUTEX_CMP_REQUEUE_PI    12

/* Futex flags */
#define FUTEX_PRIVATE_FLAG      128
#define FUTEX_CLOCK_REALTIME    256

/* Combined operations with flags */
#define FUTEX_WAIT_PRIVATE      (FUTEX_WAIT | FUTEX_PRIVATE_FLAG)
#define FUTEX_WAKE_PRIVATE      (FUTEX_WAKE | FUTEX_PRIVATE_FLAG)
#define FUTEX_REQUEUE_PRIVATE   (FUTEX_REQUEUE | FUTEX_PRIVATE_FLAG)
#define FUTEX_CMP_REQUEUE_PRIVATE (FUTEX_CMP_REQUEUE | FUTEX_PRIVATE_FLAG)
#define FUTEX_WAKE_OP_PRIVATE   (FUTEX_WAKE_OP | FUTEX_PRIVATE_FLAG)
#define FUTEX_LOCK_PI_PRIVATE   (FUTEX_LOCK_PI | FUTEX_PRIVATE_FLAG)
#define FUTEX_UNLOCK_PI_PRIVATE (FUTEX_UNLOCK_PI | FUTEX_PRIVATE_FLAG)
#define FUTEX_TRYLOCK_PI_PRIVATE (FUTEX_TRYLOCK_PI | FUTEX_PRIVATE_FLAG)
#define FUTEX_WAIT_BITSET_PRIVATE (FUTEX_WAIT_BITSET | FUTEX_PRIVATE_FLAG)
#define FUTEX_WAKE_BITSET_PRIVATE (FUTEX_WAKE_BITSET | FUTEX_PRIVATE_FLAG)
#define FUTEX_WAIT_REQUEUE_PI_PRIVATE (FUTEX_WAIT_REQUEUE_PI | FUTEX_PRIVATE_FLAG)
#define FUTEX_CMP_REQUEUE_PI_PRIVATE (FUTEX_CMP_REQUEUE_PI | FUTEX_PRIVATE_FLAG)

/* Bitset match all */
#define FUTEX_BITSET_MATCH_ANY  0xffffffff

/* FUTEX_WAKE_OP operations */
#define FUTEX_OP_SET            0   /* uaddr2 = oparg */
#define FUTEX_OP_ADD            1   /* uaddr2 += oparg */
#define FUTEX_OP_OR             2   /* uaddr2 |= oparg */
#define FUTEX_OP_ANDN           3   /* uaddr2 &= ~oparg */
#define FUTEX_OP_XOR            4   /* uaddr2 ^= oparg */

/* FUTEX_WAKE_OP comparison operations */
#define FUTEX_OP_CMP_EQ         0   /* == */
#define FUTEX_OP_CMP_NE         1   /* != */
#define FUTEX_OP_CMP_LT         2   /* < */
#define FUTEX_OP_CMP_LE         3   /* <= */
#define FUTEX_OP_CMP_GT         4   /* > */
#define FUTEX_OP_CMP_GE         5   /* >= */

/* FUTEX_OP macro to encode operation */
#define FUTEX_OP(op, oparg, cmp, cmparg) \
    (((op) & 0xf) | (((cmp) & 0xf) << 4) | \
     (((oparg) & 0xfff) << 8) | (((cmparg) & 0xfff) << 20))

#endif /* _LINUX_FUTEX_H */
