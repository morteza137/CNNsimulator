
#pragma once

#include <systemc>
#include <array>
#include <deque>

template <
    int DATA_WIDTH = 8,
    int DEPTH = 256,
    int READ_LATENCY = 1,
    int WRITE_LATENCY = 1,
    int MAX_OUTSTANDING_REQUESTS = 4
>
SC_MODULE(SRAM) {
    static_assert(DATA_WIDTH > 0,"DATA_WIDTH must be positive");
    static_assert(DEPTH > 0,"DEPTH must be positive");
    static_assert(READ_LATENCY > 0,"READ_LATENCY must be positive");
    static_assert(WRITE_LATENCY > 0,"WRITE_LATENCY must be positive");
    static_assert(MAX_OUTSTANDING_REQUESTS > 0,"MAX_OUTSTANDING_REQUESTS must be positive");

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

    // Read request interface
    sc_core::sc_in<bool> read_enable{"read_enable"};
    sc_core::sc_in<AddrType> read_address{"read_address"};
    sc_core::sc_out<bool> read_request_ready{"read_request_ready"};

    // Read response interface
    sc_core::sc_out<DataType> read_data{"read_data"};
    sc_core::sc_out<bool> read_ready{"read_ready"};
    sc_core::sc_out<bool> read_error{"read_error"};

    // Write request interface
    sc_core::sc_in<bool> write_enable{"write_enable"};
    sc_core::sc_in<AddrType> write_address{"write_address"};
    sc_core::sc_in<DataType> write_data{"write_data"};
    sc_core::sc_out<bool> write_request_ready{"write_request_ready"};

    // Write response interface
    sc_core::sc_out<bool> write_ready{"write_ready"};
    sc_core::sc_out<bool> write_error{"write_error"};

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

    struct ReadRequest {
        unsigned address;
        DataType data;
        bool invalid_address;
    };

    struct WriteRequest {
        unsigned address;
        DataType data;
        bool invalid_address;
    };

    std::deque<ReadRequest> read_queue_;
    std::deque<WriteRequest> write_queue_;

    bool read_busy_ = false;
    bool write_busy_ = false;

    int read_remaining_ = 0;
    int write_remaining_ = 0;

    ReadRequest active_read_{0, DataType{0}, false};
    WriteRequest active_write_{0, DataType{0}, false};

    std::size_t read_outstanding() const {
        return read_queue_.size() + (read_busy_ ? 1U : 0U);
    }

    std::size_t write_outstanding() const {
        return write_queue_.size() + (write_busy_ ? 1U : 0U);
    }

    void update_request_ready() {
        read_request_ready.write(
            read_outstanding() <
            static_cast<std::size_t>(MAX_OUTSTANDING_REQUESTS));

        write_request_ready.write(
            write_outstanding() <
            static_cast<std::size_t>(MAX_OUTSTANDING_REQUESTS));
    }

    void clock_tick() {
        if (reset.read()) {
            read_queue_.clear();
            write_queue_.clear();

            read_busy_ = false;
            write_busy_ = false;

            read_remaining_ = 0;
            write_remaining_ = 0;

            read_data.write(DataType{0});
            read_ready.write(false);
            read_error.write(false);

            write_ready.write(false);
            write_error.write(false);

            // Reset cancels pending requests but preserves SRAM data.
            update_request_ready();
            return;
        }

        // Response signals are one-cycle completion pulses.
        read_ready.write(false);
        read_error.write(false);
        write_ready.write(false);
        write_error.write(false);

        // Accept requests using the ready state visible before this edge.
        // Read data is snapshotted at request acceptance.
        if (read_enable.read() && read_request_ready.read()) {
            const unsigned address = read_address.read().to_uint();
            const bool invalid = address >= static_cast<unsigned>(DEPTH);

            const DataType snapshot =
                invalid ? DataType{0} : memory_[address];

            read_queue_.push_back(
                ReadRequest{address, snapshot, invalid});
        }

        if (write_enable.read() && write_request_ready.read()) {
            const unsigned address = write_address.read().to_uint();
            const bool invalid = address >= static_cast<unsigned>(DEPTH);

            write_queue_.push_back(
                WriteRequest{address, write_data.read(), invalid});
        }

        // Advance the active read request.
        if (read_busy_) {
            if (read_remaining_ <= 1) {
                read_data.write(active_read_.data);
                read_ready.write(true);
                read_error.write(active_read_.invalid_address);

                read_busy_ = false;
                read_remaining_ = 0;
            } else {
                --read_remaining_;
            }
        }

        // Advance the active write request.
        if (write_busy_) {
            if (write_remaining_ <= 1) {
                if (!active_write_.invalid_address) {
                    memory_[active_write_.address] = active_write_.data;
                }

                write_ready.write(true);
                write_error.write(active_write_.invalid_address);

                write_busy_ = false;
                write_remaining_ = 0;
            } else {
                --write_remaining_;
            }
        }

        // Start the next queued request after processing completions.
        if (!read_busy_ && !read_queue_.empty()) {
            active_read_ = read_queue_.front();
            read_queue_.pop_front();

            read_remaining_ = READ_LATENCY;
            read_busy_ = true;
        }

        if (!write_busy_ && !write_queue_.empty()) {
            active_write_ = write_queue_.front();
            write_queue_.pop_front();

            write_remaining_ = WRITE_LATENCY;
            write_busy_ = true;
        }

        update_request_ready();
    }
};