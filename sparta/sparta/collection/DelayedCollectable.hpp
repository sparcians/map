// <DelayedCollectable.hpp>  -*- C++ -*-

/**
 * \file DelayedCollectable.hpp
 *
 * \brief Implementation of the DelayedCollectable class that allows
 * a user to collect an object into an Argos pipeline file
 */

#ifndef __DELAYED_PIPELINE_COLLECTABLE_H__
#define __DELAYED_PIPELINE_COLLECTABLE_H__

#include <sstream>
#include <functional>
#include "sparta/collection/Collectable.hpp"
#include "sparta/argos/transaction_structures.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/PayloadEvent.hpp"

namespace sparta {

namespace collection
{
    /**
     * \class DelayedCollectable
     * \brief Class used to record data in pipeline collection, but
     *        recorded in a delayed fashion
     *
     * DelayedCollectable is useful for delivering a collected chunk
     * of data to the PipelineCollector in the future.  Such an
     * example is SyncPort where data can be sent with a delay of N
     * and the view should show this data only on cycle N for
     * delivery.
     */
    template<class DataT>
    class DelayedCollectable : public Collectable<DataT>
    {
        using CollectableTreeNode::isCollected;

        struct DurationData
        {
            DurationData() {}

            DurationData(const DataT & dat,
                         sparta::Clock::Cycle duration) :
                data(dat), duration(duration)
            {}

            DataT data;
            sparta::Clock::Cycle duration;
        };

    public:

        // Promote the collect method from Collectable
        using Collectable<DataT>::collect;

        /**
         * \brief Construct the DelayedCollectable, no data object associated, part of a group
         * \param parent A pointer to a parent treenode.  Must not be null
         * \param name The name for which to create this object as a child sparta::TreeNode
         * \param group The group for which to create this object as a child sparta::TreeNode
         * \param index The index within the group for this collectable
         * \param parentid The transaction id of a parent for this collectable; 0 for no parent
         * \param desc A description for the interface
         */
        DelayedCollectable(sparta::TreeNode* parent,
                           const std::string& name,
                           const std::string& group,
                           uint32_t index,
                           uint64_t parentid = 0,
                           const std::string & desc = "DelayedCollectable <manual, no desc>") :
            Collectable<DataT>(parent, name, group, index, desc)
        {
            (void) parentid;
        }

        /**
         * \brief Construct the DelayedCollectable
         * \param parent A pointer to a parent treenode.  Must not be null
         * \param name The name for which to create this object as a child sparta::TreeNode
         * \param collected_object Pointer to the object to collect during the "COLLECT" phase
         * \param parentid The transaction id of a parent for this collectable; 0 for no parent
         * \param desc A description for the interface
         */
        DelayedCollectable(sparta::TreeNode* parent,
                           const std::string& name,
                           const DataT * collected_object,
                           uint64_t parentid = 0,
                           const std::string & desc = "DelayedCollectable <no desc>") :
            Collectable<DataT>(parent, name, collected_object, parentid, desc)
        {}

        /**
         * \brief Construct the DelayedCollectable, no data object associated
         * \param parent A pointer to a parent treenode.  Must not be null
         * \param name The name for which to create this object as a child sparta::TreeNode
         * \param parentid The transaction id of a parent for this collectable; 0 for no parent
         * \param desc A description for the interface
         */
        DelayedCollectable(sparta::TreeNode* parent,
                           const std::string& name,
                           uint64_t parentid = 0,
                           const std::string & desc = "DelayedCollectable <manual, no desc>") :
            DelayedCollectable(parent, name, nullptr, parentid, desc)
        {}

        /*!
         * \brief Explicitly collect a value in the future
         * \param val The value to collect in the future
         * \param delay The delay before recording this value to file
         *
         * Explicitly collect a value for this collectable in the
         * future, ignoring what the Collectable is currently pointing
         * to.
         */
        void collect(const DataT & val, sparta::Clock::Cycle delay)
        {
            if(SPARTA_EXPECT_FALSE(isCollected()))
            {
                if(delay != 0) {
                    ev_collect_.schedule(val, delay);
                }
                else {
                    Collectable<DataT>::collect(val);
                }
            }
        }

        /*!
         * \brief Explicitly collect a value in the future
         * \param val The value to collect in the future
         * \param delay The delay before recording this value to file
         * \param duration The amount of time in cycles the value is available
         *
         * Explicitly collect a value for this collectable in the
         * future, ignoring what the Collectable is currently pointing
         * to.
         */
        void collectWithDuration(const DataT & val,
                                 sparta::Clock::Cycle delay,
                                 sparta::Clock::Cycle duration)
        {
            if(SPARTA_EXPECT_FALSE(isCollected()))
            {
                if(delay != 0) {
                    ev_collect_duration_.preparePayload({val, duration})->schedule(delay);
                }
                else {
                    Collectable<DataT>::collectWithDuration(val, duration);
                }
            }
        }

    private:

        // Called from collectWithDuration where the data needs to be
        // delivered at a given delayed time, but only for a short
        // duration.
        void collectWithDuration_(const DurationData & dur_dat) {
            Collectable<DataT>::collectWithDuration(dur_dat.data, dur_dat.duration);
        }

        // For those folks that want collection to appear in the
        // future
        sparta::PayloadEvent<DataT, SchedulingPhase::Collection> ev_collect_{
            &Collectable<DataT>::getEventSet_(), "delayedpipelinecollectable_event",
                CREATE_SPARTA_HANDLER_WITH_DATA(Collectable<DataT>, collect, DataT)};

        // For those folks that want collection to appear in the
        // future with a duration
        sparta::PayloadEvent<DurationData, SchedulingPhase::Collection> ev_collect_duration_{
            &Collectable<DataT>::getEventSet_(), "delayedpipelinecollectable_duration_event",
                CREATE_SPARTA_HANDLER_WITH_DATA(DelayedCollectable<DataT>,
                                              collectWithDuration_, DurationData)};


    };
}//namespace collection
}//namespace sparta

#endif
