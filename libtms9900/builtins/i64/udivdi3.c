// __udivdi3 - unsigned 64-bit integer divide for TMS9900
//
// Algorithm from LLVM compiler-rt (Apache-2.0 WITH LLVM-exception)
// Adapted for TMS9900 (16-bit int, 32-bit long, 64-bit long long)
//
// Returns: a / b (unsigned)

typedef unsigned long long uint64_t;
typedef long long int64_t;

extern uint64_t __udivmoddi4(uint64_t a, uint64_t b, uint64_t *rem);

uint64_t __udivdi3(uint64_t a, uint64_t b) {
    return __udivmoddi4(a, b, (uint64_t *)0);
}
