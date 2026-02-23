#ifndef _TORTURE_STDLIB_H
#define _TORTURE_STDLIB_H

typedef __SIZE_TYPE__ size_t;

void abort(void) __attribute__((noreturn));
void exit(int) __attribute__((noreturn));
void *malloc(size_t);
void *calloc(size_t, size_t);
void free(void *);

#define NULL ((void *)0)

#endif
