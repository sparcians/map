// <VirtualGlobalTreeNode> -*- C++ -*-

#ifndef __VIRTUAL_GLOBAL_TREE_NODE_H__
#define __VIRTUAL_GLOBAL_TREE_NODE_H__

#include <regex>
#include <string>
#include <vector>
#include <memory>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/log/MessageSource.hpp"

/*!
 * \file VirtualGlobalTreeNode.hpp
 * \brief Contains the virtual global tree node which receives propagating
 * messages from every other node
 */

namespace sparta
{
    /*!
     * \brief Virtual global node for all device trees in a single
     * simulation. This node acts a potential notification observation point
     * for every node in the simulation regardless hierarchy.
     * \see getVirtualGlobalNode
     *
     * This node disallows children and cannot generate notifications
     */
    class VirtualGlobalTreeNode final : public TreeNode
    {
    public:

        /*!
         * \brief Constructor
         */
        VirtualGlobalTreeNode() :
            TreeNode(NODE_NAME_VIRTUAL_GLOBAL,
                     "Global pseudo-TreeNode capturing propagating messages from ANY TreeNode in the simulator")
        {
            untrackParentlessNode_(this); // Does not actually have a parent

            // Construct in teardown so that static destruction can kill this
            // and its subtree
            setPhase_(TREE_TEARDOWN);
        }

        /*!
         * \brief Destructor
         */
        ~VirtualGlobalTreeNode() {};

        /*!
         * \brief Gets the virtual global node singleton. This is the same
         * object as TreeNode::getVirtualGlobalNode but is of this specific type
         * instead of a generic TreeNode.
         */
        static VirtualGlobalTreeNode* getInstance();

        // Returns true. This node is always considered "attached"
        virtual bool isAttached() const override final {
            return true;
        }

        // Returns 0. Global node will never have a parent
        virtual TreeNode* getParent() override final { return 0; }

        // Returns 0. Global node will never have a parent
        virtual const TreeNode* getParent() const override final { return 0; }

        // Searches parentless nodes
        virtual TreeNode* getImmediateChildByIdentity_(const std::string& name,
                                                       bool must_exist=true) override final {
            for(auto& n : statics_->parentless_map_){
                sparta::TreeNode::WeakPtr& child_wptr = n.second;
                if(child_wptr.expired() == true){
                    continue;
                }
                std::shared_ptr<sparta::TreeNode> child = child_wptr.lock();
                sparta_assert(child != nullptr, "No null nodes (groups) should ever be added to the parentless_nodes list");
                std::vector<const std::string*> idents = child->getIdentifiers();
                for(const std::string* ident_id : idents){
                    if(*ident_id == name){
                        return child.get();
                    }
                }
            }

            if(false == must_exist){
                return nullptr;
            }

            // Note: parentless nodes will never be groups (see asserts above)

            throw SpartaException("Could not get immediate child named \"")
                << name << "\" in node \"" << getLocation();
        }

        // Overload of getImmediateChildByIdentity_
        //! \note Intended to be overridden by special purpose nodes with virtual children
        virtual const TreeNode* getImmediateChildByIdentity_(const std::string& name,
                                                             bool must_exist=true) const override final {
            for(auto& n : statics_->parentless_map_){
                sparta::TreeNode::WeakPtr& child_wptr = n.second;
                if(child_wptr.expired() == true){
                    continue;
                }
                std::shared_ptr<sparta::TreeNode> child = child_wptr.lock();
                sparta_assert(child != nullptr, "No null nodes (groups) should ever be added to the parentless_nodes list");
                std::vector<const std::string*> idents = child->getIdentifiers();
                for(const std::string* ident_id : idents){
                    if(*ident_id == name){
                        return child.get();
                    }
                }
            }

            if(false == must_exist){
                return nullptr;
            }

            // Note: parentless nodes will never be groups (see asserts above)

            throw SpartaException("Could not get immediate child named \"")
                << name << "\" in node \"" << getLocation();
        }

        // Searches parentless nodes

