// <EnumCycleHistogram.h> -*- C++ -*-


/**
 * \file  EnumCycleHistogram.hpp
 * \brief EnumCycleHistogram implementation using sparta CycleCounters
 *        that models enum state lifetimes.
 *        This Histogram class can be templated on both C++ enums and
 *        sparta::utils::Enums.
 */

#ifndef __ENUM_CYCLE_HISTOGRAM_H__
#define __ENUM_CYCLE_HISTOGRAM_H__

#include <iostream>
#include <string>
#include <vector>
#include "sparta/utils/MathUtils.hpp"
#include "sparta/utils/MetaStructs.hpp"
#include "sparta/utils/DetectMemberUtils.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/statistics/CycleCounter.hpp"
#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/simulation/Resource.hpp"
#include "sparta/statistics/CycleHistogram.hpp"

namespace sparta{

//! Detect whether template parameter is sparta::utils::Enum type.
//  Case when it is not a sparta::utils::Enum type.
template<typename T>
struct is_sparta_enum : public std::false_type{};

//! Detect whether template parameter is sparta::utils::Enum type.
//  Case when it is a sparta::utils::Enum type.
template<typename T>
struct is_sparta_enum<sparta::utils::Enum<T>> : public std::true_type{};

//! Given an enum class type, this method figures out the string
//  name equivalents of the different enum constants. For this,
//  we need to invoke the << operator on individual enum constants.
//  This template overload is SFINAEd out to enable if this enum
//  type U has a << operator overloaded for it.
template<typename U>
MetaStruct::enable_if_t<not sparta::is_sparta_enum<U>::value and
                        sparta::utils::has_ostream_operator<U>::value, void>
getHumanReadableHistogramBinNames(std::vector<std::string>& enum_name_strings){
    typedef typename std::underlying_type<U>::type enum_type;
    constexpr enum_type last_index = static_cast<enum_type>(U::__LAST);
    enum_name_strings.reserve(last_index);
    std::stringstream ss;
    for(enum_type e = 0; e < last_index; ++e){
        auto val = static_cast<U>(e);
        ss << val;
        enum_name_strings.emplace_back(ss.str());
        ss.str("");
        ss.clear();
    }
}

//! Given an enum class type, this method figures out the string
//  name equivalents of the different enum constants. For this,
//  we need to invoke the << operator on individual enum constants.
//  This template overload is SFINAEd out to disable if this enum
//  type U does not have a << operator overloaded for it. We fill
//  the resulting vector with empty strings.
template<typename U>
MetaStruct::enable_if_t<not sparta::is_sparta_enum<U>::value and
                        not sparta::utils::has_ostream_operator<U>::value, void>
getHumanReadableHistogramBinNames(std::vector<std::string>&){}

//! Given an enum class type, this method figures out the string
//  name equivalents of the different enum constants.
//  Template specialization for types which are actually sparta::utils::Enum type.
template<typename U>
MetaStruct::enable_if_t<sparta::is_sparta_enum<U>::value, void>
getHumanReadableHistogramBinNames(std::vector<std::string>& enum_name_strings){
    U::populateNames(enum_name_strings);
}

/*!
 * \brief EnumCycleHistogram class for C++ Enum values.
 *
 * A histogram is usually thought of has  having a lower limit, upper limit,
 * and number of bins.  This histogram class requires lower and upper limits,
 * but instead of number of bins, it requires the user to specify
 * number of values per bin.  The number of bins is then calculated as below:
 *     number_of_bins = (upper_limit - lower_limit) / values_per_bin + 1
 *
 * This is possible because this histogram only deals with positive integer
 * values.
 *
 */
template<typename EnumType>
class EnumCycleHistogram final : public sparta::TreeNode, private sparta::CycleHistogramBase{
public:

    /*!
     * \brief Not default constructable
     */
    EnumCycleHistogram() = delete;

    /*!
     * \brief Not copy-constructable
     */
    EnumCycleHistogram(const EnumCycleHistogram&) = delete;

    /*!
     * \brief Not move-constructable
     */
    EnumCycleHistogram(EnumCycleHistogram&&) = delete;

