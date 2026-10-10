
#include <systemc>
#include <gtest/gtest.h>

#include <iostream>
#include <string>

#include "MUX.h"

using namespace sc_core;
using namespace sc_dt;

SC_MODULE(MUXTestbench) {
    sc_vector<sc_signal<sc_int<8>>> inputs;
    sc_signal<sc_uint<2>> select;
    sc_signal<sc_int<8>> output;

    MUX<4, sc_int<8>> mux;

    SC_CTOR(MUXTestbench)
        : inputs("inputs", 4),
          mux("mux")
    {
        for (int i = 0; i < 4; ++i) {
            mux.input[i](inputs[i]);
        }

        mux.select(select);
        mux.output(output);
    }

    void apply(int a, int b, int c, int d, unsigned int sel) {
        inputs[0].write(a);
        inputs[1].write(b);
        inputs[2].write(c);
        inputs[3].write(d);
        select.write(sel);

        sc_start(1, SC_NS);
    }
};

TEST(MUXTest, AllScenarios) {
    MUXTestbench tb{"tb"};

    int passed = 0;
    int failed = 0;

    auto check = [&](const std::string& name, int expected) {
        const int actual = tb.output.read().to_int();
        const bool ok = actual == expected;

        std::cout << (ok ? "[ PASS ] " : "[ FAIL ] ")
                  << name
                  << " | expected=" << expected
                  << " | actual=" << actual << '\n';

        if (ok) {
            ++passed;
        } else {
            ++failed;
        }

        EXPECT_EQ(actual, expected) << name;
    };

    // Initialize input signals and allow combinational logic to settle.
    tb.apply(10, 20, 30, 40, 0);
    check("Select input 0", 10);

    tb.apply(10, 20, 30, 40, 1);
    check("Select input 1", 20);

    tb.apply(10, 20, 30, 40, 2);
    check("Select input 2", 30);

    tb.apply(10, 20, 30, 40, 3);
    check("Select input 3", 40);

    // Negative input values.
    tb.apply(-10, -20, -30, -40, 2);
    check("Negative input", -30);

    // Zero input.
    tb.apply(10, 0, 30, 40, 1);
    check("Select zero", 0);

    // Change inputs while keeping the selected index.
    tb.apply(1, 2, 3, 4, 2);
    check("Initial dynamic selection", 3);

    tb.apply(11, 22, 33, 44, 2);
    check("Updated selected input", 33);

    // Large values within signed 8-bit range.
    tb.apply(127, -128, 55, 66, 0);
    check("Maximum signed 8-bit value", 127);

    tb.apply(127, -128, 55, 66, 1);
    check("Minimum signed 8-bit value", -128);

    std::cout << "\nMUX test summary: "
              << passed << " passed, "
              << failed << " failed\n";
}

int sc_main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}