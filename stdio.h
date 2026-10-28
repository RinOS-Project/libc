/* SPDX-License-Identifier: MIT */
/*
 * RinOS libc - stdio ABI
 * 共有FILE型とprocess-global stdio ownerへの宣言。
 */

#ifndef _STDIO_H
#define _STDIO_H

#include "stddef.h"
#include "stdint.h"
#include "stdarg.h"
#include "sys/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EOF (-1)
#define BUFSIZ 512
#define _RIN_STDIO_BUFFER_POSITION_MASK ((size_t)-1 / 2u)

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

#define FILENAME_MAX 256
#define FOPEN_MAX 16
#define TMP_MAX 238328
#define L_tmpnam 20

#ifndef _FILE_DECLARED
#define _FILE_DECLARED
struct _FILE;
typedef struct _FILE FILE;
#endif

#ifndef _FILE_DEFINED
#define _FILE_DEFINED
struct _FILE {
    int fd;
    int eof;
    int error;
    int mode;
    unsigned char* buf;
    size_t buf_size;
    size_t buf_pos;
    size_t buf_end;
    int buf_mode;
    int orientation;
    uint32_t wide_pushback;
    int wide_pushback_valid;
    uint32_t wide_pushback_second;
    int wide_pushback_second_valid;
    int buf_owned;
};
#endif

#if defined(__RIN_FILE_OFFSET_BITS64)
typedef off64_t fpos_t;
#else
typedef long fpos_t;
#endif

#if !defined(RIN_USERSPACE)
/* The kernel and source-isolated host harnesses still need their local
 * implementation. User processes use the runtime object below. */
#define RIN_STDIO_LOCAL_FALLBACK 1
#include "stdio_impl.h"
#undef RIN_STDIO_LOCAL_FALLBACK
#else

FILE* rin_stdio_get_stdin(void);
FILE* rin_stdio_get_stdout(void);
FILE* rin_stdio_get_stderr(void);
#define stdin  (rin_stdio_get_stdin())
#define stdout (rin_stdio_get_stdout())
#define stderr (rin_stdio_get_stderr())

void flockfile(FILE* stream);
int ftrylockfile(FILE* stream);
void funlockfile(FILE* stream);

int putchar(int c);
int puts(const char* s);
int fputc(int c, FILE* stream);
int fputs(const char* s, FILE* stream);
int fgetc(FILE* stream);
int getchar(void);
int getc(FILE* stream);
int putc(int c, FILE* stream);
char* fgets(char* s, int size, FILE* stream);
ssize_t getline(char** lineptr, size_t* n, FILE* stream);
ssize_t getdelim(char** lineptr, size_t* n, int delimiter, FILE* stream);
int ungetc(int c, FILE* stream);

FILE* fopen(const char* path, const char* mode);
int fclose(FILE* stream);
FILE* freopen(const char* path, const char* mode, FILE* stream);
FILE* fdopen(int fd, const char* mode);
int fseek(FILE* stream, long offset, int whence);
long ftell(FILE* stream);
int fseeko(FILE* stream, off_t offset, int whence);
off_t ftello(FILE* stream);
int fseeko64(FILE* stream, off64_t offset, int whence);
off64_t ftello64(FILE* stream);
void rewind(FILE* stream);
int fgetpos(FILE* stream, fpos_t* pos);
int fsetpos(FILE* stream, const fpos_t* pos);
int setvbuf(FILE* stream, char* buf, int mode, size_t size);
void setbuf(FILE* stream, char* buf);
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);
int feof(FILE* stream);
int ferror(FILE* stream);
void clearerr(FILE* stream);
int fflush(FILE* stream);
int fileno(FILE* stream);

void perror(const char* s);
int remove(const char* pathname);
int rename(const char* oldpath, const char* newpath);
FILE* tmpfile(void);
char* tmpnam(char* s);
FILE* fmemopen(void* buffer, size_t size, const char* mode);
FILE* open_memstream(char** buffer, size_t* size);

int vsnprintf(char* buf, size_t size, const char* fmt, va_list ap);
int snprintf(char* buf, size_t size, const char* fmt, ...);
int sprintf(char* buf, const char* fmt, ...);
int vsprintf(char* buf, const char* fmt, va_list ap);
int vprintf(const char* fmt, va_list ap);
int printf(const char* fmt, ...);
int vfprintf(FILE* stream, const char* fmt, va_list ap);
int fprintf(FILE* stream, const char* fmt, ...);
int vasprintf(char** result, const char* fmt, va_list ap);
int asprintf(char** result, const char* fmt, ...);

#include "stdio_scanf.h"

#if defined(__RIN_FILE_OFFSET_BITS64) && !defined(RIN_STDIO_OWNER_BUILD)
#define fseeko fseeko64
#define ftello ftello64
#endif

#endif /* RIN_USERSPACE */

#ifdef __cplusplus
}
#endif

#endif /* _STDIO_H */
