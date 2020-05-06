// <TreeNode> -*- C++ -*-


#pragma once

#include "sparta/simulation/TreeNode.hpp"

namespace sparta {

    /**
     * Follows the attorney client pattern to expose accessors to get at all children
     * in the tree reguardless of methods related to visibility with the private subtrees.
     *
     * This is mainly to be used by internal sparta things like Reports, Pipeout generation,
     * etc that needs to see the entire device tree for processing.
     *
     * It should NOT be used by simulation developers.
     */
    class TreeNodePrivateAttorney
    {
    public:
        /**
         * Access to a list of all of the nodes children both public and private.
         */
        static const TreeNode::ChildrenVector& getAllChildren(const TreeNode& node)
        {
            return node.getAllChildren_();
        }

        /**
         * Access to a list of all of the nodes children both public and private.
         */
        static const TreeNode::ChildrenVector& getAllChildren(const TreeNode* node)
        {
            sparta_assert(node);
            return getAllChildren(*node);
        }

        /**
         * Grab a child by name, this will return the child even if it is private.
         * Throws an exception if must_exist = true and the child was not found.
         * If must_exist = false, this will return a nullptr to denote not found.
         */
        static TreeNode* getChild(TreeNode* node, const std::string& path,
                                  const bool must_exist = false)
        {
            return node->getChild_(path, must_exist, true /* private also */);
        }

        static uint32_t findChildren(TreeNode* node, const std::string& pattern,
                                     std::vector<TreeNode*>& results)
        {
            return node->findChildren_(pattern, results, true /* allow private */);
        }

        /**
         * const qualified version.
         */
        static bool hasChild(const TreeNode* node, const std::string& path)
        {
            return node->hasChild_(path, true /* private also */);
        }

        template <typename DataT, typename T, void (T::*TMethod)(const DataT&)>
        static void registerForNotification(TreeNode* node, T* obj,
                                            const std::string& name, bool ensure_possible=true)
        {
            sparta_assert(node != nullptr);
            node->registerForNotification_<DataT, T, TMethod>(obj, name,
                                                              ensure_possible,
                                                              true /*allow_private*/);
        }
        template <typename DataT, typename T, void (T::*TMethod)(const TreeNode&, const TreeNode&, const DataT&)>
        static void registerForNotification(TreeNode* node, T* obj,
                                            const std::string& name,
                                            bool ensure_possible=true)
        {
            sparta_assert(node != nullptr);
            node->registerForNotification_<DataT, T, TMethod>(obj, name,
                                                              ensure_possible,
                                                              true /*allow_private*/);
        }

        template <typename DataT, typename T, void (T::*TMethod)(const DataT&)>
        static void deregisterForNotification(TreeNode* node,
                                              T* obj, const std::string& name)
        {
            sparta_assert(node != nullptr);
            node->deregisterForNotification_<DataT, T, TMethod>(obj,
                                                                name,
                                                                true /* allow_private */);
        }

        template <typename DataT, typename T, void (T::*TMethod)(const TreeNode&, const TreeNode&, const DataT&)>
        static void deregisterForNotification(TreeNode* node,
                                              T* obj, const std::string& name)
        {
            sparta_assert(node != nullptr);
            node->deregisterForNotification_<DataT, T, TMethod>(obj,
                                                                name,
                                                                true /* allow_private */);
        }
    };

} // namespace sparta

