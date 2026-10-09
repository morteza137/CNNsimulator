
#include <systemc>
#include <gtest/gtest.h>

#include <cmath>
#include <iostream>
#include <string>

#include "MAC.h"

using namespace sc_core;
using namespace sc_dt;

template<typename W, typename A, typename P>
struct MACTestbench : sc_module {
    sc_signal<bool> clk{"clk"};
    sc_signal<bool> reset{"reset"};
    sc_signal<bool> enable{"enable"};
    sc_signal<W> weight{"weight"};
    sc_signal<A> ifmap{"ifmap"};
    sc_signal<P> psum{"psum"};
    sc_signal<bool> approximate{"approximate"};
    sc_signal<P> result{"result"};

    MAC<W, A, P> mac{"mac"};

    SC_CTOR(MACTestbench) {
        mac.clk(clk);
        mac.reset(reset);
        mac.enable(enable);
        mac.weight(weight);
        mac.ifmap(ifmap);
        mac.psum(psum);
        mac.approximate(approximate);
        mac.result(result);
    }

    void tick() {
        clk.write(false);
        sc_core::sc_start(1, sc_core::SC_NS);
        clk.write(true);
        sc_core::sc_start(1, sc_core::SC_NS);
        clk.write(false);
        sc_core::sc_start(1, sc_core::SC_NS);
    }

    void set(W w, A a, P p, bool approx = false) {
        weight.write(w);
        ifmap.write(a);
        psum.write(p);
        approximate.write(approx);
    }
};

using IntTB = MACTestbench<sc_int<8>, sc_int<8>, sc_int<32>>;
using WideTB = MACTestbench<sc_int<16>, sc_int<16>, sc_int<48>>;
using FloatTB = MACTestbench<float, float, float>;
using DoubleTB = MACTestbench<double, double, double>;

