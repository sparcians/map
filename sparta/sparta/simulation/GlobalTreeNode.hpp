// <GlobalTreeNode> -*- C++ -*-

/*!
 * \file GlobalTreeNode.hpp
 * \brief TreeNode refinement representing the global namespace (above "top") of
 * a device tree.
 *
 * This node does not show on locations and all children must be RootTreeNode
 * instances
 */

#ifndef __GLOBAL_TREE_NODE_H__
#define __GLOBAL_TREE_NODE_H__

#include "sparta/simulation/TreeNode.hpp"

namespace sparta
{
    /*!
     * \brief TreeNode which represents some "global" namespace of the device
     * tree, containing only RootTreeNodes, for performing searches.
     *
     * Has special behavior in that it contains all RootTreeNodes as
     * children but is not a parent of any node. This node type has no purpose
     * except to allow findChildren or getChild type queries which include the
     * name of some root tree node (e.g. "top.x.y.z")
     *
     * The global node is not concerned with tree construction phases. Phase
     * queries are meanginless if made through instances of this node.
     *
     * This class cannot be subclassed
     */
    class GlobalTreeNode final : public TreeNode
    {
    public:

        /*!
         * \brief Reserved name for this GlobalTreeNode
         */
        static constexpr char GLOBAL_NODE_NAME[] = "_SPARTA_global_node_";

        /*!
         * \brief Constructor
         */
        GlobalTreeNode() :
            TreeNode(GLOBAL_NODE_NAME, GROUP_NAME_BUILTIN, GROUP_IDX_NONE, "Global space of device tree")
        { }

        /*!
         * \brief Destructor
         */
        virtual ~GlobalTreeNode() {};

        // Returns true. Tree root is always considered "attached"
        virtual bool isAttached() const override final {
            return true;
        }

        // Returns 0. Tree global node will never have a parent
        virtual TreeNode* getParent() override final { return nullptr; }

        // Returns 0. Tree global node will never have a parent
        virtual const TreeNode* getParent() const override final { return nullptr; }

        // Override from TreeNode
        // Virtual global node cannot generate any notifications!
        virtual bool canGenerateNotification_(const std::type_info&,
                                              const std::string*,
                                              const std::string*&) const override final {
            return false;
        }

        /*!
         * \brief Render description of this TreeNode as a string
         * \return String description of the form
         * \verbatim
         * <location (root)>
         * \endverbatim
         */
        virtual std::string stringize(bool pretty=false) const override {
            (void) pretty;
            std::stringstream ss;
            ss << "<" << getName() << ">";
            return ss.str();
        }

    protected:

        // No effect on root
        virtual void createResource_() override {};

        /*!
         * \brief Disallow assining a parent to this node.
         *
         * The root of the tree can have no parent
         */
        virtual void setParent_(TreeNode* parent, bool) override {
            SpartaException ex("This GlobalTreeNode \"");
            ex << "\" cannot be a child of any other node. Someone attempted "
                  "to add it as a child of " << parent->getLocation();
            throw ex;
        }
    };

} // namespace sparta

//! \brief Required in simulator source to define some globals.
#define SPARTA_GLOBAL_TREENODE_BODY                               \
    constexpr char sparta::GlobalTreeNode::GLOBAL_NODE_NAME[];

// __GLOBAL_TREE_NODE_H__
#endif
