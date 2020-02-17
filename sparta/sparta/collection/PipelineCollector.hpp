// <PipelineCollector.hpp> -*- C++ -*-

/**
 * \file PipelineCollector.hpp
 *
 * \brief Class to facilitate pipeline collection operations.
 */

#ifndef __PIPELINE_COLLECTOR_H__
#define __PIPELINE_COLLECTOR_H__

#include <set>
#include <map>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/collection/CollectableTreeNode.hpp"
#include "sparta/collection/Collector.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/kernel/Scheduler.hpp"

#include "sparta/argos/Outputter.hpp"
#include "sparta/argos/ClockFileWriter.hpp"
#include "sparta/argos/LocationFileWriter.hpp"
#include "sparta/simulation/TreeNodePrivateAttorney.hpp"

namespace sparta{
namespace collection
{
    /**
     * \class PipelineCollector
     *
     * \brief A class that facilitates all universal pipeline
     *        collection operations such as outputting finalized
     *        records, generating unique transaction Id's, maintaining
     *        heartbeat functionality, writing the location file,
     *        writing the clock file.
     *
     * The class must be initialized with
     * PipelineCollector::getPipelineCollector()->init(...)  before
     * collection is to occure. This method is required to have
     * important parameters required during pipeline collection.
     *
     * This class operates on a specific scheduler specified at construction.
     * It's implementation should not access the sparta Scheduler singleton
     * directly.
     *
     * destroy() should also be called at the end of the programs life
     * to perform post collection maintenance. Any transactions alive
     * at this point will have their current data written to disk with
     * the end time as the end time of simulation. If the singleton
     * was never initialized, destroy will have no effect.
     *
     * Once the singleton is created and initialized with init
     * pipeline collection still needs to be switched on. This can be
     * done via the startCollection() at the treenode level. This
     * method recursively turns on collection at and below the
     * treenode pointer passed in.
     *
     * Likewise collection can be turned off via stopCollection at any
     * time.
     */
    class PipelineCollector : public Collector
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
                ev_collect_->schedule(sparta::Clock::Cycle(0));
            }

            void disable(CollectableTreeNode * ctn) {
                enabled_ctns_.erase(ctn);
            }

            bool anyCollected() const {
                return !enabled_ctns_.empty();
            }

            void performCollection() {
                // std::for_each(enabled_ctns_.begin(),
                //               enabled_ctns_.end(), [&](CollectableTreeNode * it) {
                //                   std::cout << it->getName() << " " << ev_collect_.getClock()->currentCycle() << std::endl;
                //               });

                for(auto & ctn : enabled_ctns_) {
                    if(ctn->isCollected()) {
                        ctn->collect();
                    }
                }
                if(!enabled_ctns_.empty()) {
                    ev_collect_->schedule();
                }
            }

