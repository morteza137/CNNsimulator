#pragma once

#include <systemc>
#include <array>

template <
    int DATA_WIDTH = 8,
    int DEPTH = 256,
    int READ_LATENCY = 1,
    int WRITE_LATENCY = 1
>
SC_MODULE(SRAM) {
    static_assert(DATA_WIDTH > 0, "DATA_WIDTH must be positive");
    static_assert(DEPTH > 0, "DEPTH must be positive");
    static_assert(READ_LATENCY > 0, "READ_LATENCY must be positive");
    static_assert(WRITE_LATENCY > 0, "WRITE_LATENCY must be positive");

    static constexpr int calc_addr_width() {
        int width = 0;
        int capacity = 1;

        while (capacity < DEPTH) {
            capacity <<= 1;
            ++width;
        }

        return width > 0 ? width : 1;
    }

    static constexpr int ADDR_WIDTH = calc_addr_width();

    using DataType = sc_dt::sc_int<DATA_WIDTH>;
    using AddrType = sc_dt::sc_uint<ADDR_WIDTH>;

    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_in<bool> reset{"reset"};

    // Read port
    sc_core::sc_in<bool> read_enable{"read_enable"};
    sc_core::sc_in<AddrType> read_address{"read_address"};
    sc_core::sc_out<DataType> read_data{"read_data"};
    sc_core::sc_out<bool> read_ready{"read_ready"};

    // Write port
    sc_core::sc_in<bool> write_enable{"write_enable"};
    sc_core::sc_in<AddrType> write_address{"write_address"};
    sc_core::sc_in<DataType> write_data{"write_data"};
    sc_core::sc_out<bool> write_ready{"write_ready"};

    SC_HAS_PROCESS(SRAM);

    explicit SRAM(sc_core::sc_module_name name)
        : sc_core::sc_module(name) {
        memory_.fill(DataType{0});

        SC_METHOD(clock_tick);
        sensitive << clk.pos();
        dont_initialize();
    }

private:
    std::array<DataType, DEPTH> memory_{};

    bool read_busy_ = false;
    bool write_busy_ = false;

    int read_remaining_ = 0;
    int write_remaining_ = 0;

    AddrType pending_read_address_ = 0;
    AddrType pending_write_address_ = 0;

    DataType pending_read_data_ = 0;
    DataType pending_write_data_ = 0;

    void clock_tick() {
        if (reset.read()) {
            read_busy_ = false;
            write_busy_ = false;

            read_remaining_ = 0;
            write_remaining_ = 0;

            read_ready.write(false);
            write_ready.write(false);
            read_data.write(DataType{0});

            // SRAM contents are preserved during reset.
            return;
        }

        // Ready signals are completion pulses.
        read_ready.write(false);
        write_ready.write(false);

        // Process the read port independently.
        if (read_busy_) {
            if (read_remaining_ <= 1) {
                read_data.write(pending_read_data_);
                read_ready.write(true);

                read_busy_ = false;
                read_remaining_ = 0;
            } else {
                --read_remaining_;
            }
        } else if (read_enable.read()) {
            const unsigned address =
                read_address.read().to_uint();

            pending_read_address_ = read_address.read();

            // Capture the data when the request is accepted.
            if (address < static_cast<unsigned>(DEPTH)) {
                pending_read_data_ = memory_[address];
            } else {
                pending_read_data_ = DataType{0};
            }

            read_remaining_ = READ_LATENCY;
            read_busy_ = true;
        }

        // Process the write port independently.
        if (write_busy_) {
            if (write_remaining_ <= 1) {
                const unsigned address =
                    pending_write_address_.to_uint();

                if (address < static_cast<unsigned>(DEPTH)) {
                    memory_[address] = pending_write_data_;
                }

                write_ready.write(true);

                write_busy_ = false;
                write_remaining_ = 0;
            } else {
                --write_remaining_;
            }
        } else if (write_enable.read()) {
            pending_write_address_ = write_address.read();
            pending_write_data_ = write_data.read();

            write_remaining_ = WRITE_LATENCY;
            write_busy_ = true;
        }
    }
};