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
        expected_capacity_(expected_capacity)
    {
        sparta_assert(iterable_object_ != nullptr,
                      "IterableCollector " << getLocation() << " cannot be constructed with a nullptr iterable object");

        for (size_type i = 0; i < expected_capacity_; ++i)
        {
            std::stringstream name_str;
            name_str << name << i;
            positions_.emplace_back(new IterableCollectorBin(this, name_str.str(), group, i));
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

    //! Collect the contents of the associated iterable object
    void collect() override {
        if(SPARTA_EXPECT_TRUE(isCollected())) {
            simdb_collectable_->activate(iterable_object_);
        }
    }

    //! Reattach to a new iterable object (used for moves)
    void reattach(const IterableType * obj) {
        iterable_object_ = obj;
    }

    /*!
     * \brief The pipeline collector will call this method on all nodes
     * as soon as the collector is created.
     */
    void configCollectable(simdb::CollectionMgr *mgr) override final {
        simdb_collectable_ = mgr->createIterableCollector<IterableType, sparse_array_type>(
            getLocation(),
            getClock()->getName(),
            expected_capacity_);
    }

private:
    class IterableCollectorBin : public CollectableTreeNode
    {
    public:
        IterableCollectorBin(TreeNode* parent,
                            const std::string& name,
                            const std::string& group,
                            const uint32_t bin_idx)
            : CollectableTreeNode(parent, name, group, bin_idx, "IterableCollectorBin <no desc>")
        {}

        void configCollectable(simdb::CollectionMgr *) override final
        {
            // Nothing to do here
        }

        void collect() override final
        {
            // Nothing to do here
        }
    };

    //! Virtual method called by CollectableTreeNode when collection
    //! is enabled on the TreeNode
    void setCollecting_(bool collect, PipelineCollector* collector, simdb::DatabaseManager* db_mgr) override {
        if (collect) {
            // Add this Collectable to the PipelineCollector's
            // list of objects requiring collection
            collector->addToAutoCollection(this, collection_phase);
        } else {
            // If we are no longer collecting, remove this Collectable from the
            // once-a-cycle sweep() method.
            //
            // Note that removeFromAutoCollection() implicitly calls removeFromAutoSweep().
            collector->removeFromAutoSweep(this);
            closeRecord();
        }
    }

    const IterableType * iterable_object_;
    std::vector<std::unique_ptr<IterableCollectorBin>> positions_;
    const size_type expected_capacity_ = 0;

    using simdb_collectable_type = std::conditional_t<sparse_array_type,
                                                      simdb::SparseIterableCollectionPoint,
                                                      simdb::ContigIterableCollectionPoint>;
                                          
    std::shared_ptr<simdb_collectable_type> simdb_collectable_;
};

} // namespace collection
} // namespace sparta
