/* cpp_adv_test -- Advanced C++ features test for TMS9900
 *
 * Exercises modern C++ features on a freestanding 16-bit target:
 *   1. Move constructor (ownership transfer)
 *   2. Move assignment (resource cleanup + transfer)
 *   3. std::move (explicit move, source left in moved-from state)
 *   4. Rule of five (all five special members, verify correct one called)
 *   5. Perfect forwarding (lvalue/rvalue dispatch via std::forward)
 *   6. Variadic templates (recursive sum over parameter pack)
 *   7. Structured bindings (std::pair and arrays)
 *   8. constexpr functions (compile-time and runtime evaluation)
 *   9. Enum class (scoped enums with underlying type)
 *  10. Static local initialization (initialized exactly once)
 *
 * Accumulates a checksum; halts with IDLE on correct result.
 */

// Must define before any includes
#define _LIBCPP_VERBOSE_ABORT(...) __builtin_trap()

#include <utility>   // std::move, std::forward, std::pair

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

/* Minimal dynamic array buffer for move semantics testing.
 * Owns a heap-allocated int, tracks via a tag for identification.
 */
struct OwnedInt {
    int *ptr;
    unsigned int tag;

    OwnedInt() : ptr(nullptr), tag(0) {}

    explicit OwnedInt(int val, unsigned int t) : tag(t) {
        ptr = new int;
        *ptr = val;
    }

    ~OwnedInt() {
        if (ptr) {
            delete ptr;
            ptr = nullptr;
        }
    }

    /* Move constructor */
    OwnedInt(OwnedInt&& other) : ptr(other.ptr), tag(other.tag) {
        other.ptr = nullptr;
        other.tag = 0;
    }

    /* Move assignment */
    OwnedInt& operator=(OwnedInt&& other) {
        if (this != &other) {
            /* Clean up existing resource */
            if (ptr) {
                delete ptr;
            }
            ptr = other.ptr;
            tag = other.tag;
            other.ptr = nullptr;
            other.tag = 0;
        }
        return *this;
    }

    /* Disable copy */
    OwnedInt(const OwnedInt&) = delete;
    OwnedInt& operator=(const OwnedInt&) = delete;

    bool valid() const { return ptr != nullptr; }
    int value() const { return ptr ? *ptr : -1; }
};

/* ---- Test 1: Move constructor ---- */

__attribute__((noinline))
static void test_move_constructor() {
    OwnedInt a(42, 1);
    check_val(1, a.valid() ? 1u : 0u);
    check_val(42, (unsigned int)a.value());

    /* Move construct b from a */
    OwnedInt b(static_cast<OwnedInt&&>(a));
    check_val(0, a.valid() ? 1u : 0u);   /* a is now empty */
    check_val(1, b.valid() ? 1u : 0u);   /* b owns the resource */
    check_val(42, (unsigned int)b.value());
    check_val(0, a.tag);                  /* a's tag was zeroed */
    check_val(1, b.tag);                  /* b has a's old tag */
}

/* ---- Test 2: Move assignment ---- */

__attribute__((noinline))
static void test_move_assignment() {
    OwnedInt a(100, 2);
    OwnedInt b(200, 3);

    check_val(100, (unsigned int)a.value());
    check_val(200, (unsigned int)b.value());

    /* Move assign a into b -- b's old resource (200) should be freed */
    b = static_cast<OwnedInt&&>(a);

    check_val(0, a.valid() ? 1u : 0u);   /* a is moved-from */
    check_val(1, b.valid() ? 1u : 0u);   /* b owns a's old resource */
    check_val(100, (unsigned int)b.value());
    check_val(2, b.tag);                  /* b has a's old tag */
}

/* ---- Test 3: std::move ---- */

__attribute__((noinline))
static void test_std_move() {
    OwnedInt a(77, 4);
    check_val(77, (unsigned int)a.value());

    /* std::move casts to rvalue reference, triggering move constructor */
    OwnedInt b(std::move(a));
    check_val(0, a.valid() ? 1u : 0u);
    check_val(77, (unsigned int)b.value());
    check_val(4, b.tag);
}

/* ---- Test 4: Rule of five ---- */

/* Track which special member was last called */
static unsigned int last_special_call = 0;
enum SpecialCall : unsigned int {
    NONE = 0,
    DEFAULT_CTOR = 1,
    COPY_CTOR = 2,
    MOVE_CTOR = 3,
    COPY_ASSIGN = 4,
    MOVE_ASSIGN = 5,
    DTOR = 6
};

