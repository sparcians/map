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
                          const size_t heartbeat = 10)
            : db_mgr_(simdb_filename, true)
            , filename_(simdb_filename)
        {
            // Note that we only care about the collection data and have
            // no need for any other tables, aside from the tables that the
            // DatabaseManager adds automatically to support this feature.
            simdb::Schema schema;
            db_mgr_.createDatabaseFromSchema(schema);

            std::function<uint64_t()> func_ptr = std::bind(&PipelineCollector::getCollectionTick_, this);
            db_mgr_.getCollectionMgr()->useTimestampsFrom(func_ptr);
            db_mgr_.getCollectionMgr()->setHeartbeat(heartbeat);
        }

        ~PipelineCollector() {
            db_mgr_.closeDatabase();
        }

        //! \return the pipeout file path
        const std::string & getFilePath() const {
            return filename_;
        }

        void enableCollection(TreeNode* starting_node) {
            for (auto c : getCollectablesFrom_(starting_node)) {
                if (!scheduler_) {
                    scheduler_ = c->getScheduler();
                }
                auto &collectables = collectables_by_clk_[c->getClock()];
                if (!collectables) {
                    collectables.reset(new CollectablesByClock(c->getClock(), db_mgr_));
                }
                collectables->addToAutoCollection(c);
            }
        }

        void finalizeCollector(RootTreeNode* rtn) {
            createCollections_(rtn);
        }

        void disableCollection() {
            db_mgr_.onPipelineCollectorClosing();
            db_mgr_.getConnection()->getTaskQueue()->stopThread();
            db_mgr_.closeDatabase();
        }

    private:
        std::vector<CollectableTreeNode*> getCollectablesFrom_(TreeNode* starting_node) const
        {
            std::vector<CollectableTreeNode*> collectables;
            std::function<void(TreeNode*)> recursiveFindCollectables;
            recursiveFindCollectables = [&recursiveFindCollectables, &collectables](TreeNode* node) {
                if (auto c = dynamic_cast<CollectableTreeNode*>(node)) {
                    collectables.push_back(c);
                }
                for (auto & child : node->getChildren()) {
                    recursiveFindCollectables(child);
                }
            };
            recursiveFindCollectables(starting_node);
            return collectables;
        }

        uint64_t getCollectionTick_() const {
            return scheduler_->getCurrentTick() - 1;
        }

        void createCollections_(RootTreeNode* root) {
            CollectionPoints collectables;
            recurseAddCollectables_(root, collectables);
            collectables.createCollections(db_mgr_.getCollectionMgr());
            db_mgr_.finalizeCollections();
        }

        void recurseAddCollectables_(sparta::TreeNode * node,
                                     CollectionPoints & collectables)
        {
            if (auto c = dynamic_cast<CollectableTreeNode*>(node)) {
                if (c->isCollected()) {
                    c->addCollectionPoint(collectables);
                }
            }
            for (auto & child : node->getChildren()) {
                recurseAddCollectables_(child, collectables);
            }
        }

        class CollectablesByClock
        {
        public:
            CollectablesByClock(const Clock* clk, simdb::DatabaseManager &db_mgr)
                : db_mgr_(db_mgr)
                , ev_set_(nullptr)
            {
                ev_collect_.reset(new sparta::UniqueEvent<SchedulingPhase::Collection>
                                  (&ev_set_, sparta::notNull(clk)->getName() + "_auto_collection_event_collection",
                                   CREATE_SPARTA_HANDLER(CollectablesByClock, performCollection_), 1));
                ev_collect_->setScheduleableClock(clk);
                ev_collect_->setScheduler(clk->getScheduler());
                ev_collect_->setContinuing(false);
            }

            void addToAutoCollection(CollectableTreeNode* ctn) {
                ctn->enable();
                sparta_assert(clk_domain_ == ctn->getClock()->getName() || clk_domain_.empty());
                clk_domain_ = ctn->getClock()->getName();

                // Schedule collect event in the next cycle in case
                // this is called in an unavailable phase.
                ev_collect_->schedule(sparta::Clock::Cycle(1));
            }

        private:
            void performCollection_() {
                db_mgr_.getCollectionMgr()->collectDomain(clk_domain_);
                ev_collect_->schedule();
            }

            simdb::DatabaseManager &db_mgr_;
            sparta::EventSet ev_set_;
            std::unique_ptr<sparta::UniqueEvent<SchedulingPhase::Collection>> ev_collect_;
            std::string clk_domain_;
        };

        //! The SimDB database
        simdb::DatabaseManager db_mgr_;

        //! The SimDB filename e.g. "pipeline.db"
        std::string filename_;

        //! Scheduler
        sparta::Scheduler * scheduler_ = nullptr;

        //! Collectables by clock
        std::unordered_map<const Clock*, std::unique_ptr<CollectablesByClock>> collectables_by_clk_;
    };

}// namespace collection
}// namespace sparta
