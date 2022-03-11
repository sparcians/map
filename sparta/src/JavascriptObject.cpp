
#include "sparta/report/format/JavascriptObject.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <math.h>
#include <cstdlib>
#include <boost/algorithm/string/replace.hpp>
#include <boost/iterator/iterator_traits.hpp>
#include <cmath>
#include <map>
#include <utility>

#include "sparta/simulation/Resource.hpp"
#include "sparta/statistics/InstrumentationNode.hpp"
#include "sparta/app/SimulationInfo.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/statistics/StatisticInstance.hpp"


void sparta::report::format::JavascriptObject::writeContentToStream_(std::ostream& out) const
{
    sparta_assert(report_ != 0);
    decimal_places_ = strtoul(report_->getStyle("decimal_places", "2").c_str(), nullptr, 0);

    out << "{\n";
    out << "  \"units\" : {\n";

    std::vector<std::string> all_key_names;
    writeReport_(out, *report_, all_key_names);

    out << "    \"ordered_keys\" : [    \n";
    for (uint32_t idx = 0; idx < all_key_names.size(); idx++) {
        out << "      \"" << all_key_names[idx] << "\"";
        if (idx != (all_key_names.size() - 1)) {
            out << ", ";
        }
        out << "\n";
    }
    out << "    ]\n";
    out << "  },\n";

    out << "  \"vis\" : {\n";
    out << "    \"hidden\"  : " << sparta::InstrumentationNode::VIS_HIDDEN << ",\n";
    out << "    \"support\" : "  << sparta::InstrumentationNode::VIS_SUPPORT << ",\n";
    out << "    \"detail\"  : "  << sparta::InstrumentationNode::VIS_DETAIL << ",\n";
    out << "    \"normal\"  : "  << sparta::InstrumentationNode::VIS_NORMAL << ",\n";
    out << "    \"summary\" : "  << sparta::InstrumentationNode::VIS_SUMMARY << "\n";
    out << "  },\n";

    out << "  \"siminfo\" : {\n";
    out << "    \"name\" : \"" << SimulationInfo::getInstance().sim_name << "\",\n";
    out << "    \"sim_version\" : \"" << SimulationInfo::getInstance().simulator_version << "\",\n";
    out << "    \"sparta_version\" : \"" << SimulationInfo::getInstance().getSpartaVersion() << "\",\n";
    //         out << "    \"command_line\" : \"" << SimulationInfo::getInstance().command_line << "\",\n";
    out << "    \"reproduction\" : \"" << SimulationInfo::getInstance().reproduction_info << "\"\n";
    out << "  },\n";

    out << "  \"report_metadata\": ";
    if (metadata_kv_pairs_.empty()) {
        out << "{}\n";
    } else {
        out << "{\n";
        const size_t num_metadata = metadata_kv_pairs_.size();
        size_t num_written_metadata = 0;
        for (const auto & md : metadata_kv_pairs_) {
            out << std::string(4, ' ') << "\"" << md.first << "\": \"" << md.second << "\"";
            ++num_written_metadata;
            if (num_written_metadata <= num_metadata - 1) {
                out << ",\n";
            }
        }
        out << "  }\n";
    }

    out << "}\n";
    out << std::endl;
}


// Leaf node information
std::set<std::string> sparta::report::format::JavascriptObject::leaf_nodes_;
std::set<std::string> sparta::report::format::JavascriptObject::parents_of_leaf_nodes_;

bool sparta::report::format::JavascriptObject::isLeafNode_(const Report & report) const
{
    std::string report_name = getReportName_(report);
    for (const std::string & cmp_name : leaf_nodes_) {
        if (report_name == cmp_name) {
            return true;
        }
    }

    report_name += ".";
    for (const std::string & cmp_name : parents_of_leaf_nodes_) {
        if (boost::starts_with(report_name, cmp_name) && (report_name != cmp_name)) {
            return true;
        }
    }
    return false;
}

// Get the name of the report, removing any unwanted starting characters
std::string sparta::report::format::JavascriptObject::getReportName_(const Report & report) const
{

    std::string report_name = report.getName();

    // Remove the '@ on ' string prefix in the auto-generated reports
    const std::string & remove_this_prefix = "@ on ";
    if (boost::starts_with(report_name, remove_this_prefix)) {
        report_name = report_name.substr(remove_this_prefix.length());
    }

    return report_name;
}

