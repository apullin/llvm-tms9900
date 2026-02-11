// Minimal stdio.h for TMS9900 freestanding libc++
#ifndef _TMS9900_STDIO_H
#define _TMS9900_STDIO_H
#define _LIBCPP_STDIO_H  // satisfy libc++ cstdio check

typedef __SIZE_TYPE__ size_t;

#define EOF (-1)
#define NULL ((void *)0)

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define BUFSIZ 512

typedef struct _FILE FILE;
typedef long fpos_t;

#ifdef __cplusplus
extern "C" {
#endif

int remove(const char *__path);
int rename(const char *__old, const char *__new);
FILE *fopen(const char *__path, const char *__mode);
int fclose(FILE *__stream);
int fflush(FILE *__stream);
int fprintf(FILE *__stream, const char *__fmt, ...);
int printf(const char *__fmt, ...);
int sprintf(char *__buf, const char *__fmt, ...);
int snprintf(char *__buf, size_t __size, const char *__fmt, ...);
int sscanf(const char *__buf, const char *__fmt, ...);
int fgetc(FILE *__stream);
int fputc(int __c, FILE *__stream);
char *fgets(char *__buf, int __size, FILE *__stream);
int fputs(const char *__s, FILE *__stream);
int getc(FILE *__stream);
int putc(int __c, FILE *__stream);
int puts(const char *__s);
size_t fread(void *__buf, size_t __size, size_t __count, FILE *__stream);
size_t fwrite(const void *__buf, size_t __size, size_t __count, FILE *__stream);
int fseek(FILE *__stream, long __offset, int __whence);
long ftell(FILE *__stream);
void rewind(FILE *__stream);
int feof(FILE *__stream);
int ferror(FILE *__stream);
void clearerr(FILE *__stream);
int fgetpos(FILE *__stream, fpos_t *__pos);
int fsetpos(FILE *__stream, const fpos_t *__pos);
int ungetc(int __c, FILE *__stream);
int vfprintf(FILE *__stream, const char *__fmt, __builtin_va_list __ap);
int vprintf(const char *__fmt, __builtin_va_list __ap);
int vsprintf(char *__buf, const char *__fmt, __builtin_va_list __ap);
int vsnprintf(char *__buf, size_t __size, const char *__fmt, __builtin_va_list __ap);
int vsscanf(const char *__buf, const char *__fmt, __builtin_va_list __ap);

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

#ifdef __cplusplus
}
#endif

#endif // _TMS9900_STDIO_H
