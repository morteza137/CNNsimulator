#include <systemc>
#include <gtest/gtest.h>

#include <iostream>

#include "SRAM.h"

using namespace sc_core;
using namespace sc_dt;

// ============================================================
// SRAM Testbench
//
// DATA_WIDTH                = 8
// DEPTH                     = 5
// READ_LATENCY              = 4
// WRITE_LATENCY             = 3
// MAX_OUTSTANDING_REQUESTS = 4
// ============================================================

struct SRAMTestBench : sc_module {
    using DataType = sc_int<8>;
    using AddressType = sc_uint<3>;

    sc_signal<bool> clk{"clk"};
    sc_signal<bool> reset{"reset"};

    // Read request
    sc_signal<bool> read_enable{"read_enable"};
    sc_signal<AddressType> read_address{"read_address"};
    sc_signal<bool> read_request_ready{"read_request_ready"};

    // Read response
    sc_signal<DataType> read_data{"read_data"};
    sc_signal<bool> read_ready{"read_ready"};
    sc_signal<bool> read_error{"read_error"};

    // Write request
    sc_signal<bool> write_enable{"write_enable"};
    sc_signal<AddressType> write_address{"write_address"};
    sc_signal<DataType> write_data{"write_data"};
    sc_signal<bool> write_request_ready{"write_request_ready"};

    // Write response
    sc_signal<bool> write_ready{"write_ready"};
    sc_signal<bool> write_error{"write_error"};

    SRAM<8, 5, 4, 3, 4> sram{"sram"};

    SC_CTOR(SRAMTestBench) {
        sram.clk(clk);
        sram.reset(reset);

        sram.read_enable(read_enable);
        sram.read_address(read_address);
        sram.read_request_ready(read_request_ready);

        sram.read_data(read_data);
        sram.read_ready(read_ready);
        sram.read_error(read_error);

        sram.write_enable(write_enable);
        sram.write_address(write_address);
        sram.write_data(write_data);
        sram.write_request_ready(write_request_ready);

        sram.write_ready(write_ready);
        sram.write_error(write_error);

        clk.write(false);
        reset.write(true);

        read_enable.write(false);
        read_address.write(0);

        write_enable.write(false);
        write_address.write(0);
        write_data.write(0);
    }

    // --------------------------------------------------------
    // Clock
    // --------------------------------------------------------

    void tick() {
        clk.write(false);
        sc_start(1, SC_NS);

        clk.write(true);
        sc_start(1, SC_NS);

        clk.write(false);
        sc_start(1, SC_NS);
    }

    void initialize() {
        reset.write(true);
        read_enable.write(false);
        write_enable.write(false);

        tick();

        reset.write(false);
        tick();
    }

    // --------------------------------------------------------
    // Ready helpers
    // --------------------------------------------------------

    bool wait_read_request_ready(int timeout = 50) {
        for (int i = 0; i < timeout; ++i) {
            if (read_request_ready.read()) {
                return true;
            }
            tick();
        }

        return read_request_ready.read();
    }

    bool wait_write_request_ready(int timeout = 50) {
        for (int i = 0; i < timeout; ++i) {
            if (write_request_ready.read()) {
                return true;
            }
            tick();
        }

        return write_request_ready.read();
    }

    // --------------------------------------------------------
    // Issue read request
    // --------------------------------------------------------

    bool start_read(unsigned address) {
        if (!wait_read_request_ready()) {
            std::cout
                << "[FAIL] Read request rejected by timeout"
                << " | address=" << address << '\n';
            return false;
        }

        read_address.write(address);
        read_enable.write(true);

        tick();

        read_enable.write(false);

        std::cout
            << "[REQ ] READ"
            << " | address=" << address
            << " | accepted=1\n";

        return true;
    }

    // --------------------------------------------------------
    // Issue write request
    // --------------------------------------------------------

