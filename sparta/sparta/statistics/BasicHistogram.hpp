// <BasicHistogram.h> -*- C++ -*-

/**
 * \file BasicHistogram.hpp
 * \brief A simple histogram with programmable ranges, using sparta::Counters
 */
#pragma once

#include <string>
#include <vector>
#include <sstream>
#include "sparta/statistics/Counter.hpp"

namespace sparta
{
/**
 * \class BasicHistogram
 * \tparam BucketT Type contained in the buckets
 * \tparam ASSERT_ON_UNDERFLOW (default false) true will assert if an underflow is detected
 *
 * This class will create sparta::Counters for each "bucket" of BucketT
 * given in the contructor
 *
 * The objects contained in the buckets must follow these rules:
 *  # The object type must be copyable (for initialization)
 *  # The object must respond to the comparison operator== and operator< operator>
 *
 * A "bucket" is charged a count if an object being added is less than
 * the given bucket.  Examples:
 *
 * \code
 *    sparta::BasicHistogram<int> example_bh(sset_, "example_bh", "Example BasicHistogram", {0,10,20});
 *    example_bh.addValue(-1);  // Will add a charge to the  0 -> 10 bucket
 *    example_bh.addValue( 1);  // Will add a charge to the  0 -> 10 bucket
 *    example_bh.addValue(10);  // Will add a charge to the  0 -> 10 bucket
 *    example_bh.addValue(11);  // Will add a charge to the 10 -> 20 bucket
 *    example_bh.addValue(20);  // Will add a charge to the 10 -> 20 bucket
 *    example_bh.addValue(21);  // Will add a charge to the 10 -> 20 bucket
 * \endcode
 */
template<typename BucketT, bool ASSERT_ON_UNDERFLOW=false>
class BasicHistogram
{
public:
    /**
     * \brief Construct a BasicHistogram
     * \param sset The sparta::StatisticSet this histogram belongs to
     * \param name The name of thie BasicHistogram
     * \param desc A useful description
     * \param buckets one bucket will be created per value (plus one for overflow) - values must be sorted
     */
    BasicHistogram(sparta::StatisticSet &sset,
                   const std::string &name,
                   const std::string &desc,
                   const std::vector<BucketT> &buckets) :
        bucket_vals_(buckets)
    {
        sparta_assert(std::is_sorted(buckets.begin(), buckets.end()), "Buckets must be sorted");

        const auto bucket_size = bucket_vals_.size();
        ctrs_.reserve(bucket_size);

        // create one counter per bucket
        for (unsigned i = 0; i < bucket_size; ++i)
        {
            auto &v = bucket_vals_[i];
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

            ctrs_.emplace_back(&sset, os_name.str(), os_desc.str(), sparta::Counter::COUNT_NORMAL);
        }
    }

    /// Destroy, non-virtual
    ~BasicHistogram() {}

    /**
     * \brief Charge a bucket where the given val falls
     * \param val The value to charge
     *
     * A "bucket" is charged a count if an object being added is less
     * than the given bucket.  Overflows will go into the last bucket.
     * Undeflows will either assert or charge to the smallest bucket
     * (depending on class template parameter ASSERT_ON_UNDERFLOW)
     */
    void addValue(const BucketT &val)
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

    /// Disallow copies/assignment/move
    BasicHistogram(const BasicHistogram &) = delete;
    BasicHistogram(      BasicHistogram &&) = delete;
    const BasicHistogram & operator=(const BasicHistogram &) = delete;
    BasicHistogram       & operator=(      BasicHistogram &&) = delete;

private: // data
    std::vector<BucketT> bucket_vals_; ///< user-specified buckets
    std::vector<sparta::Counter> ctrs_; ///< one counter per bucket
};
} // namespace sparta
