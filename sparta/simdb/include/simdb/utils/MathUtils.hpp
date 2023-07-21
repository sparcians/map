// <MathUtils> -*- C++ -*-

#pragma once

#include <cmath>
#include <cinttypes>
#include <limits>
#include <random>
#include <time.h>

namespace simdb {
namespace utils {

//! \brief Comparison of two floating-point values with
//! a supplied tolerance. The tolerance value defaults
//! to machine epsilon.
template <typename T>
typename std::enable_if<
    std::is_floating_point<T>::value,
bool>::type
approximatelyEqual(const T a, const T b,
                   const T epsilon = std::numeric_limits<T>::epsilon())
{
    const T fabs_a = std::fabs(a);
    const T fabs_b = std::fabs(b);
    const T fabs_diff = std::fabs(a - b);

    return fabs_diff <= ((fabs_a < fabs_b ? fabs_b : fabs_a) * epsilon);
}

//! Static/global random number generator
struct RandNumGen {
    static std::mt19937 & get() {
        static std::mt19937 rng(time(nullptr));
        return rng;
    }
};

//! \brief Pick a random integral number
template <typename T>
typename std::enable_if<
    std::is_integral<T>::value,
T>::type
chooseRand()
{
    std::uniform_int_distribution<T> dist;
    return dist(RandNumGen::get());
}

//! \brief Pick a random floating-point number
template <typename T>
typename std::enable_if<
    std::is_floating_point<T>::value,
T>::type
chooseRand()
{
    std::normal_distribution<T> dist(0, 1000);
    return dist(RandNumGen::get());
}

} // namespace utils
} // namespace simdb
