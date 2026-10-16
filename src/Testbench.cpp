#include "Testbench.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

Testbench::Testbench(sc_core::sc_module_name name,
                                       std::string workload_dir,
                                       std::size_t sample_index)
    : sc_core::sc_module(name),
      workload_dir_(std::move(workload_dir)),
      sample_index_(sample_index),
      weight_sig_("weight_sig", TERMS),
      ifmap_sig_("ifmap_sig", TERMS),
      psum_out_sig_("psum_out_sig", TERMS),
      dataflow_("conv_dataflow", TERMS, 1, DataflowMode::WS)
{
    dataflow_.clk(clk);
    dataflow_.reset(reset_sig_);
    dataflow_.enable(enable_sig_);
    dataflow_.load_weight(load_weight_sig_);
    dataflow_.load_ifmap(load_ifmap_sig_);
    dataflow_.use_feedback(use_feedback_sig_);
    dataflow_.psum_in[0](psum_in_sig_);
    dataflow_.final_out[0](final_out_sig_);

    for (std::size_t i = 0; i < TERMS; ++i) {
        dataflow_.weight_in[i](weight_sig_[i]);
        dataflow_.ifmap_in[i](ifmap_sig_[i]);
        dataflow_.psum_out[i](psum_out_sig_[i]);
    }

    SC_THREAD(run);
}

void Testbench::parse_model()
{
    const std::string path = workload_dir_ + "/model.dat";
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Cannot open model file: " + path);

    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream row(line);
        std::string key;
        row >> key;
        if (key == "INPUT") {
            row >> config_.input_channels >> config_.input_h >> config_.input_w;
        } else if (key == "CONV2D") {
            int relu = 0;
            row >> config_.filters >> config_.kernel_h >> config_.kernel_w
                >> config_.stride >> config_.padding >> config_.conv_shift
                >> relu >> config_.weights_file >> config_.biases_file;
            config_.relu = relu != 0;
        }
    }

    if (config_.filters == 0 || config_.input_channels != 1 ||
        config_.input_h != INPUT_H || config_.input_w != INPUT_W ||
        config_.kernel_h != KERNEL || config_.kernel_w != KERNEL ||
        config_.stride != 1 || config_.padding != 1) {
        throw std::runtime_error(
            "This first simulator supports the repository model's 1x7x7 input, "
            "3x3 convolution, stride=1 and padding=1 only.");
    }
}

void Testbench::load_image()
{
    const std::string path = workload_dir_ + "/test_images.dat";
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Cannot open MNIST images: " + path);

    const std::size_t count = (sample_index_ + 1) * INPUT_H * INPUT_W;
    std::vector<int> values(count);
    for (std::size_t i = 0; i < count; ++i) {
        if (!(input >> values[i])) {
            throw std::runtime_error("Image index is out of range or test_images.dat is truncated");
        }
        if (values[i] < 0 || values[i] > 127) {
            throw std::runtime_error("Expected quantized MNIST pixels in [0,127]");
        }
    }
    image_.assign(values.end() - static_cast<std::ptrdiff_t>(INPUT_H * INPUT_W), values.end());
}

void Testbench::load_parameters()
{
    const auto read_ints = [](const std::string& path) {
        std::ifstream input(path);
        if (!input) throw std::runtime_error("Cannot open parameter file: " + path);
        std::vector<long long> values;
        long long value = 0;
        while (input >> value) values.push_back(value);
        return values;
    };

    const auto raw_weights = read_ints(workload_dir_ + "/" + config_.weights_file);
    const auto raw_biases = read_ints(workload_dir_ + "/" + config_.biases_file);
    if (raw_weights.size() != config_.filters * TERMS || raw_biases.size() != config_.filters) {
        throw std::runtime_error("Convolution parameter count does not match model.dat");
    }
    weights_.reserve(raw_weights.size());
    for (long long w : raw_weights) {
        if (w < -128 || w > 127) throw std::runtime_error("Convolution weight does not fit int8");
        weights_.push_back(static_cast<int>(w));
    }
    biases_ = raw_biases;
}

void Testbench::load_workload()
{
    parse_model();
    load_image();
    load_parameters();
}

std::array<int, Testbench::TERMS>
Testbench::make_window(std::size_t y, std::size_t x) const
{
    std::array<int, TERMS> window{};
    std::size_t k = 0;
    for (int ky = -1; ky <= 1; ++ky) {
        for (int kx = -1; kx <= 1; ++kx) {
            const int iy = static_cast<int>(y) + ky;
            const int ix = static_cast<int>(x) + kx;
            if (iy >= 0 && iy < static_cast<int>(INPUT_H) &&
                ix >= 0 && ix < static_cast<int>(INPUT_W)) {
                window[k] = image_[static_cast<std::size_t>(iy) * INPUT_W +
                                   static_cast<std::size_t>(ix)];
            } else {
                window[k] = 0;
            }
            ++k;
        }
    }
    return window;
}

int Testbench::quantize_output(long long accumulator) const
{
    // Matches tools/train_mnist.py: arithmetic right shift, ReLU, then int8 range.
    long long value = accumulator >> config_.conv_shift;
    if (config_.relu) value = std::max(0LL, value);
    value = std::max(-128LL, std::min(127LL, value));
    return static_cast<int>(value);
}

