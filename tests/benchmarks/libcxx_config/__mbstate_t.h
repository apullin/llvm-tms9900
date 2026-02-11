// Override libc++'s __mbstate_t.h for TMS9900 freestanding
#ifndef _LIBCPP___MBSTATE_T_H
#define _LIBCPP___MBSTATE_T_H

typedef struct {
    unsigned char __mbstate8[8];
} mbstate_t;

#endif
