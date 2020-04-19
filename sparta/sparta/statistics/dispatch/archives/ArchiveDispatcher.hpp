// <ArchiveDispatcher> -*- C++ -*-

#pragma once

#include "sparta/statistics/dispatch/archives/ArchiveSource.hpp"
#include "sparta/statistics/dispatch/archives/ArchiveSink.hpp"

namespace sparta {
namespace statistics {

/*!
 * \brief This class holds exactly one generic ArchiveSource, and
 * any number of generic ArchiveSink's.
 */
class ArchiveDispatcher
{
public:
    virtual ~ArchiveDispatcher() {}

    void setStatisticsSource(std::unique_ptr<ArchiveSource> source) {
        source_ = std::move(source);
    }

    void addStatisticsSink(std::unique_ptr<ArchiveSink> sink) {
        sinks_.emplace_back(sink.release());
    }

    const std::vector<std::unique_ptr<ArchiveSink>> & getSinks() const {
        return sinks_;
    }

    //Take a reading from the (one, and only one) data source, and
    //send those data values out to all of the registered sinks
    void dispatch() {
        const std::vector<double> & values = source_->readFromSource();
        for (auto & sink : sinks_) {
            sink->sendToSink(values);
        }
    }

    //Force all sinks to flush their data. Sinks may use internal
    //data buffers, asynchronous operations, etc. to boost performance
    //of their own sink implementation. Force a synchronous flush with
    //a call to this method.
    void flush() {
        for (auto & sink : sinks_) {
            sink->flush();
        }
    }

protected:
    static const std::string & getSimulationTimeStamp_() {
        return simulation_time_stamp_;
    }

private:
    std::unique_ptr<ArchiveSource> source_;
    std::vector<std::unique_ptr<ArchiveSink>> sinks_;
    static std::string simulation_time_stamp_;
};

} // namespace statistics
} // namespace sparta

