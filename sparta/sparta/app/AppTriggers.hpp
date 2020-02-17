// <AppTriggers> -*- C++ -*-


/*!
 * \file AppTriggers.hpp
 * \brief Application-infrastructure triggers.
 */

#ifndef __APP_TRIGGERS_H__
#define __APP_TRIGGERS_H__


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

class PipelineTrigger : public trigger::Triggerable
{
public:
    PipelineTrigger(const std::string& pipeline_collection_path,
                    const std::set<std::string>& pipeline_enabled_node_names,
                    uint64_t pipeline_heartbeat,
                    sparta::Clock * clk, sparta::RootTreeNode * rtn) :
        pipeline_enabled_node_names_(pipeline_enabled_node_names),
        root_(rtn)
    {
        pipeline_collector_.
            reset(new sparta::collection::PipelineCollector(pipeline_collection_path,
                                                          pipeline_heartbeat,
                                                          clk,
                                                          rtn));

    }

    void go() override
    {
        triggered_ = true;
        std::cout << "Pipeline collection started, output to files with prefix '"
                  << pipeline_collector_->getFilePath() << "'" << std::endl;

        if(pipeline_enabled_node_names_.empty()) {
            // Start collection at the root node
            pipeline_collector_->startCollection(root_);
        }
        else {
            // Find the nodes in the root and enable them
            for(const auto & node_name : pipeline_enabled_node_names_) {
                std::vector<TreeNode*> results;
                root_->getSearchScope()->findChildren(node_name, results);
                if(results.size() == 0) {
                    std::cerr << SPARTA_CURRENT_COLOR_RED
                              << "WARNING (Pipeline collection): Could not find node named: '"
                              << node_name
                              <<"' Collection will not occur on that node!"
                              << SPARTA_CURRENT_COLOR_NORMAL
                              << std::endl;
                }
                for(auto & tn : results) {
                    std::cout << "Collection enabled on node: '" << tn->getLocation() << "'" << std::endl;
                    pipeline_collector_->startCollection(tn);
                }
            }
        }
    }

    void stop() override
    {
        if(triggered_) {
            if(pipeline_enabled_node_names_.empty()) {
                // Start collection at the root node
                pipeline_collector_->stopCollection(root_);
            }
            else {
                // Find the nodes in the root and enable them
                for(const auto & node_name : pipeline_enabled_node_names_) {
                    std::vector<TreeNode*> results;
                    root_->getSearchScope()->findChildren(node_name, results);
                    for(auto & tn : results) {
                        pipeline_collector_->stopCollection(tn);
                    }
                }
            }
            pipeline_collector_->destroy();
        }
        triggered_ = false;
    }

private:

    std::unique_ptr<collection::PipelineCollector> pipeline_collector_;
    const std::set<std::string> pipeline_enabled_node_names_;
    sparta::RootTreeNode * root_;
    bool triggered_ = false;
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

#endif // #ifndef __APP_TRIGGERS_H__
