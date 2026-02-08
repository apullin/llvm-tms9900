// __muldi3 - 64-bit integer multiply for TMS9900
//
// Algorithm from LLVM compiler-rt (Apache-2.0 WITH LLVM-exception)
// Adapted for TMS9900 (16-bit int, 32-bit long, 64-bit long long)
//
// Returns: a * b (64-bit)

typedef unsigned int uint16_t;
typedef unsigned long uint32_t;
typedef long int32_t;
typedef unsigned long long uint64_t;
typedef long long int64_t;

// Multiply two 32-bit unsigned values, producing a full 64-bit result.
// Splits each 32-bit value into 16-bit halves and uses 16-bit partial
// products (which the TMS9900 hardware MPY can do natively via __mulsi3).
static uint64_t __muldsi3(uint32_t a, uint32_t b) {
    // On TMS9900: sizeof(uint32_t) = 4, bits_in_word_2 = 16
    // lower_mask = 0x0000FFFF
    const uint32_t lower_mask = 0x0000FFFFUL;

    uint32_t r_lo = (a & lower_mask) * (b & lower_mask);
    uint32_t t = r_lo >> 16;
    r_lo &= lower_mask;
    t += (a >> 16) * (b & lower_mask);
    r_lo += (t & lower_mask) << 16;
    uint32_t r_hi = t >> 16;
    t = r_lo >> 16;
    r_lo &= lower_mask;
    t += (b >> 16) * (a & lower_mask);
    r_lo += (t & lower_mask) << 16;
    r_hi += t >> 16;
    r_hi += (a >> 16) * (b >> 16);

    return ((uint64_t)r_hi << 32) | r_lo;
}

// 64-bit multiply: a * b
// Uses __muldsi3 for the low*low partial product (full 64-bit result),
// then adds cross products shifted left by 32.
int64_t __muldi3(int64_t a, int64_t b) {
    uint32_t a_lo = (uint32_t)a;
    uint32_t a_hi = (uint32_t)((uint64_t)a >> 32);
    uint32_t b_lo = (uint32_t)b;
    uint32_t b_hi = (uint32_t)((uint64_t)b >> 32);

    uint64_t result = __muldsi3(a_lo, b_lo);
    result += ((uint64_t)(a_hi * b_lo)) << 32;
    result += ((uint64_t)(a_lo * b_hi)) << 32;
    return (int64_t)result;
}
