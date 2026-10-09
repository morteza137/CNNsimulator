
#pragma once

#include <systemc>

template <int W = 8>
SC_MODULE(Register) {
    static_assert(W > 0, "Register width must be greater than zero");

    using DataType = sc_dt::sc_int<W>;

    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_in<bool> reset{"reset"};
    sc_core::sc_in<bool> enable{"enable"};
    sc_core::sc_in<DataType> data_in{"data_in"};
    sc_core::sc_out<DataType> data_out{"data_out"};

    SC_CTOR(Register) {
        SC_METHOD(update);
        sensitive << clk.pos();
    }

private:
    DataType value = 0;

    void update() {
        if (reset.read()) {
            value = 0;
        } else if (enable.read()) {
            value = data_in.read();
        }

        data_out.write(value);
    }
};