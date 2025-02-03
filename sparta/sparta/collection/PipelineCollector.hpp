// <PipelineCollector.hpp> -*- C++ -*-

/**
 * \file PipelineCollector.hpp
 *
 * \brief Class to facilitate pipeline collection operations.
 */

#pragma once

#include <set>
#include <map>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/collection/CollectableTreeNode.hpp"
#include "sparta/collection/CollectionPoints.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/events/GlobalOrderingPoint.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/simulation/TreeNodePrivateAttorney.hpp"

#include "simdb/collection/Scalars.hpp"
#include "simdb/sqlite/DatabaseManager.hpp"

namespace sparta {
namespace collection
{
    class PipelineCollector
    {
    public:
        PipelineCollector(const std::string& simdb_filename,
                          sparta::RootTreeNode * rtn,
                          const size_t heartbeat = 10,
                          const std::set<std::string>& enabled_nodes = {},
                          const sparta::CounterBase * insts_retired_counter = nullptr)
            : db_mgr_(simdb_filename, true)
            , filename_(simdb_filename)
            , root_(rtn)
            , ev_set_(nullptr)
            , insts_retired_counter_(insts_retired_counter)
        {
            // Note that we only care about the collection data and have
            // no need for any other tables, aside from the tables that the
            // DatabaseManager adds automatically to support this feature.
            simdb::Schema schema;
            db_mgr_.createDatabaseFromSchema(schema);

            std::function<uint64_t()> func_ptr = std::bind(&PipelineCollector::getCollectionTick_, this);
            db_mgr_.getCollectionMgr()->useTimestampsFrom(func_ptr);

            if (insts_retired_counter_) {
                std::function<double()> ipc_func = std::bind(&PipelineCollector::getIPC_, this);
                db_mgr_.getCollectionMgr()->enableArgosIPC(ipc_func);
            }

            db_mgr_.getCollectionMgr()->setHeartbeat(heartbeat);
            createCollections_(enabled_nodes);
        }

        ~PipelineCollector() {
            db_mgr_.closeDatabase();
        }

        //! \return the pipeout file path
        const std::string & getFilePath() const {
            return filename_;
        }

        void startCollecting() {
            // Schedule collect event in the next cycle in case
            // this is called in an unavailable phase.
            ev_collect_->schedule(sparta::Clock::Cycle(1));
        }

        void stopCollecting() {
            db_mgr_.onPipelineCollectorClosing();
            db_mgr_.getConnection()->getTaskQueue()->stopThread();
            db_mgr_.closeDatabase();
        }

    private:
        uint64_t getCollectionTick_() const {
            return scheduler_->getCurrentTick() - 1;
        }

        void createCollections_(const std::set<std::string>& enabled_nodes) {
            CollectionPoints collectables;
            recurseAddCollectables_(root_, collectables, enabled_nodes);
            collectables.createCollections(db_mgr_.getCollectionMgr());
            db_mgr_.finalizeCollections();

            recurseFindFastestCollectableClock_(root_, fastest_clk_, enabled_nodes);
            sparta_assert(fastest_clk_, "No clock found! Are there any collectables?");

            ev_collect_.reset(new sparta::UniqueEvent<SchedulingPhase::Collection>
                (&ev_set_, sparta::notNull(fastest_clk_)->getName() + "_auto_collection_event_collection",
                CREATE_SPARTA_HANDLER(PipelineCollector, performCollection_), 1));

            ev_collect_->setScheduleableClock(fastest_clk_);
            ev_collect_->setScheduler(fastest_clk_->getScheduler());
            ev_collect_->setContinuing(false);
            scheduler_ = fastest_clk_->getScheduler();
            sparta_assert(scheduler_, "Cannot run pipeline collection without a scheduler");
        }

        void recurseAddCollectables_(sparta::TreeNode * node,
                                     CollectionPoints & collectables,
                                     const std::set<std::string>& enabled_nodes)
        {
            if (auto c = dynamic_cast<CollectableTreeNode*>(node)) {
                if (enabled_nodes.empty() || enabled_nodes.count(c->getLocation())) {
                    c->addCollectionPoint(collectables);
                }
            }
            for (auto & child : node->getChildren()) {
                recurseAddCollectables_(child, collectables, enabled_nodes);
            }
        }

        void recurseFindFastestCollectableClock_(const sparta::TreeNode * node,
                                                 const Clock *& fastest_clk,
                                                 const std::set<std::string>& enabled_nodes)
        {
            if (auto c = dynamic_cast<const CollectableTreeNode*>(node)) {
                if (!enabled_nodes.empty() && !enabled_nodes.count(c->getLocation())) {
                    return;
                }
                if (c->getClock() != nullptr) {
                    if (fastest_clk == nullptr) {
                        fastest_clk = c->getClock();
                    } else {
                        if (c->getClock()->getPeriod() < fastest_clk->getPeriod()) {
                            fastest_clk = c->getClock();
                        }
                    }
                }
            }

            for (auto & child : node->getChildren()) {
                recurseFindFastestCollectableClock_(child, fastest_clk, enabled_nodes);
            }
        }

        void performCollection_() {
            db_mgr_.getCollectionMgr()->collectAll();
            ev_collect_->schedule();
        }

        double getIPC_() const
        {
            auto tick = scheduler_->getCurrentTick();
            auto cycle = fastest_clk_->getCycle(tick);
            auto num_retired = insts_retired_counter_->get();
            return static_cast<double>(num_retired) / static_cast<double>(cycle);
        }

        //! The SimDB database
        simdb::DatabaseManager db_mgr_;

        //! The SimDB filename e.g. "pipeline.db"
        std::string filename_;

        //! The root tree node
        sparta::RootTreeNode * root_ = nullptr;

        //! The event set for the performCollection_() callback event 
        EventSet ev_set_;

        //! The unique event for the performCollection_() callback
        std::unique_ptr<sparta::Scheduleable> ev_collect_;

        //! The simulation scheduler
        const sparta::Scheduler * scheduler_;

        //! The "total instruction retired" counter
        const sparta::CounterBase * insts_retired_counter_;

        //! Fastest clock across all collectable tree nodes
        const Clock * fastest_clk_ = nullptr;
    };

}// namespace collection
}// namespace sparta
