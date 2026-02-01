/* Copyright (C) 2002 by  Red Hat, Incorporated. All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software
 * is freely granted, provided that this notice is preserved.
 */

#include "fdlibm.h"

float
fminf(float x, float y)
{
    if (__issignalingf(x) || __issignalingf(y))
        return x + y;

    if (__isnanf(x))
        return y;

    if (__isnanf(y))
        return x;

    return x < y ? x : y;
}

_MATH_ALIAS_f_ff(fmin)
