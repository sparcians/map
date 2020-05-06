// <ArchiveSink> -*- C++ -*-

#pragma once

#include "sparta/statistics/dispatch/archives/ArchiveStream.hpp"

#include <memory>
#include <vector>

namespace sparta {
namespace statistics {

class RootArchiveNode;

/*!
 * \brief Generic statistic sink base class for report archives
 */
class ArchiveSink : public ArchiveStream
{
public:
    void setRoot(const std::shared_ptr<RootArchiveNode> & root) {
        root_ = root;
    }

    virtual void copyMetadataFrom(const ArchiveStream * stream) = 0;

    virtual void sendToSink(const std::vector<double> & values) = 0;

    virtual void flush() = 0;

protected:
    RootArchiveNode * getRoot_() {
        return root_.get();
    }

    const RootArchiveNode * getRoot_() const {
        return root_.get();
    }

private:
    std::shared_ptr<RootArchiveNode> root_;
};

} // namespace statistics
} // namespace sparta