struct RuleOfFive {
    int val;

    RuleOfFive() : val(0) {
        last_special_call = DEFAULT_CTOR;
    }

    explicit RuleOfFive(int v) : val(v) {
        last_special_call = DEFAULT_CTOR;
    }

    RuleOfFive(const RuleOfFive& other) : val(other.val) {
        last_special_call = COPY_CTOR;
    }

    RuleOfFive(RuleOfFive&& other) : val(other.val) {
        other.val = -1;
        last_special_call = MOVE_CTOR;
    }

    RuleOfFive& operator=(const RuleOfFive& other) {
        val = other.val;
        last_special_call = COPY_ASSIGN;
        return *this;
    }

    RuleOfFive& operator=(RuleOfFive&& other) {
        val = other.val;
        other.val = -1;
        last_special_call = MOVE_ASSIGN;
        return *this;
    }

    ~RuleOfFive() {
        last_special_call = DTOR;
    }
};

__attribute__((noinline))
static void test_rule_of_five() {
    {
        RuleOfFive a(10);
        check_val(DEFAULT_CTOR, last_special_call);

        RuleOfFive b(a);        /* copy ctor */
        check_val(COPY_CTOR, last_special_call);
        check_val(10, (unsigned int)b.val);

        RuleOfFive c(std::move(a));  /* move ctor */
        check_val(MOVE_CTOR, last_special_call);
        check_val(10, (unsigned int)c.val);
        /* a.val is now -1 (0xFFFF unsigned) */
        check_val(0xFFFF, (unsigned int)a.val);

        RuleOfFive d;
        check_val(DEFAULT_CTOR, last_special_call);

        d = b;                  /* copy assign */
        check_val(COPY_ASSIGN, last_special_call);
        check_val(10, (unsigned int)d.val);

        d = std::move(c);       /* move assign */
        check_val(MOVE_ASSIGN, last_special_call);
        check_val(10, (unsigned int)d.val);
        check_val(0xFFFF, (unsigned int)c.val);  /* c moved-from */
    }
    /* All destructors should have fired */
    check_val(DTOR, last_special_call);
}

/* ---- Test 5: Perfect forwarding ---- */

static unsigned int forward_result = 0;

__attribute__((noinline))
static void process_lvalue(int& x) {
    forward_result = 1;   /* called for lvalue */
    x += 10;
}

__attribute__((noinline))
static void process_rvalue(int&& x) {
    forward_result = 2;   /* called for rvalue */
    (void)x;
}

/* Tag-dispatch implementation: must precede the template */
__attribute__((noinline))
static void forwarder_impl(int& x, std::true_type) {
    process_lvalue(x);
}

__attribute__((noinline))
static void forwarder_impl(int&& x, std::false_type) {
    process_rvalue(std::move(x));
}

/* Overloaded forwarder that dispatches based on value category */
template<typename T>
__attribute__((noinline))
static void forwarder(T&& arg) {
    /* Use tag dispatch: if T deduces as int& -> lvalue; if int -> rvalue */
    forwarder_impl(std::forward<T>(arg), std::is_lvalue_reference<T>{});
}

__attribute__((noinline))
static void test_perfect_forwarding() {
    int lval = 5;

    forwarder(lval);         /* lvalue -> should call process_lvalue */
    check_val(1, forward_result);
    check_val(15, (unsigned int)lval);  /* was modified by process_lvalue */

    forwarder(42);           /* rvalue -> should call process_rvalue */
    check_val(2, forward_result);
}

/* ---- Test 6: Variadic templates ---- */

/* Recursive variadic sum */
__attribute__((noinline))
static int var_sum() {
    return 0;  /* base case */
}

template<typename T, typename... Rest>
__attribute__((noinline))
static int var_sum(T first, Rest... rest) {
    return first + var_sum(rest...);
}

/* Count number of args */
template<typename... Args>
constexpr int count_args(Args...) {
    return sizeof...(Args);
}

__attribute__((noinline))
static void test_variadic_templates() {
    check_val(0, (unsigned int)var_sum());
    check_val(10, (unsigned int)var_sum(10));
    check_val(30, (unsigned int)var_sum(10, 20));
    check_val(60, (unsigned int)var_sum(10, 20, 30));
    check_val(100, (unsigned int)var_sum(10, 20, 30, 40));

    check_val(0, (unsigned int)count_args());
    check_val(1, (unsigned int)count_args(1));
    check_val(3, (unsigned int)count_args(1, 2, 3));
    check_val(5, (unsigned int)count_args(1, 2, 3, 4, 5));
}

