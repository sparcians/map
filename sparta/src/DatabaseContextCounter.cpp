// <DatabaseContextCounter> -*- C++ -*-


#include "sparta/report/db/DatabaseContextCounter.hpp"

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <istream>

#include "sparta/report/Report.hpp"
#include "rapidjson/document.h"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/statistics/StatisticInstance.hpp"

namespace sparta {
namespace db {

//! Pick off the last part of an SI's location, for example
//! if the location is "top.core0.rob.stats.ipc", the name
//! returned from this function would be "ipc".
std::string getNameFromSILocation(const StatisticInstance * si)
{
    const std::string name = si->getLocation();
    auto dot = static_cast<long>(name.find_last_of("."));
    if (dot < static_cast<long>(name.size()) - 1) {
        return name.substr(static_cast<size_t>(dot) + 1ul);
    }
    return name;
}

//! Construct with a root SI, and a list of SI's which are
//! "unprintable" to the outside world (the report formatters).
//! The root SI corresponds to the original simulation's
//! ContextCounter<T> (StatisticDef), and the unprintable
//! SI's correspond to the original simulation's Context
//! Counter sub-statistics (internal_counters_).
DatabaseContextCounter::DatabaseContextCounter(
    const StatisticInstance * cc_node,
    std::shared_ptr<UnprintableSIs> & unprintable_sis)
{
    cc_node_ = cc_node;
    cc_desc_ = cc_node_->getDesc(false);
    cc_name_ = getNameFromSILocation(cc_node);
    unprintable_sis_ = unprintable_sis;
}

//! Analogous to TreeNode::getName()
const std::string & DatabaseContextCounter::getName() const
{
    return cc_name_;
}

//! Analogous to InstrumentationNode::groupedPrinting()
bool DatabaseContextCounter::groupedPrinting(
    const std::vector<const StatisticInstance*> & sub_stats,
    std::set<const void*> & dont_print_these,
    void * grouped_json,
    void * doc) const
{
    extractCtxInfo_(sub_stats);

    const bool printing_taken_care_of = groupedPrinting_(
        dont_print_these, grouped_json, doc,
        ctx_info_, cc_desc_, cc_node_->getVisibility());

    if (printing_taken_care_of) {
        appendUnprintablesToSet_(dont_print_these);
    }
    return printing_taken_care_of;
}

//! Analogous to InstrumentationNode::groupedPrintingReduced()
bool DatabaseContextCounter::groupedPrintingReduced(
    const std::vector<const StatisticInstance*> & sub_stats,
    std::set<const void*> & dont_print_these,
    void * grouped_json,
    void * doc) const
{
    extractCtxInfo_(sub_stats);

    const bool printing_taken_care_of = groupedPrintingReduced_(
        dont_print_these, grouped_json, doc, ctx_info_);

    if (printing_taken_care_of) {
        appendUnprintablesToSet_(dont_print_these);
    }
    return printing_taken_care_of;
}

//! Analogous to InstrumentationNode::groupedPrintingDetail()
bool DatabaseContextCounter::groupedPrintingDetail(
    const std::vector<const StatisticInstance*> & sub_stats,
    std::set<const void*> & dont_print_these,
    void * grouped_json,
    void * doc) const
{
    extractCtxInfo_(sub_stats);

    const bool printing_taken_care_of = groupedPrintingDetail_(
        dont_print_these, grouped_json, doc, ctx_info_);

    if (printing_taken_care_of) {
        appendUnprintablesToSet_(dont_print_these);
    }
    return printing_taken_care_of;
}

//! Mimics ContextCounter<T>::extractCtxInfo_
void DatabaseContextCounter::extractCtxInfo_(
    const std::vector<const StatisticInstance*> & sub_stats) const
{
    if (!ctx_info_.empty()) {
        sparta_assert(sub_stats.size() == ctx_info_.size());
        for (size_t idx = 0; idx < sub_stats.size(); ++idx) {
            const auto stat_si = sub_stats[idx];
            auto & ctx_info = ctx_info_[idx];
            ctx_info.val_ = stat_si->getValue();
        }
        return;
    }

    ctx_info_.reserve(sub_stats.size());
    for (size_t idx = 0; idx < sub_stats.size(); ++idx) {
        const auto stat_si = sub_stats[idx];

        ContextCounterInfo info;
        info.name_ = getNameFromSILocation(stat_si);
        info.desc_ = stat_si->getDesc(false);
        info.vis_ = stat_si->getVisibility();
        info.val_ = stat_si->getValue();
        info.ctx_addr_ = stat_si;
        ctx_info_.push_back(info);
    }
}

//! Mimics __groupedPrinting() free function in sparta/ContextCounter.h
bool DatabaseContextCounter::groupedPrinting_(
    std::set<const void*> & dont_print_these,
    void * grouped_json_, void * doc_,
    const std::vector<ContextCounterInfo> & ctx_info,
    const std::string & aggregate_desc,
    const InstrumentationNode::visibility_t aggregate_vis) const
{
    rapidjson::Value & grouped_json = *reinterpret_cast<rapidjson::Value*>(grouped_json_);
    rapidjson::Document & doc = *reinterpret_cast<rapidjson::Document*>(doc_);

    if (ctx_info.empty()) {
        return false;
    }

    rapidjson::Value ordered_keys;
    ordered_keys.SetArray();
    grouped_json.SetObject();

    double aggregate = 0;
    for (const auto & counter : ctx_info) {
        rapidjson::Value counter_info;
        counter_info.SetObject();

        counter_info.AddMember("desc", rapidjson::StringRef(counter.desc_.c_str()),
                               doc.GetAllocator());
        counter_info.AddMember("vis", rapidjson::Value(counter.vis_),
                               doc.GetAllocator());

        const std::string & counter_name = counter.name_;
        const double dbl_value = counter.val_;

        if (std::isnan(dbl_value)) {
            counter_info.AddMember("val", rapidjson::Value("nan"), doc.GetAllocator());
        } else if(std::isinf(dbl_value)) {
            counter_info.AddMember("val", rapidjson::Value("inf"), doc.GetAllocator());
        } else {
            double dbl_formatted = 0;
            std::stringstream ss;
            ss << Report::formatNumber(dbl_value);
            ss >> dbl_formatted;

            double int_part = 0;
            const double remainder = std::modf(dbl_formatted, &int_part);
            if (remainder == 0) {
                //This double has no remainder, so pretty print it as an integer
                counter_info.AddMember("val", rapidjson::Value(static_cast<uint64_t>(dbl_formatted)),
                                       doc.GetAllocator());
            } else {
                //This double has some remainder, so pretty print it as-is
                counter_info.AddMember("val", rapidjson::Value(dbl_formatted), doc.GetAllocator());
            }
        }

        aggregate += counter.val_;
        dont_print_these.insert(counter.ctx_addr_);
        grouped_json.AddMember(rapidjson::StringRef(counter_name.c_str()), counter_info,
                               doc.GetAllocator());
        ordered_keys.PushBack(rapidjson::StringRef(counter_name.c_str()),
                              doc.GetAllocator());
    }

    rapidjson::Value aggregate_info;
    aggregate_info.SetObject();
    aggregate_info.AddMember("desc", rapidjson::StringRef(aggregate_desc.c_str()),
                             doc.GetAllocator());
    aggregate_info.AddMember("vis", rapidjson::Value(aggregate_vis),
                             doc.GetAllocator());

    const double dbl_aggregate = static_cast<double>(aggregate);
    if (std::isnan(dbl_aggregate)) {
        aggregate_info.AddMember("val", rapidjson::Value("nan"), doc.GetAllocator());
    } else if (std::isinf(dbl_aggregate)) {
        aggregate_info.AddMember("val", rapidjson::Value("inf"), doc.GetAllocator());
    } else {
        double dbl_formatted = 0;
        std::stringstream ss;
        ss << Report::formatNumber(dbl_aggregate);
        ss >> dbl_formatted;

        double int_part = 0;
        const double remainder = std::modf(dbl_formatted, &int_part);
        if (remainder == 0) {
            //This double has no remainder, so pretty print it as an integer
            aggregate_info.AddMember("val", rapidjson::Value(static_cast<uint64_t>(dbl_formatted)),
                                   doc.GetAllocator());
        } else {
            //This double has some remainder, so pretty print it as-is
            aggregate_info.AddMember("val", rapidjson::Value(dbl_formatted), doc.GetAllocator());
        }
    }

    ordered_keys.PushBack("agg", doc.GetAllocator());
    grouped_json.AddMember("agg", aggregate_info, doc.GetAllocator());
    grouped_json.AddMember("ordered_keys", ordered_keys, doc.GetAllocator());

    return true;
}

//! Mimics __groupedPrintingReduced() free function in sparta/ContextCounter.h
bool DatabaseContextCounter::groupedPrintingReduced_(
    std::set<const void*> & dont_print_these,
    void * grouped_json_, void * doc_,
    const std::vector<ContextCounterInfo> & ctx_info) const
{
    rapidjson::Value & grouped_json = *reinterpret_cast<rapidjson::Value*>(grouped_json_);
    rapidjson::Document & doc = *reinterpret_cast<rapidjson::Document*>(doc_);

    if (ctx_info.empty()) {
        return false;
    }

    grouped_json.SetObject();

    double aggregate = 0;
    for (const auto & counter : ctx_info) {
        const std::string & counter_name = counter.name_;
        const double dbl_value = counter.val_;

        if (std::isnan(dbl_value)) {
            grouped_json.AddMember(rapidjson::StringRef(counter_name.c_str()),
                                   rapidjson::Value("nan"),
                                   doc.GetAllocator());
        } else if(std::isinf(dbl_value)) {
            grouped_json.AddMember(rapidjson::StringRef(counter_name.c_str()),
                                   rapidjson::Value("inf"),
                                   doc.GetAllocator());
        } else {
            double dbl_formatted = 0;
            std::stringstream ss;
            ss << Report::formatNumber(dbl_value);
            ss >> dbl_formatted;

            double int_part = 0;
            const double remainder = std::modf(dbl_formatted, &int_part);
            if (remainder == 0) {
                //This double has no remainder, so pretty print it as an integer
                grouped_json.AddMember(rapidjson::StringRef(counter_name.c_str()),
                                       rapidjson::Value(static_cast<uint64_t>(dbl_formatted)),
                                       doc.GetAllocator());
            } else {
                //This double has some remainder, so pretty print it as-is
                grouped_json.AddMember(rapidjson::StringRef(counter_name.c_str()),
                                       rapidjson::Value(dbl_formatted),
                                       doc.GetAllocator());
            }
        }

        aggregate += counter.val_;
        dont_print_these.insert(counter.ctx_addr_);
    }
    grouped_json.AddMember("agg", rapidjson::Value(aggregate), doc.GetAllocator());

    return true;
}

//! Mimics __groupedPrintingDetail() free function in sparta/ContextCounter.h
bool DatabaseContextCounter::groupedPrintingDetail_(
    std::set<const void*> & dont_print_these,
    void * grouped_json_, void * doc_,
    const std::vector<ContextCounterInfo> & ctx_info) const
{
    (void) grouped_json_;
    (void) doc_;
    for (const auto & info : ctx_info) {
        dont_print_these.insert(info.ctx_addr_);
    }
    return true;
}

//! At the end of the various "grouped printing" methods, tack on
//! any "unprintable SI(s)" into the "dont_print_these" sets.
void DatabaseContextCounter::appendUnprintablesToSet_(
    std::set<const void*> & dont_print_these) const
{
    for (const auto unprintable : *unprintable_sis_) {
        dont_print_these.insert(unprintable);
    }
}

} // namespace db
} // namespace sparta
