#include "PEArray.h"

#include <stdexcept>
#include <string>
#include <utility>

PEArray::PEArray(
    sc_core::sc_module_name name,
    std::size_t rows_count,
    std::size_t cols_count,
    PEArchitecture architecture
)
    : sc_core::sc_module(name),
      rows(rows_count),
      cols(cols_count),
      weight_in("weight_in", rows_count * cols_count),
      ifmap_in("ifmap_in", rows_count * cols_count),
      psum_in("psum_in", rows_count * cols_count),
      psum_out("psum_out", rows_count * cols_count),
      architecture_(architecture)
{
    if (rows == 0 || cols == 0) {
        throw std::invalid_argument(
            "PEArray dimensions must be greater than zero"
        );
    }

    pes_.reserve(rows * cols);

    for (std::size_t r = 0; r < rows; ++r) {
        for (std::size_t c = 0; c < cols; ++c) {

            const std::size_t i = index(r, c);

            const std::string pe_name =
                "pe_" + std::to_string(r) +
                "_" + std::to_string(c);

            // Create one PE with the selected architecture.
            auto pe = std::make_unique<PE>(
                pe_name.c_str(),
                architecture_
            );

            // Shared control signals.
            pe->clk(clk);
            pe->reset(reset);
            pe->enable(enable);

            pe->load_weight(load_weight);
            pe->load_ifmap(load_ifmap);
            pe->use_feedback(use_feedback);

            // Per-PE data connections.
            pe->weight_in(weight_in[i]);
            pe->ifmap_in(ifmap_in[i]);
            pe->psum_in(psum_in[i]);
            pe->psum_out(psum_out[i]);

            pes_.push_back(std::move(pe));
        }
    }
}

std::size_t PEArray::index(
    std::size_t row,
    std::size_t col
) const
{
    if (row >= rows || col >= cols) {
        throw std::out_of_range(
            "PEArray coordinates are out of range"
        );
    }

    return row * cols + col;
}

PE& PEArray::at(
    std::size_t row,
    std::size_t col
)
{
    return *pes_.at(index(row, col));
}

const PE& PEArray::at(
    std::size_t row,
    std::size_t col
) const
{
    return *pes_.at(index(row, col));
}
