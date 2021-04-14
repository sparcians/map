// <BasicHistogram.h> -*- C++ -*-

/**
 * \file BasicHistogram.hpp
 * \brief A simple histogram with programmable ranges, using sparta::Counters
 */
#pragma once

#include "sparta/statistics/Counter.hpp"

namespace sparta
{
/// Histogram with programmable buckets
template<typename T, bool ASSERT_ON_UNDERFLOW>
class BasicHistogram
{
public:
    /// constructor
    ///\param buckets one bucket will be created per value (plus one for overflow) - values must be sorted
    BasicHistogram(sparta::StatisticSet &sset,
                   const std::string &name,
                   const std::string &desc,
                   const std::vector<T> &buckets)
    : bucket_vals_(buckets)
    {
        sparta_assert(std::is_sorted(buckets.begin(), buckets.end()), "Buckets must be sorted");

        // create one counter per bucket
        for (unsigned i = 0; i < bucket_vals_.size(); ++i)
        {
            const T &v = bucket_vals_[i];
            std::ostringstream os_name;
            if (v < 0)
                os_name << name << "_n" << -v; // negative
            else
                os_name << name << '_' << v;

            std::ostringstream os_desc;
            if (i == 0)
            {
                os_desc << desc << " with values less than or equal to " << v;
            }
            else
            {
                os_desc << desc << " with values greater than " << bucket_vals_[i-1] << " and less than or equal to " << v;
            }

            ctrs_.push_back(sparta::Counter(&sset, os_name.str(), os_desc.str(), sparta::Counter::COUNT_NORMAL));
        }
    }

    /// increment the bucket corresponding to 'val'
    void addValue(const T &val)
    {
        // upper_bound will yield the bucket beyond the one we want
        auto bucket = std::upper_bound(bucket_vals_.begin(), bucket_vals_.end(), val);

        // check for underflow (value below first bucket)
        if (bucket == bucket_vals_.begin())
        {
            sparta_assert(!ASSERT_ON_UNDERFLOW, "Value below first bucket");
            ++ctrs_[0]; // put underflow in first bucket
            return;
        }
        //else calculate offset into counter array
        auto off = bucket - bucket_vals_.begin() - 1;
        ++ctrs_[off];
    }

protected: // data
    std::vector<T> bucket_vals_; ///< user-specified buckets
    std::vector<sparta::Counter> ctrs_; ///< one counter per bucket
};
} // namespace sparta

