#include <systemc>
#include <gtest/gtest.h>
#include <iostream>
#include "MAC.h"

SC_MODULE(MACTest) {
    sc_core::sc_signal<sc_dt::sc_int<8>> weight, ifmap;
    sc_core::sc_signal<sc_dt::sc_int<32>> psum, result;
    sc_core::sc_signal<bool> approximate;

    MAC<8, 8, 32> mac{"mac"};

    SC_CTOR(MACTest) {
        mac.weight(weight);
        mac.ifmap(ifmap);
        mac.psum(psum);
        mac.approximate(approximate);
        mac.result(result);
    }

    int calculate(int w, int a, int p) {
        weight.write(w);
        ifmap.write(a);
        psum.write(p);
        approximate.write(false);

        sc_core::sc_start(1, sc_core::SC_NS);

        int output = result.read().to_int();

        std::cout << "Weight = " << w
                  << ", Ifmap = " << a
                  << ", PSUM = " << p
                  << " -> Output = " << output << '\n';

        return output;
    }
};

TEST(MACTest, MultipleCases) {
    MACTest test("test");

    EXPECT_EQ(test.calculate(3, 4, 5), 17);
    EXPECT_EQ(test.calculate(0, 10, 7), 7);
    EXPECT_EQ(test.calculate(5, 0, 9), 9);
    EXPECT_EQ(test.calculate(-3, 4, 5), -7);
    EXPECT_EQ(test.calculate(3, -4, 5), -7);
    EXPECT_EQ(test.calculate(-3, -4, 5), 17);
    EXPECT_EQ(test.calculate(10, 10, -20), 80);
    EXPECT_EQ(test.calculate(100, 100, 100), 10100);
    EXPECT_EQ(test.calculate(127, 127, 0), 16129);
    EXPECT_EQ(test.calculate(-128, 127, 0), -16256);
}

int sc_main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}