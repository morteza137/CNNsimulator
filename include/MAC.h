#pragma once

#include <systemc>

template<
    int WEIGHT_WIDTH = 8,
    int IFMAP_WIDTH  = 8,
    int PSUM_WIDTH   = 32
>
SC_MODULE(MAC) {
    using WeightType = sc_dt::sc_int<WEIGHT_WIDTH>;
    using IfmapType  = sc_dt::sc_int<IFMAP_WIDTH>;
    using PsumType   = sc_dt::sc_int<PSUM_WIDTH>;

    sc_core::sc_in<WeightType> weight{"weight"};
    sc_core::sc_in<IfmapType>  ifmap{"ifmap"};
    sc_core::sc_in<PsumType>   psum{"psum"};

    sc_core::sc_in<bool> approximate{"approximate"};
    sc_core::sc_out<PsumType> result{"result"};

    SC_CTOR(MAC) {
        SC_METHOD(compute);
        sensitive << weight << ifmap << psum << approximate;
    }

private:
    // Exact multiplication and accumulation
    PsumType exact(
        WeightType w,
        IfmapType a,
        PsumType p
    ) {
        return PsumType(w * a) + p;
    }

    // Approximate multiplication: placeholder for future implementation
    PsumType approximate_mac(
        WeightType w,
        IfmapType a,
        PsumType p
    ) {
        const PsumType product = w * a;
        return product + p;
    }

    void compute() {
        const auto w = weight.read();
        const auto a = ifmap.read();
        const auto p = psum.read();

        if (approximate.read()) {
            result.write(approximate_mac(w, a, p));
        } else {
            result.write(exact(w, a, p));
        }
    }
};