        // Const variant of findImmediateChildren_
        virtual uint32_t findImmediateChildren_(std::regex& expr,
                                                std::vector<TreeNode*>& found,
                                                std::vector<std::vector<std::string>>& replacements,
                                                bool allow_private) override final {
            uint32_t num_found = 0;
            for(auto& n : statics_->parentless_map_){
                sparta::TreeNode::WeakPtr& child_wptr = n.second;
                if(child_wptr.expired() == true){
                    continue;
                }
                std::shared_ptr<TreeNode> child = child_wptr.lock();
                std::vector<std::string> replaced; // Replacements per child
                if(identityMatchesPattern_(child->getName(), expr, replaced)){
                    if(child != nullptr){ // Ensure child is not null (e.g. grouping)
                        if (allow_private || canSeeChild_(child.get()))
                        {
                            ++num_found;
                            found.push_back(child.get());
                            replacements.push_back(replaced);
                        }
                    }
                }
            }
            return num_found;
        }

        // Const variant of findImmediateChildren_
        virtual uint32_t findImmediateChildren_(std::regex& expr,
                                                std::vector<const TreeNode*>& found,
                                                std::vector<std::vector<std::string>>& replacements,
                                                bool allow_private) const override final {
            uint32_t num_found = 0;
            for(auto& n : statics_->parentless_map_){
                sparta::TreeNode::WeakPtr& child_wptr = n.second;
                if(child_wptr.expired() == true){
                    continue;
                }
                std::shared_ptr<TreeNode> child = child_wptr.lock();
                std::vector<std::string> replaced; // Replacements per child
                if(identityMatchesPattern_(child->getName(), expr, replaced)){
                    if(child != nullptr){ // Ensure child is not null (e.g. grouping)
                        if (allow_private || canSeeChild_(child.get()))
                        {
                            ++num_found;
                            found.push_back(child.get());
                            replacements.push_back(replaced);
                        }
                    }
                }
            }
            return num_found;
        }

    private:

        // Override form TreeNode
        // Disallow adding children
        virtual void onAddingChild_(TreeNode*) override final {
            // Should disallow adding childre, but global loggers must be allowe to be attached
            //throw SpartaException("Cannot add a child to the virtual global TreeNode unless it "
            //                    "is one of its builtin message sources");
        }

        // Override from TreeNode
        // Broadcast to all nodes except self (excluded from all_nodes_ list)
        virtual void broadcastRegistrationForNotificationToChildren_(const std::type_info& tinfo,
                                                                     const std::vector<const std::string*>& name_ids,
                                                                     TreeNode* obs_node,
                                                                     const delegate* del,
                                                                     const bool private_only) override final {
            (void)private_only;
            for(auto& n : statics_->parentless_map_){
                sparta::TreeNode::WeakPtr& child = n.second;
                if(child.expired() == false){
                    child.lock()->broadcastRegistrationForNotificationToChildren_(tinfo, name_ids, obs_node, del, private_only);
                }
            }
        }

        // Override from TreeNode
        // Broadcast to all nodes except self (excluded from all_nodes_ list)
        virtual void broadcastDeregistrationForNotificationToChildren_(const std::type_info& tinfo,
                                                                       const std::vector<const std::string*>& name_ids,
                                                                       TreeNode* obs_node,
                                                                       const delegate* del,
                                                                       const bool private_only) override final {
            (void)private_only;
            for(auto& n : statics_->parentless_map_){
                sparta::TreeNode::WeakPtr& child = n.second;
                if(child.expired() == false){
                    child.lock()->broadcastDeregistrationForNotificationToChildren_(tinfo, name_ids, obs_node, del, private_only);
                }
            }
        }

        // Override from TreeNode
        // Virtual global node cannot generate any notifications!
        virtual bool canGenerateNotification_(const std::type_info&,
                                              const std::string*,
                                              const std::string*&) const override final {
            return false;
        }
    };

} // namespace sparta

// __VIRTUAL_GLOBAL_TREE_NODE_H__
#endif
