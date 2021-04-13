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
class BasicHistogram
{
public:
    /// constructor
    ///\param buckets one bucket will be created per value (plus one for overflow) - values must be sorted
    BasicHistogram(sparta::StatisticSet &sset, const std::string &name, const std::string &desc, const std::vector<int64_t> &buckets)
    : bucket_vals_(buckets)
    {
        // create one counter per bucket
        for (unsigned i = 0; i < bucket_vals_.size(); ++i)
        {
            const int64_t v = bucket_vals_[i];
            std::ostringstream os_name;
            if (v < 0)
                os_name << name << "_n" << -v; // negative
            else
                os_name << name << '_' << v;

            std::ostringstream os_desc;
            if (i == 0)
            {
                os_desc << desc << " with values less than " << v;
            }
            else
            {
                os_desc << desc << " with values greater than or equal to " << bucket_vals_[i-1] << " and less than " << v;
            }

            ctrs_.push_back(sparta::Counter(&sset, os_name.str(), os_desc.str(), sparta::Counter::COUNT_NORMAL));
        }

        // create a final bucket for everything beyond the last value
        std::ostringstream os_name;
        os_name << name << "_of";
        std::ostringstream os_desc;
        os_desc << desc << " overflow";
        ctrs_.push_back(sparta::Counter(&sset, os_name.str(), os_desc.str(), sparta::Counter::COUNT_NORMAL));
    }

    /// increment the bucket corresponding to 'val'
    void addValue(int64_t val)
    {
        auto bucket = std::lower_bound(bucket_vals_.begin(), bucket_vals_.end(), val);
        auto off = bucket - bucket_vals_.begin();
        ++ctrs_[off];
    }

private: // data
    std::vector<int64_t> bucket_vals_; ///< user-specified buckets
    std::vector<sparta::Counter> ctrs_; ///< one counter per bucket
};
} // namespace sparta

