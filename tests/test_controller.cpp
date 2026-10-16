
#include <gtest/gtest.h>
#include <systemc>

#include "Controller.h"

using namespace sc_core;

// ============================================================
// Testbench
// ============================================================

SC_MODULE(ControllerTestbench) {
    sc_clock clk;

    sc_signal<bool> reset;
    sc_signal<bool> start;

    sc_signal<bool> enable;
    sc_signal<bool> load_weight;
    sc_signal<bool> load_ifmap;
    sc_signal<bool> use_feedback;

    sc_signal<bool> busy;
    sc_signal<bool> done;

    Controller dut;

    SC_CTOR(ControllerTestbench)
        : clk("clk", 10, SC_NS, 0.5, 5, SC_NS, true),
          dut("controller", DataflowMode::WS, 3)
    {
        dut.clk(clk);
        dut.reset(reset);
        dut.start(start);

        dut.enable(enable);
        dut.load_weight(load_weight);
        dut.load_ifmap(load_ifmap);
        dut.use_feedback(use_feedback);

        dut.busy(busy);
        dut.done(done);

        reset.write(true);
        start.write(false);
    }

    // Advance simulation by one full clock period.
    void tick() {
        sc_start(10, SC_NS);
    }
};

static ControllerTestbench* tb = nullptr;

// ============================================================
// Controller WS test
// ============================================================

TEST(ControllerTest, WSControlSequence)
{
    ASSERT_NE(tb, nullptr);

    // 1. Hold reset active.
    tb->reset.write(true);
    tb->start.write(false);

    tb->tick();
    tb->tick();

    EXPECT_FALSE(tb->enable.read());
    EXPECT_FALSE(tb->load_weight.read());
    EXPECT_FALSE(tb->load_ifmap.read());
    EXPECT_FALSE(tb->use_feedback.read());
    EXPECT_FALSE(tb->busy.read());
    EXPECT_FALSE(tb->done.read());

    // 2. Release reset and allow the controller
    //    to enter its idle state.
    tb->reset.write(false);

    tb->tick();
    tb->tick();

    EXPECT_FALSE(tb->busy.read());
    EXPECT_FALSE(tb->done.read());

    // 3. Request an operation.
    tb->start.write(true);

    // Wait until the controller starts loading weights.
    bool loading_started = false;

    for (int i = 0; i < 5; ++i) {
        tb->tick();

        if (tb->load_weight.read()) {
            loading_started = true;
            break;
        }
    }

    ASSERT_TRUE(loading_started)
        << "Controller did not enter the weight-loading phase";

    EXPECT_TRUE(tb->busy.read());
    EXPECT_FALSE(tb->enable.read());
    EXPECT_TRUE(tb->load_weight.read());
    EXPECT_FALSE(tb->done.read());

    // Deassert start to avoid requesting another operation.
    tb->start.write(false);

    // 4. After one loading cycle, computation begins.
    tb->tick();

    EXPECT_FALSE(tb->load_weight.read());
    EXPECT_TRUE(tb->enable.read());
    EXPECT_TRUE(tb->busy.read());
    EXPECT_FALSE(tb->done.read());

    EXPECT_FALSE(tb->load_ifmap.read());
    EXPECT_FALSE(tb->use_feedback.read());

    // 5. The controller was configured for 3 compute cycles.
    //    Check that enable remains asserted for the
    //    remaining two sampled clock periods.
    tb->tick();
    EXPECT_TRUE(tb->enable.read());
    EXPECT_TRUE(tb->busy.read());
    EXPECT_FALSE(tb->done.read());

    tb->tick();
    EXPECT_TRUE(tb->enable.read());
    EXPECT_TRUE(tb->busy.read());
    EXPECT_FALSE(tb->done.read());

    // 6. The operation finishes after the third
    //    compute cycle.
    tb->tick();

    EXPECT_FALSE(tb->enable.read());
    EXPECT_FALSE(tb->load_weight.read());
    EXPECT_FALSE(tb->busy.read());
    EXPECT_TRUE(tb->done.read());

    // 7. done should be a one-cycle pulse.
    tb->tick();

    EXPECT_FALSE(tb->enable.read());
    EXPECT_FALSE(tb->load_weight.read());
    EXPECT_FALSE(tb->busy.read());
    EXPECT_FALSE(tb->done.read());
}

// ============================================================
// SystemC entry point
// ============================================================

int sc_main(int argc, char* argv[])
{
    ::testing::InitGoogleTest(&argc, argv);

    ControllerTestbench testbench("testbench");
    tb = &testbench;

    return RUN_ALL_TESTS();
}