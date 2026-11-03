/*
 * RinOS libc - getopt.c
 * Process-owned getopt state shared by every translation unit.
 */

#define RIN_LIBC_GETOPT_IMPLEMENTATION 1
#include "getopt.h"
#if !defined(_RIN_LIBC_GETOPT_HOSTED_FORWARD)
#include "stdlib.h"

char* optarg = NULL;
int optind = 1;
int opterr = 1;
int optopt = '?';
int optreset = 0;
int _rin_getopt_pos = 0;
int _rin_getopt_first_nonopt = 1;
int _rin_getopt_last_nonopt = 1;
int _rin_getopt_ordering = 0;
int _rin_getopt_initialized = 0;
int _rin_getopt_finished = 0;

int _rin_getopt_posixly_correct(void)
{
    return getenv("POSIXLY_CORRECT") != NULL;
}
#else
typedef int _rin_getopt_hosted_crt_owner_translation_unit;
#endif
