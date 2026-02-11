/* stl_util_test — libc++ STL utilities and algorithms test for TMS9900
 *
 * Exercises non-allocating STL features on a freestanding 16-bit target:
 *   1. std::pair
 *   2. std::tuple
 *   3. std::optional
 *   4. std::string_view
 *   5. std::unique_ptr (dynamic allocation)
 *   6. std::initializer_list
 *   7. std::bitset
 *   8. std::numeric_limits
 *   9. Algorithms: find, count, reverse, min, max
 *
 * Accumulates a checksum; halts with IDLE on correct result.
 */

// Must define before any includes
#define _LIBCPP_VERBOSE_ABORT(...) __builtin_trap()

#include <utility>
#include <tuple>
#include <optional>
#include <string_view>
#include <memory>
#include <initializer_list>
#include <bitset>
#include <limits>
#include <algorithm>
#include <array>

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

/* ---- Test 1: std::pair ---- */

static void test_pair() {
    auto p = std::make_pair(42u, 99u);
    check_val(42, p.first);
    check_val(99, p.second);

    /* Structured swap via std::swap */
    std::swap(p.first, p.second);
    check_val(99, p.first);
    check_val(42, p.second);
}

/* ---- Test 2: std::tuple ---- */

static void test_tuple() {
    auto t = std::make_tuple(10u, 20u, 30u);
    check_val(10, std::get<0>(t));
    check_val(20, std::get<1>(t));
    check_val(30, std::get<2>(t));

    check_val(3, (unsigned int)std::tuple_size<decltype(t)>::value);
}

/* ---- Test 3: std::optional ---- */

static void test_optional() {
    std::optional<unsigned int> empty;
    check_val(0, (unsigned int)empty.has_value());

    std::optional<unsigned int> engaged(77);
    check_val(1, (unsigned int)engaged.has_value());
    check_val(77, engaged.value());

    check_val(55, empty.value_or(55));
    check_val(77, engaged.value_or(55));
}

/* ---- Test 4: std::string_view ---- */

static void test_string_view() {
    std::string_view sv("Hello, world!");
    check_val(13, (unsigned int)sv.size());
    check_val('H', (unsigned int)(unsigned char)sv[0]);
    check_val('!', (unsigned int)(unsigned char)sv[12]);

    std::string_view sub = sv.substr(7, 5);
    check_val(5, (unsigned int)sub.size());
    check_val('w', (unsigned int)(unsigned char)sub[0]);
    check_val('d', (unsigned int)(unsigned char)sub[4]);
}

/* ---- Test 5: std::unique_ptr ---- */

static void test_unique_ptr() {
    std::unique_ptr<unsigned int> p(new unsigned int(42));
    check_val(42, *p);

    *p = 100;
    check_val(100, *p);

    unsigned int *raw = p.release();
    check_val(1, (unsigned int)(p.get() == nullptr));
    check_val(100, *raw);
    delete raw;

    p.reset(new unsigned int(200));
    check_val(200, *p);
}

/* ---- Test 6: std::initializer_list ---- */

static unsigned int sum_ilist(std::initializer_list<unsigned int> il) {
    unsigned int sum = 0;
    for (auto v : il)
        sum += v;
    return sum;
}

static void test_initializer_list() {
    check_val(150, sum_ilist({10, 20, 30, 40, 50}));
    check_val(5, (unsigned int)std::initializer_list<unsigned int>({10, 20, 30, 40, 50}).size());
}

/* ---- Test 7: std::bitset ---- */

static void test_bitset() {
    std::bitset<16> bs;
    check_val(0, (unsigned int)bs.count());

    bs.set(3);
    bs.set(7);
    bs.set(15);
    check_val(3, (unsigned int)bs.count());
    check_val(1, (unsigned int)bs.test(3));
    check_val(0, (unsigned int)bs.test(4));
    check_val(1, (unsigned int)bs.test(15));

    bs.flip(3);
    check_val(0, (unsigned int)bs.test(3));
    check_val(2, (unsigned int)bs.count());
}

/* ---- Test 8: std::numeric_limits ---- */

static void test_numeric_limits() {
    /* TMS9900: int is 16-bit */
    check_val(0x7FFF, (unsigned int)std::numeric_limits<int>::max());
    /* min() for signed int is -32768 = 0x8000 as unsigned */
    check_val(0x8000, (unsigned int)std::numeric_limits<int>::min());
    check_val(0xFFFF, std::numeric_limits<unsigned int>::max());
    check_val(0, std::numeric_limits<unsigned int>::min());
    check_val(1, (unsigned int)std::numeric_limits<int>::is_signed);
    check_val(0, (unsigned int)std::numeric_limits<unsigned int>::is_signed);
}

/* ---- Test 9: Algorithms ---- */

static void test_algorithms() {
    std::array<unsigned int, 6> a = {5, 3, 8, 1, 9, 3};

    /* std::min / std::max */
    check_val(10, std::min(10u, 20u));
    check_val(20, std::max(10u, 20u));

    /* std::find */
    auto it = std::find(a.begin(), a.end(), 8u);
    check_val(8, *it);
    check_val(2, (unsigned int)(it - a.begin()));

    /* std::count */
    check_val(2, (unsigned int)std::count(a.begin(), a.end(), 3u));
    check_val(0, (unsigned int)std::count(a.begin(), a.end(), 99u));

    /* std::reverse */
    std::array<unsigned int, 4> r = {1, 2, 3, 4};
    std::reverse(r.begin(), r.end());
    check_val(4, r[0]);
    check_val(3, r[1]);
    check_val(2, r[2]);
    check_val(1, r[3]);

    /* std::min_element / std::max_element */
    check_val(1, *std::min_element(a.begin(), a.end()));
    check_val(9, *std::max_element(a.begin(), a.end()));
}

/* ---- Main ---- */

extern "C" int main() {
    test_pair();
    test_tuple();
    test_optional();
    test_string_view();
    test_unique_ptr();
    test_initializer_list();
    test_bitset();
    test_numeric_limits();
    test_algorithms();

    if (check != 0)
        halt_ok();
    else
        fail_loop();
    return 0;
}
