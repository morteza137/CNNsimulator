
#pragma once

#include <systemc>
#include "ArithmeticPolicy.h"

template<
    typename WEIGHT_T = sc_dt::sc_int<8>,
    typename IFMAP_T  = sc_dt::sc_int<8>,
    typename PSUM_T   = sc_dt::sc_int<32>,
    typename ARITHMETIC_POLICY = ExactArithmetic
>
SC_MODULE(MAC) {
    using WeightType = WEIGHT_T;
    using IfmapType  = IFMAP_T;
    using PsumType   = PSUM_T;
    using PolicyType = ARITHMETIC_POLICY;

    // Control ports
    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_in<bool> reset{"reset"};
    sc_core::sc_in<bool> enable{"enable"};

    // Data input ports
    sc_core::sc_in<WeightType> weight{"weight"};
    sc_core::sc_in<IfmapType>  ifmap{"ifmap"};
    sc_core::sc_in<PsumType>   psum{"psum"};

    // Data output port
    sc_core::sc_out<PsumType> result{"result"};

    SC_HAS_PROCESS(MAC);

    explicit MAC(sc_core::sc_module_name name)
        : sc_core::sc_module(name) {

        SC_METHOD(compute);
        sensitive << clk.pos();
        dont_initialize();
    }

private:
    void compute() {
        if (reset.read()) {
            result.write(PsumType(0));
            return;
        }

        if (!enable.read()) {
            return; // Hold previous output.
        }

        const WeightType w = weight.read();
        const IfmapType  a = ifmap.read();
        const PsumType   p = psum.read();

        result.write(
            PolicyType::template compute<
                WeightType,
                IfmapType,
                PsumType
            >(w, a, p)
        );
    }
};