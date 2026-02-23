#ifndef _TORTURE_ASSERT_H
#define _TORTURE_ASSERT_H

extern void abort(void) __attribute__((noreturn));

#define assert(expr) ((expr) ? (void)0 : abort())

#endif