    /*!
     * \brief Not copy-assignable
     */
    EnumCycleHistogram& operator = (const EnumCycleHistogram&) = delete;

    /*!
     * \brief Not move-assignable
     */
    EnumCycleHistogram& operator = (EnumCycleHistogram&&) = delete;

    /*!
     * \brief EnumCycleHistogram constructor
     * \param parent_treenode parent node. Must not be nullptr
     * \param histogram_name Name of this histograms which is used as name of the
     * TreeNode representing this histogram
     * \param description Description of this histogran. Stored as TreeNode
     * description
     * \param idle_value the value to capture when none nothing was updated
     * (default = 0)
     * \param stat_vis_general Sets the visibility of the stat
     *                         counters for the 0th and last index
     *                         of the utilization counts, so the
     *                         empty and full counts.
     * \param stat_vis_detailed Sets the visibility of the stat
     *                          counts inbetween 0 and the last
     *                          index. i.e. more detailed than the
     *                          general stats.
     *
     * \warning By default the stat_vis_* options are set to
     *          VIS_SPARTA_DEFAULT, for this structure
     *          AUTO_VISIBILITY resolves to
     *          CONTAINER_DEFAULT_VISIBILITY which at the time of
     *          writing this comment is set to VIS_HIDDEN. If you
     *          rely on the stats from this container you should
     *          explicity set the visibility.
     *
     * Enable constructor only if template parameter is C++ enum class.
     */
    template<typename T = EnumType>
    EnumCycleHistogram(TreeNode* parent_treenode,
                       const std::string& histogram_name,
                       const std::string& description,
                       const T idle_value = T::__FIRST,
                       const InstrumentationNode::visibility_t stat_vis_general =
                                           InstrumentationNode::AUTO_VISIBILITY,
                       const InstrumentationNode::visibility_t stat_vis_detailed =
                                            InstrumentationNode::AUTO_VISIBILITY,
                       InstrumentationNode::visibility_t stat_vis_max =
                                            InstrumentationNode::AUTO_VISIBILITY,
                       InstrumentationNode::visibility_t stat_vis_avg =
                                            InstrumentationNode::AUTO_VISIBILITY,
                       MetaStruct::enable_if_t<not sparta::is_sparta_enum<T>::value>* = 0) :
        sparta::TreeNode(histogram_name, description),
        sparta::CycleHistogramBase(static_cast<uint64_t>(T::__FIRST),
                                 static_cast<uint64_t>(T::__LAST) - 1,
                                 1,
                                 static_cast<uint64_t>(idle_value)),
        stats_(this)
        {
            static_assert(std::is_enum<T>::value, "The template type must be a std::enum class type.");
            sparta_assert(parent_treenode and parent_treenode->getClock());
            setExpectedParent_(parent_treenode);
            std::vector<std::string> enum_name_strings;
            //! Store the individual enum constant names
            getHumanReadableHistogramBinNames<T>(enum_name_strings);
            initializeStats_ (&stats_, parent_treenode->getClock(), std::string(),
                              description, stat_vis_general, stat_vis_detailed,
                              stat_vis_max, stat_vis_avg, enum_name_strings);
            parent_treenode->addChild(this);
            total_->startCounting();
            // Start capturing the idle value
            startCounting_(idle_value_);
            updateMaxValues_(idle_value_);
    }

