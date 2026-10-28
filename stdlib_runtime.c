/* SPDX-License-Identifier: MIT */
/* C++ standard-library bridges to the libc-owned conversion and process APIs. */

#define RIN_STDLIB_NO_INLINE_GETSUBOPT 1
#include "stdlib.h"
#include "rin_stdlib_runtime.h"

int getsubopt(char** optionp, char* const* keylistp, char** valuep) {
    return _rin_stdlib_getsubopt_parse(optionp, keylistp, valuep);
}

double rin_stdlib_strtod(const char* text, char** end) {
    return strtod(text, end);
}

float rin_stdlib_strtof(const char* text, char** end) {
    return strtof(text, end);
}

long double rin_stdlib_strtold(const char* text, char** end) {
    return strtold(text, end);
}

char* rin_stdlib_getenv(const char* name) {
    return getenv(name);
}

int rin_stdlib_system(const char* command) {
    return system(command);
}
