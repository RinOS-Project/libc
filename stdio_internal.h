/* SPDX-License-Identifier: MIT */
/* Narrow stdio hooks shared only by the libc runtime translation units. */

#ifndef _RIN_STDIO_INTERNAL_H
#define _RIN_STDIO_INTERNAL_H

#if defined(RIN_USERSPACE)
#include "stdio.h"
#include "stdint.h"

int _rin_stdio_claim_wide_orientation(FILE* stream);
void _rin_stdio_wide_pushback_clear(FILE* stream);
int _rin_stdio_wide_pushback_pop(FILE* stream, uint32_t* value_out);
int _rin_stdio_wide_pushback_push(FILE* stream, uint32_t value);
void _rin_stdio_wide_decode_state_get(FILE* stream, uint32_t* state_out,
                                     uint32_t* codepoint_out);
void _rin_stdio_wide_decode_state_set(FILE* stream, uint32_t state,
                                     uint32_t codepoint);
size_t _rin_stdio_write_buffered(FILE* stream, const char* data,
                                 size_t length, int* failed);
intptr_t _rin_stdio_read_block(FILE* stream, unsigned char* output,
                               size_t capacity);
#endif

#endif /* _RIN_STDIO_INTERNAL_H */
