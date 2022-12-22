// <DynamicResourceTreeNode> -*- C++ -*-

#pragma once

#include <iostream>
#include <string>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/log/MessageSource.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/ResourceFactory.hpp"
#include "sparta/functional/ArchData.hpp"

namespace sparta
{
    /*!
     * \brief TreeNode subclass representing a node in the device tree
     *        which creates and owns a resource. This node is created
     *        at run-time and, although it takes a parameter set at
     *        construction, is not part of the SPARTA configuration
     *        phase and is not user configurable through any
     *        sparta::ParameterSet.
     *
     * \tparam ResourceT Resource class. See sparta::ResourceFactory template
     *         arguments
     * \tparam ParamsT Parameters class for that Resource. See
     *                 sparta::ResourceFactory template arguments.
     *
     * This node is not externally associated with a ResourceFactory
     * and does not own or publish its own ParameterSet like a regular
     * ResourceTreeNode
     *
     * Upong successful construction, this node will NOT contain a
     * resource, but the owner can immediately invoke a finalize()
     * method in this object to create it's resource based on the
     * ParameterSet specified at construction. If the owner does not
     * invoke finalize, this node will be finalized along with the
     * rest of the tree.
     *
     * This node should be constructed within a Resource constructor,
     * which is always invoked during finalization.
     *
     * \see DynamicResourceTreeNode::finalize
     */
    template <typename ResourceT, typename ParamsT>
    class DynamicResourceTreeNode : public TreeNode
    {
    public:

        /*!
         * \brief Size of an ArchData line for ResourceTreeNode (bytes)
         * ArchData for ResourceTreeNode is a catch-all space for miscellaneous
         * children that store data but are not registers.
         * Increase this value if larger children must be supported.
         */
        static const ArchData::offset_type ARCH_DATA_LINE_SIZE = 256;

        /*!
         * \brief Dynamic, Non-factory constructor. Useful when no predefined
         *        factory object is necessary. Creates a local factory.
         * \param parent Parent treenode. Must not be nullptr

         * \param params ParameterSet to pass to resource at
         *               construction. Read counts are not reset and
         *               may be incremented. Params are re-validated
         *               at finalization in case they have not yet
         *               been used. Caller owns \a params and they
         *               must exist as long as this object.
         * \post Resource in this node is immediately available.
         * \note This resource will only be configurable through the
         *       \a params argument passed in at construction. It will
         *       not be visible to the Sparta onfiguration phase.
         */
        DynamicResourceTreeNode(TreeNode* parent,
                                const std::string & name,
                                const std::string & group,
                                group_idx_type group_idx,
                                const std::string & desc,
                                const ParamsT* params) :
            TreeNode(name, group, group_idx, desc),
            adata_(this, ARCH_DATA_LINE_SIZE),
            params_(params)
        {
            setExpectedParent_(parent);

            if(nullptr == parent){
                throw SpartaException("Cannot create a DynamicResourceTreeNode without a non-null parent. Error at: ")
                    << getLocation();
            }

            // Ensure that this node has a parent that is attached (because this node isn't actually attached yet)
            if(false == parent->isAttached()){
                throw SpartaException("Cannot create resource for TreeNode \"")
                    << getName() << "\"@" << (void*)this << " because it is not attached to a tree with a RootTreeNode";
            }

            if(nullptr == params){
                throw SpartaException("Params for DynamicResourceTreeNode \"")
                    << getLocation() << " cannot be null";
            }

            if(getClock() == nullptr && parent->getClock() == nullptr){
                throw SpartaException("No clock associated with TreeNode ")
                    << getLocation() << " and no ancestor has an associated clock. A DynamicResourceTreeNode "
                    << "must have at least one clock associated with a node in their ancestry";
            }

            // Layout the contained ArchData
            adata_.layout();
            if(parent != nullptr){
                parent->addChild(this);
            }
        }

        //! \brief Alternate constructor
        DynamicResourceTreeNode(TreeNode* parent,
                                const std::string & name,
                                const std::string & desc,
                                const ParamsT* params) :
            DynamicResourceTreeNode(parent,
                                    name,
                                    TreeNode::GROUP_NAME_NONE,
                                    TreeNode::GROUP_IDX_NONE,
                                    desc,
                                    params)
        { }

        //! Virtual Destructor
        virtual ~DynamicResourceTreeNode() {
        }