void sparta::report::format::JavascriptObject::writeReport_(std::ostream& out, const Report & report,
                                                          std::vector<std::string> & all_unit_names) const
{
    // Early out - no stats in this report or any sub-reports
    if (report.getRecursiveNumStatistics() == 0) {
        return;
    }

    bool merge_subreports = isLeafNode_(report);

    // If we need to start a new report
    if (merge_subreports || report.getNumStatistics() > 0) {
        std::string unit_name = getReportName_(report);
        out << "    \"" << unit_name << "\": {\n";
        all_unit_names.push_back(unit_name);

        std::vector<std::string> all_stat_names;
        writeStats_(out, report, "", all_stat_names);

        if (merge_subreports) {
            mergeReportList_(out, report.getSubreports(), getReportName_(report), all_stat_names);
        }

        out << "      \"ordered_keys\": [\n";
        for (uint32_t idx = 0; idx < all_stat_names.size(); idx++) {
            out << "        \"" << all_stat_names[idx] << "\"";
            if (idx != (all_stat_names.size() - 1)) {
                out << ", ";
            }
            out << "\n";
        }
        out << "      ]\n";

        out << "    },\n";
    }

    if (!merge_subreports) {
        writeReportList_(out, report.getSubreports(), all_unit_names);
    }
}

void sparta::report::format::JavascriptObject::mergeReport_(std::ostream& out, const Report & report,
                                                          const std::string & merge_top_name,
                                                          std::vector<std::string> & all_stat_names) const
{
    // Remove the common prefix between the top-level merge report and this
    // report name.
    const std::string & report_name = getReportName_(report);
    sparta_assert_context(report_name.length() > merge_top_name.length(),
                        "Expected the current report name (" << report_name << ") " <<
                        " to be long than the top-level report name (" << merge_top_name << ")");

    uint32_t substr_idx = 0;
    for (substr_idx = 0; substr_idx < merge_top_name.length(); substr_idx++) {
        if (merge_top_name[substr_idx] != report_name[substr_idx]) {
            break;
        }
    }

    substr_idx++;  // skip over the '.'
    sparta_assert(substr_idx < report_name.length());

    std::string stat_prefix = report_name.substr(substr_idx);
    writeStats_(out, report, stat_prefix, all_stat_names);
    if (report.getNumSubreports() > 0) {
        mergeReportList_(out, report.getSubreports(), merge_top_name, all_stat_names);
    }
}

void sparta::report::format::JavascriptObject::mergeReportList_(std::ostream& out, const std::list<Report> & reports,
                                                              const std::string & merge_top_name,
                                                              std::vector<std::string> & all_stat_names) const
{
    for (const auto & r : reports) {
        mergeReport_(out, r, merge_top_name, all_stat_names);
    }
}


void sparta::report::format::JavascriptObject::writeStats_(std::ostream& out, const Report & report,
                                                         const std::string & stat_prefix, std::vector<std::string> & all_stat_names) const
{
    const std::vector<Report::stat_pair_t>& stats = report.getStatistics();
    for (uint32_t idx = 0; idx < stats.size(); idx++) {

        const Report::stat_pair_t & si = stats[idx];

        std::string sname = stat_prefix;
        if (si.first == "") {
            // Use location as stat name since report creator did not give it an
            // explicit name. Since ths full path is used, do not append the
            // stat_prefix because it would make the resulting stat name very
            // confusing.
            sname = si.second.getLocation();
        } else {
            if (sname.length() > 0) {
                sname += ".";
            }
            sname += si.first;
        }

        all_stat_names.push_back(sname);
        out << "      \"" << sname << "\": { ";
        double val = si.second.getValue();
        out << "\"val\" : ";
        if (isnan(val)){
            out << "\"nan\"";
        } else if (isinf(val)) {
            out << "\"inf\"";
        } else{
            out << Report::formatNumber(val, false, decimal_places_);
        }

        // Escape all " characters

        std::string desc = si.second.getDesc(false);
        boost::replace_all(desc, "\"", "\\\"");

        out << ", \"vis\" : " << si.second.getVisibility();
        out << ", \"desc\" : \"" << desc << "\"},";
        out << "\n";
    }

}

void sparta::report::format::JavascriptObject::writeReportList_(std::ostream& out, const std::list<Report> & reports,
                                                              std::vector<std::string> & all_unit_names) const
{
    for (const auto & r : reports) {
        writeReport_(out, r, all_unit_names);
    }
}
