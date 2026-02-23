#ifndef _STDLIB_H
#define _STDLIB_H
#define NULL ((void*)0)
void abort(void);
void exit(int status);
int rand(void);
void *malloc(unsigned int size);
void *calloc(unsigned int n, unsigned int size);
void free(void *ptr);
#endif