TEST(MACTest, AllScenarios) {
    // Construct every module before the first simulation step.
    IntTB int_tb{"int_tb"};
    WideTB wide_tb{"wide_tb"};
    FloatTB float_tb{"float_tb"};
    DoubleTB double_tb{"double_tb"};

    auto check_int = [](const std::string& name,
                        long long actual,
                        long long expected) {
        const bool pass = actual == expected;

        std::cout << (pass ? "[ PASS ] " : "[ FAIL ] ")
                  << name
                  << " | expected=" << expected
                  << " | actual=" << actual << '\n';

        EXPECT_EQ(actual, expected) << name;
    };

    auto check_float = [](const std::string& name,
                          float actual,
                          float expected) {
        const bool pass = std::fabs(actual - expected) < 1e-5f;

        std::cout << (pass ? "[ PASS ] " : "[ FAIL ] ")
                  << name
                  << " | expected=" << expected
                  << " | actual=" << actual << '\n';

        EXPECT_NEAR(actual, expected, 1e-5f) << name;
    };

    auto check_double = [](const std::string& name,
                           double actual,
                           double expected) {
        const bool pass = std::fabs(actual - expected) < 1e-10;

        std::cout << (pass ? "[ PASS ] " : "[ FAIL ] ")
                  << name
                  << " | expected=" << expected
                  << " | actual=" << actual << '\n';

        EXPECT_NEAR(actual, expected, 1e-10) << name;
    };

    // Initialize all modules before simulation starts.
    int_tb.reset.write(true);
    int_tb.enable.write(false);
    wide_tb.reset.write(true);
    wide_tb.enable.write(false);
    float_tb.reset.write(true);
    float_tb.enable.write(false);
    double_tb.reset.write(true);
    double_tb.enable.write(false);

    sc_core::sc_start(1, sc_core::SC_NS);

    int_tb.reset.write(false);
    int_tb.enable.write(true);
    wide_tb.reset.write(false);
    wide_tb.enable.write(true);
    float_tb.reset.write(false);
    float_tb.enable.write(true);
    double_tb.reset.write(false);
    double_tb.enable.write(true);

    // 1. Exact positive
    int_tb.set(3, 5, 10);
    int_tb.tick();
    check_int("Exact positive", int_tb.result.read().to_int(), 25);

    // 2. Zero product
    int_tb.set(0, 7, 0);
    int_tb.tick();
    check_int("Zero product", int_tb.result.read().to_int(), 0);

    // 3. Multiplication
    int_tb.set(6, 7, 0);
    int_tb.tick();
    check_int("Multiply", int_tb.result.read().to_int(), 42);

    // 4. Negative weight
    int_tb.set(-3, 5, 10);
    int_tb.tick();
    check_int("Negative weight", int_tb.result.read().to_int(), -5);

    // 5. Negative input activation
    int_tb.set(3, -5, 10);
    int_tb.tick();
    check_int("Negative ifmap", int_tb.result.read().to_int(), -5);

    // 6. Negative partial sum
    int_tb.set(3, 5, -20);
    int_tb.tick();
    check_int("Negative psum", int_tb.result.read().to_int(), -5);

    // 7. Approximate arithmetic
    int_tb.set(3, 5, 10, true);
    int_tb.tick();
    check_int("Approximate product", int_tb.result.read().to_int(), 26);

    // 8. Product already divisible by four
    int_tb.set(4, 4, 3, true);
    int_tb.tick();
    check_int("Approximate multiple of four",
              int_tb.result.read().to_int(), 19);

    // 9. Approximate zero product
    int_tb.set(0, 9, 7, true);
    int_tb.tick();
    check_int("Approximate zero product",
              int_tb.result.read().to_int(), 7);

    // 10. Disabled MAC holds its output
    int_tb.set(2, 3, 4);
    int_tb.tick();
    int_tb.enable.write(false);
    int_tb.set(7, 8, 0);
    int_tb.tick();
    check_int("Disabled holds output", int_tb.result.read().to_int(), 10);
    int_tb.enable.write(true);

    // 11. Reset
    int_tb.reset.write(true);
    int_tb.tick();
    check_int("Reset clears output", int_tb.result.read().to_int(), 0);
    int_tb.reset.write(false);

    // 12. Wider integer inputs
    wide_tb.set(1000, 20, 100);
    wide_tb.tick();
    check_int("16-bit inputs", wide_tb.result.read().to_int64(), 20100);

    // 13. Larger integer values
    wide_tb.set(20000, 2, 1000);
    wide_tb.tick();
    check_int("Large integer values", wide_tb.result.read().to_int64(), 41000);

    // 14. Float exact
    float_tb.set(1.5f, 2.0f, 0.5f);
    float_tb.tick();
    check_float("Float exact", float_tb.result.read(), 3.5f);

    // 15. Fractional operands
    float_tb.set(0.5f, 0.5f, 0.25f);
    float_tb.tick();
    check_float("Float fractions", float_tb.result.read(), 0.5f);

    // 16. Negative float
    float_tb.set(-2.0f, 3.0f, 1.0f);
    float_tb.tick();
    check_float("Float negative", float_tb.result.read(), -5.0f);

    // 17. Approximate float
    float_tb.set(1.5f, 2.0f, 0.5f, true);
    float_tb.tick();
    check_float("Float approximate", float_tb.result.read(), 4.5f);

    // 18. Double precision
    double_tb.set(1.25, 4.0, 0.5);
    double_tb.tick();
    check_double("Double exact", double_tb.result.read(), 5.5);

    // 19. Switch from approximate to exact
    int_tb.set(3, 5, 10, true);
    int_tb.tick();
    check_int("Approximate before switch", int_tb.result.read().to_int(), 26);

    int_tb.set(3, 5, 10, false);
    int_tb.tick();
    check_int("Exact after switch", int_tb.result.read().to_int(), 25);

    // 20. Repeated operations
    for (int i = 1; i <= 10; ++i) {
        int_tb.set(i, 2, 1);
        int_tb.tick();

        check_int("Repeated operation " + std::to_string(i),
                  int_tb.result.read().to_int(), i * 2 + 1);
    }
}

int sc_main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}