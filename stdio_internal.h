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
size_t _rin_stdio_write_buffered(FILE* stream, const char* data,
                                 size_t length, int* failed);
intptr_t _rin_stdio_read_block(FILE* stream, unsigned char* output,
                               size_t capacity);

enum {
    RIN_STDIO_MEMSTREAM_NONE = 0,
    RIN_STDIO_MEMSTREAM_FIXED = 1,
    RIN_STDIO_MEMSTREAM_BYTES = 2,
    RIN_STDIO_MEMSTREAM_WIDE = 3
};

int rin_stdio_memstream_kind(FILE* stream);
int rin_stdio_memstream_read(FILE* stream, unsigned char* output,
                             size_t capacity, intptr_t* result);
int rin_stdio_memstream_write(FILE* stream, const unsigned char* input,
                              size_t length, intptr_t* result);
int rin_stdio_memstream_read_wide(FILE* stream, uint32_t* value);
int rin_stdio_memstream_write_wide(FILE* stream, uint32_t value);
int rin_stdio_memstream_seek(FILE* stream, int64_t offset, int whence);
int rin_stdio_memstream_tell(FILE* stream, int64_t* position);
int rin_stdio_memstream_flush(FILE* stream);
int rin_stdio_memstream_close(FILE* stream, int* handled);
#endif

#endif /* _RIN_STDIO_INTERNAL_H */
