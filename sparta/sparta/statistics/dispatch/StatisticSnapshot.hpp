// <StatisticSnapshot> -*- C++ -*-

#pragma once

namespace sparta {
namespace statistics {

/*!
 * \brief User-friendly wrapper around a double reference. This is
 * like a std::reference_wrapper that connects one StatisticInstance
 * with somebody else's double in another data structure. It lets SI's
 * write their double data value directly into a contiguous std::vector
 * sitting on top of the report archive system. This is done for improved
 * performance and overall ease of use - a single std::vector<double> vs.
 * individual StatisticInstance's scattered all over.
 */
class StatisticSnapshot
{
public:
    explicit StatisticSnapshot(double & value) :
        value_(value)
    {}

    inline double takeSnapshot(const double value) {
        value_ = value;
        return value_;
    }

private:
    double & value_;
};

} // namespace statistics
} // namespace sparta