    bool start_write(unsigned address, int value) {
        if (!wait_write_request_ready()) {
            std::cout
                << "[FAIL] Write request rejected by timeout"
                << " | address=" << address << '\n';
            return false;
        }

        write_address.write(address);
        write_data.write(value);
        write_enable.write(true);

        tick();

        write_enable.write(false);

        std::cout
            << "[REQ ] WRITE"
            << " | address=" << address
            << " | data=" << value
            << " | accepted=1\n";

        return true;
    }

    // --------------------------------------------------------
    // Wait for responses
    // --------------------------------------------------------

    bool wait_read_response(int timeout = 50) {
        for (int i = 0; i < timeout; ++i) {
            if (read_ready.read()) {
                return true;
            }
            tick();
        }

        return read_ready.read();
    }

    bool wait_write_response(int timeout = 50) {
        for (int i = 0; i < timeout; ++i) {
            if (write_ready.read()) {
                return true;
            }
            tick();
        }

        return write_ready.read();
    }

    // --------------------------------------------------------
    // Verify read response
    // --------------------------------------------------------

    void check_read_response(
        const char* test_name,
        unsigned address,
        int expected_data,
        bool expected_error = false
    ) {
        const int actual_data = read_data.read().to_int();
        const bool actual_error = read_error.read();

        const bool passed =
            actual_data == expected_data &&
            actual_error == expected_error;

        std::cout
            << "[" << (passed ? "PASS" : "FAIL") << "] "
            << test_name
            << " | address=" << address
            << " | expected_data=" << expected_data
            << " | actual_data=" << actual_data
            << " | expected_error=" << expected_error
            << " | actual_error=" << actual_error
            << '\n';

        EXPECT_EQ(actual_data, expected_data);
        EXPECT_EQ(actual_error, expected_error);
    }

    // --------------------------------------------------------
    // Verify write response
    // --------------------------------------------------------

    void check_write_response(
        const char* test_name,
        unsigned address,
        int input_data,
        bool expected_error = false
    ) {
        const bool completed = write_ready.read();
        const bool actual_error = write_error.read();

        const bool passed =
            completed && actual_error == expected_error;

        std::cout
            << "[" << (passed ? "PASS" : "FAIL") << "] "
            << test_name
            << " | address=" << address
            << " | input_data=" << input_data
            << " | completed=" << completed
            << " | expected_error=" << expected_error
            << " | actual_error=" << actual_error
            << '\n';

        EXPECT_TRUE(completed);
        EXPECT_EQ(actual_error, expected_error);
    }

    // --------------------------------------------------------
    // Convenience functions for a complete operation
    // --------------------------------------------------------

    bool write_value(unsigned address, int value) {
        if (!start_write(address, value)) {
            return false;
        }

        if (!wait_write_response()) {
            std::cout << "[FAIL] Write response timeout\n";
            return false;
        }

        const bool error = write_error.read();

        check_write_response(
            "Write response",
            address,
            value,
            address >= 5
        );

        tick();

        return !error;
    }

    bool read_value(unsigned address, int& value) {
        if (!start_read(address)) {
            return false;
        }

        if (!wait_read_response()) {
            std::cout << "[FAIL] Read response timeout\n";
            return false;
        }

        value = read_data.read().to_int();

        const bool error = read_error.read();

        check_read_response(
            "Read response",
            address,
            address >= 5 ? 0 : value,
            address >= 5
        );

        tick();

        return !error;
    }
};

static SRAMTestBench* tb = nullptr;

// ============================================================
// TEST 1: Basic read and write
// ============================================================

TEST(SRAMTest, BasicReadWrite) {
    auto& s = *tb;

    std::cout << "\n========== BASIC READ/WRITE ==========\n";

    s.initialize();

    ASSERT_TRUE(s.write_value(0, 42));
    ASSERT_TRUE(s.write_value(1, 99));

    int value = 0;

    ASSERT_TRUE(s.read_value(0, value));
    EXPECT_EQ(value, 42);

    ASSERT_TRUE(s.read_value(1, value));
    EXPECT_EQ(value, 99);

    std::cout << "======================================\n";
}

