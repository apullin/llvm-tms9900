// Libc stubs for compiler-rt builtin tests
// All tests use printf/fprintf for error diagnostics only; we no-op them.
// NOTE: This file is cross-compiled with -I include (our shim headers),
// not the host stdio.h. IDE warnings about conflicting types are spurious.

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wbuiltin-requires-header"

int printf(const char *fmt, ...) { (void)fmt; return 0; }
int fprintf(void *stream, const char *fmt, ...) { (void)stream; (void)fmt; return 0; }

#pragma clang diagnostic pop

// stderr symbol (some tests reference it)
void *stderr = (void *)0;

// Simple LCG random (used by absvsi2, absvdi2 tests)
static unsigned long _seed = 42;
int rand(void) {
    _seed = _seed * 1103515245UL + 12345UL;
    return (_seed >> 16) & 0x7FFF;
}

// compilerrt_abort_impl (called by overflow-checking builtins)
void __compilerrt_abort_impl(const char *file, int line, const char *func) {
    (void)file; (void)line; (void)func;
    for(;;) {}
}

// fegetround / fesetround stubs (used by some float tests)
static int _rounding_mode = 0; // FE_TONEAREST
int fegetround(void) { return _rounding_mode; }
int fesetround(int round) { _rounding_mode = round; return 0; }

// fmaxf stub
float fmaxf(float a, float b) {
    if (__builtin_isnan(a)) return b;
    if (__builtin_isnan(b)) return a;
    return a > b ? a : b;
}
