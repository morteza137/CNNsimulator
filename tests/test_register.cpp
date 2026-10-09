
#include <systemc>
#include <gtest/gtest.h>
#include "Register.h"

using namespace sc_core;

SC_MODULE(RegisterTestBench) {
    sc_clock clk{"clk", 10, SC_NS};

    sc_signal<bool> reset8, enable8;
    sc_signal<sc_dt::sc_int<8>> input8, output8;
    Register<8> reg8{"reg8"};

    sc_signal<bool> reset32, enable32;
    sc_signal<sc_dt::sc_int<32>> input32, output32;
    Register<32> reg32{"reg32"};

    SC_CTOR(RegisterTestBench) {
        reg8.clk(clk);
        reg8.reset(reset8);
        reg8.enable(enable8);
        reg8.data_in(input8);
        reg8.data_out(output8);

        reg32.clk(clk);
        reg32.reset(reset32);
        reg32.enable(enable32);
        reg32.data_in(input32);
        reg32.data_out(output32);
    }
};

TEST(RegisterTest, AllBehaviors) {
    // Construct all SystemC objects before simulation starts.
    static RegisterTestBench tb{"tb"};

    tb.reset8.write(true);
    tb.enable8.write(false);
    tb.input8.write(0);

    tb.reset32.write(true);
    tb.enable32.write(false);
    tb.input32.write(0);

    sc_start(1, SC_NS);
    sc_start(9, SC_NS); // First rising edge at 5 ns.

    EXPECT_EQ(tb.output8.read().to_int(), 0);
    EXPECT_EQ(tb.output32.read().to_int(), 0);

    // Store a positive value.
    tb.reset8.write(false);
    tb.enable8.write(true);
    tb.input8.write(42);

    sc_start(10, SC_NS);
    EXPECT_EQ(tb.output8.read().to_int(), 42);

    // Disabled register must retain its previous value.
    tb.enable8.write(false);
    tb.input8.write(99);

    sc_start(10, SC_NS);
    EXPECT_EQ(tb.output8.read().to_int(), 42);

    // Store a negative value.
    tb.enable8.write(true);
    tb.input8.write(-50);

    sc_start(10, SC_NS);
    EXPECT_EQ(tb.output8.read().to_int(), -50);

    // Test 32-bit width.
    tb.reset32.write(false);
    tb.enable32.write(true);
    tb.input32.write(100000);

    sc_start(10, SC_NS);
    EXPECT_EQ(tb.output32.read().to_int(), 100000);
}

int sc_main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}