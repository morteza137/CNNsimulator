
#pragma once

#include <systemc>
#include "Register.h"

template <int W = 8, int N = 8>
SC_MODULE(RegisterFile) {
    static_assert(W > 0, "Width must be greater than zero");
    static_assert(N > 0, "Number of registers must be greater than zero");

    static constexpr int calc_addr_width() {
        int width = 0;
        int capacity = 1;

        while (capacity < N) {
            capacity <<= 1;
            ++width;
        }

        return width > 0 ? width : 1;
    }

    static constexpr int ADDR_WIDTH = calc_addr_width();

    using DataType = sc_dt::sc_int<W>;
    using AddrType = sc_dt::sc_uint<ADDR_WIDTH>;

    // Clock and reset
    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_in<bool> reset{"reset"};

    // Write port
    sc_core::sc_in<bool> write_enable{"write_enable"};
    sc_core::sc_in<AddrType> write_addr{"write_addr"};
    sc_core::sc_in<DataType> data_in{"data_in"};

    // Asynchronous read port
    sc_core::sc_in<AddrType> read_addr{"read_addr"};
    sc_core::sc_out<DataType> data_out{"data_out"};

    // Register storage and internal connections
    sc_core::sc_vector<Register<W>> registers;
    sc_core::sc_vector<sc_core::sc_signal<bool>> reg_enable;
    sc_core::sc_vector<sc_core::sc_signal<DataType>> reg_data_in;
    sc_core::sc_vector<sc_core::sc_signal<DataType>> reg_data_out;

    SC_HAS_PROCESS(RegisterFile);

    explicit RegisterFile(sc_core::sc_module_name name)
        : sc_core::sc_module(name),
          registers("registers", N),
          reg_enable("reg_enable", N),
          reg_data_in("reg_data_in", N),
          reg_data_out("reg_data_out", N) {

        for (int i = 0; i < N; ++i) {
            registers[i].clk(clk);
            registers[i].reset(reset);
            registers[i].enable(reg_enable[i]);
            registers[i].data_in(reg_data_in[i]);
            registers[i].data_out(reg_data_out[i]);
        }

        SC_METHOD(decode_write);
        sensitive << write_enable << write_addr << data_in;

        SC_METHOD(read_mux);
        sensitive << read_addr;

        for (int i = 0; i < N; ++i) {
            sensitive << reg_data_out[i];
        }
    }

private:
    void decode_write() {
        const bool we = write_enable.read();
        const unsigned addr = write_addr.read().to_uint();
        const DataType input = data_in.read();

        for (int i = 0; i < N; ++i) {
            reg_enable[i].write(we && (addr == static_cast<unsigned>(i)));
            reg_data_in[i].write(input);
        }
    }

    void read_mux() {
        const unsigned addr = read_addr.read().to_uint();

        if (addr < static_cast<unsigned>(N)) {
            data_out.write(reg_data_out[addr].read());
        } else {
            data_out.write(DataType{0});
        }
    }
};