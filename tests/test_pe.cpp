
#include <systemc>
#include <gtest/gtest.h>

#include <string>

#include "PE.h"

using namespace sc_core;
using namespace sc_dt;

// ============================================================
// PE Test Rig
// ============================================================

struct PERig {
    using WeightType = PE::WeightType;
    using IfmapType  = PE::IfmapType;
    using PsumType   = PE::PsumType;

    sc_signal<bool> clk;
    sc_signal<bool> reset;
    sc_signal<bool> enable;

    sc_signal<bool> load_weight;
    sc_signal<bool> load_ifmap;
    sc_signal<bool> use_feedback;

    sc_signal<WeightType> weight_in;
    sc_signal<IfmapType> ifmap_in;
    sc_signal<PsumType> psum_in;

    sc_signal<PsumType> psum_out;

    PE pe;

    explicit PERig(
        const std::string& name,
        PEArchitecture architecture
    )
        : clk((name + "_clk").c_str()),
          reset((name + "_reset").c_str()),
          enable((name + "_enable").c_str()),
          load_weight((name + "_load_weight").c_str()),
          load_ifmap((name + "_load_ifmap").c_str()),
          use_feedback((name + "_use_feedback").c_str()),
          weight_in((name + "_weight_in").c_str()),
          ifmap_in((name + "_ifmap_in").c_str()),
          psum_in((name + "_psum_in").c_str()),
          psum_out((name + "_psum_out").c_str()),
          pe((name + "_pe").c_str(), architecture)
    {
        pe.clk(clk);
        pe.reset(reset);
        pe.enable(enable);

        pe.load_weight(load_weight);
        pe.load_ifmap(load_ifmap);
        pe.use_feedback(use_feedback);

        pe.weight_in(weight_in);
        pe.ifmap_in(ifmap_in);
        pe.psum_in(psum_in);
        pe.psum_out(psum_out);

        clk.write(false);
        reset.write(true);
        enable.write(false);

        load_weight.write(false);
        load_ifmap.write(false);
        use_feedback.write(false);

        weight_in.write(0);
        ifmap_in.write(0);
        psum_in.write(0);
    }

    // One complete clock cycle.
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
        enable.write(false);

        tick();

        reset.write(false);
        enable.write(true);

        tick();
    }

    void set_inputs(int weight, int ifmap, int psum) {
        weight_in.write(weight);
        ifmap_in.write(ifmap);
        psum_in.write(psum);
    }

    int output() const {
        return psum_out.read().to_int();
    }
};

// ============================================================
// Comprehensive PE test
// ============================================================

TEST(PETest, AllArchitecturesAndControls) {
    // Construct all SystemC modules before simulation starts.
    static PERig os("os", PEArchitecture::OS);
    static PERig ws("ws", PEArchitecture::WS);
    static PERig is("is", PEArchitecture::IS);
    static PERig no("no", PEArchitecture::NO);

    // --------------------------------------------------------
    // 1. Reset
    // --------------------------------------------------------

    os.initialize();
    ws.initialize();
    is.initialize();
    no.initialize();

    EXPECT_EQ(os.output(), 0);
    EXPECT_EQ(ws.output(), 0);
    EXPECT_EQ(is.output(), 0);
    EXPECT_EQ(no.output(), 0);

    // --------------------------------------------------------
    // 2. NO: direct MAC path
    //
    // 3 * 5 + 10 = 25
    // --------------------------------------------------------

    no.set_inputs(3, 5, 10);
    no.tick();

    EXPECT_EQ(no.output(), 25);

    // --------------------------------------------------------
    // 3. OS: external partial sum
    //
    // use_feedback = false
    // 3 * 5 + 10 = 25
    // --------------------------------------------------------

    os.set_inputs(3, 5, 10);
    os.use_feedback.write(false);
    os.tick();

    EXPECT_EQ(os.output(), 25);

    // --------------------------------------------------------
    // 4. OS: feedback accumulation
    //
    // Previous result = 25
    // 3 * 5 + 25 = 40
    // --------------------------------------------------------

    os.use_feedback.write(true);
    os.tick();

    EXPECT_EQ(os.output(), 40);

    // --------------------------------------------------------
    // 5. WS: weight register
    //
    // Load weight = 3.
    // First edge loads the register; MAC sees its old value.
    // Following edge:
    // 3 * 5 + 10 = 25
    // --------------------------------------------------------

    ws.set_inputs(3, 5, 10);
    ws.load_weight.write(true);
    ws.tick();

    ws.load_weight.write(false);
    ws.tick();

    EXPECT_EQ(ws.output(), 25);

    // Change external weight while register loading is disabled.
    // The stored weight must remain 3.
    ws.set_inputs(9, 5, 10);
    ws.tick();

    EXPECT_EQ(ws.output(), 25); // 3 * 5 + 10

    // --------------------------------------------------------
    // 6. IS: input register
    //
    // Load ifmap = 5.
    // First edge loads the register; MAC sees its old value.
    // Following edge:
    // 3 * 5 + 10 = 25
    // --------------------------------------------------------

    is.set_inputs(3, 5, 10);
    is.load_ifmap.write(true);
    is.tick();

    is.load_ifmap.write(false);
    is.tick();

    EXPECT_EQ(is.output(), 25);

    // Change external ifmap while loading is disabled.
    // Stored ifmap must remain 5.
    is.set_inputs(3, 9, 10);
    is.tick();

    EXPECT_EQ(is.output(), 25); // 3 * 5 + 10

    // --------------------------------------------------------
    // 7. Enable: MAC output must hold when disabled
    // --------------------------------------------------------

    no.enable.write(false);
    no.set_inputs(9, 9, 100);
    no.tick();

    EXPECT_EQ(no.output(), 25);

    // --------------------------------------------------------
    // 8. Reset: MAC result returns to zero
    // --------------------------------------------------------

    no.reset.write(true);
    no.tick();

    EXPECT_EQ(no.output(), 0);

    no.reset.write(false);
    no.enable.write(true);
    no.set_inputs(2, 4, 1);
    no.tick();

    EXPECT_EQ(no.output(), 9); // 2 * 4 + 1
}

// ============================================================
// SystemC entry point
// ============================================================

int sc_main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
