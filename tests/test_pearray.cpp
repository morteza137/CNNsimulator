
#include <systemc>
#include <iostream>
#include <stdexcept>
#include <string>

#include "PE.h"
#include "PEArray.h"
#include "ArithmeticPolicy.h"

using namespace sc_core;
using namespace std;

// ------------------------------------------------------------
// Test utilities
// ------------------------------------------------------------

static bool all_passed = true;

static void check(
    const string& name,
    int actual,
    int expected
) {
    if (actual != expected) {
        cerr << "[FAIL] " << name
             << " | expected=" << expected
             << ", actual=" << actual << '\n';
        all_passed = false;
    } else {
        cout << "[PASS] " << name
             << " = " << actual << '\n';
    }
}

// Manual clock: low -> high (active edge) -> low.
// Allows signals to settle after every transition.
static void tick(sc_signal<bool>& clk) {
    clk.write(false);
    sc_start(1, SC_NS);

    clk.write(true);
    sc_start(1, SC_NS);

    clk.write(false);
    sc_start(1, SC_NS);
}

// ------------------------------------------------------------
// Main integration test
// ------------------------------------------------------------

int sc_main(int argc, char* argv[]) {
    constexpr size_t ROWS = 2;
    constexpr size_t COLS = 3;
    constexpr size_t COUNT = ROWS * COLS;

    cout << "\n========== CNNsimulator hardware tests ==========\n";

    // --------------------------------------------------------
    // Shared control signals
    // --------------------------------------------------------

    sc_signal<bool> clk("clk");
    sc_signal<bool> reset("reset");
    sc_signal<bool> enable("enable");
    sc_signal<bool> load_weight("load_weight");
    sc_signal<bool> load_ifmap("load_ifmap");
    sc_signal<bool> use_feedback("use_feedback");

    // --------------------------------------------------------
    // Four standalone PEs: test each architecture separately
    // --------------------------------------------------------

    PE no_pe("no_pe", PEArchitecture::NO);
    PE os_pe("os_pe", PEArchitecture::OS);
    PE ws_pe("ws_pe", PEArchitecture::WS);
    PE is_pe("is_pe", PEArchitecture::IS);

    // NO inputs/outputs
    sc_signal<PE::WeightType> no_w("no_w");
    sc_signal<PE::IfmapType>  no_a("no_a");
    sc_signal<PE::PsumType>   no_p("no_p");
    sc_signal<PE::PsumType>   no_out("no_out");

    // OS inputs/outputs
    sc_signal<PE::WeightType> os_w("os_w");
    sc_signal<PE::IfmapType>  os_a("os_a");
    sc_signal<PE::PsumType>   os_p("os_p");
    sc_signal<PE::PsumType>   os_out("os_out");

    // WS inputs/outputs
    sc_signal<PE::WeightType> ws_w("ws_w");
    sc_signal<PE::IfmapType>  ws_a("ws_a");
    sc_signal<PE::PsumType>   ws_p("ws_p");
    sc_signal<PE::PsumType>   ws_out("ws_out");

    // IS inputs/outputs
    sc_signal<PE::WeightType> is_w("is_w");
    sc_signal<PE::IfmapType>  is_a("is_a");
    sc_signal<PE::PsumType>   is_p("is_p");
    sc_signal<PE::PsumType>   is_out("is_out");

    // Connect common controls to every standalone PE.
    PE* standalone_pes[] = {&no_pe, &os_pe, &ws_pe, &is_pe};

    for (PE* pe : standalone_pes) {
        pe->clk(clk);
        pe->reset(reset);
        pe->enable(enable);
        pe->load_weight(load_weight);
        pe->load_ifmap(load_ifmap);
        pe->use_feedback(use_feedback);
    }

    no_pe.weight_in(no_w);
    no_pe.ifmap_in(no_a);
    no_pe.psum_in(no_p);
    no_pe.psum_out(no_out);

    os_pe.weight_in(os_w);
    os_pe.ifmap_in(os_a);
    os_pe.psum_in(os_p);
    os_pe.psum_out(os_out);

    ws_pe.weight_in(ws_w);
    ws_pe.ifmap_in(ws_a);
    ws_pe.psum_in(ws_p);
    ws_pe.psum_out(ws_out);

    is_pe.weight_in(is_w);
    is_pe.ifmap_in(is_a);
    is_pe.psum_in(is_p);
    is_pe.psum_out(is_out);

    // --------------------------------------------------------
    // A 2x3 PEArray, testing indexing and per-PE connectivity
    // --------------------------------------------------------

    PEArray array(
        "array",
        ROWS,
        COLS,
        PEArchitecture::NO
    );

    array.clk(clk);
    array.reset(reset);
    array.enable(enable);
    array.load_weight(load_weight);
    array.load_ifmap(load_ifmap);
    array.use_feedback(use_feedback);

    sc_vector<sc_signal<PE::WeightType>>
        arr_w("arr_w", COUNT);

    sc_vector<sc_signal<PE::IfmapType>>
        arr_a("arr_a", COUNT);

    sc_vector<sc_signal<PE::PsumType>>
        arr_p("arr_p", COUNT);

    sc_vector<sc_signal<PE::PsumType>>
        arr_out("arr_out", COUNT);

    for (size_t r = 0; r < ROWS; ++r) {
        for (size_t c = 0; c < COLS; ++c) {
            const size_t i = array.index(r, c);

            array.weight_in[i](arr_w[i]);
            array.ifmap_in[i](arr_a[i]);
            array.psum_in[i](arr_p[i]);
            array.psum_out[i](arr_out[i]);
        }
    }

    // --------------------------------------------------------
    // Initialize every input before simulation
    // --------------------------------------------------------

    clk.write(false);
    reset.write(true);
    enable.write(false);
    load_weight.write(false);
    load_ifmap.write(false);
    use_feedback.write(false);

    no_w.write(0);
    no_a.write(0);
    no_p.write(0);

    os_w.write(0);
    os_a.write(0);
    os_p.write(0);

    ws_w.write(0);
    ws_a.write(0);
    ws_p.write(0);

    is_w.write(0);
    is_a.write(0);
    is_p.write(0);

    for (size_t i = 0; i < COUNT; ++i) {
        arr_w[i].write(0);
        arr_a[i].write(0);
        arr_p[i].write(0);
    }

    // Synchronous reset: must be sampled on a rising edge.
    tick(clk);

    check("NO reset", no_out.read().to_int(), 0);
    check("OS reset", os_out.read().to_int(), 0);
    check("WS reset", ws_out.read().to_int(), 0);
    check("IS reset", is_out.read().to_int(), 0);

    reset.write(false);
    enable.write(true);
    sc_start(1, SC_NS);

    // ========================================================
    // TEST 1: NO PE / MAC exact arithmetic
    // Expected: 3 * 5 + 10 = 25
    // ========================================================

    cout << "\n--- TEST 1: NO PE / exact MAC ---\n";

    no_w.write(3);
    no_a.write(5);
    no_p.write(10);

    tick(clk);

    check("NO: 3*5+10", no_out.read().to_int(), 25);

    // Negative operand: -3 * 5 + 2 = -13
    no_w.write(-3);
    no_a.write(5);
    no_p.write(2);

    tick(clk);

    check("NO: negative product", no_out.read().to_int(), -13);

    // ========================================================
    // TEST 2: Enable must hold the previous MAC result
    // ========================================================

    cout << "\n--- TEST 2: MAC enable/hold ---\n";

    enable.write(false);
    no_w.write(7);
    no_a.write(8);
    no_p.write(100);

    tick(clk);

    check("MAC holds result when disabled",
          no_out.read().to_int(), -13);

    enable.write(true);
    sc_start(1, SC_NS);

    // ========================================================
    // TEST 3: PEArray connectivity and row-major indexing
    // Each PE(i): weight=(i+1), ifmap=2, psum=1
    // Expected output: 2*(i+1)+1
    // ========================================================

    cout << "\n--- TEST 3: PEArray 2x3 outputs ---\n";

    for (size_t i = 0; i < COUNT; ++i) {
        arr_w[i].write(static_cast<int>(i + 1));
        arr_a[i].write(2);
        arr_p[i].write(1);
    }

    tick(clk);

    for (size_t r = 0; r < ROWS; ++r) {
        for (size_t c = 0; c < COLS; ++c) {
            const size_t i = array.index(r, c);
            const int expected = 2 * static_cast<int>(i + 1) + 1;

            check(
                "PEArray(" + to_string(r) + "," +
                    to_string(c) + ")",
                arr_out[i].read().to_int(),
                expected
            );
        }
    }

    // Check invalid coordinates.
    cout << "\n--- TEST 4: PEArray index bounds ---\n";

    bool threw = false;
    try {
        (void)array.index(ROWS, 0);
    } catch (const out_of_range&) {
        threw = true;
    }

    check("index rejects row == ROWS", threw ? 1 : 0, 1);

    threw = false;
    try {
        (void)array.index(0, COLS);
    } catch (const out_of_range&) {
        threw = true;
    }

    check("index rejects col == COLS", threw ? 1 : 0, 1);

    // ========================================================
    // TEST 5: OS PE feedback path / MUX
    // First cycle uses external psum: 3*5+10 = 25.
    // Second cycle uses previous MAC result: 3*5+25 = 40.
    // ========================================================

    cout << "\n--- TEST 5: OS feedback / MUX ---\n";

    os_w.write(3);
    os_a.write(5);
    os_p.write(10);

    use_feedback.write(false);
    sc_start(1, SC_NS);
    tick(clk);

    check("OS: external psum", os_out.read().to_int(), 25);

    use_feedback.write(true);
    sc_start(1, SC_NS);
    tick(clk);

    check("OS: feedback accumulation", os_out.read().to_int(), 40);

    // ========================================================
    // TEST 6: WS weight register
    //
    // On the loading edge, MAC sees the OLD register value.
    // On the next edge, it sees the loaded weight.
    // ========================================================

    cout << "\n--- TEST 6: WS weight register ---\n";

    // Reset all stateful hardware before this scenario.
    reset.write(true);
    use_feedback.write(false);
    load_weight.write(false);
    load_ifmap.write(false);
    tick(clk);

    reset.write(false);
    enable.write(true);
    sc_start(1, SC_NS);

    ws_w.write(4);
    ws_a.write(6);
    ws_p.write(1);
    load_weight.write(true);

    tick(clk);

    check("WS: MAC sees old weight on load edge",
          ws_out.read().to_int(), 1);

    load_weight.write(false);
    ws_w.write(99);  // Must be ignored by WS MAC.
    tick(clk);

    check("WS: stored weight 4 is used",
          ws_out.read().to_int(), 25);

    // ========================================================
    // TEST 7: IS ifmap register
    // ========================================================

    cout << "\n--- TEST 7: IS ifmap register ---\n";

    reset.write(true);
    load_weight.write(false);
    load_ifmap.write(false);
    tick(clk);

    reset.write(false);
    enable.write(true);
    sc_start(1, SC_NS);

    is_w.write(4);
    is_a.write(6);
    is_p.write(1);
    load_ifmap.write(true);

    tick(clk);

    check("IS: MAC sees old ifmap on load edge",
          is_out.read().to_int(), 1);

    load_ifmap.write(false);
    is_a.write(99);  // Must be ignored by IS MAC.
    tick(clk);

    check("IS: stored ifmap 6 is used",
          is_out.read().to_int(), 25);

    // ========================================================
    // TEST 8: Reset clears the MAC result
    // ========================================================

    cout << "\n--- TEST 8: reset clears state ---\n";

    reset.write(true);
    tick(clk);

    check("NO reset clears output", no_out.read().to_int(), 0);
    check("OS reset clears output", os_out.read().to_int(), 0);
    check("WS reset clears output", ws_out.read().to_int(), 0);
    check("IS reset clears output", is_out.read().to_int(), 0);

    // ========================================================
    // TEST 9: Approximate arithmetic policy
    //
    // ApproximateArithmetic rounds the PRODUCT to the nearest
    // multiple of four, then adds psum.
    // This is a policy test; PE currently uses ExactArithmetic.
    // ========================================================

    cout << "\n--- TEST 9: approximate arithmetic policy ---\n";

    const auto approx_positive =
        ApproximateArithmetic::compute<
            PE::WeightType,
            PE::IfmapType,
            PE::PsumType
        >(PE::WeightType(3), PE::IfmapType(5), PE::PsumType(0));

    check("Approx: 3*5 rounds to 16",
          approx_positive.to_int(), 16);

    const auto approx_negative =
        ApproximateArithmetic::compute<
            PE::WeightType,
            PE::IfmapType,
            PE::PsumType
        >(PE::WeightType(-3), PE::IfmapType(5), PE::PsumType(0));

    check("Approx: -3*5 rounds to -16",
          approx_negative.to_int(), -16);

    const auto approx_with_psum =
        ApproximateArithmetic::compute<
            PE::WeightType,
            PE::IfmapType,
            PE::PsumType
        >(PE::WeightType(3), PE::IfmapType(5), PE::PsumType(7));

    check("Approx: rounded product plus psum",
          approx_with_psum.to_int(), 23);

    // ========================================================
    // Summary
    // ========================================================

    cout << "\n========== TEST SUMMARY ==========\n";

    if (all_passed) {
        cout << "ALL CHECKS PASSED\n";
        return 0;
    }

    cerr << "SOME CHECKS FAILED\n";
    return 1;
}
