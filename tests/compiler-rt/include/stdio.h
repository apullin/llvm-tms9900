#ifndef _STDIO_H
#define _STDIO_H
typedef void FILE;
extern FILE *stderr;
int printf(const char *fmt, ...);
int fprintf(FILE *stream, const char *fmt, ...);
#endif
