#include "Dataflow.h"

#include <stdexcept>

Dataflow::Dataflow(
    sc_core::sc_module_name name,
    std::size_t rows_count,
    std::size_t cols_count,
    DataflowMode selected_mode
)
    : sc_core::sc_module(name),
      rows(rows_count),
      cols(cols_count),
      mode(selected_mode),
      weight_in("weight_in", rows_count * cols_count),
      ifmap_in("ifmap_in", rows_count),
      psum_in("psum_in", cols_count),
      psum_out("psum_out", rows_count * cols_count),
      final_out("final_out", cols_count),
      ifmap_pipe_("ifmap_pipe", rows_count * cols_count),
      psum_bus_("psum_bus", rows_count * cols_count),
      array_("pe_array", rows_count, cols_count, PEArchitecture::WS)
{
    if (rows == 0 || cols == 0) {
        throw std::invalid_argument(
            "Dataflow dimensions must be greater than zero"
        );
    }

    // Select the architecture during module construction.
    switch (mode) {
        case DataflowMode::WS:
            WS();
            break;

        case DataflowMode::OS:
            OS();
            throw std::logic_error(
                "Output Stationary dataflow is not implemented"
            );

        case DataflowMode::IS:
            IS();
            throw std::logic_error(
                "Input Stationary dataflow is not implemented"
            );

        case DataflowMode::DIRECT:
            DIRECT();
            throw std::logic_error(
                "Direct dataflow is not implemented"
            );
    }

    // Publish PE outputs and the final row outputs.
    SC_METHOD(publish_outputs);

    for (std::size_t i = 0; i < rows * cols; ++i) {
        sensitive << psum_bus_[i];
    }
}

std::size_t Dataflow::index(
    std::size_t row,
    std::size_t col
) const {
    if (row >= rows || col >= cols) {
        throw std::out_of_range(
            "Dataflow coordinates are out of range"
        );
    }

    return row * cols + col;
}

// ============================================================
// Weight Stationary
// ============================================================

void Dataflow::WS()
{
    // Connect shared control signals.
    array_.clk(clk);
    array_.reset(reset);
    array_.enable(enable);
    array_.load_weight(load_weight);
    array_.load_ifmap(load_ifmap);
    array_.use_feedback(use_feedback);

    // Connect the PE array.
    for (std::size_t r = 0; r < rows; ++r) {
        for (std::size_t c = 0; c < cols; ++c) {
            const std::size_t i = index(r, c);

            // Each PE receives its own weight.
            array_.weight_in[i](weight_in[i]);

            // Internal PE partial-sum output.
            array_.psum_out[i](psum_bus_[i]);

            // Each PE receives its activation from the pipeline.
            array_.ifmap_in[i](ifmap_pipe_[i]);

            // Partial sums flow from top to bottom.
            if (r == 0) {
                array_.psum_in[i](psum_in[c]);
            } else {
                const std::size_t above = index(r - 1, c);
                array_.psum_in[i](psum_bus_[above]);
            }
        }
    }

    // Shift activations horizontally on each rising edge.
    SC_METHOD(shift_activations);
    sensitive << clk.pos();
    dont_initialize();
}

// ============================================================
// Output Stationary - TODO
// ============================================================

void Dataflow::OS()
{
    // TODO:
    // 1. Configure PE connections for Output Stationary.
    // 2. Keep partial sums stationary inside each PE.
    // 3. Implement the required activation and weight movement.
}

// ============================================================
// Input Stationary - TODO
// ============================================================

void Dataflow::IS()
{
    // TODO:
    // 1. Configure PE connections for Input Stationary.
    // 2. Keep activations stationary inside each PE.
    // 3. Implement the required weight and partial-sum movement.
}

// ============================================================
// Direct Dataflow - TODO
// ============================================================

void Dataflow::DIRECT()
{
    // TODO:
    // Implement direct input/output connections if required.
}

// ============================================================
// WS activation pipeline
// ============================================================

void Dataflow::shift_activations()
{
    for (std::size_t r = 0; r < rows; ++r) {

        // Shift activations from left to right.
        for (std::size_t c = cols; c-- > 1;) {
            const std::size_t current = index(r, c);
            const std::size_t previous = index(r, c - 1);

            ifmap_pipe_[current].write(
                ifmap_pipe_[previous].read()
            );
        }

        // Inject a new activation at the leftmost PE.
        ifmap_pipe_[index(r, 0)].write(
            ifmap_in[r].read()
        );
    }
}

// ============================================================
// Output publishing
// ============================================================

void Dataflow::publish_outputs()
{
    // Expose every PE's partial-sum output.
    for (std::size_t r = 0; r < rows; ++r) {
        for (std::size_t c = 0; c < cols; ++c) {
            const std::size_t i = index(r, c);

            psum_out[i].write(
                psum_bus_[i].read()
            );
        }
    }

    // Expose the last row as the final output.
    for (std::size_t c = 0; c < cols; ++c) {
        final_out[c].write(
            psum_bus_[index(rows - 1, c)].read()
        );
    }
}