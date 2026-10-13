#pragma once

#include <systemc>
#include <memory>
#include <vector>
#include <cstddef>

#include "PE.h"

// ============================================================
// PEArray
//
// Configurable 2D array of PEs, inspired by Eyeriss PE_ARRAY.
//
// Each PE has its own:
//   - weight input
//   - ifmap input
//   - psum output
//
// Control signals are shared across all PEs.
//
// This module does not implement neighbor-to-neighbor forwarding.
// The Scheduler or a parent module supplies each PE's inputs.
// ============================================================

SC_MODULE(PEArray) {
public:
    using WeightType = PE::WeightType;
    using IfmapType  = PE::IfmapType;
    using PsumType   = PE::PsumType;

    // --------------------------------------------------------
    // Configuration
    // --------------------------------------------------------
    const std::size_t rows;
    const std::size_t cols;

    // --------------------------------------------------------
    // Shared control ports
    // --------------------------------------------------------
    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_in<bool> reset{"reset"};
    sc_core::sc_in<bool> enable{"enable"};

    sc_core::sc_in<bool> load_weight{"load_weight"};
    sc_core::sc_in<bool> load_ifmap{"load_ifmap"};
    sc_core::sc_in<bool> use_feedback{"use_feedback"};

    // --------------------------------------------------------
    // Per-PE data ports
    //
    // PE(r,c) uses index:
    //     index = r * cols + c
    //
    // Example for a 2x3 array:
    //     index 0 -> PE(0,0)
    //     index 1 -> PE(0,1)
    //     index 2 -> PE(0,2)
    //     index 3 -> PE(1,0)
    //     ...
    // --------------------------------------------------------
    sc_core::sc_vector<sc_core::sc_in<WeightType>> weight_in;
    sc_core::sc_vector<sc_core::sc_in<IfmapType>>  ifmap_in;
    sc_core::sc_vector<sc_core::sc_in<PsumType>>   psum_in;

    sc_core::sc_vector<sc_core::sc_out<PsumType>>  psum_out;

    // --------------------------------------------------------
    // Constructor
    // --------------------------------------------------------
    PEArray(
        sc_core::sc_module_name name,
        std::size_t rows,
        std::size_t cols,
        PEArchitecture architecture = PEArchitecture::NO
    );

    // Return the linear index of PE(row, col).
    std::size_t index(
        std::size_t row,
        std::size_t col
    ) const;

    // Access a PE by coordinates.
    PE& at(std::size_t row, std::size_t col);
    const PE& at(std::size_t row, std::size_t col) const;

private:
    PEArchitecture architecture_;

    std::vector<std::unique_ptr<PE>> pes_;
};
