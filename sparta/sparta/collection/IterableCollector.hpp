// <IterableCollector.hpp> -*- C++ -*-

/*
 */

#pragma once

#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <iterator>
#include <type_traits>
#include "sparta/utils/Utils.hpp"
#include "sparta/log/MessageSource.hpp"
#include "sparta/events/SchedulingPhases.hpp"
#include "sparta/collection/Collectable.hpp"
#include "sparta/collection/CollectableTreeNode.hpp"
#include "sparta/collection/PipelineCollector.hpp"

namespace sparta {
namespace collection {

/**
 * \class IterableCollector
 * \brief A collector of any iterable type (std::vector, std::list, sparta::Buffer, etc)
 *
 * \tname IterableType     The type of the collected object
 * \tname collection_phase The phase collection will occur.
 *                         Collection happens automatically in this
 *                         phase, unless it is disabled by a call to
 *                         setManualCollection()
 * \tname sparse_array_type Set to true if the iterable type is
 *                          sparse, meaning iteration will occur on
 *                          the entire iterable object, but each
 *                          iterator might not be valid to
 *                          de-reference.  When this is true, it is
 *                          expected that the iterator returned from
 *                          the IterableType can be queried for
 *                          validity (by a call to itr->isValid()).
 *
 * This collector will iterable over an std::array type, std::list
 * type, std::vector type, sparta::Buffer, sparta::Queue, sparta::Array, or
 * even a simple C-array type.  The class needs to constructed with an
 * expected capacity of the container, and the container should never
 * grow beyond this expected capacity.  If so, during collection, the
 * class will output a warning message (only once).
 *
 * How collection is performed:
 *
 * - During the phase given by the template argument, this class will
 *   be given an opportunity to collect the object (convert the
 *   container's objects to a string using operator<<).
 *
 * - The main collection loop always iterates from 0 to the expected
 *   capacity, even if the \i size of the container is smaller.  For
 *   every element in the collected object (begin() -> end()[size]),
 *   the object is collected.  From end()[size] -> expected capacity,
 *   records are closed:
 *
 *   <pre>
 *   |  collected   |     closed      |
 *   | begin -> end | end -> capacity |
 *   </pre>
 *
 * \note This Template Overload is switched on only for POD Data Types and/or Non-Pair-Collectable User Defined DataTypes.
 */
template <typename IterableType, SchedulingPhase collection_phase = SchedulingPhase::Collection, bool sparse_array_type = false>
class IterableCollector : public CollectableTreeNode
{
public:
    typedef typename IterableType::size_type size_type;

    /**
     * \brief constructor
     * \param parent the parent treenode for the collector
     * \param name the name of the collector
     * \param group Group this collector is part of
     * \param index the index within the group
     * \param desc Description of this node
     * \param iterable Pointer to the iterable object to collect
     * \param expected_capacity The maximum size this item should grow to
     */
    IterableCollector (TreeNode * parent,
                       const std::string & name,
                       const std::string & group,
                       const uint32_t index,
                       const std::string & desc,
                       const IterableType * iterable,
                       const size_type expected_capacity) :
        CollectableTreeNode(parent, name, group, index, desc),
        event_set_(this),
        ev_close_record_(&event_set_, name + "_pipeline_collectable_close_event",
                         CREATE_SPARTA_HANDLER_WITH_DATA(IterableCollector, closeRecord, bool))
    {
        if (auto collection = parent->getRoot()->getCollectionSystem(false)) {
            auto path = getLocation();
            auto clk_name = notNull(getClock())->getName();
            auto default_enabled = false;

            if(iterable){
                impl_ = collection->collectContainerWithAutoCollection<IterableType, sparse_array_type>(
                    path, clk_name, iterable, expected_capacity, default_enabled);
            }else{
                impl_ = collection->collectContainerManually<IterableType, sparse_array_type>(
                    path, clk_name, expected_capacity, default_enabled);
            }
            impl_->autoEnableOnCollect();
        }
    }


    /**
     * \brief constructor
     * \param parent the parent treenode for the collector
     * \param name the name of the collector
     * \param group Group this collector is part of
     * \param index the index within the group
     * \param desc Description of this node
     * \param iterable the iterable object to collect
     * \param expected_capacity The maximum size this item should grow to
     */
    IterableCollector (TreeNode * parent,
                       const std::string & name,
                       const std::string & group,
                       const uint32_t index,
                       const std::string & desc,
                       const IterableType & iterable,
                       const size_type expected_capacity) :
        IterableCollector(parent, name, group, index, desc, &iterable, expected_capacity)
    {}

    /**
     * \brief constructor
     * \param parent the parent treenode for the collector
     * \param name the name of the collector
     * \param desc Description of this node
     * \param iterable Pointer to the iterable object to collect
     * \param expected_capacity The maximum size this item should grow to
     */
    IterableCollector (TreeNode * parent,
                       const std::string & name,
                       const std::string & desc,
                       const IterableType * iterable,
                       const size_type expected_capacity) :
        IterableCollector(parent, name, name, 0, desc, iterable, expected_capacity)
    {}

