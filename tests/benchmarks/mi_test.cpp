/* mi_test — Multiple Inheritance and Advanced Vtable Tests for TMS9900
 *
 * Exercises:
 *   1. Simple multiple inheritance (C : A, B)
 *   2. This-pointer adjustment (C* -> B*, virtual call)
 *   3. Diamond inheritance (non-virtual)
 *   4. Virtual inheritance (shared base)
 *   5. Deep hierarchy (4 levels)
 *   6. Override from both bases in MI
 *   7. Non-virtual base with data members (layout verification)
 *   8. Upcasting and downcasting (static_cast)
 *
 * Build: clang --target=tms9900 -O2 -ffreestanding -fno-builtin -nostdlib
 *        -fno-exceptions -fno-rtti -fno-threadsafe-statics -nostdinc++
 *
 * Uses noinline dispatch helpers and escape() to prevent the optimizer from
 * devirtualizing calls, ensuring vtable dispatch is exercised at all -O levels.
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

/* Prevent the compiler from knowing the concrete type behind a pointer.
 * The inline asm clobbers nothing but acts as a compiler barrier that
 * prevents devirtualization / constant-folding of the pointer. */
template<typename T>
__attribute__((always_inline))
static inline T *escape(T *p) {
    __asm__ volatile("" : "+r"(p) : : );
    return p;
}

/* ================================================================
 * Test 1: Simple Multiple Inheritance
 *
 * C inherits from both A and B. Each base has a virtual method.
 * Call both through C object and through base pointers.
 * ================================================================ */

struct MI_A {
    virtual unsigned int get_a() { return 0xAA; }
    virtual ~MI_A() {}
};

struct MI_B {
    virtual unsigned int get_b() { return 0xBB; }
    virtual ~MI_B() {}
};

struct MI_C : public MI_A, public MI_B {
    unsigned int get_a() override { return 0xCA; }
    unsigned int get_b() override { return 0xCB; }
};

__attribute__((noinline))
static void test_simple_mi() {
    MI_C c;

    /* Call through A pointer (escaped to prevent devirtualization) */
    MI_A *pa = escape(static_cast<MI_A *>(&c));
    check_val(0xCA, pa->get_a());

    /* Call through B pointer */
    MI_B *pb = escape(static_cast<MI_B *>(&c));
    check_val(0xCB, pb->get_b());
}

/* ================================================================
 * Test 2: This-pointer Adjustment
 *
 * When casting C* to B*, the pointer must be adjusted to point to
 * the B subobject. Verify by using data members that depend on
 * correct `this` pointer.
 * ================================================================ */

struct Adj_A {
    unsigned int a_val;
    Adj_A() : a_val(0x1111) {}
    virtual unsigned int get_val() { return a_val; }
    virtual ~Adj_A() {}
};

struct Adj_B {
    unsigned int b_val;
    Adj_B() : b_val(0x2222) {}
    virtual unsigned int get_val() { return b_val; }
    virtual ~Adj_B() {}
};

struct Adj_C : public Adj_A, public Adj_B {
    unsigned int c_val;
    Adj_C() : c_val(0x3333) {}
    unsigned int get_val() override { return c_val; }
};

__attribute__((noinline))
static void test_this_adjustment() {
    Adj_C c;

    /* Verify data members initialized correctly */
    check_val(0x1111, c.a_val);
    check_val(0x2222, c.b_val);
    check_val(0x3333, c.c_val);

    /* Cast to A* — should use C's override, returning c_val */
    Adj_A *pa = escape(static_cast<Adj_A *>(&c));
    check_val(0x3333, pa->get_val());

    /* Cast to B* — this pointer adjusts, thunk adjusts back, calls C's override */
    Adj_B *pb = escape(static_cast<Adj_B *>(&c));
    check_val(0x3333, pb->get_val());

    /* Verify B pointer is offset from C pointer */
    unsigned int c_addr = (unsigned int)(Adj_A *)&c;
    unsigned int b_addr = (unsigned int)pb;
    /* B subobject should be at a higher address than A/C start */
    if (b_addr <= c_addr)
        fail_loop();
    check_val(1, 1); /* marker: pointer adjustment verified */
}

