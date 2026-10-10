#include <systemc>
#include <gtest/gtest.h>
#include <iostream>

#include "MAC.h"
#include "ArithmeticPolicy.h"

using namespace sc_core;
using namespace sc_dt;

// ============================================================
// MAC Testbench
// ============================================================

template<typename W, typename A, typename P, typename Policy>
struct MACTestBench : sc_module {
    sc_signal<bool> clk{"clk"};
    sc_signal<bool> reset{"reset"};
    sc_signal<bool> enable{"enable"};

    sc_signal<W> weight{"weight"};
    sc_signal<A> ifmap{"ifmap"};
    sc_signal<P> psum{"psum"};
    sc_signal<P> result{"result"};

    MAC<W, A, P, Policy> mac{"mac"};

    SC_CTOR(MACTestBench) {
        mac.clk(clk);
        mac.reset(reset);
        mac.enable(enable);
        mac.weight(weight);
        mac.ifmap(ifmap);
        mac.psum(psum);
        mac.result(result);

        clk.write(false);
        reset.write(true);
        enable.write(false);
        weight.write(0);
        ifmap.write(0);
        psum.write(0);
    }

    void tick() {
        clk.write(false);
        sc_start(1, SC_NS);

        clk.write(true);
        sc_start(1, SC_NS);

        clk.write(false);
        sc_start(1, SC_NS);
    }

    void set_inputs(W w, A a, P p) {
        weight.write(w);
        ifmap.write(a);
        psum.write(p);
    }

    void reset_mac() {
        reset.write(true);
        tick();

        reset.write(false);
        tick();
    }

    void check_result(
        const char* name,
        W w,
        A a,
        P p,
        int expected
    ) {
        enable.write(true);
        set_inputs(w, a, p);
        tick();

        const int actual = result.read().to_int();
        const bool passed = (actual == expected);

        std::cout
            << "[" << (passed ? "PASS" : "FAIL") << "] "
            << name
            << " | weight=" << w
            << " | ifmap=" << a
            << " | psum=" << p
            << " | expected=" << expected
            << " | actual=" << actual
            << '\n';

        EXPECT_EQ(actual, expected) << name;
    }

    void check_held_output(
        const char* name,
        W w,
        A a,
        P p,
        int expected
    ) {
        enable.write(false);
        set_inputs(w, a, p);
        tick();

        const int actual = result.read().to_int();
        const bool passed = (actual == expected);

        std::cout
            << "[" << (passed ? "PASS" : "FAIL") << "] "
            << name
            << " | enable=0"
            << " | expected_output=" << expected
            << " | actual_output=" << actual
            << '\n';

        EXPECT_EQ(actual, expected) << name;
    }

    void check_reset() {
        reset_mac();

        const int actual = result.read().to_int();
        const bool passed = (actual == 0);

        std::cout
            << "[" << (passed ? "PASS" : "FAIL") << "] "
            << "Reset output"
            << " | expected=0"
            << " | actual=" << actual
            << '\n';

        EXPECT_EQ(actual, 0);
    }
};

using ExactTB = MACTestBench<
    sc_int<8>, sc_int<8>, sc_int<32>, ExactArithmetic
>;

using ApproxTB = MACTestBench<
    sc_int<8>, sc_int<8>, sc_int<32>, ApproximateArithmetic
>;

static ExactTB* exact_tb = nullptr;
static ApproxTB* approx_tb = nullptr;

// ============================================================
// Exact MAC tests
// ============================================================

TEST(MACTest, ExactArithmetic) {
    auto& tb = *exact_tb;

    std::cout << "\n========== EXACT MAC TEST ==========\n";

    tb.check_reset();

    // 3 * 5 + 10 = 25
    tb.check_result("Positive operands", 3, 5, 10, 25);

    // -3 * 5 + 10 = -5
    tb.check_result("Negative weight", -3, 5, 10, -5);

    // 3 * -5 + 10 = -5
    tb.check_result("Negative activation", 3, -5, 10, -5);

    // 0 * 100 + 7 = 7
    tb.check_result("Zero product", 0, 100, 7, 7);

    // Previous result is 7; disabled MAC must hold it.
    tb.check_held_output("Disabled MAC holds output", 7, 8, 100, 7);

    tb.check_reset();

    std::cout << "====================================\n";
}

// ============================================================
// Approximate MAC tests
// ============================================================

TEST(MACTest, ApproximateArithmetic) {
    auto& tb = *approx_tb;

    std::cout << "\n======== APPROXIMATE MAC TEST ========\n";

    tb.check_reset();

    // 3 * 5 = 15 -> nearest multiple of 4 is 16.
    // 16 + 10 = 26.
    tb.check_result("Round 15 to 16", 3, 5, 10, 26);

    // 4 * 4 = 16; 16 + 3 = 19.
    tb.check_result("Already multiple of four", 4, 4, 3, 19);

    // 0 * 100 + 7 = 7.
    tb.check_result("Zero product", 0, 100, 7, 7);

    // -3 * 5 = -15 -> -16; -16 + 10 = -6.
    tb.check_result("Negative product", -3, 5, 10, -6);

    // 2 * 3 = 6 -> 8; 8 + 4 = 12.
    tb.check_result("Round 6 to 8", 2, 3, 4, 12);

    // Previous result is 12; disabled MAC must hold it.
    tb.check_held_output("Disabled MAC holds output", 7, 8, 100, 12);

    tb.check_reset();

    std::cout << "======================================\n";
}

// ============================================================
// SystemC entry point
// ============================================================

int sc_main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);

    static ExactTB exact{"exact_tb"};
    static ApproxTB approximate{"approximate_tb"};

    exact_tb = &exact;
    approx_tb = &approximate;

    return RUN_ALL_TESTS();
}