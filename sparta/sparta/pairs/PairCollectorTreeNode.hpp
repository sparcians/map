#pragma once


#include "sparta/simulation/TreeNode.hpp"

namespace sparta{

    class PairCollectorTreeNode : public sparta::TreeNode
    {
    protected:
        /**
         * \brief a description string for collectable TreeNodes.
         */
        static constexpr char COLLECTABLE_DESCRIPTION[] = "TreeNode to represent a pipeline collection location in the tree.\n Any transactions that occured at this location are recorded to file if collection in turned on";

    public:
        /**
         * \brief Construct.
         * \param parent a pointer to the parent treenode
         * \param name the name of this treenode
         * \param group the name of the group for this treenode
         * \param index the index within the group
         * \param desc A description for this treenode.
         */
        PairCollectorTreeNode(sparta::TreeNode* parent, const std::string& name, const std::string& group, uint32_t index, const std::string& desc =COLLECTABLE_DESCRIPTION) :
            sparta::TreeNode(parent, name, group, index, desc)
        {
            markHidden(); // Mark self as hidden from the default printouts
                          // (to reduce clutter)
        }
        /**
         * \brief Construct.
         * \param parent a pointer to the parent treenode
         * \param name the name of this treenode
         */
        PairCollectorTreeNode(sparta::TreeNode* parent, const std::string& name, const std::string& desc=COLLECTABLE_DESCRIPTION) :
            PairCollectorTreeNode(parent, name, sparta::TreeNode::GROUP_NAME_NONE, sparta::TreeNode::GROUP_IDX_NONE, desc)
        {}

        virtual ~PairCollectorTreeNode() {}

    };


} // namespace sparta

