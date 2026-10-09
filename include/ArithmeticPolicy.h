#pragma once

#include <cmath>
#include <type_traits>

struct ExactArithmetic {
    template<typename W, typename A, typename P>
    static P compute(const W& weight, const A& ifmap, const P& psum) {
        return P(weight * ifmap + psum);
    }
};

struct ApproximateArithmetic {
    template<typename W, typename A, typename P>
    static P compute(const W& weight, const A& ifmap, const P& psum) {
        if constexpr (std::is_floating_point_v<P>) {
            // Approximate floating-point multiplication.
            const P product = P(weight * ifmap);
            const P rounded = P(std::round(product / P(4)) * P(4));
            return P(rounded + psum);
        } else {
            // Approximate integer multiplication.
            const long long product =
                static_cast<long long>(weight) *
                static_cast<long long>(ifmap);

            // Round to the nearest multiple of four.
            const long long rounded =
                product >= 0
                    ? ((product + 2) / 4) * 4
                    : ((product - 2) / 4) * 4;

            return P(rounded + static_cast<long long>(psum));
        }
    }
};