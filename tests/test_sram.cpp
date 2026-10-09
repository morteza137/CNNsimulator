#include <systemc>
#include <gtest/gtest.h>
#include "SRAM.h"

using namespace sc_core;
using namespace sc_dt;

SC_MODULE(SRAMTestBench) {
    sc_signal<bool> clk{"clk"};
    sc_signal<bool> reset{"reset"};

    sc_signal<bool> read_enable{"read_enable"};
    sc_signal<sc_uint<3>> read_address{"read_address"};
    sc_signal<sc_int<8>> read_data{"read_data"};
    sc_signal<bool> read_ready{"read_ready"};

    sc_signal<bool> write_enable{"write_enable"};
    sc_signal<sc_uint<3>> write_address{"write_address"};
    sc_signal<sc_int<8>> write_data{"write_data"};
    sc_signal<bool> write_ready{"write_ready"};

    SRAM<8, 5, 2, 3> sram{"sram"};

    SC_CTOR(SRAMTestBench) {
        sram.clk(clk);
        sram.reset(reset);

        sram.read_enable(read_enable);
        sram.read_address(read_address);
        sram.read_data(read_data);
        sram.read_ready(read_ready);

        sram.write_enable(write_enable);
        sram.write_address(write_address);
        sram.write_data(write_data);
        sram.write_ready(write_ready);

        clk.write(false);
        reset.write(false);
        read_enable.write(false);
        read_address.write(0);
        write_enable.write(false);
        write_address.write(0);
        write_data.write(0);
    }

    void tick() {
        clk.write(false);
        sc_start(1, SC_NS);

        clk.write(true);
        sc_start(1, SC_NS);

        clk.write(false);
        sc_start(1, SC_NS);
    }

    void initialize() {
        reset.write(true);
        read_enable.write(false);
        write_enable.write(false);
        tick();

        reset.write(false);
        tick();
    }

    void start_write(unsigned address, int value) {
        write_address.write(address);
        write_data.write(value);
        write_enable.write(true);
        tick();
        write_enable.write(false);
    }

    void start_read(unsigned address) {
        read_address.write(address);
        read_enable.write(true);
        tick();
        read_enable.write(false);
    }

    void wait_write_completion() {
        for (int i = 0; i < 3; ++i) {
            tick();
        }
    }

    void wait_read_completion() {
        for (int i = 0; i < 2; ++i) {
            tick();
        }
    }

    int read_value(unsigned address) {
        start_read(address);
        wait_read_completion();

        EXPECT_TRUE(read_ready.read());
        return read_data.read().to_int();
    }
};

static SRAMTestBench* tb = nullptr;

int sc_main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);

    static SRAMTestBench testbench{"testbench"};
    tb = &testbench;

    return RUN_ALL_TESTS();
}

TEST(SRAMTest, AllBehaviors) {
    ASSERT_NE(tb, nullptr);

    tb->initialize();

    // Verify write latency and data storage.
    tb->start_write(1, 77);
    EXPECT_FALSE(tb->write_ready.read());

    tb->tick();
    EXPECT_FALSE(tb->write_ready.read());

    tb->tick();
    EXPECT_FALSE(tb->write_ready.read());

    tb->tick();
    EXPECT_TRUE(tb->write_ready.read());

    // Verify read latency and returned data.
    tb->start_read(1);
    EXPECT_FALSE(tb->read_ready.read());

    tb->tick();
    EXPECT_FALSE(tb->read_ready.read());

    tb->tick();
    EXPECT_TRUE(tb->read_ready.read());
    EXPECT_EQ(tb->read_data.read().to_int(), 77);

    // Verify that different addresses store independent values.
    tb->start_write(2, -35);
    tb->wait_write_completion();
    EXPECT_TRUE(tb->write_ready.read());

    EXPECT_EQ(tb->read_value(1), 77);
    EXPECT_EQ(tb->read_value(2), -35);

    // Verify that an invalid read address returns zero.
    EXPECT_EQ(tb->read_value(7), 0);

    // Verify that a disabled write does not modify memory.
    tb->write_enable.write(false);
    tb->write_address.write(1);
    tb->write_data.write(100);
    tb->tick();

    EXPECT_EQ(tb->read_value(1), 77);

    // Verify that reset cancels a pending read and preserves memory.
    tb->start_read(1);

    tb->reset.write(true);
    tb->tick();

    EXPECT_FALSE(tb->read_ready.read());
    EXPECT_FALSE(tb->write_ready.read());

    tb->reset.write(false);
    tb->tick();

    EXPECT_EQ(tb->read_value(1), 77);
}