#include "fp_mode.h"

/*
 * The TMS9900 has no floating-point environment. The software runtime uses
 * round-to-nearest and does not expose IEEE exception flags.
 */
CRT_FE_ROUND_MODE __fe_getround(void) { return CRT_FE_TONEAREST; }

int __fe_raise_inexact(void) { return 0; }
