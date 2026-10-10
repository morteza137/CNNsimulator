#pragma once

#include <systemc>
#include <cstddef>

template<
    std::size_t N,
    typename DATA_T = sc_dt::sc_int<32>
>
SC_MODULE(MUX) {
    static_assert(N >= 2, "MUX must have at least 2 inputs");

    // Calculate ceil(log2(N)) at compile time.
    static constexpr std::size_t calc_select_width() {
        std::size_t width = 0;
        std::size_t capacity = 1;

        while (capacity < N) {
            capacity <<= 1;
            ++width;
        }

        return width > 0 ? width : 1;
    }

    static constexpr std::size_t SELECT_WIDTH =
        calc_select_width();

    using DataType = DATA_T;
    using SelectType = sc_dt::sc_uint<SELECT_WIDTH>;

    // N parameterized data inputs.
    sc_core::sc_vector<sc_core::sc_in<DataType>> input;

    // Input index.
    sc_core::sc_in<SelectType> select{"select"};

    // Selected data.
    sc_core::sc_out<DataType> output{"output"};

    // True when select is within [0, N-1].
    sc_core::sc_out<bool> select_valid{"select_valid"};

    SC_CTOR(MUX)
        : input("input", N)
    {
        SC_METHOD(compute);

        sensitive << select;

        for (std::size_t i = 0; i < N; ++i) {
            sensitive << input[i];
        }
    }

private:
    void compute() {
        const std::size_t index =
            static_cast<std::size_t>(select.read().to_uint());

        if (index >= N) {
            // Deterministic behavior for invalid selections.
            output.write(DataType{});
            select_valid.write(false);
            return;
        }

        output.write(input[index].read());
        select_valid.write(true);
    }
};