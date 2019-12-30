// <StatsMapping> -*- C++ -*-

#ifndef __SPARTA_REPORT_FORMAT_STATS_MAPPING_H__
#define __SPARTA_REPORT_FORMAT_STATS_MAPPING_H__

#include <iosfwd>
#include <string>

#include "sparta/report/format/BaseOstreamFormatter.hpp"

namespace sparta {
class Report;

namespace report {
namespace format {

/*!
 * \brief Statistics mapping output formatter. Produces a
 * JSON dictionary which maps CSV column headers to
 * StatisticInstance names.
 */
class StatsMapping : public BaseOstreamFormatter
{
public:
    StatsMapping(const Report * r, std::ostream & output) :
        BaseOstreamFormatter(r, output)
    {}

    StatsMapping(const Report * r, const std::string & filename) :
        BaseOstreamFormatter(r, filename, std::ios::out)
    {}

    StatsMapping(const Report * r) :
        BaseOstreamFormatter(r)
    {}

private:
    bool supportsUpdate() const override final;
    void writeHeaderToStream_(std::ostream & out) const override final;
    void writeContentToStream_(std::ostream & out) const override final;
    void updateToStream_(std::ostream &) const override final;
};

//! \brief StatsMapping stream operator
inline std::ostream & operator<<(std::ostream & out, StatsMapping & f) {
    out << &f;
    return out;
}

} // namespace format
} // namespace report
} // namespace sparta

#endif
