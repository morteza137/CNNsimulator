
#pragma once

#include <systemc>
#include "ArithmeticPolicy.h"

template<
    typename WEIGHT_T = sc_dt::sc_int<8>,
    typename IFMAP_T  = sc_dt::sc_int<8>,
    typename PSUM_T   = sc_dt::sc_int<32>
>
SC_MODULE(MAC) {
    using WeightType = WEIGHT_T;
    using IfmapType  = IFMAP_T;
    using PsumType   = PSUM_T;

    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_in<bool> reset{"reset"};
    sc_core::sc_in<bool> enable{"enable"};

    sc_core::sc_in<WeightType> weight{"weight"};
    sc_core::sc_in<IfmapType>  ifmap{"ifmap"};
    sc_core::sc_in<PsumType>   psum{"psum"};

    sc_core::sc_in<bool> approximate{"approximate"};
    sc_core::sc_out<PsumType> result{"result"};

    SC_CTOR(MAC) {
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
            return; // Hold the previous output.
        }

        const auto w = weight.read();
        const auto a = ifmap.read();
        const auto p = psum.read();

        if (approximate.read()) {
            result.write(
                ApproximateArithmetic::compute<WeightType, IfmapType, PsumType>(
                    w, a, p
                )
            );
        } else {
            result.write(
                ExactArithmetic::compute<WeightType, IfmapType, PsumType>(
                    w, a, p
                )
            );
        }
    }
};