        /*!
         * \brief Finalize this node and construct its resource
         * \see createResource_
         */
        void finalize() {
            if(getResource_() != nullptr){
                throw SpartaException("Cannot re-finalize this DynamicResourceTreeNode: ")
                    << getLocation() << " because it already has a resource";
            }

            createResource_();
        }

        /*!
         * \brief Render description of this ResourceTreeNode as a string
         * \return String description of the form
         * \verbatim
         * <location resource: "resource_type">
         * \endverbatim
         */
        virtual std::string stringize(bool pretty=false) const override {
            (void) pretty;
            std::stringstream ss;
            ss << "<" << getLocation() << " dynamic resource: \"" << demangle(typeid(ResourceT).name()) << "\">";
            return ss.str();
        }

    private:

        /*!
         * \brief After setting parameters, create a resource
         * \todo Check for orphaned trees (no common parent). Defining nodes
         * with "top" semantics would simplify this.
         * \pre Must be attached to a tree (have parent or children). Node
         * cannot be an oprhan, even if intended.
         * \pre Resource must not already have been created for this node.
         * \pre Clock must have been set to a non-NULL Clock through setClock
         * or parent must have a clock. If clock not set, borrows clock from
         * parent. If parent has no clock, will throw SpartaException.
         * \pre Must be in TREE_FINALIZING state.
         * \note Resets the read count on all Parameters in the contained
         * ParameterSet. Instantiation of a resource should read all of these
         * parameters and increment their read counts (getReadCount()).
         * \post Contained ArchData will be laid out.
         * \post Resource will be instantiated
         *
         * \note Does not reset params_ read count
         *
         * Validates parameters and throws exception if any fail.
         * Creates resource if all preconditions met and all parameters are
         * valid.
         */
        virtual void createResource_() override {
            sparta_assert(params_ != nullptr); // Constructor should prevent this
            sparta_assert(getParent() != nullptr); // Constructor should prevent this

            if(getResource_() != nullptr){
                // Already has a resource. Nothing left to do
                return;
            }

            if(getPhase() != TREE_FINALIZING){
                throw SpartaException("Tried to create resource through DynamicResourceTreeNode ")
                    << getLocation() << " but tree was not in TREE_FINALIZING phase";
            }

            if(getClock() == nullptr){
                throw SpartaException("No clock associated with DynamicResourceTreeNode ")
                    << getLocation() << " and no ancestor has an associated clock. All DynamicResourceTreeNodes "
                    << "must have at least one clock associated with a node in their ancestry";
            }

            std::string errs;
            if(!params_->validateIndependently(errs)){
                throw SpartaException("Parameter limits violated:")
                    << errs;
            }

            if(!params_->validateDependencies(this, errs)){
                throw SpartaException("Parameter validation callbacks indicated invalid parameters: ")
                    << errs;
            }

            res_.reset(new ResourceT(this, params_));
            sparta_assert(res_ != nullptr); // Resource created must not be null
            if(getResource_() == nullptr){
                throw SpartaException("DynamicResourceTreeNode ") << getLocation()
                    << " created a resource of type " << demangle(typeid(ResourceT).name())
                    << " but that resource did not register itself with this node. Ensure that "
                       "this resource class uses the proper sparta::Resource base-class constructor "
                       "which takes a ResourceContainer";
            }
            if(getResource_() != res_.get()){
                throw SpartaException("DynamicResourceTreeNode ") << getLocation()
                    << " created a resource of type " << demangle(typeid(ResourceT).name())
                    << " but that resource was different than the resource registered with this "
                       "node.";
            }
            lockResource_();
        }

        // Prevent addition to the tree after the TREE_BUILDING phase
        virtual void onSettingParent_(const TreeNode* parent) const override {
            (void) parent;
            if(isBuilt()){
                throw SpartaException("Cannot add DynamicResourceTreeNode  \"")
                    << getName() << "\" as child of device tree node \""
                    << getLocation() << "\". This tree has exited the TREE_BUILDING "
                    << "phase and ResourceTreeNodes can no longer be added.";
            }
        }

        std::unique_ptr<ResourceT> res_; //!< Resource owned by this Node

        /*!
         * \brief Data space for this ResourceTreeNode because these nodes tend
         * to have children like counters and statistics
         * Immediate children (without IDs) can be placed in here for fast
         * checkpointing. This is accessible through the overloaded getAssociatedArchDatas
         * function.
         */
        ArchData adata_;

        /*!
         * \brief Parameter set with which this object should be created.
         */
        const ParamsT* params_;
    };


} // namespace sparta
