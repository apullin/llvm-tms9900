// Minimal wchar.h for TMS9900 freestanding libc++
#ifndef _TMS9900_WCHAR_H
#define _TMS9900_WCHAR_H

typedef __SIZE_TYPE__ size_t;

// mbstate_t — minimal definition for libc++ headers
typedef struct {
    unsigned char __mbstate8[8];
} mbstate_t;

#endif // _TMS9900_WCHAR_H
