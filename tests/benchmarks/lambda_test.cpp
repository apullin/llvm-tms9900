/* lambda_test -- C++ lambda test for TMS9900
 *
 * Exercises lambda features on a freestanding 16-bit target:
 *   1. Stateless lambda (no captures)
 *   2. Lambda capture by value
 *   3. Lambda capture by reference
 *   4. Lambda with mutable (capture by value + mutable)
 *   5. Stateless lambda to function pointer conversion
 *   6. Lambda passed to std::sort (custom comparator)
 *   7. Lambda capturing `this` pointer
 *   8. Nested lambdas (lambda returning a lambda)
 *
 * Accumulates a checksum; halts with IDLE on correct result.
 */

// Must define before any includes
#define _LIBCPP_VERBOSE_ABORT(...) __builtin_trap()

#include <array>
#include <algorithm>

typedef __SIZE_TYPE__ size_t;

__attribute__((noinline)) static void halt_ok(void) { __asm__ volatile("idle"); }
__attribute__((noinline)) static void fail_loop(void) { for (;;) {} }

static unsigned int check = 0;

static void check_val(unsigned int expected, unsigned int actual) {
    if (expected != actual)
        fail_loop();
    check ^= actual;
}

/* ---- Test 1: Stateless lambda ---- */

static void test_stateless_lambda() {
    auto add = [](int a, int b) { return a + b; };
    check_val(30, (unsigned int)add(10, 20));
    check_val(0, (unsigned int)add(-5, 5));
    check_val(100, (unsigned int)add(99, 1));
}

/* ---- Test 2: Capture by value ---- */

static void test_capture_by_value() {
    int x = 42;
    int y = 58;
    auto sum_xy = [x, y]() { return x + y; };
    check_val(100, (unsigned int)sum_xy());

    /* Verify capture is a copy: changing x doesn't affect lambda */
    x = 0;
    check_val(100, (unsigned int)sum_xy());
}

/* ---- Test 3: Capture by reference ---- */

static void test_capture_by_ref() {
    int counter = 0;
    auto increment = [&counter](int amount) { counter += amount; };

    increment(10);
    check_val(10, (unsigned int)counter);

    increment(25);
    check_val(35, (unsigned int)counter);

    /* Capture-all by reference */
    int a = 3, b = 7;
    auto swap_ab = [&]() {
        int tmp = a;
        a = b;
        b = tmp;
    };
    swap_ab();
    check_val(7, (unsigned int)a);
    check_val(3, (unsigned int)b);
}

/* ---- Test 4: Mutable lambda ---- */

static void test_mutable_lambda() {
    int x = 10;
    auto counter = [x]() mutable -> int {
        x += 5;
        return x;
    };

    /* Each call increments the lambda's own copy */
    check_val(15, (unsigned int)counter());
    check_val(20, (unsigned int)counter());
    check_val(25, (unsigned int)counter());

    /* Original x is unchanged */
    check_val(10, (unsigned int)x);
}

/* ---- Test 5: Stateless lambda to function pointer ---- */

static void test_lambda_to_fptr() {
    int (*fp)(int, int) = [](int a, int b) -> int { return a * b; };
    check_val(200, (unsigned int)fp(10, 20));
    check_val(0, (unsigned int)fp(0, 999));
    check_val(1, (unsigned int)fp(1, 1));

    /* Reassign to a different lambda-converted function pointer */
    fp = [](int a, int b) -> int { return a - b; };
    check_val(80, (unsigned int)fp(100, 20));
}

/* ---- Test 6: Lambda with std::sort ---- */

static void test_lambda_sort() {
    std::array<unsigned int, 6> a = {42, 17, 99, 3, 55, 8};

    /* Sort descending using a lambda comparator */
    std::sort(a.begin(), a.end(), [](unsigned int x, unsigned int y) {
        return x > y;
    });

    /* Verify descending order */
    check_val(99, a[0]);
    check_val(55, a[1]);
    check_val(42, a[2]);
    check_val(17, a[3]);
    check_val(8, a[4]);
    check_val(3, a[5]);
}

/* ---- Test 7: Lambda capturing this ---- */

struct Accumulator {
    unsigned int total;

    Accumulator() : total(0) {}

    void add_values(const unsigned int *vals, int count) {
        auto adder = [this](unsigned int v) {
            this->total += v;
        };
        for (int i = 0; i < count; i++) {
            adder(vals[i]);
        }
    }

    unsigned int get_total() const { return total; }
};

static void test_lambda_this_capture() {
    Accumulator acc;
    unsigned int vals[] = {10, 20, 30, 40};
    acc.add_values(vals, 4);
    check_val(100, acc.get_total());

    /* Call again to accumulate further */
    unsigned int more[] = {5, 15};
    acc.add_values(more, 2);
    check_val(120, acc.get_total());
}

/* ---- Test 8: Nested lambdas ---- */

static void test_nested_lambdas() {
    /* A lambda that returns a lambda (via auto) */
    int base = 100;
    auto make_adder = [base](int offset) {
        return [base, offset](int x) {
            return base + offset + x;
        };
    };

    auto add10 = make_adder(10);
    auto add20 = make_adder(20);

    check_val(111, (unsigned int)add10(1));   /* 100 + 10 + 1 = 111 */
    check_val(125, (unsigned int)add20(5));   /* 100 + 20 + 5 = 125 */
    check_val(200, (unsigned int)add10(90));  /* 100 + 10 + 90 = 200 */
}

/* ---- Main ---- */

extern "C" int main() {
    test_stateless_lambda();
    test_capture_by_value();
    test_capture_by_ref();
    test_mutable_lambda();
    test_lambda_to_fptr();
    test_lambda_sort();
    test_lambda_this_capture();
    test_nested_lambdas();

    if (check != 0)
        halt_ok();
    else
        fail_loop();
    return 0;
}
