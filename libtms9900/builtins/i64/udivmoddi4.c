// __udivmoddi4 - 64-bit unsigned divide and modulo for TMS9900
//
// Algorithm from LLVM compiler-rt (Apache-2.0 WITH LLVM-exception)
// Adapted for TMS9900 (16-bit int, 32-bit long, 64-bit long long)
//
// Returns: a / b
// If rem != 0, *rem = a % b

typedef unsigned int uint16_t;
typedef unsigned long uint32_t;
typedef long int32_t;
typedef unsigned long long uint64_t;
typedef long long int64_t;

static int clzsi(uint32_t a) {
    return __builtin_clzl(a);
}

static int ctzsi(uint32_t a) {
    return __builtin_ctzl(a);
}

// Split/join helpers for 64-bit values (big-endian TMS9900)
static inline uint32_t hi32(uint64_t x) { return (uint32_t)(x >> 32); }
static inline uint32_t lo32(uint64_t x) { return (uint32_t)x; }
static inline uint64_t make64(uint32_t hi, uint32_t lo) {
    return ((uint64_t)hi << 32) | lo;
}

// Effects: if rem != 0, *rem = a % b
// Returns: a / b
//
// Translated from Figure 3-40 of The PowerPC Compiler Writer's Guide
uint64_t __udivmoddi4(uint64_t a, uint64_t b, uint64_t *rem) {
    const unsigned n_uword_bits = 32;  // sizeof(uint32_t) * 8
    const unsigned n_udword_bits = 64; // sizeof(uint64_t) * 8

    uint32_t n_hi = hi32(a);
    uint32_t n_lo = lo32(a);
    uint32_t d_hi = hi32(b);
    uint32_t d_lo = lo32(b);

    uint32_t q_hi, q_lo;
    uint32_t r_hi, r_lo;
    unsigned sr;

    // special cases, X is unknown, K != 0
    if (n_hi == 0) {
        if (d_hi == 0) {
            // 0 X / 0 X
            if (rem)
                *rem = (uint64_t)(n_lo % d_lo);
            return (uint64_t)(n_lo / d_lo);
        }
        // 0 X / K X
        if (rem)
            *rem = (uint64_t)n_lo;
        return 0;
    }
    // n_hi != 0
    if (d_lo == 0) {
        if (d_hi == 0) {
            // K X / 0 0  (division by zero)
            if (rem)
                *rem = (uint64_t)(n_hi % d_lo);
            return (uint64_t)(n_hi / d_lo);
        }
        // d_hi != 0
        if (n_lo == 0) {
            // K 0 / K 0
            if (rem)
                *rem = make64(n_hi % d_hi, 0);
            return (uint64_t)(n_hi / d_hi);
        }
        // K K / K 0
        if ((d_hi & (d_hi - 1)) == 0) { // d is a power of 2
            if (rem)
                *rem = make64(n_hi & (d_hi - 1), n_lo);
            return (uint64_t)(n_hi >> ctzsi(d_hi));
        }
        // K K / K 0
        sr = clzsi(d_hi) - clzsi(n_hi);
        // 0 <= sr <= n_uword_bits - 2 or sr large
        if (sr > n_uword_bits - 2) {
            if (rem)
                *rem = a;
            return 0;
        }
        ++sr;
        // 1 <= sr <= n_uword_bits - 1
        // q.all = n.all << (n_udword_bits - sr);
        q_lo = 0;
        q_hi = n_lo << (n_uword_bits - sr);
        // r.all = n.all >> sr;
        r_hi = n_hi >> sr;
        r_lo = (n_hi << (n_uword_bits - sr)) | (n_lo >> sr);
    } else { // d_lo != 0
        if (d_hi == 0) {
            // K X / 0 K
            if ((d_lo & (d_lo - 1)) == 0) { // d is a power of 2
                if (rem)
                    *rem = (uint64_t)(n_lo & (d_lo - 1));
                if (d_lo == 1)
                    return a;
                sr = ctzsi(d_lo);
                q_hi = n_hi >> sr;
                q_lo = (n_hi << (n_uword_bits - sr)) | (n_lo >> sr);
                return make64(q_hi, q_lo);
            }
            // K X / 0 K
            sr = 1 + n_uword_bits + clzsi(d_lo) - clzsi(n_hi);
            // 2 <= sr <= n_udword_bits - 1
            // q.all = n.all << (n_udword_bits - sr);
            // r.all = n.all >> sr;
            if (sr == n_uword_bits) {
                q_lo = 0;
                q_hi = n_lo;
                r_hi = 0;
                r_lo = n_hi;
            } else if (sr < n_uword_bits) { // 2 <= sr <= n_uword_bits - 1
                q_lo = 0;
                q_hi = n_lo << (n_uword_bits - sr);
                r_hi = n_hi >> sr;
                r_lo = (n_hi << (n_uword_bits - sr)) | (n_lo >> sr);
            } else { // n_uword_bits + 1 <= sr <= n_udword_bits - 1
                q_lo = n_lo << (n_udword_bits - sr);
                q_hi = (n_hi << (n_udword_bits - sr)) |
                       (n_lo >> (sr - n_uword_bits));
                r_hi = 0;
                r_lo = n_hi >> (sr - n_uword_bits);
            }
        } else {
            // K X / K K
            sr = clzsi(d_hi) - clzsi(n_hi);
            // 0 <= sr <= n_uword_bits - 1 or sr large
            if (sr > n_uword_bits - 1) {
                if (rem)
                    *rem = a;
                return 0;
            }
            ++sr;
            // 1 <= sr <= n_uword_bits
            // q.all = n.all << (n_udword_bits - sr);
            q_lo = 0;
            if (sr == n_uword_bits) {
                q_hi = n_lo;
                r_hi = 0;
                r_lo = n_hi;
            } else {
                q_hi = n_lo << (n_uword_bits - sr);
                r_hi = n_hi >> sr;
                r_lo = (n_hi << (n_uword_bits - sr)) | (n_lo >> sr);
            }
        }
    }
    // Not a special case
    // q and r are initialized with:
    // q.all = n.all << (n_udword_bits - sr);
    // r.all = n.all >> sr;
    // 1 <= sr <= n_udword_bits - 1
    uint32_t carry = 0;
    for (; sr > 0; --sr) {
        // r:q = ((r:q) << 1) | carry
        r_hi = (r_hi << 1) | (r_lo >> (n_uword_bits - 1));
        r_lo = (r_lo << 1) | (q_hi >> (n_uword_bits - 1));
        q_hi = (q_hi << 1) | (q_lo >> (n_uword_bits - 1));
        q_lo = (q_lo << 1) | carry;
        // carry = 0;
        // if (r >= d) { r -= d; carry = 1; }
        // Using 32-bit operations only (avoids buggy i64 inline expansion):
        // Compare r >= d using 32-bit words
        if (r_hi > d_hi || (r_hi == d_hi && r_lo >= d_lo)) {
            // r -= d with borrow propagation
            uint32_t new_lo = r_lo - d_lo;
            uint32_t borrow = (new_lo > r_lo) ? 1 : 0;
            r_hi = r_hi - d_hi - borrow;
            r_lo = new_lo;
            carry = 1;
        } else {
            carry = 0;
        }
    }
    q_hi = (q_hi << 1) | (q_lo >> (n_uword_bits - 1));
    q_lo = (q_lo << 1) | carry;
    if (rem)
        *rem = make64(r_hi, r_lo);
    return make64(q_hi, q_lo);
}
