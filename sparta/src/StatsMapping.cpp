
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
                                 std::unordered_map<std::string, std::string> & mapping)
{
    for (const auto & si : r->getStatistics()) {
        if (!si.first.empty()) {
            mapping[prefix + si.first] = si.second.getLocation();
        } else {
            mapping[prefix + si.second.getLocation()] = si.second.getLocation();
        }
    }

    for (const Report & sr : r->getSubreports()) {
        createStatsMappingForReport(&sr, sr.getName() + ".", mapping);
    }
}

bool StatsMapping::supportsUpdate() const
{
    return false;
}

void StatsMapping::writeHeaderToStream_(std::ostream & out) const
{
    std::unordered_map<std::string, std::string> mapping;
    createStatsMappingForReport(report_, "", mapping);

    rapidjson::Document doc;
    doc.SetObject();

    rapidjson::Value headers2stats;
    headers2stats.SetObject();

    rapidjson::Value stats2headers;
    stats2headers.SetObject();

    for (const auto & m : mapping) {
        const std::string & header = m.first;
        const std::string & stat_loc = m.second;

        headers2stats.AddMember(rapidjson::StringRef(header.c_str()),
                                rapidjson::StringRef(stat_loc.c_str()),
                                doc.GetAllocator());

        stats2headers.AddMember(rapidjson::StringRef(stat_loc.c_str()),
                                rapidjson::StringRef(header.c_str()),
                                doc.GetAllocator());
    }

    doc.AddMember("Column-header-to-statistic", headers2stats, doc.GetAllocator());
    doc.AddMember("Statistic-to-column-header", stats2headers, doc.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    out << buffer.GetString();
}

void StatsMapping::writeContentToStream_(std::ostream &) const
{
}

void StatsMapping::updateToStream_(std::ostream &) const
{
}

} // namespace format
} // namespace report
} // namespace sparta
