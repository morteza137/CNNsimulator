#pragma once

#include <systemc>
#include <cstddef>

#include "Dataflow.h"

SC_MODULE(Controller) {
public:
    // Clock and reset
    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_in<bool> reset{"reset"};

    // Start a new operation
    sc_core::sc_in<bool> start{"start"};

    // Control signals connected to Dataflow
    sc_core::sc_out<bool> enable{"enable"};
    sc_core::sc_out<bool> load_weight{"load_weight"};
    sc_core::sc_out<bool> load_ifmap{"load_ifmap"};
    sc_core::sc_out<bool> use_feedback{"use_feedback"};

    // Status signals
    sc_core::sc_out<bool> busy{"busy"};
    sc_core::sc_out<bool> done{"done"};

    SC_HAS_PROCESS(Controller);

    Controller(
        sc_core::sc_module_name name,
        DataflowMode mode = DataflowMode::WS,
        std::size_t compute_cycles = 1
    );

private:
    const DataflowMode mode_;
    const std::size_t compute_cycles_;

    void control_thread();
};