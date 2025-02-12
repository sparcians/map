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
        class CollectablesByClock
        {
        public:
            CollectablesByClock(const Clock * clk,
                                SchedulingPhase collection_phase) :
                ev_set_(nullptr)
            {
                switch(collection_phase)
                {
                case SchedulingPhase::Trigger:
                    ev_collect_.reset(new sparta::UniqueEvent<SchedulingPhase::Trigger>
                                      (&ev_set_, sparta::notNull(clk)->getName() + "_auto_collection_event_trigger",
                                       CREATE_SPARTA_HANDLER(CollectablesByClock, performCollection), 1));
                    break;
                case SchedulingPhase::Update:
                    ev_collect_.reset(new sparta::UniqueEvent<SchedulingPhase::Update>
                                      (&ev_set_, sparta::notNull(clk)->getName() + "_auto_collection_event_update",
                                       CREATE_SPARTA_HANDLER(CollectablesByClock, performCollection), 1));
                    break;
                case SchedulingPhase::PortUpdate:
                    ev_collect_.reset(new sparta::UniqueEvent<SchedulingPhase::PortUpdate>
                                      (&ev_set_, sparta::notNull(clk)->getName() + "_auto_collection_event_portupdate",
                                       CREATE_SPARTA_HANDLER(CollectablesByClock, performCollection), 1));
                    break;
                case SchedulingPhase::Flush:
                    ev_collect_.reset(new sparta::UniqueEvent<SchedulingPhase::Flush>
                                      (&ev_set_, sparta::notNull(clk)->getName() + "_auto_collection_event_flush",
                                       CREATE_SPARTA_HANDLER(CollectablesByClock, performCollection), 1));
                    break;
                case SchedulingPhase::Collection:
                    ev_collect_.reset(new sparta::UniqueEvent<SchedulingPhase::Collection>
                                      (&ev_set_, sparta::notNull(clk)->getName() + "_auto_collection_event_collection",
                                       CREATE_SPARTA_HANDLER(CollectablesByClock, performCollection), 1));
                    break;
                case SchedulingPhase::Tick:
                    ev_collect_.reset(new sparta::UniqueEvent<SchedulingPhase::Tick>
                                      (&ev_set_, sparta::notNull(clk)->getName() + "_auto_collection_event_tick",
                                       CREATE_SPARTA_HANDLER(CollectablesByClock, performCollection), 1));
                    break;
                case SchedulingPhase::PostTick:
                    ev_collect_.reset(new sparta::UniqueEvent<SchedulingPhase::PostTick>
                                      (&ev_set_, sparta::notNull(clk)->getName() + "_auto_collection_event_posttick",
                                       CREATE_SPARTA_HANDLER(CollectablesByClock, performCollection), 1));
                    break;
                case SchedulingPhase::__last_scheduling_phase:
                    sparta_assert(!"Should not have gotten here");
                    break;
                    // NO DEFAULT!  Allows for compiler errors if the enum
                    // class is updated.
                }
                ev_collect_->setScheduleableClock(clk);
                ev_collect_->setScheduler(clk->getScheduler());
                ev_collect_->setContinuing(false);
            }

            void enable(CollectableTreeNode * ctn) {
                enabled_ctns_.insert(ctn);
                // Schedule collect event in the next cycle in case
                // this is called in an unavailable pphase.
                ev_collect_->schedule(sparta::Clock::Cycle(1));
            }

            void disable(CollectableTreeNode * ctn) {
                enabled_ctns_.erase(ctn);
            }

            bool anyCollected() const {
                return !enabled_ctns_.empty();
            }

            void performCollection() {
                for(auto & ctn : enabled_ctns_) {
                    if(ctn->isCollected()) {
                        ctn->collect();
                    }
                }
                if(!enabled_ctns_.empty()) {
                    ev_collect_->schedule();
                }
            }

        private:
            EventSet ev_set_;
            std::unique_ptr<sparta::Scheduleable> ev_collect_;
            std::set<CollectableTreeNode*> enabled_ctns_;
        };

        // A map of the clock pointer and the structures that
        // represent collectables on that clock.
        std::map<const sparta::Clock *,
                 std::array<std::unique_ptr<CollectablesByClock>,
                            sparta::NUM_SCHEDULING_PHASES>> clock_ctn_map_;

        // Registered collectables
        std::set<CollectableTreeNode*> registered_collectables_;

    public:
        PipelineCollector(const std::string& simdb_filename,
                          const size_t heartbeat,
                          const sparta::TreeNode * root)
            : db_mgr_(std::make_unique<simdb::DatabaseManager>(simdb_filename, true))
        {
            sparta_assert(root->isFinalized() == true,
                          "Pipeline collection cannot be constructed until the sparta tree has been finalized.");

            sparta_assert(root->getClock() != nullptr,
                          "Cannot construct PipelineCollector because root clock is a nullptr");

            sparta_assert(root->getClock()->getScheduler()->isFinalized() == false,
                          "Pipeline Collection cannot be instantiated after scheduler finalization");

            scheduler_ = root->getClock()->getScheduler();

            // Note that we only care about the collection data and have
            // no need for any other tables, aside from the tables that the
            // DatabaseManager adds automatically to support this feature.
            simdb::Schema schema;
            db_mgr_->createDatabaseFromSchema(schema);

            std::function<uint64_t()> func_ptr = std::bind(&PipelineCollector::getCollectionTick_, this);
            db_mgr_->getCollectionMgr()->useTimestampsFrom(func_ptr);
            db_mgr_->getCollectionMgr()->setHeartbeat(heartbeat);

            // Initialize the clock/collectable map
            std::function<void (const sparta::Clock*)> addClks;
            addClks = [&addClks, this] (const sparta::Clock* clk)
                {
                    if(clk != nullptr){
                        auto & u_p = clock_ctn_map_[clk];
                        for(uint32_t i = 0; i < NUM_SCHEDULING_PHASES; ++i) {
                            u_p[i].reset(new CollectablesByClock(clk, static_cast<SchedulingPhase>(i)));
                        }
                        for(const sparta::TreeNode* child : sparta::TreeNodePrivateAttorney::getAllChildren(clk)) {
                            const sparta::Clock* child_clk = dynamic_cast<const sparta::Clock*>(child);
                            if(child_clk){
                                addClks(child_clk);
                            }
                        }
                    }
                };

            addClks(root->getClock());
        }

        ~PipelineCollector() {
            db_mgr_->closeDatabase();

            // XXX This is a little goofy looking.  Should we make sparta_abort that takes conditional
            //    be sparta_abort_unless()?
            sparta_abort(collection_active_ != true,
                         "The PipelineCollector was not torn down properly. Before "
                         "tearing down the simulation tree, you must call "
                         "destroy() on the collector");
        }

        /*!
         * \brief Teardown the pipeline collector
         *
         * Tear down the PipelineCollector.  Should be called before
         * Tree teardown to close all open transactions.
         */
        void destroy()
        {
            if(collection_active_) {
                for(auto & ctn : registered_collectables_) {
                    if(ctn->isCollected()) {
                        ctn->closeRecord(true);
                    }
                }
            }
            registered_collectables_.clear();
            collection_active_ = false;
        }

        void reactivate(const std::string& simdb_filename)
        {
            (void)simdb_filename;
            sparta_assert(false, "TODO cnyce: Not implemented");

            if (db_mgr_) {
                db_mgr_->closeDatabase();
                db_mgr_.reset();
            }
        }

        /**
         * \brief Turn on collection for everything below a TreeNode.
         *        Recursively transverse the tree and turn on child
         *        nodes for pipeline collection.
         *
         * \param starting_node TreeNode to start collection on. This
         *                      TreeNode will try to start collection
         *                      as well as any node below it.
         *
         * \note The Scheduler MUST be finalized before this method is
         * called
         */
        void startCollection(TreeNode* starting_node)
        {
            // Recursively collect the start node and children
            std::function<void (sparta::TreeNode* starting_node)> recursiveCollect;
            recursiveCollect = [&recursiveCollect, this] (sparta::TreeNode* starting_node)
                {
                    // First turn on this node if it's actually a CollectableTreeNode
                    CollectableTreeNode* c_node = dynamic_cast<CollectableTreeNode*>(starting_node);
                    if(c_node != nullptr) {
                        c_node->startCollecting(this, db_mgr_.get());
                        registered_collectables_.insert(c_node);
                    }

                    // Recursive step. Go through the children and turn them on as well.
                    for(sparta::TreeNode* node : sparta::TreeNodePrivateAttorney::getAllChildren(starting_node))
                    {
                        recursiveCollect(node);
                    }
                };

            recursiveCollect(starting_node);
        }

        /**
         * \brief Stop pipeline collection on only those
         *        CollectableTreeNodes given
         * \param starting_node The node to shut collection down on
         *
         */
        void stopCollection(sparta::TreeNode* starting_node)
        {
            std::function<void (sparta::TreeNode* starting_node)> recursiveStopCollect;
            recursiveStopCollect = [&recursiveStopCollect, this] (sparta::TreeNode* starting_node) {
                // First turn off this node if it's actually a CollectableTreeNode
                CollectableTreeNode* c_node = dynamic_cast<CollectableTreeNode*>(starting_node);
                if(c_node != nullptr)
                {
                    c_node->stopCollecting(this, db_mgr_.get());
                    registered_collectables_.erase(c_node);
                }

                // Recursive step. Go through the children and turn them on as well.
                for(sparta::TreeNode* node : sparta::TreeNodePrivateAttorney::getAllChildren(starting_node))
                {
                    recursiveStopCollect(node);
                }
            };
            recursiveStopCollect(starting_node);

            collection_active_ = !registered_collectables_.empty();
        }

        /**
         * \brief Stop pipeline collection on only those
         *        CollectableTreeNodes that this PipelineCollector
         *        was started with
         */
        void stopCollection() {
            for(auto & col : registered_collectables_) {
                col->stopCollecting(this, db_mgr_.get());
            }
            registered_collectables_.clear();
        }

        /**
         * \brief Add the CollectableTreeNode to auto collection
         * \param ctn The CollectableTreeNode that is to be collected
         * \param collection_phase The phase to collect the object in
         *
         * Enable collection on the given CollectableTreeNode.  This
         * is a runtime call. There are some rules here:
         *
         * #. The Scheduler must be finialized and simulation started
         * #. The clock that the CollectableTreeNode belongs to must
         *    have been registered with the PipelineCollector at init time.
         */
        void addToAutoCollection(CollectableTreeNode * ctn,
                                 SchedulingPhase collection_phase = SchedulingPhase::Tick)
        {
            auto ccm_pair = clock_ctn_map_.find(ctn->getClock());
            sparta_assert(ccm_pair != clock_ctn_map_.end());
            ccm_pair->second[static_cast<uint32_t>(collection_phase)]->enable(ctn);
        }

        /**
         * \brief Remove the given CollectableTreeNode from collection
         * \param ctn The CollectableTreeNode that is to be removed from collection
         *
         * Disable collection on the given CollectableTreeNode.  This
         * is a runtime call. There are some rules here:
         *
         * #. The Scheduler must be finialized and simulation started
         * #. The clock that the CollectableTreeNode belongs to must
         *    have been registered with the PipelineCollector at init time.
         */
        void removeFromAutoCollection(CollectableTreeNode * ctn)
        {
            auto ccm_pair = clock_ctn_map_.find(ctn->getClock());
            sparta_assert(ccm_pair != clock_ctn_map_.end());
            for(auto & u_p : ccm_pair->second) {
                u_p->disable(ctn);
            }
        }

        //! \return the pipeout file path
        const std::string & getFilePath() const
        {
            return db_mgr_->getDatabaseFilePath();
        }

        //! \return the scheduler for this collector
        Scheduler* getScheduler() const {
            return scheduler_;
        }

    private:
        uint64_t getCollectionTick_() const
        {
            return scheduler_->getCurrentTick() - 1;
        }

        //! The SimDB database
        std::unique_ptr<simdb::DatabaseManager> db_mgr_;

        //! Scheduler on which this collector operates
        Scheduler * scheduler_ = nullptr;

        //! Is collection enabled on at least one node?
        bool collection_active_ = false;
    };

}// namespace collection
}// namespace sparta
