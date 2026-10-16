#include "Testbench.h"

#include <cstdlib>
#include <iostream>
#include <string>

int sc_main(int argc, char* argv[])
{
    const std::string workload_dir = argc > 1 ? argv[1] : "workloads/mnist";
    std::size_t sample_index = 0;
    if (argc > 2) {
        try {
            sample_index = static_cast<std::size_t>(std::stoull(argv[2]));
        } catch (...) {
            std::cerr << "Usage: mnist_conv [workload_dir] [sample_index]\n";
            return EXIT_FAILURE;
        }
    }

    sc_core::sc_clock clk("clk", sc_core::sc_time(10, sc_core::SC_NS),
                          0.5, sc_core::sc_time(5, sc_core::SC_NS), true);
    Testbench simulator("mnist_conv", workload_dir, sample_index);
    simulator.clk(clk);
    sc_core::sc_start();
    return simulator.passed() ? EXIT_SUCCESS : EXIT_FAILURE;
}
