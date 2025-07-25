// <StatsMapping> -*- C++ -*-

#pragma once

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
    void writeHeaderToStream_(std::ostream & out) const override final { (void)out; }
    void writeContentToStream_(std::ostream & out) const override final;
};

//! \brief StatsMapping stream operator
inline std::ostream & operator<<(std::ostream & out, StatsMapping & f) {
    out << &f;
    return out;
}

} // namespace format
} // namespace report
} // namespace sparta