/* ---- Test 7: Structured bindings ---- */

__attribute__((noinline))
static void test_structured_bindings() {
    /* Structured binding with std::pair */
    auto p = std::pair<int, int>(10, 20);
    auto [a, b] = p;
    check_val(10, (unsigned int)a);
    check_val(20, (unsigned int)b);

    /* Structured binding with C array */
    int arr[3] = {100, 200, 300};
    auto [x, y, z] = arr;
    check_val(100, (unsigned int)x);
    check_val(200, (unsigned int)y);
    check_val(300, (unsigned int)z);

    /* Structured binding with struct */
    struct Point { int px; int py; };
    Point pt = {7, 13};
    auto [px, py] = pt;
    check_val(7, (unsigned int)px);
    check_val(13, (unsigned int)py);
}

/* ---- Test 8: constexpr functions ---- */

constexpr int factorial(int n) {
    return (n <= 1) ? 1 : n * factorial(n - 1);
}

constexpr int fib_ce(int n) {
    return (n <= 1) ? n : fib_ce(n - 1) + fib_ce(n - 2);
}

__attribute__((noinline))
static void test_constexpr() {
    /* Compile-time evaluation */
    static_assert(factorial(0) == 1, "factorial(0)");
    static_assert(factorial(1) == 1, "factorial(1)");
    static_assert(factorial(5) == 120, "factorial(5)");

    /* Runtime evaluation with the same function */
    volatile int n = 5;  /* volatile prevents constant folding */
    check_val(120, (unsigned int)factorial(n));

    volatile int m = 7;
    check_val(13, (unsigned int)fib_ce(m));   /* fib(7) = 13 */

    /* Compile-time known value used at runtime */
    constexpr int f6 = factorial(6);
    check_val(720, (unsigned int)f6);

    constexpr int fib8 = fib_ce(8);
    check_val(21, (unsigned int)fib8);     /* fib(8) = 21 */
}

/* ---- Test 9: Enum class ---- */

enum class Color : unsigned int {
    Red = 1,
    Green = 2,
    Blue = 4
};

enum class Direction : int {
    North = 0,
    East = 1,
    South = 2,
    West = 3
};

__attribute__((noinline))
static unsigned int color_to_int(Color c) {
    return static_cast<unsigned int>(c);
}

__attribute__((noinline))
static void test_enum_class() {
    check_val(1, color_to_int(Color::Red));
    check_val(2, color_to_int(Color::Green));
    check_val(4, color_to_int(Color::Blue));

    /* Bitwise OR on the underlying type */
    unsigned int rg = static_cast<unsigned int>(Color::Red) |
                      static_cast<unsigned int>(Color::Green);
    check_val(3, rg);

    /* Direction enum */
    Direction d = Direction::South;
    check_val(2, (unsigned int)static_cast<int>(d));

    /* Enum comparison */
    volatile Color r1 = Color::Red;
    check_val(1, (r1 == Color::Red) ? 1u : 0u);
    check_val(0, (Color::Red == Color::Blue) ? 1u : 0u);
    check_val(1, (Direction::East != Direction::West) ? 1u : 0u);
}

/* ---- Test 10: Static local initialization ---- */

static unsigned int static_init_count = 0;

struct StaticInitTracker {
    unsigned int id;
    StaticInitTracker(unsigned int i) : id(i) {
        static_init_count++;
    }
};

__attribute__((noinline))
static StaticInitTracker& get_static_local() {
    static StaticInitTracker instance(42);
    return instance;
}

__attribute__((noinline))
static void test_static_local() {
    check_val(0, static_init_count);  /* not yet initialized */

    StaticInitTracker& ref1 = get_static_local();
    check_val(1, static_init_count);  /* initialized once */
    check_val(42, ref1.id);

    StaticInitTracker& ref2 = get_static_local();
    check_val(1, static_init_count);  /* still just once */
    check_val(42, ref2.id);

    /* Both references should point to the same object */
    check_val(1, (&ref1 == &ref2) ? 1u : 0u);
}

/* ---- Main ---- */

extern "C" int main() {
    test_move_constructor();
    test_move_assignment();
    test_std_move();
    test_rule_of_five();
    test_perfect_forwarding();
    test_variadic_templates();
    test_structured_bindings();
    test_constexpr();
    test_enum_class();
    test_static_local();

    if (check != 0)
        halt_ok();
    else
        fail_loop();
    return 0;
}