            void print() {
                for(auto ctn : enabled_ctns_) {
                    std::cout << '\t' << ctn->getName() << std::endl;
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

        /**
         * \brief Instantiate the collector with required parameters before
         *         pipeline collection can occur.
         * \param filepath The relative path to the output directory or file prefix.
         * \param heartbeat_interval The interval offset at which to write an
         * index file (in Ticks). If 0, the heartbeat will be derived from the
         * known clocks in the simulation.
         * \param root_clk A pointer to the clock that maintains the hyper
         *                 cycle time.
         * \param root A pointer to the root sparta::TreeNode during
         *             simulation which will be walked when producing
         *             a location map file that maps location id
         *             numbers with the sparta tree location for
         *             collected objects.
         * \param scheduler Scheduler on which this collector operates. If null,
         * uses the Scheduler belonging to root_clk. This allows us to
         * run different pipeline collectors on different schedulers
         * which is useful for standalone analyzers which run multiple
         * instances of some model in parallel over the same trace data
         *
         * \warning If filepath is a directory, the directory must
         *          already exist.
         * \pre The sparta tree must be finalized.
         * \note This method does NOT start collection. To start
         *       collection, call startCollection
         *
         */
        PipelineCollector(const std::string& filepath, Scheduler::Tick heartbeat_interval,
                          const sparta::Clock* root_clk, const sparta::TreeNode* root,
                          Scheduler* scheduler=nullptr) :
            Collector("PipelineCollector"),
            scheduler_(scheduler != nullptr ? scheduler : root_clk->getScheduler()),
            collector_events_(nullptr),
            ev_heartbeat_(&collector_events_, Collector::getName() + "_heartbeat_event",
                          CREATE_SPARTA_HANDLER(PipelineCollector, performHeartBeat_), 0)
        {
            // Sanity check - pipeline collection cannot occur without a scheduler
            sparta_assert(scheduler_);

            ev_heartbeat_.setScheduleableClock(root_clk);
            ev_heartbeat_.setScheduler(scheduler_);

            sparta_assert(root != nullptr, "Pipeline Collection will not be able to create location file because it was passed a nullptr root treenode.");
            sparta_assert(root->isFinalized(), "Pipeline collection cannot be constructed until the sparta tree has been finalized.");
            // Assert that we got valid pointers necessary for pipeline collection.
            sparta_assert(root_clk != nullptr, "Cannot construct PipelineCollector because root clock is a nullptr");
            sparta_assert(scheduler_->isFinalized() == false, "Pipeline Collection cannot be instantiated after scheduler finalization -- it creates events");

            // Initialize the clock/collectable map and find the fastest clock
            const sparta::Clock* fastest_clk = nullptr;
            std::function<void (const sparta::Clock*)> addClks;
            addClks = [&addClks, this, &fastest_clk] (const sparta::Clock* clk)
                {
                    if(clk != nullptr){
                        auto & u_p = clock_ctn_map_[clk];
                        for(uint32_t i = 0; i < NUM_SCHEDULING_PHASES; ++i) {
                            u_p[i].reset(new CollectablesByClock(clk, static_cast<SchedulingPhase>(i)));
                        }
                        for(const sparta::TreeNode* child : sparta::TreeNodePrivateAttorney::getAllChildren(clk)) {
                            const sparta::Clock* child_clk = dynamic_cast<const sparta::Clock*>(child);
                            if(child_clk){
                                auto clk_period = child_clk->getPeriod();
                                // If this clock has a non-1 period (i.e. not the root clock)
                                // AND there is either
                                //   (A) no fastest clock yet
                                //    or
                                //   (B) this clock is a higher frequency than the fastest clock.
                                // then choose this as the fastest
                                if(clk_period != 1 && (!fastest_clk || (clk_period < fastest_clk->getPeriod()))) {
                                    fastest_clk = child_clk;
                                }
                                addClks(child_clk);
                            }
                        }
                    }
                };
            addClks(root_clk);
            if(fastest_clk == nullptr){
                fastest_clk = root_clk;
            }

            // A multiple to multiply against the fastest clock when no heartbeat was set.
            //! \todo The multiplier should also be scaled slightly by the number of locations
            //! registered in order to better estimate the ideal heartbeat size.
            static const uint32_t heartbeat_multiplier = 200;
            // round heartbeat using a multiple of the fastest clock if one was not set.
            if (heartbeat_interval == 0)
            {
                sparta_assert(fastest_clk->getPeriod() != 0);
                heartbeat_interval = fastest_clk->getPeriod() * heartbeat_multiplier;                //round up to the nearest multiple of 100
                heartbeat_interval = heartbeat_interval + (100 - (heartbeat_interval % 100));
                // Argos requires that intervals be a multiple of 100.
                sparta_assert(heartbeat_interval % 100 == 0)
            }

            // We are gonna subtract one from the heartbeat_interval
            // later.. Better be greater than one.
            sparta_assert(heartbeat_interval > 1);
            // Initialize some values.
            filepath_           = filepath;
            heartbeat_interval_ = heartbeat_interval;
            closing_time_       = heartbeat_interval;
            root_clk_           = root_clk;

            ev_heartbeat_.setContinuing(false); // This event does not keep simulation going
        }

        ~PipelineCollector() {
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
            sparta_assert(writer_ != nullptr, "You cannot call PipelineCollector->destroy() more than once");

            if(collection_active_) {
                for(auto & ctn : registered_collectables_) {
                    if(ctn->isCollected()) {
                        ctn->closeRecord(true); // set true for simulation termination
                    }
                }
            }
            registered_collectables_.clear();
            writer_.reset();
            collection_active_ = false;
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
        void startCollection(sparta::TreeNode* starting_node)
        {
            if(collection_active_ == false)
            {
                // Create the outputter used for writing transactions to disk.
                writer_.reset(new argos::Outputter(filepath_, heartbeat_interval_));

                // We need to write an index on the start BEFORE any transactions have been written.
                writer_->writeIndex();

                // Write the clock information out
                writeClockFile_();

                // Open the locations file
                location_writer_.reset(new argos::LocationFileWriter(filepath_));

                // The reader needs heartbeat indexes up to the current
                // collection point.  This can happen on delayed pipeline
                // collection.  Start the heartbeats at the first interval
                // as the Outputter class handles hb 0
                last_heartbeat_ = 0;
                const uint64_t num_hb = scheduler_->getCurrentTick()/heartbeat_interval_;
                uint64_t cnt = 0;
                while(cnt != num_hb) {
                    // write an index
                    writer_->writeIndex();
                    last_heartbeat_ += heartbeat_interval_;
                    ++cnt;
                }

                // Schedule a heartbeat at the next interval, offset from
                // the current tick.  For example, if the scheduler is at
                // 6,600,456, then we want to schedule at 7,000,000 if the
                // interval is 1M and the num_hb is 6
                ev_heartbeat_.scheduleRelativeTick(((num_hb + 1) * heartbeat_interval_) -
                                                   scheduler_->getCurrentTick(), scheduler_);

                collection_active_ = true;
            }

            *(location_writer_.get()) << (*starting_node);

            // Recursively collect the start node and children
            std::function<void (sparta::TreeNode* starting_node)> recursiveCollect;
            recursiveCollect = [&recursiveCollect, this] (sparta::TreeNode* starting_node)
                {
                    // First turn on this node if it's actually a CollectableTreeNode
                    CollectableTreeNode* c_node = dynamic_cast<CollectableTreeNode*>(starting_node);
                    if(c_node != nullptr) {
                        c_node->startCollecting(this);
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
                    c_node->stopCollecting(this);
                    registered_collectables_.erase(c_node);
                }

                // Recursive step. Go through the children and turn them on as well.
                for(sparta::TreeNode* node : sparta::TreeNodePrivateAttorney::getAllChildren(starting_node))
                {
                    recursiveStopCollect(node);
                }
            };
            recursiveStopCollect(starting_node);

            bool still_active = !registered_collectables_.empty();
            // for(auto & cp : clock_ctn_map_) {
            //     if(cp.second->anyCollected()) {
            //         still_active = true;
            //         break;
            //     }
            // }
            collection_active_ = still_active;
        }

        /**
         * \brief Stop pipeline collection on only those
         *        CollectableTreeNodes that this PipelineCollector
         *        was started with
         */
        void stopCollection() {
            for(auto & col : registered_collectables_) {
                col->stopCollecting(this);
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

        /**
         * \brief Return a unique transaction id using a dummy counter
         */
        uint64_t getUniqueTransactionId()
        {
            // make sure we are not going to overflow our int,
            // if we did overflow our id's are no longer unique!
            sparta_assert(last_transaction_id_ < (~(uint64_t)0));
            return ++last_transaction_id_;
        }

        /**
         * \brief Output a finized transaction to our Outputter class.
         * \param dat The transaction to be outputted.
         * \param R_Type the type of transaction struct
         */
        template<class R_Type>
        void writeRecord(const R_Type& dat)
        {
            sparta_assert(collection_active_, "The pipeline head must be running in order to write a transaction");

            // Make sure it's within the heartbeat window
            sparta_assert(dat.time_End <= (last_heartbeat_ + heartbeat_interval_));
            sparta_assert(dat.time_Start >= last_heartbeat_);


            // Make sure transactions are exclusive.
            // transaction [4999-5000] should be written BEFORE the index is written for transactions
            // starting at 5000
            sparta_assert(closing_time_ < heartbeat_interval_  // Ignore first heartbeat
                              || dat.time_Start >= closing_time_ - heartbeat_interval_,
                              "Attempted to write a pipeout record with exclusive start =("
                              << dat.time_Start << "), less than closing of previous interval"
                              << closing_time_ - heartbeat_interval_ );

            // std::cout << "writing annt. " << "loc: " << dat.location_ID << " start: "
            //           << dat.time_Start << " end: " << dat.time_End
            //           << " parent: " << dat.parent_ID << std::endl;

            writer_->writeTransaction<R_Type>(dat);
            ++transactions_written_;
        }

        /**
         * \brief Return the number of transactions that this singleton
         * has passed to it's output. This is useful for testing purposes.
         */
        uint64_t numTransactionsWritten() const
        {
            return transactions_written_;
        }

        /**
         * \brief Return true if the collector is actively collecting
         *
         *  Will be true if there are any registered collectables that
         *  are being collected on any clock.
         *
         * \note This method should not be used to determine whether
         *       pipeline collection is running at a specific tree
         *       node location.
         */
        bool isCollectionActive() const {
            return collection_active_;
        }

        void printMap() {
            //std::cout << "Printing Map Not Supported" << std::endl;
            // for(auto & p : clock_ctn_map_) {
            //     std::cout << "\nClock            : " << p.first->getName()
            //               << "\nAuto collectables: " << std::endl;
            //     p.second->print();
            // }
        }

        //! \return the pipeout file path
        const std::string & getFilePath() const {
            return filepath_;
        }

        //! \return the scheduler for this collector
        Scheduler* getScheduler() const {
            return scheduler_;
        }

    private:

        /**
         * \brief Write the clock file based off of a pointer to the root clock,
         * that was established in the parameters of startCollection
         */
        void writeClockFile_()
        {
            // We only need the ClockFileWriter to exist during the writing of the clock file.
            // there for it was created on the stack.
            //std::cout << "Writing Pipeline Collection clock file. " << std::endl;
            argos::ClockFileWriter clock_writer(filepath_);
            clock_writer << (*root_clk_);
        }

        //! Perform a heartbeat on the collector.  This is required to
        //! enable the writing of an index file used by the pipeout
        //! reader for fast access
        void performHeartBeat_()
        {
            if(collection_active_) {
                // Close all transactions
                for(auto & ctn : registered_collectables_) {
                    if(ctn->isCollected()) {
                        ctn->restartRecord();
                    }
                }

                // write an index
                writer_->writeIndex();

                // Remember the last time we recorded a heartbeat
                last_heartbeat_ = scheduler_->getCurrentTick();

                // Schedule another heartbeat
                ev_heartbeat_.schedule(heartbeat_interval_);
            }
        }

        //! A pointer to the outputter class used for writing
        //! transactions to physical disk.
        std::unique_ptr<argos::Outputter> writer_;

        //! A pointer to the root sparta TreeNode of the
        //! simulation. Important for writing the location map file.
        std::unique_ptr<argos::LocationFileWriter> location_writer_;
        //sparta::TreeNode* collected_treenode_ = nullptr;

        //! Pointer to the root clock.  This clock is considered the
        //! hyper-clock or the clock with the hypercycle
        const sparta::Clock * root_clk_ = nullptr;

        //! The filepath/prefix for writing pipeline collection files too
        std::string filepath_;

        //! Keep track of the last transaction id given to ensure that
        //! each transaction is written with a unique id
        uint64_t last_transaction_id_ = 0;

        //! The number of transactions written to disk so far
        uint64_t transactions_written_ = 0;

        //! The number of ticks between heart beats. Also the offset
        //! between index pointer writes in the Outputter
        uint64_t heartbeat_interval_ = 0;

        //! The last heartbeat we recorded
        Scheduler::Tick last_heartbeat_ = 0;

        //! Scheduler on which this collector operates
        Scheduler * scheduler_;

        //! Event and EventSet for performing heartbeats
        EventSet collector_events_;
        UniqueEvent<SchedulingPhase::Collection> ev_heartbeat_;

        //! The time that the next heartbeat will occur
        uint64_t closing_time_ = 0;

        //! Is collection enabled on at least one node?
        bool collection_active_ = false;

    };

}// namespace collection
}// namespace sparta


//__PIPELINE_COLLECTOR_H__
#endif
