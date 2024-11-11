// <AppTriggers.hpp> -*- C++ -*-


/*!
 * \file AppTriggers.hpp
 * \brief Application-infrastructure triggers.
 */

#pragma once


#include "sparta/sparta.hpp"
#include "sparta/app/Simulation.hpp"
#include "sparta/app/SimulationConfiguration.hpp"
#include "sparta/trigger/Trigger.hpp"
#include "sparta/trigger/Triggerable.hpp"
#include "sparta/pevents/PeventTrigger.hpp"
#include "sparta/pevents/PeventController.hpp"
#include "sparta/log/Tap.hpp"
#include "sparta/collection/PipelineCollector.hpp"

namespace sparta {
    namespace app {

/*!
 * \class PipelineTrigger
 * \brief Trigger used to enable/disable Pipeline collection
 */
class PipelineTrigger : public trigger::Triggerable
{
public:
    PipelineTrigger(const std::string& simdb_filename,
                      const size_t heartbeat,
                      sparta::RootTreeNode * rtn,
                      const sparta::CounterBase * insts_retired_counter)
    {
        pipeline_collector_.reset(new sparta::collection::PipelineCollector(simdb_filename, heartbeat, rtn, insts_retired_counter));
    }

    void go() override
    {
        sparta_assert(!triggered_, "Why has pipeline trigger been triggered?");
        triggered_ = true;
        pipeline_collector_->startCollecting();
    }

    void stop() override
    {
        sparta_assert(triggered_, "Why stop an inactivated trigger?");
        triggered_ = false;
        pipeline_collector_->stopCollecting();
    }

private:
    std::unique_ptr<collection::PipelineCollector> pipeline_collector_;
};

/*!
 * \brief Trigger for strating logging given a number of tap descriptors
 * \note Attaches all taps on go, reports warning on stop
 */
class LoggingTrigger : public trigger::Triggerable
{
    Simulation& sim_;
    log::TapDescVec taps_;

public:

    LoggingTrigger(Simulation& sim,
                   const log::TapDescVec& taps) :
        Triggerable(),
        sim_(sim),
        taps_(taps)
    {;}

public:

    virtual void go() override {
        sim_.installTaps(taps_);
    }
    virtual void stop() override {
        std::cerr << "Warning: no support for STOPPING a LoggingTrigger" << std::endl;
    }
};



    } // namespace app
} // namespace sparta
