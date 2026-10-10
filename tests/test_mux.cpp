#include <systemc>
#include <gtest/gtest.h>

#include <array>
#include <iostream>

#include "MUX.h"

// Verify select width at compile time.
static_assert(MUX<2>::SELECT_WIDTH == 1,
              "2 inputs require 1 select bit");

static_assert(MUX<5>::SELECT_WIDTH == 3,
              "5 inputs require 3 select bits");

static_assert(MUX<512>::SELECT_WIDTH == 9,
              "512 inputs require 9 select bits");

static_assert(MUX<513>::SELECT_WIDTH == 10,
              "513 inputs require 10 select bits");

static_assert(MUX<1024>::SELECT_WIDTH == 10,
              "1024 inputs require 10 select bits");

// ============================================================
// Testbench: 5-input MUX
// ============================================================

struct MUXTestBench : sc_core::sc_module {
    using DataType = sc_dt::sc_int<16>;
    using SelectType = MUX<5, DataType>::SelectType;

    static constexpr std::size_t INPUT_COUNT = 5;

    sc_core::sc_vector<sc_core::sc_signal<DataType>>
        input_signals;

    sc_core::sc_signal<SelectType> select_signal{"select_signal"};
    sc_core::sc_signal<DataType> output_signal{"output_signal"};
    sc_core::sc_signal<bool> valid_signal{"valid_signal"};

    MUX<INPUT_COUNT, DataType> mux{"mux"};

    SC_CTOR(MUXTestBench)
        : input_signals("input_signals", INPUT_COUNT)
    {
        for (std::size_t i = 0; i < INPUT_COUNT; ++i) {
            mux.input[i](input_signals[i]);
        }

        mux.select(select_signal);
        mux.output(output_signal);
        mux.select_valid(valid_signal);

        select_signal.write(0);

        for (std::size_t i = 0; i < INPUT_COUNT; ++i) {
            input_signals[i].write(0);
        }
    }

    void settle() {
        // Allow signal updates and combinational logic to execute.
        sc_core::sc_start(1, sc_core::SC_NS);
    }

    void set_inputs(
        const std::array<int, INPUT_COUNT>& values
    ) {
        for (std::size_t i = 0; i < INPUT_COUNT; ++i) {
            input_signals[i].write(values[i]);
        }

        settle();
    }

    void check_selection(
        const char* test_name,
        unsigned index,
        int expected_data,
        bool expected_valid
    ) {
        select_signal.write(index);
        settle();

        const int actual_data =
            output_signal.read().to_int();

        const bool actual_valid =
            valid_signal.read();

        const bool passed =
            actual_data == expected_data &&
            actual_valid == expected_valid;

        std::cout
            << "[" << (passed ? "PASS" : "FAIL") << "] "
            << test_name
            << " | select=" << index
            << " | expected_output=" << expected_data
            << " | actual_output=" << actual_data
            << " | expected_valid=" << expected_valid
            << " | actual_valid=" << actual_valid
            << '\n';

        EXPECT_EQ(actual_data, expected_data);
        EXPECT_EQ(actual_valid, expected_valid);
    }
};

static MUXTestBench* tb = nullptr;

// ============================================================
// Test 1: Every valid input can be selected.
// ============================================================

TEST(MUXTest, SelectsEveryValidInput) {
    auto& s = *tb;

    std::cout << "\n========== VALID INPUT SELECTION ==========\n";

    const std::array<int, 5> values = {
        11, 22, 33, 44, 55
    };

    s.set_inputs(values);

    for (unsigned i = 0; i < values.size(); ++i) {
        s.check_selection(
            "Valid input",
            i,
            values[i],
            true
        );
    }

    std::cout << "===========================================\n";
}

// ============================================================
// Test 2: Invalid indices for N=5.
//
// SELECT_WIDTH=3 permits binary values 0 through 7.
// Only 0 through 4 are valid.
// ============================================================

TEST(MUXTest, InvalidSelectionReturnsZeroAndInvalid) {
    auto& s = *tb;

    std::cout << "\n========== INVALID SELECTION ==========\n";

    const std::array<int, 5> values = {
        11, 22, 33, 44, 55
    };

    s.set_inputs(values);

    for (unsigned index = 5; index <= 7; ++index) {
        s.check_selection(
            "Out-of-range selection",
            index,
            0,
            false
        );
    }

    std::cout << "=======================================\n";
}

// ============================================================
// Test 3: Output responds to input changes even if select
// remains unchanged.
// ============================================================

TEST(MUXTest, RespondsToInputChanges) {
    auto& s = *tb;

    std::cout << "\n========== INPUT CHANGE ==========\n";

    s.select_signal.write(2);

    std::array<int, 5> values = {
        10, 20, 30, 40, 50
    };

    s.set_inputs(values);

    s.check_selection(
        "Initial selected value",
        2,
        30,
        true
    );

    // Change input 2 while keeping select equal to 2.
    values[2] = 99;
    s.set_inputs(values);

    const int actual = s.output_signal.read().to_int();
    const bool valid = s.valid_signal.read();

    std::cout
        << "[" << ((actual == 99 && valid) ? "PASS" : "FAIL") << "] "
        << "Selected input changes"
        << " | select=2"
        << " | expected_output=99"
        << " | actual_output=" << actual
        << " | valid=" << valid
        << '\n';

    EXPECT_EQ(actual, 99);
    EXPECT_TRUE(valid);

    std::cout << "==================================\n";
}

// ============================================================
// Test 4: Selection remains valid at the upper boundary.
// ============================================================

TEST(MUXTest, SelectsLastValidInput) {
    auto& s = *tb;

    std::cout << "\n========== UPPER BOUNDARY ==========\n";

    const std::array<int, 5> values = {
        100, 200, 300, 400, 500
    };

    s.set_inputs(values);

    s.check_selection(
        "Last valid input",
        4,
        500,
        true
    );

    std::cout << "====================================\n";
}

// ============================================================
// SystemC entry point
// ============================================================

int sc_main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);

    static MUXTestBench testbench{"mux_testbench"};
    tb = &testbench;

    return RUN_ALL_TESTS();
}