#pragma once

#include <systemc>
#include <cstddef>
#include "PE.h"
#include "PEArray.h"

enum class DataflowMode {
    DIRECT,
    OS,
    WS,
    IS
};

SC_MODULE(Dataflow) {
public:
    using WeightType = PE::WeightType;
    using IfmapType  = PE::IfmapType;
    using PsumType   = PE::PsumType;

    const std::size_t rows;
    const std::size_t cols;
    const DataflowMode mode;

    // Shared control signals
    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_in<bool> reset{"reset"};
    sc_core::sc_in<bool> enable{"enable"};
    sc_core::sc_in<bool> load_weight{"load_weight"};
    sc_core::sc_in<bool> load_ifmap{"load_ifmap"};
    sc_core::sc_in<bool> use_feedback{"use_feedback"};

    // External inputs
    sc_core::sc_vector<sc_core::sc_in<WeightType>> weight_in;
    sc_core::sc_vector<sc_core::sc_in<IfmapType>> ifmap_in;
    sc_core::sc_vector<sc_core::sc_in<PsumType>> psum_in;

    // Individual PE outputs and final row outputs
    sc_core::sc_vector<sc_core::sc_out<PsumType>> psum_out;
    sc_core::sc_vector<sc_core::sc_out<PsumType>> final_out;

    SC_HAS_PROCESS(Dataflow);

    Dataflow(
        sc_core::sc_module_name name,
        std::size_t rows,
        std::size_t cols,
        DataflowMode mode = DataflowMode::WS
    );

    std::size_t index(
        std::size_t row,
        std::size_t col
    ) const;

private:
    // Internal activation and partial-sum connections
    sc_core::sc_vector<sc_core::sc_signal<IfmapType>> ifmap_pipe_;
    sc_core::sc_vector<sc_core::sc_signal<PsumType>> psum_bus_;

    // Processing-element array
    PEArray array_;

    // Architecture-specific configuration
    void WS();
    void OS();
    void IS();
    void DIRECT();

    // WS processes
    void shift_activations();
    void publish_outputs();
};