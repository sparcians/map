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
#include "sparta/collection/PipelineCollector.hpp"
#include "sparta/log/Tap.hpp"

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
                    sparta::RootTreeNode * rtn,
                    const size_t heartbeat = 10,
                    const std::set<std::string>& enabled_nodes = {})
        : rtn_(rtn)
        , enabled_nodes_(enabled_nodes)
    {
        pipeline_collector_.reset(new sparta::collection::PipelineCollector(simdb_filename, heartbeat));
    }

    void go() override
    {
        sparta_assert(!triggered_, "Why has pipeline trigger been triggered?");
        triggered_ = true;

        std::cout << "Pipeline collection started, output to database file '"
                  << pipeline_collector_->getFilePath() << "'" << std::endl;

        startCollection_();
    }

    void stop() override
    {
        sparta_assert(triggered_, "Why stop an inactivated trigger?");
        triggered_ = false;
        stopCollection_();
    }

private:
    void startCollection_()
    {
        if (enabled_nodes_.empty()) {
            pipeline_collector_->enableCollection(rtn_);
        } else {
            for (const auto & node_name : enabled_nodes_) {
                std::vector<TreeNode*> results;
                rtn_->getSearchScope()->findChildren(node_name, results);
                if (results.empty()) {
                    std::cerr << SPARTA_CURRENT_COLOR_RED
                              << "WARNING (Pipeline collection): Could not find node named: '"
                              << node_name
                              <<"' Collection will not occur on that node!"
                              << SPARTA_CURRENT_COLOR_NORMAL
                              << std::endl;
                }
                for (auto & tn : results) {
                    std::cout << "Collection enabled on node: '" << tn->getLocation() << "'" << std::endl;
                    pipeline_collector_->enableCollection(tn);
                }
            }
        }

        pipeline_collector_->finalizeCollector(rtn_);
    }

    void stopCollection_()
    {
        pipeline_collector_->disableCollection();
    }

    std::unique_ptr<collection::PipelineCollector> pipeline_collector_;
    sparta::RootTreeNode * rtn_;
    std::set<std::string> enabled_nodes_;
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