// ============================================================
// TEST 2: Independent memory addresses
// ============================================================

TEST(SRAMTest, IndependentAddresses) {
    auto& s = *tb;

    std::cout << "\n========== INDEPENDENT ADDRESSES ==========\n";

    s.initialize();

    ASSERT_TRUE(s.write_value(0, 11));
    ASSERT_TRUE(s.write_value(2, 33));
    ASSERT_TRUE(s.write_value(4, 55));

    int value = 0;

    ASSERT_TRUE(s.read_value(0, value));
    EXPECT_EQ(value, 11);

    ASSERT_TRUE(s.read_value(2, value));
    EXPECT_EQ(value, 33);

    ASSERT_TRUE(s.read_value(4, value));
    EXPECT_EQ(value, 55);

    std::cout << "===========================================\n";
}

// ============================================================
// TEST 3: Invalid addresses
// Valid addresses: 0 through 4
// ============================================================

TEST(SRAMTest, InvalidAddresses) {
    auto& s = *tb;

    std::cout << "\n========== INVALID ADDRESSES ==========\n";

    s.initialize();

    // Invalid read should return zero and assert read_error.
    ASSERT_TRUE(s.start_read(7));
    ASSERT_TRUE(s.wait_read_response());

    s.check_read_response(
        "Invalid read",
        7,
        0,
        true
    );

    s.tick();

    // Invalid write should complete with an error.
    ASSERT_TRUE(s.start_write(7, 123));
    ASSERT_TRUE(s.wait_write_response());

    s.check_write_response(
        "Invalid write",
        7,
        123,
        true
    );

    s.tick();

    std::cout << "======================================\n";
}

// ============================================================
// TEST 4: Disabled ports
// ============================================================

TEST(SRAMTest, DisabledPortsDoNothing) {
    auto& s = *tb;

    std::cout << "\n========== DISABLED PORTS ==========\n";

    s.initialize();

    ASSERT_TRUE(s.write_value(1, 45));

    // Change the address and data without asserting enable.
    s.write_address.write(1);
    s.write_data.write(100);
    s.write_enable.write(false);

    s.read_address.write(1);
    s.read_enable.write(false);

    s.tick();

    int value = 0;

    ASSERT_TRUE(s.read_value(1, value));
    EXPECT_EQ(value, 45);

    std::cout
        << "[PASS] Disabled write did not modify memory"
        << " | expected=45"
        << " | actual=" << value << '\n';

    std::cout << "====================================\n";
}

// ============================================================
// TEST 5: Reset cancels pending operations but preserves memory
// ============================================================

TEST(SRAMTest, ResetCancelsPendingRequestsAndPreservesMemory) {
    auto& s = *tb;

    std::cout << "\n========== RESET TEST ==========\n";

    s.initialize();

    ASSERT_TRUE(s.write_value(0, 73));

    // READ_LATENCY is 4, so the response should still be pending.
    ASSERT_TRUE(s.start_read(0));

    s.reset.write(true);
    s.tick();

    const bool read_completed = s.read_ready.read();

    std::cout
        << "[" << (!read_completed ? "PASS" : "FAIL") << "] "
        << "Reset cancels pending read"
        << " | read_ready=" << read_completed << '\n';

    EXPECT_FALSE(read_completed);

    s.reset.write(false);
    s.tick();

    int value = 0;

    ASSERT_TRUE(s.read_value(0, value));
    EXPECT_EQ(value, 73);

    std::cout
        << "[PASS] Memory retained after reset"
        << " | expected=73"
        << " | actual=" << value << '\n';

    std::cout << "================================\n";
}

// ============================================================
// TEST 6: Multiple queued reads and response order
// ============================================================

