#include <systemc>
#include <gtest/gtest.h>
#include "RegisterFile.h"

using namespace sc_core;
using namespace sc_dt;

SC_MODULE(RegisterFileTestBench) {
    sc_signal<bool> clk{"clk"};
    sc_signal<bool> reset{"reset"};
    sc_signal<bool> write_enable{"write_enable"};

    sc_signal<sc_uint<3>> write_addr{"write_addr"};
    sc_signal<sc_uint<3>> read_addr{"read_addr"};

    sc_signal<sc_int<8>> data_in{"data_in"};
    sc_signal<sc_int<8>> data_out{"data_out"};

    RegisterFile<8, 5> rf{"rf"};

    SC_CTOR(RegisterFileTestBench) {
        rf.clk(clk);
        rf.reset(reset);
        rf.write_enable(write_enable);
        rf.write_addr(write_addr);
        rf.data_in(data_in);
        rf.read_addr(read_addr);
        rf.data_out(data_out);

        clk.write(false);
        reset.write(false);
        write_enable.write(false);
        write_addr.write(0);
        read_addr.write(0);
        data_in.write(0);
    }

    void tick() {
        clk.write(true);
        sc_start(1, SC_NS);

        clk.write(false);
        sc_start(1, SC_NS);
    }

    void initialize() {
        clk.write(false);
        reset.write(true);
        write_enable.write(false);
        write_addr.write(0);
        read_addr.write(0);
        data_in.write(0);

        sc_start(1, SC_NS);
        tick();

        reset.write(false);
        sc_start(1, SC_NS);
    }

    void write_register(unsigned addr, int value) {
        write_addr.write(addr);
        data_in.write(value);
        write_enable.write(true);

        // Allow the write decoder to settle before the clock edge.
        sc_start(1, SC_NS);
        tick();

        write_enable.write(false);
        sc_start(1, SC_NS);
    }

    int read_register(unsigned addr) {
        read_addr.write(addr);

        // Allow the asynchronous read mux to settle.
        sc_start(1, SC_NS);

        return data_out.read().to_int();
    }

    void reset_registers() {
        reset.write(true);
        sc_start(1, SC_NS);
        tick();

        reset.write(false);
        sc_start(1, SC_NS);
    }
};

static RegisterFileTestBench* tb = nullptr;

TEST(RegisterFileTest, AllBehaviors) {
    ASSERT_NE(tb, nullptr);

    // Initialize the register file.
    tb->initialize();

    // Test writing and reading a positive value.
    tb->write_register(2, 42);
    EXPECT_EQ(tb->read_register(2), 42);

    // Test writing and reading a negative value.
    tb->write_register(3, -50);
    EXPECT_EQ(tb->read_register(3), -50);

    // Test that registers store independent values.
    EXPECT_EQ(tb->read_register(2), 42);
    EXPECT_EQ(tb->read_register(3), -50);

    // Test that a disabled write does not change stored data.
    tb->write_enable.write(false);
    tb->write_addr.write(2);
    tb->data_in.write(100);
    tb->tick();

    EXPECT_EQ(tb->read_register(2), 42);

    // Test reading an unwritten register.
    EXPECT_EQ(tb->read_register(4), 0);

    // Test that an invalid address returns zero.
    EXPECT_EQ(tb->read_register(7), 0);

    // Test synchronous reset.
    tb->reset_registers();

    EXPECT_EQ(tb->read_register(2), 0);
    EXPECT_EQ(tb->read_register(3), 0);
    EXPECT_EQ(tb->read_register(4), 0);
}

int sc_main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);

    static RegisterFileTestBench testbench{"testbench"};
    tb = &testbench;

    return RUN_ALL_TESTS();
}