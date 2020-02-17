// <CollectableTreeNode> -*- C++ -*-


/**
 *\file CollectableTreeNode.hpp
 *
 *
 *\brief define a CollectableTreeNode type TreeNode.
 */

#ifndef __SPARTA_COLLECTABLE_TREE_NODE_H__
#define __SPARTA_COLLECTABLE_TREE_NODE_H__

#include <string>
#include <inttypes.h>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/collection/Collector.hpp"

namespace sparta{
namespace collection
{

    /**
     * \class CollectableTreeNode
     * \brief An abstract type of TreeNode that
     * has virtual calls to  start collection on this node,
     * and stop collection on this node.
     */
    class CollectableTreeNode : public sparta::TreeNode
    {
    public:
        /**
         * \brief Construct.
         * \param parent a pointer to the parent treenode
         * \param name the name of this treenode
         * \param group the name of the group for this treenode
         * \param index the index within the group
         * \param desc A description for this treenode.
         */
        CollectableTreeNode(sparta::TreeNode* parent, const std::string& name,
                            const std::string& group, uint32_t index,
                            const std::string& desc = "CollectableTreeNode <no desc>") :
            sparta::TreeNode(parent, name, group, index, desc)
        {
            markHidden(); // Mark self as hidden from the default
                          // printouts (to reduce clutter)
        }

        /**
         * \brief Construct.
         * \param parent a pointer to the parent treenode
         * \param name the name of this treenode
         * \param desc Description of this CollectableTreeNode
         */
        CollectableTreeNode(sparta::TreeNode* parent, const std::string& name,
                            const std::string& desc= "CollectableTreeNode <no desc>") :
            CollectableTreeNode(parent, name, sparta::TreeNode::GROUP_NAME_NONE,
                                sparta::TreeNode::GROUP_IDX_NONE, desc)
        {}

        //!Virtual destructor
        virtual ~CollectableTreeNode()
        {}

        /**
         * \brief Method that tells this treenode that is now running
         *        collection.
         * \param collector The collector that is performing the collection
         *
         * This method should instantiate any necessary values for the
         * treenode necessary to collection as well as flip the
         * is_collecting boolean.
         */
        void startCollecting(Collector * collector) {
            is_collected_ = true;
            setCollecting_(true, collector);
        }

        /**
         * \brief Method that tells this treenode that is now not
         * running collection.
         */
        void stopCollecting(Collector * collector)
        {
            setCollecting_(false, collector);
            is_collected_ = false;
        }

        /**
         * \brief Determine whether or not this node has collection
         * turned on or off.
         */
        bool isCollected() { return is_collected_; }

        //! Pure virtual method used by deriving classes to be
        //! notified when they can perform their collection
        virtual void collect() = 0;

        //! Pure virtual method used by deriving classes to re-start a
        //! record if necessary.  This is mostly used by
        //! PipelineCollector where it will insert a heartbeat index
        //! by closing all records and opening them again.
        virtual void restartRecord() {}

        //! Pure virtual method used by deriving classes to force
        //! close a record.  This is useful for simulation end where
        //! each collectable is allowed its final say
        //! \param simulation_ending If true, the simulation is
        //!        terminating and the "end cycle" is not really the
        //!        true end.  Implementers of this method should a +1
        //!        to their end cycle in their records to ensure it
        //!        does not get closed out.
        virtual void closeRecord(const bool & simulation_ending = false) { (void) simulation_ending; }

    protected:

        /**
         * \brief Indicate to sub-classes that collection has flipped
         * \param collect true if collection is enabled; false otherwise
         */
        virtual void setCollecting_(bool collect, Collector *) { (void) collect; }

    private:

        /**
         * \brief A value that represents whether or not this TreeNode is
         * being collected by a collector
         */
        bool is_collected_ = false;

    };

}
}

 //__COLLECTABLE_TREE_NODE_H__
 #endif
