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
        iterable_object_(iterable),
        expected_capacity_(expected_capacity),
        event_set_(this),
        ev_close_record_(&event_set_, name + "_pipeline_collectable_close_event",
                         CREATE_SPARTA_HANDLER_WITH_DATA(IterableCollector, closeRecord, bool))
    {
        for (size_type i = 0; i < expected_capacity_; ++i)
        {
            std::stringstream name_str;
            name_str << name << i;
            positions_.emplace_back(new CollectableT(this, name_str.str(), group, i));
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
        // Can't auto collect without setting iterable_object_
        setManualCollection();
    }

    //! Collectable classes must be able to register themselves with
    //! the ArgosCollector and store a ScalarCollector/ContainerCollector.
    void createSimDbEntryPoint(simdb::argos::ArgosCollector* argos_collector) override final
    {
        auto loc = getLocation();
        auto clk_name = notNull(getClock())->getName();
        entry_point_ = argos_collector->createContainerCollector<BinValueT, sparse_array_type>(loc, clk_name, expected_capacity_);
    }

    //! Whether a scalar (Collectable) or container (IterableCollector),
    //! serialize struct-like data structure hierarchies (field name + dtype)
    //! to the database.
    void serializeStructSchema(simdb::DatabaseManager* db_mgr, std::map<std::string, int>& schema_ids_by_dtype_name) override final
    {
        CollectableT::template serializeStructSchema<BinT>(db_mgr, schema_ids_by_dtype_name);
    }

    //! Collect the contents of the iterable object.  This function
    //! will walk starting from index 0 -> expected_capacity, clearing
    //! out any records where the iterable object does not contain
    //! data.
    void collect(const IterableType * iterable_object)
    {
        // If pointer has become nullified, close the records
        if(nullptr == iterable_object) {
            closeRecord();
        }
        else if (SPARTA_EXPECT_FALSE(isCollected()))
        {
            if(SPARTA_EXPECT_FALSE(iterable_object->size() > expected_capacity_))
            {
                if(SPARTA_EXPECT_FALSE(warn_on_size_))
                {
                    sparta::log::MessageSource::getGlobalWarn()
                        << "WARNING! The collected object '"
                        << getLocation() << "' has grown beyond the "
                        << "expected capacity (given at construction) for collection. "
                        << "Expected " << expected_capacity_ << " but grew to "
                        << iterable_object->size()
                        << " This is your first and last warning.";
                    warn_on_size_ = false;
                }
            }
            collectImpl_(iterable_object, std::integral_constant<bool, sparse_array_type>());
        }
    }

    //! Collect the contents of the associated iterable object
    void collect() override {
        collect(iterable_object_);
    }

    //! Force close all records for this iterable type.  This will
    //! close the record immediately and clear the field for the next
    //! cycle
    void closeRecord(const bool & = false) override {
        entry_point_->quiet();
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
        auto_collect_ = false;
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
        iterable_object_ = obj;
    }

private:
    typedef typename std::iterator_traits<typename IterableType::iterator>::value_type BinT;
    typedef MetaStruct::remove_any_pointer_t<BinT> BinValueT;
    typedef Collectable<BinT> CollectableT;

    // Standard walk of iterable types
    void collectImpl_(const IterableType * iterable_object, std::false_type)
    {
        sparta_assert(nullptr != iterable_object,
            "Can't collect iterable_object because it's a nullptr! How did we get here?");
        auto itr = iterable_object->begin();
        auto eitr = iterable_object->end();
        auto size = std::distance(itr, eitr);
        std::vector<std::vector<char>> bin_bytes(size);
        for (uint32_t i = 0; i < size; ++i)
        {
            // TODO cnyce
            //sparta_assert(itr != eitr);
            //auto& bytes = bin_bytes[i];
            //simdb::StreamBuffer buf(bytes);
            //positions_[i]->extractBytes(&buf, *itr);
            //++itr;
            (void)i;
        }

        entry_point_->setContigContainerBinBytes(bin_bytes);
    }

    // Full iteration walk, checking validity of the iterator.  This
    // is used for Pipe and Array where the iterator points to valid
    // and not valid entries in the component
    void collectImpl_(const IterableType * iterable_object, std::true_type)
    {
        sparta_assert(nullptr != iterable_object,
            "Can't collect iterable_object because it's a nullptr! How did we get here?");
        auto itr = iterable_object->begin();
        std::map<uint16_t, std::vector<char>> bin_bytes;
        for (uint32_t i = 0; i < expected_capacity_; ++i)
        {
            sparta_assert(i <= UINT16_MAX);
            sparta_assert(itr != iterable_object->end());
            if (itr.isValid()) {
                // TODO cnyce
                //auto& bytes = bin_bytes[(uint16_t)i];
                //simdb::StreamBuffer buf(bytes);
                //positions_[i]->extractBytes(&buf, *itr);
            }
            ++itr;
        }

        entry_point_->setSparseContainerBinBytes(bin_bytes);
    }

    //! Virtual method called by CollectableTreeNode when collection
    //! is enabled on the TreeNode
    void setCollecting_(bool collect, Collector * collector) override
    {
        PipelineCollector * pipeline_col = dynamic_cast<PipelineCollector *>(collector);
        sparta_assert(pipeline_col != nullptr);

        if(collect && auto_collect_) {
            // Add this Collectable to the PipelineCollector's
            // list of objects requiring collection
            pipeline_col->addToAutoCollection(this, collection_phase);
        }
        else {
            closeRecord();
        }
    }

    const IterableType * iterable_object_;
    std::vector<std::unique_ptr<CollectableT>> positions_;
    const size_type expected_capacity_ = 0;
    bool auto_collect_ = true;
    bool warn_on_size_ = true;

    // Entry point into the SimDB collection system.
    simdb::argos::CollectionEntryPoint* entry_point_ = nullptr;

    // For those folks that want a value to automatically
    // disappear in the future
    sparta::EventSet event_set_;
    sparta::PayloadEvent<bool, sparta::SchedulingPhase::Trigger> ev_close_record_;

};
} // namespace collection
} // namespace sparta
