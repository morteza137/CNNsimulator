
#pragma once

#include <systemc>
#include <memory>

enum class PEArchitecture {
    OS,
    WS,
    IS,
    NO
};

SC_MODULE(PE) {
public:
    using WeightType = sc_dt::sc_int<8>;
    using IfmapType  = sc_dt::sc_int<8>;
    using PsumType   = sc_dt::sc_int<32>;

    // Common control ports
    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_in<bool> reset{"reset"};
    sc_core::sc_in<bool> enable{"enable"};

    // Architecture-specific control ports
    sc_core::sc_in<bool> load_weight{"load_weight"};
    sc_core::sc_in<bool> load_ifmap{"load_ifmap"};
    sc_core::sc_in<bool> use_feedback{"use_feedback"};

    // Common data ports
    sc_core::sc_in<WeightType> weight_in{"weight_in"};
    sc_core::sc_in<IfmapType>  ifmap_in{"ifmap_in"};
    sc_core::sc_in<PsumType>   psum_in{"psum_in"};

    sc_core::sc_out<PsumType> psum_out{"psum_out"};

    SC_HAS_PROCESS(PE);

    explicit PE(
        sc_core::sc_module_name name,
        PEArchitecture architecture = PEArchitecture::NO
    );

    ~PE() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    void select_psum_source();
    void drive_output();
};