
#pragma once

#include <systemc>
#include <array>
#include <cstddef>
#include <stdexcept>

template<
    std::size_t N,
    typename DATA_T = sc_dt::sc_int<32>
>
SC_MODULE(MUX) {
    static_assert(N >= 2, "MUX must have at least 2 inputs");

    static constexpr std::size_t SELECT_WIDTH =
        (N <= 2) ? 1 : (N <= 4) ? 2 :
        (N <= 8) ? 3 : (N <= 16) ? 4 :
        (N <= 32) ? 5 : (N <= 64) ? 6 :
        (N <= 128) ? 7 : (N <= 256) ? 8 : 9;

    using DataType = DATA_T;
    using SelectType = sc_dt::sc_uint<SELECT_WIDTH>;

    sc_core::sc_vector<sc_core::sc_in<DataType>> input;
    sc_core::sc_in<SelectType> select{"select"};
    sc_core::sc_out<DataType> output{"output"};

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
        const std::size_t index = select.read().to_uint();

        if (index >= N) {
            SC_REPORT_ERROR("MUX", "Select index is out of range");
            return;
        }

        output.write(input[index].read());
    }
};