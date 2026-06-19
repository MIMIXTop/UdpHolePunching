#pragma once

namespace util {

    template <typename ...Lambdas>
    struct match : Lambdas... {
        using Lambdas::operator()...;
    };

    template <typename ...Lambdas>
    match(Lambdas... lambdas) -> match<Lambdas...>;

}
