#include <gtest/gtest.h>
#include <systemc>

#include <iostream>

#include "Dataflow.h"

using namespace sc_core;
using namespace sc_dt;

using WeightType = PE::WeightType;
using IfmapType  = PE::IfmapType;
using PsumType   = PE::PsumType;

// ============================================================
// Testbench
// ============================================================

SC_MODULE(DataflowBench) {
    sc_clock clk;

    sc_signal<bool> reset;
    sc_signal<bool> enable;
    sc_signal<bool> load_weight;
    sc_signal<bool> load_ifmap;
    sc_signal<bool> use_feedback;

    sc_vector<sc_signal<WeightType>> weights;
    sc_vector<sc_signal<IfmapType>>  ifmaps;
    sc_vector<sc_signal<PsumType>>   psums;

    sc_vector<sc_signal<PsumType>> outputs;
    sc_vector<sc_signal<PsumType>> finals;

    Dataflow dut;

    SC_CTOR(DataflowBench)
        : clk("clk", 10, SC_NS),
          weights("weights", 4),
          ifmaps("ifmaps", 2),
          psums("psums", 2),
          outputs("outputs", 4),
          finals("finals", 2),
          dut("dut", 2, 2, DataflowMode::WS)
    {
        dut.clk(clk);
        dut.reset(reset);
        dut.enable(enable);
        dut.load_weight(load_weight);
        dut.load_ifmap(load_ifmap);
        dut.use_feedback(use_feedback);

        for (int i = 0; i < 4; ++i) {
            dut.weight_in[i](weights[i]);
            dut.psum_out[i](outputs[i]);
        }

        for (int i = 0; i < 2; ++i) {
            dut.ifmap_in[i](ifmaps[i]);
            dut.psum_in[i](psums[i]);
            dut.final_out[i](finals[i]);
        }
    }
};

static DataflowBench* bench = nullptr;

// ============================================================
// WS test
// ============================================================

TEST(DataflowWSTest, WeightStationaryAndDataMovement)
{
    ASSERT_NE(bench, nullptr);

    // --------------------------------------------------------
    // 1. Initialize controls and reset
    // --------------------------------------------------------

    bench->reset.write(true);
    bench->enable.write(true);
    bench->load_weight.write(false);
    bench->load_ifmap.write(false);
    bench->use_feedback.write(false);

    for (int i = 0; i < 4; ++i) {
        bench->weights[i].write(0);
    }

    for (int i = 0; i < 2; ++i) {
        bench->ifmaps[i].write(0);
        bench->psums[i].write(0);
    }

    sc_start(12, SC_NS);

    bench->reset.write(false);

    // --------------------------------------------------------
    // 2. Load different weights into the four PEs
    //
    // PE(0,0) = 1
    // PE(0,1) = 2
    // PE(1,0) = 3
    // PE(1,1) = 4
    // --------------------------------------------------------

    bench->weights[0].write(1);
    bench->weights[1].write(2);
    bench->weights[2].write(3);
    bench->weights[3].write(4);

    bench->load_weight.write(true);

    // --------------------------------------------------------
    // 3. Provide activations
    //
    // Row 0 receives 5.
    // Row 1 receives 7.
    // --------------------------------------------------------

    bench->ifmaps[0].write(5);
    bench->ifmaps[1].write(7);

    bench->psums[0].write(0);
    bench->psums[1].write(0);

    // Allow a rising clock edge to load the weights.
    sc_start(10, SC_NS);

    bench->load_weight.write(false);

    // Let activations propagate and outputs settle.
    sc_start(100, SC_NS);

    // --------------------------------------------------------
    // 4. Print the actual PE outputs
    // --------------------------------------------------------

    std::cout << "\n========== WS Dataflow Test ==========\n";

    std::cout << "PE(0,0): "
              << bench->outputs[0].read() << '\n';

    std::cout << "PE(0,1): "
              << bench->outputs[1].read() << '\n';

    std::cout << "PE(1,0): "
              << bench->outputs[2].read() << '\n';

    std::cout << "PE(1,1): "
              << bench->outputs[3].read() << '\n';

    std::cout << "Final output column 0: "
              << bench->finals[0].read() << '\n';

    std::cout << "Final output column 1: "
              << bench->finals[1].read() << '\n';

    std::cout << "======================================\n";

    // --------------------------------------------------------
    // 5. Check expected results
    // --------------------------------------------------------
    //
    // Row 0:
    //   PE(0,0) = 1*5 + 0 = 5
    //   PE(0,1) = 2*5 + 0 = 10
    //
    // Row 1:
    //   PE(1,0) = 3*7 + 5  = 26
    //   PE(1,1) = 4*7 + 10 = 38
    //
    // The lower row receives the previous row's partial sums.
    // The activation pipeline needs time to settle.

    EXPECT_EQ(bench->outputs[0].read().to_int(), 5);
    EXPECT_EQ(bench->outputs[1].read().to_int(), 10);
    EXPECT_EQ(bench->outputs[2].read().to_int(), 26);
    EXPECT_EQ(bench->outputs[3].read().to_int(), 38);

    EXPECT_EQ(bench->finals[0].read().to_int(), 26);
    EXPECT_EQ(bench->finals[1].read().to_int(), 38);
}

// ============================================================
// SystemC entry point
// ============================================================

int sc_main(int argc, char* argv[])
{
    ::testing::InitGoogleTest(&argc, argv);

    DataflowBench testbench("testbench");
    bench = &testbench;

    return RUN_ALL_TESTS();
}