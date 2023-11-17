
#include <rapidjson/encodings.h>
#include <cstdint>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/iterator/iterator_traits.hpp>
#include <cmath>
#include <istream>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <algorithm>

#include "sparta/report/format/JSON.hpp"
#include "sparta/report/format/JSON_reduced.hpp"
#include "sparta/report/db/DatabaseContextCounter.hpp"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include "sparta/statistics/InstrumentationNode.hpp"
#include "sparta/report/Report.hpp"
#include "sparta/app/SimulationInfo.hpp"
#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/statistics/StatisticInstance.hpp"

namespace sparta {
class CounterBase;
class ParameterBase;

namespace report {
namespace format {

rapidjson::Value getReportMetadata(
    const std::map<std::string, std::string> & metadata,
    rapidjson::Document & doc)
{
    rapidjson::Value metadata_json;
    metadata_json.SetObject();

    for (const auto & md : metadata) {
        rapidjson::Value metadata_name_json;
        metadata_name_json.SetString(
            md.first.c_str(),
            md.first.size(),
            doc.GetAllocator());

        rapidjson::Value metadata_value_json;
        metadata_value_json.SetString(
            md.second.c_str(),
            md.second.size(),
            doc.GetAllocator());

        metadata_json.AddMember(
            metadata_name_json,
            metadata_value_json,
            doc.GetAllocator());
    }

    return metadata_json;
}

std::string flattenReportName(std::string full_name)
{
    std::string local_name = full_name;
    std::size_t last_dot_idx = full_name.find_last_of(".");
    if (last_dot_idx != std::string::npos){
        local_name = full_name.substr(last_dot_idx+1);
    }

    return local_name;
}

void getTotalNumReports(unsigned int & total_num_reports,
                        unsigned int & total_num_stats,
                        const Report * r)
{
    ++total_num_reports;
    total_num_stats += r->getStatistics().size();
    for (const Report & sr : r->getSubreports()) {
        getTotalNumReports(total_num_reports, total_num_stats, &sr);
    }
}

//////////////////////// JSON Formatter methods (full) /////////////////////

void extractStatisticsJsonFull(rapidjson::Document & doc,
                               rapidjson::Value & report_json,
                               const Report * r,
                               std::vector<std::vector<std::string>> & ordered_keys_,
                               std::vector<std::string> & statistics_descs_,
                               std::vector<std::string> & report_local_names_)
{
    ordered_keys_.push_back({});
    std::vector<std::string> & ordered_keys_for_report = ordered_keys_.back();

    rapidjson::Value contents;
    contents.SetObject();

    // Don't write out the complete hierarchical name
    // This information is already captured in the nested structure of the dictionary
    // XXX: This might cause name collisions if max_report_depth != -1
    std::string local_name = flattenReportName(r->getName());

    // Keep track of the order in which stats and subunits are written
    std::set<const void*> dont_print_these;
    std::set<const void*> db_dont_print_these;

    const Report::SubStaticticInstances & sub_stats = r->getSubStatistics();
    const Report::DBSubStatisticInstances & db_sub_stats = r->getDBSubStatistics();

    for (const statistics::stat_pair_t & si : r->getStatistics())
    {
        const std::string stat_name = !si.first.empty() ? si.first : si.second->getLocation();
        if (!stat_name.empty()) {
            const StatisticInstance * stat_inst = si.second.get();
            const StatisticDef * def = stat_inst->getStatisticDef();
            const CounterBase * ctr = stat_inst->getCounter();
            const ParameterBase * prm = stat_inst->getParameter();

            auto sub_stat_iter = sub_stats.find(def);
            const bool valid_stat_def = (def != nullptr);
            const bool has_valid_sub_stats =
                (valid_stat_def && sub_stat_iter != sub_stats.end());

            auto db_sub_stat_iter = db_sub_stats.find(stat_inst);
            const bool has_valid_db_sub_stats = (db_sub_stat_iter != db_sub_stats.end());

            rapidjson::Value grouped_json;
            if (has_valid_sub_stats && def->groupedPrinting(sub_stat_iter->second,
                                                            dont_print_these,
                                                            &grouped_json, &doc)) {
                const std::string & name = def->getName();
                contents.AddMember(rapidjson::StringRef(name.c_str()),
                                   grouped_json,
                                   doc.GetAllocator());
                continue;
            }
            if (dont_print_these.count(ctr) > 0 || dont_print_these.count(prm) > 0) {
                continue;
            }
            dont_print_these.clear();

            if (has_valid_db_sub_stats) {
                const std::shared_ptr<db::DatabaseContextCounter> & db_ctx_ctr =
                    db_sub_stat_iter->second.first;

                const std::vector<const StatisticInstance*> & db_sub_sis =
                    db_sub_stat_iter->second.second;

                if (db_ctx_ctr->groupedPrinting(db_sub_sis,
                                                db_dont_print_these,
                                                &grouped_json, &doc))
                {
                    const std::string & name = db_ctx_ctr->getName();
                    contents.AddMember(rapidjson::StringRef(name.c_str()),
                                       grouped_json,
                                       doc.GetAllocator());
                    continue;
                }
            }
            if (db_dont_print_these.count(stat_inst) > 0) {
                continue;
            }
            db_dont_print_these.clear();

            rapidjson::Value stats_json;
            stats_json.SetObject();

            std::string desc = si.second->getDesc(false);
            boost::replace_all(desc, "\"", "\\\"");
            statistics_descs_.emplace_back(desc);
            const std::string & desc_ref = statistics_descs_.back();

            stats_json.AddMember("desc", rapidjson::StringRef(desc_ref.c_str()),
                                 doc.GetAllocator());
            stats_json.AddMember("vis", rapidjson::Value(si.second->getVisibility()),
                                 doc.GetAllocator());

            const double val = si.second->getValue();
            if (isnan(val)) {
                stats_json.AddMember("val", rapidjson::Value("nan"), doc.GetAllocator());
            } else if (isinf(val)) {
                stats_json.AddMember("val", rapidjson::Value("inf"), doc.GetAllocator());
            } else {
                double dbl_formatted = 0;
                std::stringstream ss;
                ss << Report::formatNumber(val);
                ss >> dbl_formatted;

                double int_part = 0;
                const double remainder = std::modf(dbl_formatted, &int_part);
                if (remainder == 0) {
                    //This double has no remainder, so print it as an integer
                    stats_json.AddMember("val", rapidjson::Value(static_cast<uint64_t>(dbl_formatted)),
                                         doc.GetAllocator());
                } else {
                    //This double has some remainder, so print it as-is
                    stats_json.AddMember("val", rapidjson::Value(dbl_formatted),
                                         doc.GetAllocator());
                }
            }
            rapidjson::Value stat_name_json;
            stat_name_json.SetString(stat_name.c_str(), stat_name.size(), doc.GetAllocator());
            contents.AddMember(stat_name_json,
                               stats_json, doc.GetAllocator());
            ordered_keys_for_report.emplace_back(stat_name);
        }
    }

    if (!ordered_keys_for_report.empty()) {
        rapidjson::Value keys_array;
        keys_array.SetArray();
        for (const auto & key : ordered_keys_for_report) {
            keys_array.PushBack(rapidjson::StringRef(key.c_str()), doc.GetAllocator());
        }
        contents.AddMember("ordered_keys", keys_array, doc.GetAllocator());
    }

    report_local_names_.emplace_back(local_name);
    const std::string & local_ref = report_local_names_.back();

    for (const Report & sr : r->getSubreports()) {
        extractStatisticsJsonFull(doc, contents, &sr, ordered_keys_,
                                  statistics_descs_, report_local_names_);
    }
    report_json.AddMember(rapidjson::StringRef(local_ref.c_str()),
                          contents, doc.GetAllocator());
}

void extractVisibilitiesJsonFull(rapidjson::Document & doc)
{
    rapidjson::Value vis_json;
    vis_json.SetObject();

    auto hidden   = rapidjson::Value( static_cast<int>(sparta::InstrumentationNode::VIS_HIDDEN   ));
    auto support  = rapidjson::Value( static_cast<int>(sparta::InstrumentationNode::VIS_SUPPORT  ));
    auto detail   = rapidjson::Value( static_cast<int>(sparta::InstrumentationNode::VIS_DETAIL   ));
    auto normal   = rapidjson::Value( static_cast<int>(sparta::InstrumentationNode::VIS_NORMAL   ));
    auto summary  = rapidjson::Value( static_cast<int>(sparta::InstrumentationNode::VIS_SUMMARY  ));
    auto critical = rapidjson::Value( static_cast<int>(sparta::InstrumentationNode::VIS_CRITICAL ));

    vis_json.AddMember("hidden",   hidden,   doc.GetAllocator());
    vis_json.AddMember("support",  support,  doc.GetAllocator());
    vis_json.AddMember("detail",   detail,   doc.GetAllocator());
    vis_json.AddMember("normal",   normal,   doc.GetAllocator());
    vis_json.AddMember("summary",  summary,  doc.GetAllocator());
    vis_json.AddMember("critical", critical, doc.GetAllocator());

    doc.AddMember("vis", vis_json, doc.GetAllocator());
}

void extractSimInfoJsonFull(rapidjson::Document & doc,
                            rapidjson::Value & siminfo_json,
                            const std::string & version,
                            std::vector<std::string> & local_strings)
{
    local_strings.reserve(5);
    local_strings.emplace_back(SimulationInfo::getInstance().sim_name);
    local_strings.emplace_back(SimulationInfo::getInstance().simulator_version);
    local_strings.emplace_back(SimulationInfo::getInstance().getSpartaVersion());
    local_strings.emplace_back(version);
    local_strings.emplace_back(SimulationInfo::getInstance().reproduction_info);

    auto sim_name = rapidjson::StringRef(local_strings[0].c_str());
    auto sim_ver  = rapidjson::StringRef(local_strings[1].c_str());
    auto sparta_ver = rapidjson::StringRef(local_strings[2].c_str());
    auto ver      = rapidjson::StringRef(local_strings[3].c_str());
    auto repro    = rapidjson::StringRef(local_strings[4].c_str());

    siminfo_json.AddMember("name", sim_name, doc.GetAllocator());
    siminfo_json.AddMember("sim_version", sim_ver, doc.GetAllocator());
    siminfo_json.AddMember("sparta_version", sparta_ver, doc.GetAllocator());
    siminfo_json.AddMember("json_report_version", ver, doc.GetAllocator());
    siminfo_json.AddMember("reproduction", repro, doc.GetAllocator());
}

void JSON::writeContentToStream_(std::ostream & out) const
{
    unsigned int total_num_reports = 0;
    unsigned int total_num_stats = 0;
    getTotalNumReports(total_num_reports, total_num_stats, report_);
    report_local_names_.reserve(total_num_reports);
    statistics_descs_.reserve(total_num_stats);
    ordered_keys_.reserve(total_num_reports);

    rapidjson::Document doc;
    doc.SetObject();

    rapidjson::Value stats_json;
    stats_json.SetObject();
    extractStatisticsJsonFull(doc, stats_json, report_, ordered_keys_,
                              statistics_descs_, report_local_names_);
    doc.AddMember("Statistics", stats_json, doc.GetAllocator());

    extractVisibilitiesJsonFull(doc);

    rapidjson::Value siminfo_json;
    siminfo_json.SetObject();
    std::vector<std::string> local_strings;
    extractSimInfoJsonFull(doc, siminfo_json, getVersion(), local_strings);
    doc.AddMember("siminfo", siminfo_json, doc.GetAllocator());

    rapidjson::Value report_metadata_json = getReportMetadata(metadata_kv_pairs_, doc);
    doc.AddMember("report_metadata", report_metadata_json, doc.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    if (prettyPrintEnabled()) {
        out << buffer.GetString();
    } else {
        std::stringstream no_pretty_print_ss;
        no_pretty_print_ss << buffer.GetString();

        std::string line;
        while (std::getline(no_pretty_print_ss, line)) {
            boost::trim_left(line);
            out << line << "\n";
        }
    }
}

//////////////////////// JSON Formatter methods (reduced) //////////////////

void extractVisibilitiesJsonReduced(rapidjson::Document & doc)
{
    rapidjson::Value vis_json;
    vis_json.SetObject();

    auto hidden   = rapidjson::Value( static_cast<int>(sparta::InstrumentationNode::VIS_HIDDEN   ));
    auto support  = rapidjson::Value( static_cast<int>(sparta::InstrumentationNode::VIS_SUPPORT  ));
    auto detail   = rapidjson::Value( static_cast<int>(sparta::InstrumentationNode::VIS_DETAIL   ));
    auto normal   = rapidjson::Value( static_cast<int>(sparta::InstrumentationNode::VIS_NORMAL   ));
    auto summary  = rapidjson::Value( static_cast<int>(sparta::InstrumentationNode::VIS_SUMMARY  ));
    auto critical = rapidjson::Value( static_cast<int>(sparta::InstrumentationNode::VIS_CRITICAL ));

    vis_json.AddMember("hidden",   hidden,   doc.GetAllocator());
    vis_json.AddMember("support",  support,  doc.GetAllocator());
    vis_json.AddMember("detail",   detail,   doc.GetAllocator());
    vis_json.AddMember("normal",   normal,   doc.GetAllocator());
    vis_json.AddMember("summary",  summary,  doc.GetAllocator());
    vis_json.AddMember("critical", critical, doc.GetAllocator());

    doc.AddMember("vis", vis_json, doc.GetAllocator());
}

void extractSimInfoJsonReduced(rapidjson::Document & doc,
                               rapidjson::Value & siminfo_json,
                               const std::string & version,
                               std::vector<std::string> & local_strings)
{
    local_strings.reserve(5);
    local_strings.emplace_back(SimulationInfo::getInstance().sim_name);
    local_strings.emplace_back(SimulationInfo::getInstance().simulator_version);
    local_strings.emplace_back(SimulationInfo::getInstance().getSpartaVersion());
    local_strings.emplace_back(version);
    local_strings.emplace_back(SimulationInfo::getInstance().reproduction_info);

    auto sim_name = rapidjson::StringRef(local_strings[0].c_str());
    auto sim_ver  = rapidjson::StringRef(local_strings[1].c_str());
    auto sparta_ver = rapidjson::StringRef(local_strings[2].c_str());
    auto ver      = rapidjson::StringRef(local_strings[3].c_str());
    auto repro    = rapidjson::StringRef(local_strings[4].c_str());

    siminfo_json.AddMember("name", sim_name, doc.GetAllocator());
    siminfo_json.AddMember("sim_version", sim_ver, doc.GetAllocator());
    siminfo_json.AddMember("sparta_version", sparta_ver, doc.GetAllocator());
    siminfo_json.AddMember("json_report_version", ver, doc.GetAllocator());
    siminfo_json.AddMember("reproduction", repro, doc.GetAllocator());
}

void extractStatisticsJsonReduced(rapidjson::Document & doc,
                                  rapidjson::Value & report_json,
                                  const Report * r,
                                  std::vector<std::string> & report_local_names_,
                                  const bool omit_zero_values)
{
    rapidjson::Value contents;
    contents.SetObject();

    // Don't write out the complete hierarchical name
    // This information is already captured in the nested structure of the dictionary
    // XXX: This might cause name collisions if max_report_depth != -1
    std::string local_name = flattenReportName(r->getName());

    const Report::SubStaticticInstances & sub_stats = r->getSubStatistics();
    const Report::DBSubStatisticInstances & db_sub_stats = r->getDBSubStatistics();

    // Keep track of the order in which stats and subunits are written
    std::set<const void*> dont_print_these;
    std::set<const void*> db_dont_print_these;

    for (const statistics::stat_pair_t & si : r->getStatistics()) {
        const std::string stat_name = !si.first.empty() ? si.first : si.second->getLocation();
        if (!stat_name.empty()) {
            const StatisticInstance * stat_inst = si.second.get();
            const StatisticDef * def = stat_inst->getStatisticDef();
            const CounterBase * ctr = stat_inst->getCounter();
            const ParameterBase * prm = stat_inst->getParameter();

            auto sub_stat_iter = sub_stats.find(def);
            const bool valid_stat_def = (def != nullptr);
            const bool has_valid_sub_stats =
                (valid_stat_def && sub_stat_iter != sub_stats.end());

            auto db_sub_stat_iter = db_sub_stats.find(stat_inst);
            const bool has_valid_db_sub_stats = (db_sub_stat_iter != db_sub_stats.end());

            rapidjson::Value grouped_json;
            if (has_valid_sub_stats && def->groupedPrintingReduced(sub_stat_iter->second,
                                                                   dont_print_these,
                                                                   &grouped_json, &doc)) {
                const std::string & name = def->getName();
                contents.AddMember(rapidjson::StringRef(name.c_str()),
                                   grouped_json,
                                   doc.GetAllocator());
                continue;
            }
            if (dont_print_these.count(ctr) > 0 || dont_print_these.count(prm) > 0) {
                continue;
            }
            dont_print_these.clear();

            if (has_valid_db_sub_stats) {
                const std::shared_ptr<db::DatabaseContextCounter> & db_ctx_ctr =
                    db_sub_stat_iter->second.first;

                const std::vector<const StatisticInstance*> & db_sub_sis =
                    db_sub_stat_iter->second.second;

                if (db_ctx_ctr->groupedPrintingReduced(db_sub_sis,
                                                       db_dont_print_these,
                                                       &grouped_json, &doc))
                {
                    const std::string & name = db_ctx_ctr->getName();
                    contents.AddMember(rapidjson::StringRef(name.c_str()),
                                       grouped_json,
                                       doc.GetAllocator());
                    continue;
                }
            }
            if (db_dont_print_these.count(stat_inst) > 0) {
                continue;
            }
            db_dont_print_these.clear();

            const double val = si.second->getValue();
            if (omit_zero_values && val == 0) {
                continue;
            }
            rapidjson::Value stat_name_json;
            stat_name_json.SetString(stat_name.c_str(), stat_name.size(), doc.GetAllocator());
            if (isnan(val)) {
                contents.AddMember(stat_name_json,
                                   rapidjson::Value("nan"),
                                   doc.GetAllocator());
            } else if(isinf(val)) {
                contents.AddMember(stat_name_json,
                                   rapidjson::Value("inf"),
                                   doc.GetAllocator());
            } else {
                double dbl_formatted = 0;
                std::stringstream ss;
                ss << Report::formatNumber(val);
                ss >> dbl_formatted;

                double int_part = 0;
                const double remainder = std::modf(dbl_formatted, &int_part);
                if (remainder == 0) {
                    //This double has no remainder, so print it as an integer
                    contents.AddMember(stat_name_json,
                                       rapidjson::Value(static_cast<uint64_t>(dbl_formatted)),
                                       doc.GetAllocator());
                } else {
                    //This double has some remainder, so print it as-is
                    contents.AddMember(stat_name_json,
                                       rapidjson::Value(dbl_formatted),
                                       doc.GetAllocator());
                }
            }
        }
    }

    report_local_names_.emplace_back(local_name);
    const std::string & local_ref = report_local_names_.back();

    for (const Report & sr : r->getSubreports()) {
        extractStatisticsJsonReduced(doc, contents, &sr, report_local_names_,
                                     omit_zero_values);
    }
    report_json.AddMember(rapidjson::StringRef(local_ref.c_str()),
                          contents, doc.GetAllocator());
}

void JSON_reduced::writeContentToStream_(std::ostream & out) const
{
    unsigned int total_num_reports = 0;
    unsigned int unused_var = 0;
    getTotalNumReports(total_num_reports, unused_var, report_);
    report_local_names_.reserve(total_num_reports);

    rapidjson::Document doc;
    doc.SetObject();

    rapidjson::Value stats_json;
    stats_json.SetObject();
    const bool omit_zero_values = statsWithValueZeroAreOmitted();
    extractStatisticsJsonReduced(doc, stats_json, report_, report_local_names_,
                                 omit_zero_values);
    doc.AddMember("Statistics", stats_json, doc.GetAllocator());

    extractVisibilitiesJsonReduced(doc);

    rapidjson::Value siminfo_json;
    siminfo_json.SetObject();
    std::vector<std::string> local_strings;
    extractSimInfoJsonReduced(doc, siminfo_json, getVersion(), local_strings);
    doc.AddMember("siminfo", siminfo_json, doc.GetAllocator());

    rapidjson::Value report_metadata_json = getReportMetadata(metadata_kv_pairs_, doc);
    doc.AddMember("report_metadata", report_metadata_json, doc.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    if (prettyPrintEnabled()) {
        out << buffer.GetString();
    } else {
        std::stringstream no_pretty_print_ss;
        no_pretty_print_ss << buffer.GetString();

        std::string line;
        while (std::getline(no_pretty_print_ss, line)) {
            boost::trim_left(line);
            out << line << "\n";
        }
    }
}

}
}
}
