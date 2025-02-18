// <CollectableTreeNode> -*- C++ -*-


/**
 *\file CollectableTreeNode.hpp
 *
 *
 *\brief define a CollectableTreeNode type TreeNode.
 */

#pragma once

#include "sparta/simulation/TreeNode.hpp"

namespace simdb {
    class DatabaseManager;
}

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

        /*!
         * \brief The pipeline collector will call this method on all nodes
         * as soon as the collector is created.
         * 
         * \return true if we should recurse, false otherwise
         */
        virtual void configCollectable(simdb::CollectionMgr *) = 0;

        /**
         * \brief Method that tells this treenode that is now running
         *        collection.
         * \param collector The collector that is performing the collection
         *
         * This method should instantiate any necessary values for the
         * treenode necessary to collection as well as flip the
         * is_collecting boolean.
         */
        void startCollecting(PipelineCollector* collector, simdb::DatabaseManager* db_mgr) {
            is_collected_ = true;
            setCollecting_(true, collector, db_mgr);
        }

        /**
         * \brief Method that tells this treenode that is now not
         * running collection.
         */
        void stopCollecting(PipelineCollector* collector, simdb::DatabaseManager* db_mgr) {
            setCollecting_(false, collector, db_mgr);
            is_collected_ = false;
        }

        /**
         * \brief Determine whether or not this node has collection
         * turned on or off.
         */
        bool isCollected() const { return is_collected_; }

        //! Pure virtual method used by deriving classes to be
        //! notified when they can perform their collection
        virtual void collect() = 0;

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
        virtual void setCollecting_(bool collect, PipelineCollector*, simdb::DatabaseManager*) { (void)collect; }

    private:

        /**
         * \brief A value that represents whether or not this TreeNode is
         * being collected by a collector
         */
        bool is_collected_ = false;

    };

}
}