/* ================================================================
 * Test 3: Diamond Inheritance (non-virtual)
 *
 * D_Base -> D_Left, D_Base -> D_Right, D_Diamond : D_Left, D_Right
 * Without virtual inheritance, D_Diamond has TWO D_Base subobjects.
 * ================================================================ */

struct D_Base {
    unsigned int base_val;
    D_Base(unsigned int v) : base_val(v) {}
    virtual unsigned int get_base() { return base_val; }
    virtual ~D_Base() {}
};

struct D_Left : public D_Base {
    unsigned int left_val;
    D_Left(unsigned int bv, unsigned int lv) : D_Base(bv), left_val(lv) {}
    virtual unsigned int get_left() { return left_val; }
};

struct D_Right : public D_Base {
    unsigned int right_val;
    D_Right(unsigned int bv, unsigned int rv) : D_Base(bv), right_val(rv) {}
    virtual unsigned int get_right() { return right_val; }
};

struct D_Diamond : public D_Left, public D_Right {
    unsigned int diamond_val;
    D_Diamond() : D_Left(0x10, 0x20), D_Right(0x30, 0x40), diamond_val(0x50) {}
};

__attribute__((noinline))
static void test_diamond_nonvirtual() {
    D_Diamond d;

    /* Access both Base subobjects explicitly */
    check_val(0x10, d.D_Left::base_val);
    check_val(0x30, d.D_Right::base_val);

    /* Access derived members */
    check_val(0x20, d.left_val);
    check_val(0x40, d.right_val);
    check_val(0x50, d.diamond_val);

    /* Virtual dispatch through Left and Right (escaped) */
    D_Left *pl = escape(static_cast<D_Left *>(&d));
    check_val(0x20, pl->get_left());

    D_Right *pr = escape(static_cast<D_Right *>(&d));
    check_val(0x40, pr->get_right());

    /* Each path to D_Base gives different value */
    D_Base *pbl = escape(static_cast<D_Base *>(pl));
    D_Base *pbr = escape(static_cast<D_Base *>(pr));
    check_val(0x10, pbl->get_base());
    check_val(0x30, pbr->get_base());
}

/* ================================================================
 * Test 4: Virtual Inheritance
 *
 * Diamond with virtual inheritance: single shared V_Base.
 * Note: virtual inheritance may fail with -fno-rtti on some
 * compilers. If it does, this test will be documented as skipped.
 * ================================================================ */

struct V_Base {
    unsigned int vbase_val;
    V_Base() : vbase_val(0) {}
    V_Base(unsigned int v) : vbase_val(v) {}
    virtual unsigned int get_vbase() { return vbase_val; }
    virtual ~V_Base() {}
};

struct V_Left : virtual public V_Base {
    unsigned int vleft_val;
    V_Left() : vleft_val(0xAA) {}
    virtual unsigned int get_vleft() { return vleft_val; }
};

struct V_Right : virtual public V_Base {
    unsigned int vright_val;
    V_Right() : vright_val(0xBB) {}
    virtual unsigned int get_vright() { return vright_val; }
};

struct V_Diamond : public V_Left, public V_Right {
    unsigned int vdiamond_val;
    V_Diamond() : vdiamond_val(0xCC) {
        /* V_Base is shared, default-constructed with vbase_val=0 */
        vbase_val = 0xDD;
    }
};

__attribute__((noinline))
static void test_diamond_virtual() {
    V_Diamond vd;

    /* Single shared V_Base — vbase_val set to 0xDD */
    check_val(0xDD, vd.vbase_val);

    /* Both Left and Right paths see the same V_Base */
    check_val(0xDD, vd.V_Left::vbase_val);
    check_val(0xDD, vd.V_Right::vbase_val);

    /* Verify the addresses are the same (single V_Base) */
    V_Base *pbl = escape(static_cast<V_Base *>(static_cast<V_Left *>(&vd)));
    V_Base *pbr = escape(static_cast<V_Base *>(static_cast<V_Right *>(&vd)));
    if ((unsigned int)pbl != (unsigned int)pbr)
        fail_loop();
    check_val(0xDD, pbl->get_vbase());

    /* Access Left and Right members */
    check_val(0xAA, vd.vleft_val);
    check_val(0xBB, vd.vright_val);
    check_val(0xCC, vd.vdiamond_val);
}

