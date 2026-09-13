/*
 * RinOS libc - getopt.c
 * Process-owned getopt state shared by every translation unit.
 */

#include "getopt.h"

char* optarg = NULL;
int optind = 1;
int opterr = 1;
int optopt = '?';
int optreset = 0;
int _rin_getopt_pos = 0;
