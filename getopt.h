/*
 * RinOS libc - getopt.h
 * GNU command-line option API.
 */

#if !defined(RIN_LIBC_GETOPT_FORCE_INTERNAL) && \
    defined(__STDC_HOSTED__) && __STDC_HOSTED__ && \
    (!defined(RIN_FREESTANDING) || !(RIN_FREESTANDING)) && \
    (defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || \
     defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)) && \
    (defined(__GNUC__) || defined(__clang__))
#define _RIN_LIBC_GETOPT_HOSTED_FORWARD 1
#include_next <getopt.h>
#else

#ifndef _GETOPT_H
#define _GETOPT_H

#ifdef __cplusplus
extern "C" {
#endif

/* The POSIX getopt state is also declared from <unistd.h>. */
extern char* optarg;
extern int optind;
extern int opterr;
extern int optopt;
/* BSD reset extension; <getopt.h> is the explicit extension boundary. */
extern int optreset;

struct option {
    const char* name;
    int has_arg;
    int* flag;
    int val;
};

#define no_argument       0
#define required_argument 1
#define optional_argument 2

int getopt(int argc, char* const argv[], const char* optstring);
int getopt_long(int argc, char* const argv[], const char* optstring,
                const struct option* longopts, int* longindex);
int getopt_long_only(int argc, char* const argv[], const char* optstring,
                     const struct option* longopts, int* longindex);

#ifdef __cplusplus
}
#endif

#endif /* _GETOPT_H */

#endif /* hosted builds use the host getopt API */
