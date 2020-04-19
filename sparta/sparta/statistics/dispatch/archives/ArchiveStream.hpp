// <ArchiveStream> -*- C++ -*-

#pragma once

#include <string>

namespace sparta {
namespace statistics {

/*!
 * \brief Generic statistic stream base class for sources and
 * sinks in the report archive system
 */
class ArchiveStream
{
public:
    virtual ~ArchiveStream() {}

    void setPath(const std::string & path) {
        path_ = path;
    }

    void setSubpath(const std::string & subpath) {
        subpath_ = subpath;
    }

    const std::string & getPath() const {
        return path_;
    }

    const std::string & getSubpath() const {
        return subpath_;
    }

    virtual void initialize() = 0;

private:
    std::string path_;
    std::string subpath_;
};

} // namespace statistics
} // namespace sparta

