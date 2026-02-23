#ifndef _TORTURE_STDIO_H
#define _TORTURE_STDIO_H

/* Stub stdio.h for torture tests.
 * printf is a no-op; tests that depend on printf output will still
 * pass/fail based on abort()/exit() logic, not printed output.
 */

typedef __SIZE_TYPE__ size_t;

#define NULL ((void *)0)
#define EOF (-1)

static inline int printf(const char *fmt, ...) { (void)fmt; return 0; }
static inline int fprintf(void *stream, const char *fmt, ...) { (void)stream; (void)fmt; return 0; }
static inline int sprintf(char *buf, const char *fmt, ...) { (void)buf; (void)fmt; return 0; }
static inline int snprintf(char *buf, size_t n, const char *fmt, ...) { (void)buf; (void)n; (void)fmt; return 0; }
static inline int puts(const char *s) { (void)s; return 0; }
static inline int putchar(int c) { return c; }
static inline int fflush(void *stream) { (void)stream; return 0; }

#define stdout ((void *)0)
#define stderr ((void *)0)

#endif
