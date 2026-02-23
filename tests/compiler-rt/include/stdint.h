#ifndef _STDINT_H
#define _STDINT_H
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef long int32_t;
typedef unsigned long uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;
typedef int intptr_t;
typedef unsigned int uintptr_t;
#define INT8_MAX 127
#define INT8_MIN (-128)
#define INT16_MAX 32767
#define INT16_MIN (-32768)
#define INT32_MAX 2147483647L
#define INT32_MIN (-2147483647L - 1L)
#define INT64_MAX 9223372036854775807LL
#define INT64_MIN (-9223372036854775807LL - 1LL)
#define UINT8_MAX 255
#define UINT16_MAX 65535U
#define UINT32_MAX 4294967295UL
#define UINT64_MAX 18446744073709551615ULL
#define INTPTR_MAX 0x7FFF
#define UINTPTR_MAX 0xFFFFU
#endif
