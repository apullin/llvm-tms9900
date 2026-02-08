// __divdi3 - signed 64-bit integer divide for TMS9900
//
// Algorithm from LLVM compiler-rt (Apache-2.0 WITH LLVM-exception)
// Adapted for TMS9900 (16-bit int, 32-bit long, 64-bit long long)
//
// Returns: a / b (signed)

typedef unsigned long long uint64_t;
typedef long long int64_t;

extern uint64_t __udivmoddi4(uint64_t a, uint64_t b, uint64_t *rem);

int64_t __divdi3(int64_t a, int64_t b) {
    const int N = 63;  // (sizeof(int64_t) * 8) - 1
    int64_t s_a = a >> N;                           // s_a = a < 0 ? -1 : 0
    int64_t s_b = b >> N;                           // s_b = b < 0 ? -1 : 0
    uint64_t a_u = (uint64_t)(a ^ s_a) + (uint64_t)(-s_a); // negate if s_a == -1
    uint64_t b_u = (uint64_t)(b ^ s_b) + (uint64_t)(-s_b); // negate if s_b == -1
    s_a ^= s_b;                                     // sign of quotient
    return (int64_t)((__udivmoddi4(a_u, b_u, (uint64_t *)0) ^ (uint64_t)s_a) + (uint64_t)(-s_a));
}
