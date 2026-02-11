// Minimal uchar.h for TMS9900 freestanding libc++
// Provides mbstate_t required by libc++ internals.
#ifndef _TMS9900_UCHAR_H
#define _TMS9900_UCHAR_H

typedef struct {
    unsigned char __mbstate8[8];
} mbstate_t;

#endif // _TMS9900_UCHAR_H