TEST(SRAMTest, MultipleQueuedReadsReturnInOrder) {
    auto& s = *tb;

    std::cout << "\n========== QUEUED READS ==========\n";

    s.initialize();

    ASSERT_TRUE(s.write_value(0, 10));
    ASSERT_TRUE(s.write_value(1, 20));
    ASSERT_TRUE(s.write_value(2, 30));

    ASSERT_TRUE(s.start_read(0));
    ASSERT_TRUE(s.start_read(1));
    ASSERT_TRUE(s.start_read(2));

    const unsigned addresses[] = {0, 1, 2};
    const int expected[] = {10, 20, 30};

    for (int i = 0; i < 3; ++i) {
        ASSERT_TRUE(s.wait_read_response());

        s.check_read_response(
            "Queued read response",
            addresses[i],
            expected[i]
        );

        // Clear the completion pulse before checking the next.
        s.tick();
    }

    std::cout << "=================================\n";
}

// ============================================================
// TEST 7: Request capacity and backpressure
// ============================================================

TEST(SRAMTest, RequestCapacityAndBackpressure) {
    auto& s = *tb;

    std::cout << "\n========== REQUEST CAPACITY ==========\n";

    s.initialize();

    ASSERT_TRUE(s.write_value(0, 10));
    ASSERT_TRUE(s.write_value(1, 20));
    ASSERT_TRUE(s.write_value(2, 30));
    ASSERT_TRUE(s.write_value(3, 40));

    // READ_LATENCY=4 and capacity=4.
    // Issue one read request per cycle.
    ASSERT_TRUE(s.start_read(0));
    ASSERT_TRUE(s.start_read(1));
    ASSERT_TRUE(s.start_read(2));
    ASSERT_TRUE(s.start_read(3));

    const bool ready_when_full = s.read_request_ready.read();

    std::cout
        << "[" << (!ready_when_full ? "PASS" : "FAIL") << "] "
        << "Read request backpressure"
        << " | capacity=4"
        << " | ready_when_full=" << ready_when_full
        << '\n';

    EXPECT_FALSE(ready_when_full);

    // Once a response completes, capacity should become available.
    ASSERT_TRUE(s.wait_read_response());

    s.check_read_response(
        "First response after queue fills",
        0,
        10
    );

    s.tick();

    const bool ready_after_completion =
        s.read_request_ready.read();

    std::cout
        << "[" << (ready_after_completion ? "PASS" : "FAIL") << "] "
        << "Request capacity restored"
        << " | ready=" << ready_after_completion
        << '\n';

    EXPECT_TRUE(ready_after_completion);

    // Drain the remaining three responses in order.
    const int expected[] = {20, 30, 40};

    for (int i = 0; i < 3; ++i) {
        ASSERT_TRUE(s.wait_read_response());

        s.check_read_response(
            "Drained queued response",
            static_cast<unsigned>(i + 1),
            expected[i]
        );

        s.tick();
    }

    std::cout << "======================================\n";
}

// ============================================================
// TEST 8: Read snapshot versus later write
// ============================================================

TEST(SRAMTest, ReadSnapshotsDataAtAcceptance) {
    auto& s = *tb;

    std::cout << "\n========== READ SNAPSHOT ==========\n";

    s.initialize();

    ASSERT_TRUE(s.write_value(1, 11));

    // Read captures 11 when accepted.
    ASSERT_TRUE(s.start_read(1));

    // The later write changes memory to 99.
    ASSERT_TRUE(s.start_write(1, 99));

    ASSERT_TRUE(s.wait_read_response());

    s.check_read_response(
        "Read sees acceptance-time snapshot",
        1,
        11
    );

    ASSERT_TRUE(s.wait_write_response());

    s.check_write_response(
        "Later write completes",
        1,
        99
    );

    s.tick();

    int value = 0;

    ASSERT_TRUE(s.read_value(1, value));
    EXPECT_EQ(value, 99);

    std::cout
        << "[PASS] Final memory contents"
        << " | expected=99"
        << " | actual=" << value << '\n';

    std::cout << "===================================\n";
}

// ============================================================
// SystemC entry point
// ============================================================

int sc_main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);

    static SRAMTestBench testbench{"sram_testbench"};
    tb = &testbench;

    return RUN_ALL_TESTS();
}