#include "Controller.h"

#include <stdexcept>

Controller::Controller(
    sc_core::sc_module_name name,
    DataflowMode mode,
    std::size_t compute_cycles
)
    : sc_core::sc_module(name),
      mode_(mode),
      compute_cycles_(compute_cycles)
{
    if (compute_cycles_ == 0) {
        throw std::invalid_argument(
            "compute_cycles must be greater than zero"
        );
    }

    SC_CTHREAD(control_thread, clk.pos());
    reset_signal_is(reset, true);
}

void Controller::control_thread()
{
    // Safe initial control values.
    enable.write(false);
    load_weight.write(false);
    load_ifmap.write(false);
    use_feedback.write(false);

    busy.write(false);
    done.write(false);

    wait();

    while (true) {
        // Default values between operations.
        enable.write(false);
        load_weight.write(false);
        load_ifmap.write(false);
        use_feedback.write(false);

        busy.write(false);
        done.write(false);

        // Wait until a start request is received.
        do {
            wait();
        } while (!start.read());

        switch (mode_) {

        // =========================================
        // Weight Stationary (WS)
        // =========================================
        case DataflowMode::WS: {
            busy.write(true);

            // Phase 1: Load weights into the PEs.
            load_weight.write(true);
            enable.write(false);

            wait();

            // Phase 2: Execute for the requested
            // number of compute clock cycles.
            load_weight.write(false);
            enable.write(true);

            for (std::size_t cycle = 0;
                 cycle < compute_cycles_;
                 ++cycle) {
                wait();
            }

            // Phase 3: Finish this operation.
            enable.write(false);
            busy.write(false);
            done.write(true);

            // Keep done asserted for one clock period.
            wait();
            done.write(false);

            break;
        }

        // =========================================
        // Output Stationary (OS) - TODO
        // =========================================
        case DataflowMode::OS:
            // TODO:
            // 1. Configure the OS control sequence.
            // 2. Coordinate partial-sum feedback.
            // 3. Define the compute and completion phases.

            SC_REPORT_WARNING(
                "Controller",
                "OS scheduling is not implemented yet"
            );

            // Wait for start to be deasserted before
            // accepting another request.
            while (start.read()) {
                wait();
            }
            break;

        // =========================================
        // Input Stationary (IS) - TODO
        // =========================================
        case DataflowMode::IS:
            // TODO:
            // 1. Load and retain input activations.
            // 2. Schedule weight delivery.
            // 3. Define the compute and completion phases.

            SC_REPORT_WARNING(
                "Controller",
                "IS scheduling is not implemented yet"
            );

            while (start.read()) {
                wait();
            }
            break;

        // =========================================
        // Direct Dataflow - TODO
        // =========================================
        case DataflowMode::DIRECT:
            // TODO:
            // Implement direct-mode scheduling.

            SC_REPORT_WARNING(
                "Controller",
                "DIRECT scheduling is not implemented yet"
            );

            while (start.read()) {
                wait();
            }
            break;
        }
    }
}