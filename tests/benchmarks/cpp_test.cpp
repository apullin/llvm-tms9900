/* cpp_test — C++ feature test for TMS9900
 *
 * Exercises the four pillars of C++ on a freestanding 16-bit target:
 *   1. Global constructors (.init_array iteration)
 *   2. Virtual dispatch (vtable + indirect call)
 *   3. Templates (instantiation for multiple types)
 *   4. Dynamic allocation (operator new/delete)
 *
 * Accumulates a checksum; halts with IDLE on correct result.
 */

typedef __SIZE_TYPE__ size_t;

/* Forward declarations for C++ runtime */
void *operator new(size_t);
void operator delete(void *) noexcept;
void operator delete(void *, size_t) noexcept;

__attribute__((noinline)) static void halt_ok(void) { __asm__ volatile("idle"); }
__attribute__((noinline)) static void fail_loop(void) { for (;;) {} }

static unsigned int check = 0;

static void check_val(unsigned int expected, unsigned int actual) {
    if (expected != actual)
        fail_loop();
    check ^= actual;
}

/* ---- Test 1: Global constructor ---- */

struct GlobalInit {
    unsigned int val;
    GlobalInit(unsigned int v) : val(v) {}
};

static GlobalInit g_init(0xBEEF);

static void test_global_ctor() {
    /* g_init.val should have been set to 0xBEEF before main() */
    check_val(0xBEEF, g_init.val);
}

/* ---- Test 2: Virtual dispatch ---- */

struct Base {
    virtual unsigned int value() = 0;
    virtual ~Base() {}
};

struct DerivedA : Base {
    unsigned int x;
    DerivedA(unsigned int v) : x(v) {}
    unsigned int value() override { return x + 1; }
};

struct DerivedB : Base {
    unsigned int x;
    DerivedB(unsigned int v) : x(v) {}
    unsigned int value() override { return x * 2; }
};

static void test_virtual_dispatch() {
    DerivedA a(10);
    DerivedB b(10);

    Base *pa = &a;
    Base *pb = &b;

    check_val(11, pa->value());   /* 10 + 1 = 11 */
    check_val(20, pb->value());   /* 10 * 2 = 20 */
}

/* ---- Test 3: Templates ---- */

template<typename T>
__attribute__((noinline))
T add(T a, T b) {
    return a + b;
}

template<typename T>
__attribute__((noinline))
T mul(T a, T b) {
    return a * b;
}

static void test_templates() {
    /* 16-bit instantiation */
    unsigned int r16 = add<unsigned int>(100, 200);
    check_val(300, r16);

    unsigned int m16 = mul<unsigned int>(25, 12);
    check_val(300, m16);

    /* 32-bit instantiation */
    unsigned long r32 = add<unsigned long>(50000UL, 60000UL);
    check_val((unsigned int)(r32 & 0xFFFF), (unsigned int)(110000UL & 0xFFFF));

    unsigned long m32 = mul<unsigned long>(1000UL, 50UL);
    check_val((unsigned int)(m32 & 0xFFFF), (unsigned int)(50000UL & 0xFFFF));
}

/* ---- Test 4: Dynamic allocation ---- */

struct DynObj {
    unsigned int a, b;
    unsigned int sum() { return a + b; }
};

static void test_new_delete() {
    DynObj *p = new DynObj();
    p->a = 42;
    p->b = 58;
    check_val(100, p->sum());
    delete p;

    /* Array-style (manual) */
    DynObj *arr = new DynObj[2];
    arr[0].a = 10;
    arr[0].b = 20;
    arr[1].a = 30;
    arr[1].b = 40;
    check_val(30, arr[0].sum());
    check_val(70, arr[1].sum());
    delete[] arr;
}

/* ---- Main ---- */

extern "C" int main() {
    test_global_ctor();
    test_virtual_dispatch();
    test_templates();
    test_new_delete();

    /* Expected checksum:
     * 0xBEEF ^ 11 ^ 20 ^ 300 ^ 300 ^ 0xAE10 ^ 0xC350 ^ 100 ^ 30 ^ 70
     * = computed at runtime, verified by running at O0 first
     */
    if (check != 0)  /* non-zero means tests ran */
        halt_ok();
    else
        fail_loop();
    return 0;
}
