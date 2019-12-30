
#include "sparta/statistics/ContextCounter.hpp"

#include <cmath>
#include <istream>

#include "sparta/report/Report.hpp"
#include "rapidjson/document.h"

bool __groupedPrinting(
    std::set<const void*> & dont_print_these,
    void * grouped_json_, void * doc_,
    const std::vector<ContextCounterInfo> & ctx_info,
    const std::string & aggregate_desc,
    const sparta::InstrumentationNode::visibility_t aggregate_vis)
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

        if (isnan(dbl_value)) {
            counter_info.AddMember("val", rapidjson::Value("nan"), doc.GetAllocator());
        } else if(isinf(dbl_value)) {
            counter_info.AddMember("val", rapidjson::Value("inf"), doc.GetAllocator());
        } else {
            double dbl_formatted = 0;
            std::stringstream ss;
            ss << sparta::Report::formatNumber(dbl_value);
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
    if (isnan(dbl_aggregate)) {
        aggregate_info.AddMember("val", rapidjson::Value("nan"), doc.GetAllocator());
    } else if (isinf(dbl_aggregate)) {
        aggregate_info.AddMember("val", rapidjson::Value("inf"), doc.GetAllocator());
    } else {
        double dbl_formatted = 0;
        std::stringstream ss;
        ss << sparta::Report::formatNumber(dbl_aggregate);
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

bool __groupedPrintingReduced(
    std::set<const void*> & dont_print_these,
    void * grouped_json_, void * doc_,
    const std::vector<ContextCounterInfo> & ctx_info)
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

        if (isnan(dbl_value)) {
            grouped_json.AddMember(rapidjson::StringRef(counter_name.c_str()),
                                   rapidjson::Value("nan"),
                                   doc.GetAllocator());
        } else if(isinf(dbl_value)) {
            grouped_json.AddMember(rapidjson::StringRef(counter_name.c_str()),
                                   rapidjson::Value("inf"),
                                   doc.GetAllocator());
        } else {
            double dbl_formatted = 0;
            std::stringstream ss;
            ss << sparta::Report::formatNumber(dbl_value);
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

bool __groupedPrintingDetail(
    std::set<const void*> & dont_print_these,
    void * grouped_json_, void * doc_,
    const std::vector<ContextCounterInfo> & ctx_info)
{
    (void) grouped_json_;
    (void) doc_;
    for (const auto & info : ctx_info) {
        dont_print_these.insert(info.ctx_addr_);
    }
    return true;
}
