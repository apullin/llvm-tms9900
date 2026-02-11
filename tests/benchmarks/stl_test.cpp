/* stl_test — libc++ STL container test for TMS9900
 *
 * Exercises STL containers and algorithms on a freestanding 16-bit target:
 *   1. std::array (stack-allocated fixed container)
 *   2. std::sort (introsort — complex control flow)
 *   3. std::vector (dynamic allocation + growth)
 *   4. std::string (SSO + heap allocation, byte operations)
 *
 * Accumulates a checksum; halts with IDLE on correct result.
 */

// Must define before any includes
#define _LIBCPP_VERBOSE_ABORT(...) __builtin_trap()

#include <array>
#include <algorithm>
#include <vector>
#include <string>

typedef __SIZE_TYPE__ size_t;

void *operator new(size_t);
void *operator new[](size_t);
void operator delete(void *) noexcept;
void operator delete(void *, size_t) noexcept;
void operator delete[](void *) noexcept;
void operator delete[](void *, size_t) noexcept;

__attribute__((noinline)) static void halt_ok(void) { __asm__ volatile("idle"); }
__attribute__((noinline)) static void fail_loop(void) { for (;;) {} }

static unsigned int check = 0;

static void check_val(unsigned int expected, unsigned int actual) {
    if (expected != actual)
        fail_loop();
    check ^= actual;
}

/* ---- Test 1: std::array ---- */

static void test_array() {
    std::array<unsigned int, 5> a = {10, 20, 30, 40, 50};

    check_val(5, (unsigned int)a.size());
    check_val(10, a[0]);
    check_val(50, a[4]);
    check_val(30, a.at(2));

    /* Sum via range-based iteration */
    unsigned int sum = 0;
    for (auto v : a)
        sum += v;
    check_val(150, sum);
}

/* ---- Test 2: std::sort ---- */

static void test_sort() {
    std::array<unsigned int, 8> a = {42, 17, 99, 3, 55, 8, 71, 23};

    std::sort(a.begin(), a.end());

    /* Verify sorted */
    check_val(3, a[0]);
    check_val(8, a[1]);
    check_val(17, a[2]);
    check_val(23, a[3]);
    check_val(42, a[4]);
    check_val(55, a[5]);
    check_val(71, a[6]);
    check_val(99, a[7]);
}

/* ---- Test 3: std::vector ---- */

static void test_vector() {
    std::vector<unsigned int> v;

    v.push_back(10);
    v.push_back(20);
    v.push_back(30);
    v.push_back(40);
    v.push_back(50);

    check_val(5, (unsigned int)v.size());
    check_val(10, v[0]);
    check_val(50, v[4]);

    /* Sum via range-based iteration */
    unsigned int sum = 0;
    for (auto val : v)
        sum += val;
    check_val(150, sum);

    /* Clear and re-push to exercise reallocation */
    v.clear();
    v.push_back(100);
    v.push_back(200);
    v.push_back(300);
    check_val(3, (unsigned int)v.size());
    check_val(100, v[0]);
    check_val(300, v[2]);
}

/* ---- Test 4: std::string ---- */

static void test_string() {
    /* Short string — SSO path */
    std::string s("Hello");
    check_val(5, (unsigned int)s.size());
    check_val('H', (unsigned int)(unsigned char)s[0]);
    check_val('o', (unsigned int)(unsigned char)s[4]);

    /* Longer string — heap path (>22 chars to exceed SSO) */
    std::string long_s("The quick brown fox jumps over the lazy dog");
    check_val(43, (unsigned int)long_s.size());
    check_val('T', (unsigned int)(unsigned char)long_s[0]);
    check_val('g', (unsigned int)(unsigned char)long_s[42]);

    /* Concatenation */
    std::string a("foo");
    std::string b("bar");
    std::string c = a + b;
    check_val(6, (unsigned int)c.size());
    check_val('f', (unsigned int)(unsigned char)c[0]);
    check_val('b', (unsigned int)(unsigned char)c[3]);

    /* Equality */
    std::string d("foobar");
    check_val(1, (unsigned int)(c == d));
    check_val(0, (unsigned int)(c == a));
}

/* ---- Main ---- */

extern "C" int main() {
    test_array();
    test_sort();
    test_vector();
    test_string();

    if (check != 0)
        halt_ok();
    else
        fail_loop();
    return 0;
}

/* Explicit instantiation of basic_string<char> — at -O0, out-of-line member
 * functions (e.g. __init) aren't inlined and would normally come from
 * libc++.so. This forces all definitions into this TU. */
template class std::basic_string<char>;