/* ================================================================
 * Test 5: Deep Hierarchy (4 levels)
 *
 * Base -> Mid1 -> Mid2 -> Leaf
 * Each level adds a virtual method. Call all through Base*.
 * ================================================================ */

struct Deep_Base {
    virtual unsigned int level0() { return 0; }
    virtual ~Deep_Base() {}
};

struct Deep_Mid1 : public Deep_Base {
    virtual unsigned int level1() { return 1; }
    unsigned int level0() override { return 10; }
};

struct Deep_Mid2 : public Deep_Mid1 {
    virtual unsigned int level2() { return 2; }
    unsigned int level0() override { return 20; }
    unsigned int level1() override { return 21; }
};

struct Deep_Leaf : public Deep_Mid2 {
    virtual unsigned int level3() { return 3; }
    unsigned int level0() override { return 30; }
    unsigned int level1() override { return 31; }
    unsigned int level2() override { return 32; }
};

__attribute__((noinline))
static void test_deep_hierarchy() {
    Deep_Leaf leaf;

    /* Call through Base pointer — should use Leaf's overrides */
    Deep_Base *pb = escape(static_cast<Deep_Base *>(&leaf));
    check_val(30, pb->level0());

    /* Call through Mid1 pointer */
    Deep_Mid1 *pm1 = escape(static_cast<Deep_Mid1 *>(&leaf));
    check_val(30, pm1->level0());
    check_val(31, pm1->level1());

    /* Call through Mid2 pointer */
    Deep_Mid2 *pm2 = escape(static_cast<Deep_Mid2 *>(&leaf));
    check_val(30, pm2->level0());
    check_val(31, pm2->level1());
    check_val(32, pm2->level2());

    /* Call Leaf-specific method directly */
    Deep_Leaf *pleaf = escape(&leaf);
    check_val(3, pleaf->level3());

    /* Create Mid2 object — should use Mid2's overrides, not Leaf's */
    Deep_Mid2 m2;
    Deep_Base *pb2 = escape(static_cast<Deep_Base *>(&m2));
    check_val(20, pb2->level0());
}

/* ================================================================
 * Test 6: Overriding from Both Bases in MI
 *
 * C overrides virtual methods from both A and B.
 * Call through both base pointers to verify correct dispatch.
 * ================================================================ */

struct Ovr_A {
    virtual unsigned int compute() { return 100; }
    virtual unsigned int id() { return 1; }
    virtual ~Ovr_A() {}
};

struct Ovr_B {
    virtual unsigned int compute() { return 200; }
    virtual unsigned int id() { return 2; }
    virtual ~Ovr_B() {}
};

struct Ovr_C : public Ovr_A, public Ovr_B {
    unsigned int val;
    Ovr_C(unsigned int v) : val(v) {}
    unsigned int compute() override { return val; }
    unsigned int id() override { return 42; }
};

__attribute__((noinline))
static void test_override_both_bases() {
    Ovr_C c(999);

    /* Through A pointer */
    Ovr_A *pa = escape(static_cast<Ovr_A *>(&c));
    check_val(999, pa->compute());
    check_val(42, pa->id());

    /* Through B pointer — thunk adjusts this, calls C's method */
    Ovr_B *pb = escape(static_cast<Ovr_B *>(&c));
    check_val(999, pb->compute());
    check_val(42, pb->id());
}

/* ================================================================
 * Test 7: Non-virtual Base with Data Members (Layout Verification)
 *
 * Verify that data member layout is correct when multiple bases
 * each have data members.
 * ================================================================ */

struct Layout_A {
    unsigned int a1;
    unsigned int a2;
    Layout_A() : a1(0x1234), a2(0x5678) {}
};

