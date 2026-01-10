/*
 * Minimal picolibc configuration for TMS9900
 */

#pragma once

/* Optimize for space over speed */
#define __PREFER_SIZE_OVER_SPEED 1

/* math library does not set errno (offering only ieee semantics) */
#define __IEEE_LIBM 1

/* Use global errno variable */
#define __GLOBAL_ERRNO 1

/* No thread local storage */
/* #undef __THREAD_LOCAL_STORAGE */

/* No complex number support */
/* #undef __HAVE_COMPLEX */

/* 16-bit int */
#define __SIZEOF_INT__ 2
#define __SIZEOF_LONG__ 4
#define __SIZEOF_POINTER__ 2

/* Big endian */
#define __IEEE_BIG_ENDIAN 1

/* Disable long double variants (same as double on TMS9900) */
#define _LDBL_EQ_DBL 1
#define __OBSOLETE_MATH_FLOAT 1
#define __OBSOLETE_MATH_DOUBLE 0
#define __OBSOLETE_MATH 0

/* Skip problematic 64-bit declarations */
#define _NEED_FLOAT64 0