    /**
     * \brief constructor
     * \param parent the parent treenode for the collector
     * \param name the name of the collector
     * \param desc Description of this node
     * \param iterable the iterable object to collect
     * \param expected_capacity The maximum size this item should grow to
     */
    IterableCollector (TreeNode * parent,
                       const std::string & name,
                       const std::string & desc,
                       const IterableType & iterable,
                       const size_type expected_capacity) :
        IterableCollector(parent, name, name, 0, desc, &iterable, expected_capacity)
    {}

    /**
     * \brief constructor with no description
     * \param parent the parent treenode for the collector
     * \param name the name of the collector
     * \param iterable Pointer to the iterable object to collect
     * \param expected_capacity The maximum size this item should grow to
     */
    IterableCollector (TreeNode * parent,
                       const std::string & name,
                       const IterableType * iterable,
                       const size_type expected_capacity) :
        IterableCollector (parent, name, name + " Iterable Collector",
                           iterable, expected_capacity)
    {
        // Delegated constructor
    }

    /**
     * \brief constructor with no description
     * \param parent the parent treenode for the collector
     * \param name the name of the collector
     * \param iterable the iterable object to collect
     * \param expected_capacity The maximum size this item should grow to
     */
    IterableCollector (TreeNode * parent,
                       const std::string & name,
                       const IterableType & iterable,
                       const size_type expected_capacity) :
        IterableCollector (parent, name, name + " Iterable Collector",
                           &iterable, expected_capacity)
    {
        // Delegated constructor
    }

    /**
     * \brief constructor with no iterable object associated
     * \param parent the parent treenode for the collector
     * \param name the name of the collector
     * \param expected_capacity The maximum size this item should grow to
     */
    IterableCollector (TreeNode * parent,
                       const std::string & name,
                       const size_type expected_capacity) :
        IterableCollector (parent, name, name + " Iterable Collector",
                           nullptr, expected_capacity)
    {
        // Can't auto collect without giving a pointer to the iterable object
        setManualCollection();
    }

    //! Collect the contents of the iterable object.  This function
    //! will walk starting from index 0 -> expected_capacity, clearing
    //! out any records where the iterable object does not contain
    //! data.
    void collect(const IterableType & iterable_object)
    {
        sparta_assert(impl_);
        impl_->collect(iterable_object);
    }

    template <typename T>
    std::enable_if_t<MetaStruct::is_any_pointer_v<T>, void>
    collect(const T& iterable_pointer)
    {
        if (iterable_pointer) {
            collect(*iterable_pointer);
        } else {
            closeRecord();
        }
    }

    //! Collect the contents of the associated iterable object
    void collect() override {
        sparta_assert(impl_);
        impl_->autoCollect();
    }

    //! Force close all records for this iterable type.  This will
    //! close the record immediately and clear the field for the next
    //! cycle
    void closeRecord(const bool & = false) override {
        if (impl_ && impl_->enabled()) {
            impl_->disable();
        }
    }

    //! Schedule event to close all records for this iterable type.
    void scheduleCloseRecord(sparta::Clock::Cycle cycle) {
        if(SPARTA_EXPECT_FALSE(isCollected())) {
            ev_close_record_.preparePayload(false)->schedule(cycle);
        }
    }

    //! \brief Do not perform any automatic collection
    //! The SchedulingPhase is ignored
    void setManualCollection() {
        if (impl_) {
            impl_->enableAutoCollect(false);
        }
    }

    //! \brief Perform a collection, then close the records in the future
    //! \param duration The time to close the records, 0 is not allowed
    void collectWithDuration(sparta::Clock::Cycle duration) {
        if(SPARTA_EXPECT_FALSE(isCollected())) {
            collect();
            if(duration != 0) {
                ev_close_record_.preparePayload(false)->schedule(duration);
            }
        }
    }

    //! Reattach to a new iterable object (used for moves)
    void reattach(const IterableType * obj) {
        if (impl_) {
            impl_->reattach(obj);
        }
    }

private:
    //! Virtual method called by CollectableTreeNode when collection
    //! is enabled on the TreeNode
    void setCollecting_(bool collect, Collector * collector) override
    {
        sparta_assert(impl_);
        PipelineCollector * pipeline_col = dynamic_cast<PipelineCollector *>(collector);
        sparta_assert(pipeline_col != nullptr);

        if(collect && impl_->autoCollecting()) {
            // Add this Collectable to the PipelineCollector's
            // list of objects requiring collection
            pipeline_col->addToAutoCollection(this, collection_phase);
        }
        else {
            closeRecord();
        }
    }

    // For those folks that want a value to automatically
    // disappear in the future
    sparta::EventSet event_set_;
    sparta::PayloadEvent<bool, sparta::SchedulingPhase::Trigger> ev_close_record_;

    // SimDB collection point and entry into the DB pipeline (zlib/sqlite pipeline,
    // nothing to do with "pipeline" in "pipeline collector")
    std::shared_ptr<simdb::collection::ContainerCollector<IterableType, sparse_array_type>> impl_;
};
} // namespace collection
} // namespace sparta
