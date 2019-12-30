// <ArchiveSource> -*- C++ -*-

#ifndef __SPARTA_STATISTICS_ARCHIVE_SOURCE_H__
#define __SPARTA_STATISTICS_ARCHIVE_SOURCE_H__

#include "sparta/statistics/dispatch/archives/ArchiveStream.hpp"

#include <vector>

namespace sparta {
namespace statistics {

/*!
 * \brief Generic statistic source base class for report archives
 */
class ArchiveSource : public ArchiveStream
{
public:
    virtual const std::vector<double> & readFromSource() = 0;
};

} // namespace statistics
} // namespace sparta

#endif
