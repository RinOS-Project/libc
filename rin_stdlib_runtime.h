/* SPDX-License-Identifier: MIT */
#ifndef RIN_STDLIB_RUNTIME_H
#define RIN_STDLIB_RUNTIME_H

/* libc-owned entry points used by the freestanding C++ library.  Keeping
 * these behind a public owner contract prevents libc++ from depending on
 * libc's private parser and process-lifecycle headers. */
#if defined(RIN_FREESTANDING) && RIN_FREESTANDING
#ifdef __cplusplus
extern "C" {
#endif

double rin_stdlib_strtod(const char* text, char** end);
float rin_stdlib_strtof(const char* text, char** end);
long double rin_stdlib_strtold(const char* text, char** end);
char* rin_stdlib_getenv(const char* name);
int rin_stdlib_system(const char* command);

#ifdef __cplusplus
}
#endif
#endif

#endif /* RIN_STDLIB_RUNTIME_H */