    /*!
     * \brief EnumCycleHistogram constructor
     * \param parent_treenode parent node. Must not be nullptr
     * \param histogram_name Name of this histograms which is used as name of the
     * TreeNode representing this histogram
     * \param description Description of this histogran. Stored as TreeNode
     * description
     * \param idle_value the value to capture when none nothing was updated
     * (default = 0)
     * \param stat_vis_general Sets the visibility of the stat
     *                         counters for the 0th and last index
     *                         of the utilization counts, so the
     *                         empty and full counts.
     * \param stat_vis_detailed Sets the visibility of the stat
     *                          counts inbetween 0 and the last
     *                          index. i.e. more detailed than the
     *                          general stats.
     *
     * \warning By default the stat_vis_* options are set to
     *          VIS_SPARTA_DEFAULT, for this structure
     *          AUTO_VISIBILITY resolves to
     *          CONTAINER_DEFAULT_VISIBILITY which at the time of
     *          writing this comment is set to VIS_HIDDEN. If you
     *          rely on the stats from this container you should
     *          explicity set the visibility.
     *
     * Enable constructor only if template parameter is sparta::utils::Enum class.
     */
    template<typename T = EnumType>
    EnumCycleHistogram(TreeNode* parent_treenode,
                       const std::string& histogram_name,
                       const std::string& description,
                       const typename T::value_type idle_value = T::value_type::__FIRST,
                       const InstrumentationNode::visibility_t stat_vis_general =
                                           InstrumentationNode::AUTO_VISIBILITY,
                       const InstrumentationNode::visibility_t stat_vis_detailed =
                                            InstrumentationNode::AUTO_VISIBILITY,
                       InstrumentationNode::visibility_t stat_vis_max =
                                            InstrumentationNode::AUTO_VISIBILITY,
                       InstrumentationNode::visibility_t stat_vis_avg =
                                            InstrumentationNode::AUTO_VISIBILITY,
                       MetaStruct::enable_if_t<sparta::is_sparta_enum<T>::value>* = 0) :
        sparta::TreeNode(histogram_name, description),
        sparta::CycleHistogramBase(static_cast<uint64_t>(T::value_type::__FIRST),
                                 static_cast<uint64_t>(T::value_type::__LAST) - 1,
                                 1,
                                 static_cast<uint64_t>(idle_value)),
        stats_(this)
        {
            static_assert(sparta::is_sparta_enum<T>::value, "The template type must be sparta enum class type.");
            sparta_assert(parent_treenode and parent_treenode->getClock());
            setExpectedParent_(parent_treenode);
            std::vector<std::string> enum_name_strings;
            //! Store the individual enum constant names
            getHumanReadableHistogramBinNames<T>(enum_name_strings);
            initializeStats_ (&stats_, parent_treenode->getClock(), std::string(),
                              description, stat_vis_general, stat_vis_detailed,
                              stat_vis_max, stat_vis_avg, enum_name_strings);
            parent_treenode->addChild(this);
            total_->startCounting();
            // Start capturing the idle value
            startCounting_(idle_value_);
            updateMaxValues_(idle_value_);
    }

    /*!
     * \brief Start counting, taking into account the specified delay
     * \param delay Begin incrementing counter after this number of cycles
     * has elapsed on the clock associated with this Counter (see
     * sparta::CounterBase::getClock)
     * \pre Must not be counting already (see stopCounting)
     */
    template<typename T = EnumType>
    void startCounting(const T enum_val, uint64_t delay = 0){
        stopCounting_(last_value_);
        startCounting_(static_cast<uint64_t>(enum_val), delay);
        updateMaxValues_(static_cast<uint64_t>(enum_val));
    }

    /*!
     * \brief Stop counting and increment internal count, taking into account the specified delay
     * \param val Value to capture.  Class will determine which bin to increment
     * \param delay Begin incrementing counter after this number of cycles
     * has elapsed on the clock associated with this Counter (see
     * sparta::CounterBase::getClock)
     * \pre Must be counting already (see startCounting)
     */
    template<typename T = EnumType>
    void stopCounting(const T enum_val, uint64_t delay = 0){
        stopCounting_(static_cast<uint64_t>(enum_val), delay);
        startCounting_(idle_value_);
    }

    //! Method to return the upper value of the histogram
    uint64_t getHistogramUpperValue() const{
        return this->sparta::CycleHistogramBase::getHistogramUpperValue();
    }

    //! Method to return the lower value of the histogram
    uint64_t getHistogramLowerValue() const{
        return this->sparta::CycleHistogramBase::getHistogramLowerValue();
    }

    //! Method to return the number of bins of the histogram
    uint64_t getNumBins() const{
        return this->sparta::CycleHistogramBase::getNumBins();
    }

    //! Method to return the number of values per bin of the histogram
    uint64_t getNumValuesPerBin() const{
        return this->sparta::CycleHistogramBase::getNumValuesPerBin();
    }

    private:
    //! StatisticSet node
    sparta::StatisticSet stats_;
}; // class EnumCycleHistogram
}  // namespace sparta
#endif