void Testbench::build_reference()
{
    reference_output_.assign(config_.filters * OUTPUTS_PER_FILTER, 0);
    for (std::size_t f = 0; f < config_.filters; ++f) {
        for (std::size_t y = 0; y < OUTPUT_H; ++y) {
            for (std::size_t x = 0; x < OUTPUT_W; ++x) {
                const auto window = make_window(y, x);
                long long acc = biases_[f];
                for (std::size_t k = 0; k < TERMS; ++k) {
                    acc += static_cast<long long>(weights_[f * TERMS + k]) * window[k];
                }
                reference_output_[f * OUTPUTS_PER_FILTER + y * OUTPUT_W + x] =
                    quantize_output(acc);
            }
        }
    }
}

void Testbench::run()
{
    try {
        load_workload();
        simulated_output_.assign(config_.filters * OUTPUTS_PER_FILTER, 0);
        build_reference();

        reset_sig_.write(true);
        enable_sig_.write(false);
        load_weight_sig_.write(false);
        load_ifmap_sig_.write(false);
        use_feedback_sig_.write(false);
        for (std::size_t i = 0; i < TERMS; ++i) {
            weight_sig_[i].write(0);
            ifmap_sig_[i].write(0);
        }
        // Hold reset across two rising edges.
        wait(clk.posedge_event());
        ++cycles_;
        wait(clk.posedge_event());
        ++cycles_;
        reset_sig_.write(false);
        wait(clk.negedge_event());

        const std::size_t windows_count = OUTPUTS_PER_FILTER;
        for (std::size_t f = 0; f < config_.filters; ++f) {
            // Load this filter's nine weights into the WS weight registers.
            for (std::size_t k = 0; k < TERMS; ++k)
                weight_sig_[k].write(weights_[f * TERMS + k]);
            load_weight_sig_.write(true);
            enable_sig_.write(false);
            wait(clk.posedge_event());
            ++cycles_;
            wait(sc_core::SC_ZERO_TIME);
            load_weight_sig_.write(false);

            // Stream one output window per cycle. The activation for PE k is
            // delayed by k cycles so it meets the partial sum for the same window.
            const std::size_t total_cycles = windows_count + TERMS;
            for (std::size_t cycle = 0; cycle < total_cycles; ++cycle) {
                wait(clk.negedge_event());
                enable_sig_.write(true);
                psum_in_sig_.write(0);
                for (std::size_t k = 0; k < TERMS; ++k) {
                    if (cycle >= k && cycle - k < windows_count) {
                        const std::size_t window_id = cycle - k;
                        const std::size_t y = window_id / OUTPUT_W;
                        const std::size_t x = window_id % OUTPUT_W;
                        const auto window = make_window(y, x);
                        ifmap_sig_[k].write(window[k]);

                    } else {
                        ifmap_sig_[k].write(0);
                    }
                }
                // Bias is injected only for valid windows at PE 0.
                psum_in_sig_.write(
                    (cycle > 0 && cycle <= windows_count) ? biases_[f] : 0
                );

                wait(clk.posedge_event());
                for (int delta = 0; delta < 6; ++delta) wait(sc_core::SC_ZERO_TIME);
                ++cycles_;
                if (cycle >= TERMS) {
                    const std::size_t output_index = cycle - TERMS;
                    if (output_index < windows_count) {
                        simulated_output_[f * OUTPUTS_PER_FILTER + output_index] =
                            quantize_output(final_out_sig_.read().to_int64());
                    }
                }
            }
            enable_sig_.write(false);
            // Drain/reset the partial-sum pipeline before loading next filter.
            for (std::size_t drain = 0; drain < TERMS; ++drain) {
                wait(clk.negedge_event());
                for (std::size_t k = 0; k < TERMS; ++k) ifmap_sig_[k].write(0);
                psum_in_sig_.write(0);
                enable_sig_.write(true);
                wait(clk.posedge_event());
                for (int delta = 0; delta < 6; ++delta) wait(sc_core::SC_ZERO_TIME);
                ++cycles_;
            }
            enable_sig_.write(false);
        }

        compare_and_report();
        sc_core::sc_stop();
    } catch (const std::exception& e) {
        std::cerr << "Testbench error: " << e.what() << '\n';
        passed_ = false;
        sc_core::sc_stop();
    }
}

void Testbench::compare_and_report()
{
    std::size_t mismatches = 0;
    for (std::size_t i = 0; i < simulated_output_.size(); ++i) {
        if (simulated_output_[i] != reference_output_[i]) {
            if (mismatches < 10) {
                std::cerr << "Mismatch at filter=" << i / OUTPUTS_PER_FILTER
                          << ", y=" << (i % OUTPUTS_PER_FILTER) / OUTPUT_W
                          << ", x=" << (i % OUTPUTS_PER_FILTER) % OUTPUT_W
                          << ": simulated=" << simulated_output_[i]
                          << ", reference=" << reference_output_[i] << '\n';
            }
            ++mismatches;
        }
    }
    passed_ = mismatches == 0;
    std::cout << "MNIST convolution simulation\n"
              << "  sample index: " << sample_index_ << '\n'
              << "  input: 1x7x7 (quantized MNIST)\n"
              << "  filters: " << config_.filters << " x 3x3\n"
              << "  output: " << config_.filters << "x7x7\n"
              << "  simulated clock cycles: " << cycles_ << '\n'
              << "  output elements: " << simulated_output_.size() << '\n'
              << "  mismatches: " << mismatches << '\n'
              << "  result: " << (passed_ ? "PASS" : "FAIL") << '\n';

    if (!simulated_output_.empty()) {
        std::cout << "  filter 0 output (7x7):\n";
        for (std::size_t y = 0; y < OUTPUT_H; ++y) {
            for (std::size_t x = 0; x < OUTPUT_W; ++x) {
                std::cout << simulated_output_[y * OUTPUT_W + x]
                          << (x + 1 == OUTPUT_W ? '\n' : ' ');
            }
        }
    }
}

