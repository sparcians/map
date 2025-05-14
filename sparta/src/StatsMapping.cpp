
#include "sparta/report/format/StatsMapping.hpp"

#include <rapidjson/encodings.h>
#include <list>
#include <ostream>
#include <unordered_map>
#include <utility>
#include <vector>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include "sparta/report/Report.hpp"
#include "sparta/statistics/StatisticInstance.hpp"

namespace sparta {
namespace report {
namespace format {

void createStatsMappingForReport(const Report * r,
                                 const std::string & prefix,
                                 std::map<std::string, std::string> & mapping)
{
    for (const auto & si : r->getStatistics()) {
        if (!si.first.empty()) {
            mapping[prefix + si.first] = si.second->getLocation();
        } else {
            mapping[prefix + si.second->getLocation()] = si.second->getLocation();
        }
    }

    for (const Report & sr : r->getSubreports()) {
        createStatsMappingForReport(&sr, sr.getName() + ".", mapping);
    }
}

void StatsMapping::writeContentToStream_(std::ostream & out) const
{
    std::map<std::string, std::string> mapping;
    createStatsMappingForReport(report_, "", mapping);

    rapidjson::Document doc;
    doc.SetObject();

    rapidjson::Value headers2stats;
    headers2stats.SetObject();

    rapidjson::Value stats2headers;
    stats2headers.SetObject();

    std::map<std::string, std::string> reverse_mapping;
    for (const auto & m : mapping) {
        const std::string & header = m.first;
        const std::string & stat_loc = m.second;

        headers2stats.AddMember(rapidjson::StringRef(header.c_str()),
                                rapidjson::StringRef(stat_loc.c_str()),
                                doc.GetAllocator());

        reverse_mapping[stat_loc] = header;
    }

    for (const auto & m : reverse_mapping) {
        const std::string & stat_loc = m.first;
        const std::string & header = m.second;

        stats2headers.AddMember(rapidjson::StringRef(stat_loc.c_str()),
                                rapidjson::StringRef(header.c_str()),
                                doc.GetAllocator());
    }

    doc.AddMember("Column-header-to-statistic", headers2stats, doc.GetAllocator());
    doc.AddMember("Statistic-to-column-header", stats2headers, doc.GetAllocator());

    // For consistency with the other JSON formats:
    //
    // "report_metadata": {
    //     "report_format": "stats_mapping"
    // }
    rapidjson::Value report_metadata_json;
    report_metadata_json.SetObject();
    report_metadata_json.AddMember("report_format",
                                   rapidjson::StringRef("stats_mapping"),
                                   doc.GetAllocator());

    doc.AddMember("report_metadata", report_metadata_json, doc.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    out << buffer.GetString();
}

} // namespace format
} // namespace report
} // namespace sparta
