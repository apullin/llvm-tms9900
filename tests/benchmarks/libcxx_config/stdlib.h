// Minimal stdlib.h for TMS9900 freestanding libc++
#ifndef _TMS9900_STDLIB_H
#define _TMS9900_STDLIB_H
#define _LIBCPP_STDLIB_H  // satisfy libc++ cstdlib check

typedef __SIZE_TYPE__ size_t;

typedef struct { long quot; long rem; } ldiv_t;
typedef struct { long long quot; long long rem; } lldiv_t;

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define NULL ((void *)0)

#ifdef __cplusplus
extern "C" {
#endif

void abort(void);
ldiv_t ldiv(long __numer, long __denom);
lldiv_t lldiv(long long __numer, long long __denom);
void *malloc(size_t __size);
void *calloc(size_t __count, size_t __size);
void free(void *__ptr);

#ifdef __cplusplus
}
#endif

#endif // _TMS9900_STDLIB_H