struct Layout_B {
    unsigned int b1;
    unsigned int b2;
    unsigned int b3;
    Layout_B() : b1(0xAAAA), b2(0xBBBB), b3(0xCCCC) {}
};

struct Layout_C : public Layout_A, public Layout_B {
    unsigned int c1;
    Layout_C() : c1(0xDDDD) {}

    __attribute__((noinline)) unsigned int sum_a() { return a1 + a2; }
    __attribute__((noinline)) unsigned int sum_b() { return b1 + b2 + b3; }
};

__attribute__((noinline))
static void test_data_layout() {
    Layout_C lc;

    /* Verify all members accessible */
    check_val(0x1234, lc.a1);
    check_val(0x5678, lc.a2);
    check_val(0xAAAA, lc.b1);
    check_val(0xBBBB, lc.b2);
    check_val(0xCCCC, lc.b3);
    check_val(0xDDDD, lc.c1);

    /* Verify member functions access correct data */
    check_val(0x1234 + 0x5678, lc.sum_a());
    check_val((unsigned int)((unsigned long)0xAAAA + 0xBBBB + 0xCCCC), lc.sum_b());

    /* Verify casting preserves correct data access */
    Layout_A *pa = &lc;
    check_val(0x1234, pa->a1);
    check_val(0x5678, pa->a2);

    Layout_B *pb = &lc;
    check_val(0xAAAA, pb->b1);
    check_val(0xBBBB, pb->b2);
    check_val(0xCCCC, pb->b3);
}

/* ================================================================
 * Test 8: Upcasting and Downcasting (static_cast)
 *
 * Test static_cast between base and derived pointers.
 * ================================================================ */

struct Cast_A {
    unsigned int a_marker;
    Cast_A() : a_marker(0xA000) {}
    virtual unsigned int who() { return 0xA; }
    virtual ~Cast_A() {}
};

struct Cast_B {
    unsigned int b_marker;
    Cast_B() : b_marker(0xB000) {}
    virtual unsigned int who() { return 0xB; }
    virtual ~Cast_B() {}
};

struct Cast_D : public Cast_A, public Cast_B {
    unsigned int d_marker;
    Cast_D() : d_marker(0xD000) {}
    unsigned int who() override { return 0xD; }
};

__attribute__((noinline))
static void test_static_cast() {
    Cast_D d;

    /* Upcast to A */
    Cast_A *pa = escape(static_cast<Cast_A *>(&d));
    check_val(0xD, pa->who());  /* Virtual dispatch to Cast_D::who */
    check_val(0xA000, pa->a_marker);

    /* Upcast to B — pointer adjustment */
    Cast_B *pb = escape(static_cast<Cast_B *>(&d));
    check_val(0xD, pb->who());  /* Virtual dispatch to Cast_D::who */
    check_val(0xB000, pb->b_marker);

    /* Downcast from A* back to D* */
    Cast_D *pd1 = static_cast<Cast_D *>(pa);
    check_val(0xD000, pd1->d_marker);
    check_val(0xA000, pd1->a_marker);
    check_val(0xB000, pd1->b_marker);

    /* Downcast from B* back to D* — must adjust pointer backwards */
    Cast_D *pd2 = static_cast<Cast_D *>(pb);
    check_val(0xD000, pd2->d_marker);
    check_val(0xA000, pd2->a_marker);
    check_val(0xB000, pd2->b_marker);

    /* Verify both downcasts yield same address */
    if ((unsigned int)pd1 != (unsigned int)pd2)
        fail_loop();
    check_val(0xD, escape(pd1)->who());
}

/* ================================================================
 * Main
 * ================================================================ */

extern "C" int main() {
    test_simple_mi();
    test_this_adjustment();
    test_diamond_nonvirtual();
    test_diamond_virtual();
    test_deep_hierarchy();
    test_override_both_bases();
    test_data_layout();
    test_static_cast();

    if (check != 0)
        halt_ok();
    else
        fail_loop();
    return 0;
}
