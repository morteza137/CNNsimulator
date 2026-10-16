#pragma once

#include <systemc>
#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "Dataflow.h"

// Top-level executable simulation for the trained MNIST convolution layer.
// Reads the existing workloads/mnist text files and executes the convolution
// through the existing Dataflow module configured as a 9x1 WS array.
SC_MODULE(Testbench) {
public:
    using WeightType = PE::WeightType;
    using IfmapType = PE::IfmapType;
    using PsumType = PE::PsumType;

    sc_core::sc_in<bool> clk{"clk"};

    SC_HAS_PROCESS(Testbench);
    Testbench(sc_core::sc_module_name name,
                       std::string workload_dir = "workloads/mnist",
                       std::size_t sample_index = 0);

    bool passed() const noexcept { return passed_; }
    std::size_t cycles() const noexcept { return cycles_; }

private:
    static constexpr std::size_t INPUT_H = 7;
    static constexpr std::size_t INPUT_W = 7;
    static constexpr std::size_t KERNEL = 3;
    static constexpr std::size_t TERMS = KERNEL * KERNEL;
    static constexpr std::size_t OUTPUT_H = 7; // stride 1, padding 1
    static constexpr std::size_t OUTPUT_W = 7;
    static constexpr std::size_t OUTPUTS_PER_FILTER = OUTPUT_H * OUTPUT_W;

    struct ModelConfig {
        std::size_t filters = 0;
        std::size_t input_channels = 0;
        std::size_t input_h = 0;
        std::size_t input_w = 0;
        std::size_t kernel_h = 0;
        std::size_t kernel_w = 0;
        std::size_t stride = 0;
        std::size_t padding = 0;
        unsigned conv_shift = 0;
        bool relu = false;
        std::string weights_file;
        std::string biases_file;
    };

    const std::string workload_dir_;
    const std::size_t sample_index_;
    ModelConfig config_;

    std::vector<int> image_;
    std::vector<int> weights_;
    std::vector<long long> biases_;
    std::vector<int> simulated_output_;
    std::vector<int> reference_output_;

    // Internal signals for the 9x1 WS Dataflow convolution reduction.
    sc_core::sc_signal<bool> reset_sig_{"reset_sig"};
    sc_core::sc_signal<bool> enable_sig_{"enable_sig"};
    sc_core::sc_signal<bool> load_weight_sig_{"load_weight_sig"};
    sc_core::sc_signal<bool> load_ifmap_sig_{"load_ifmap_sig"};
    sc_core::sc_signal<bool> use_feedback_sig_{"use_feedback_sig"};
    sc_core::sc_vector<sc_core::sc_signal<WeightType>> weight_sig_;
    sc_core::sc_vector<sc_core::sc_signal<IfmapType>> ifmap_sig_;
    sc_core::sc_signal<PsumType> psum_in_sig_{"psum_in_sig"};
    sc_core::sc_vector<sc_core::sc_signal<PsumType>> psum_out_sig_;
    sc_core::sc_signal<PsumType> final_out_sig_{"final_out_sig"};
    Dataflow dataflow_;

    bool passed_ = false;
    std::size_t cycles_ = 0;

    void run();
    void load_workload();
    void parse_model();
    void load_image();
    void load_parameters();
    void build_reference();
    std::array<int, TERMS> make_window(std::size_t y, std::size_t x) const;
    int quantize_output(long long accumulator) const;
    void compare_and_report();
};
