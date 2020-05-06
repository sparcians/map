// <ResourceTreeNode> -*- C++ -*-

#pragma once

#include <iostream>
#include <string>

#include "sparta/functional/ArchData.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/log/MessageSource.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/ResourceFactory.hpp"

namespace sparta
{

    /*!
     * \brief TreeNode subclass representing a node in the device tree
     *        which contains a single ResourceFactory and a
     *        sparta::ParameterSet. A ResourceTreeNode is also
     *        associated with a clock.
     *
     * Upong entering the TREE_FINALIZED phase, this node will also
     * contain a resource constructed based on the specified
     * parameters.
     */
    class ResourceTreeNode : public TreeNode
    {
    public:

        /*!
         * \brief Size of an ArchData line for ResourceTreeNode
         *        (bytes) ArchData for ResourceTreeNode is a catch-all
         *        space for miscellaneous children that store data but
         *        are not registers.  Increase this value if larger
         *        children must be supported.
         */
        static const ArchData::offset_type ARCH_DATA_LINE_SIZE = 256;

        /*!
         * \brief Construct a new TreeNode attached to a parent.
         * \brief parent Parent TreeNode to which this node will be
         *               immediately attached after validation. If
         *               nullptr, child will not be attached.
         * \param name Name of node. Must contain only alphanumeric characters
         *                           and underscores
         * \param desc Description of node.
         * \param res_fact Resource factory which will generate a
         *                 parameter set and a resource. This is a
         *                 borrowed pointer and must not be
         *                 deleted. Caller is responsible for ensuring
         *                 that this argument is valid until until
         *                 after this TreeNode is destructed. Must not
         *                 be null.
         * \note ResourceTreeNode intentionally prohibits access to the the
         *       ResourceFactory it contains
         *
         * \todo desc should come from the resource factory as a
         * common const char*.
         */
        ResourceTreeNode(TreeNode* parent,
                         const std::string & name,
                         const std::string & group,
                         group_idx_type group_idx,
                         const std::string & desc,
                         ResourceFactoryBase* res_fact) :
            TreeNode(name, group, group_idx, desc),
            created_resource_(false),
            res_fact_(res_fact),
            params_(0),
            adata_(this, ARCH_DATA_LINE_SIZE)
        {
            setExpectedParent_(parent);

            if(nullptr == res_fact_){
                throw SpartaException("Resource factory for node \"")
                    << getLocation() << " cannot be null";
            }

            initConfigurables_();

            if(parent != nullptr){
                parent->addChild(this); // Do not inherit parent state
            }
        }

        //! \brief Alternate Constructor
        ResourceTreeNode(const std::string & name,
                         const std::string & group,
                         group_idx_type group_idx,
                         const std::string & desc,
                         ResourceFactoryBase* res_fact) :
            ResourceTreeNode(nullptr, name, group, group_idx, desc, res_fact)
        {
            // Initialization delegated to other constructor
        }

        //! \brief Alternate Constructor
        ResourceTreeNode(TreeNode* parent,
                         const std::string & name,
                         const std::string & desc,
                         ResourceFactoryBase* res_fact) :
            ResourceTreeNode(parent,
                             name,
                             TreeNode::GROUP_NAME_NONE,
                             TreeNode::GROUP_IDX_NONE,
                             desc,
                             res_fact)
        {
            // Initialization delegated to other constructor
        }

        //! \brief Alternate Constructor
        ResourceTreeNode(const std::string & name,
                         const std::string & desc,
                         ResourceFactoryBase* res_fact) :
            ResourceTreeNode(name,
                             TreeNode::GROUP_NAME_NONE,
                             TreeNode::GROUP_IDX_NONE,
                             desc,
                             res_fact)
        {
            // Initialization delegated to other constructor
        }

        //! Virtual Destructor
        virtual ~ResourceTreeNode() {
            if(res_fact_ != nullptr){
                if(params_ != nullptr){
                    res_fact_->deleteParameters(params_);
                    params_ = 0;
                }

                if(getResource_() != nullptr){
                    res_fact_->deleteResource(getResource_());
                }

                res_fact_->deleteSubtree(this);
            }
        }

        /*!
         * \brief Gets the ParameterSet associated with this ResourceTreeNode
         * \note Result is a borrowed pointer valid for as long as this TreeNode
         */
        ParameterSet* getParameterSet() {
            sparta_assert(params_ != 0);
            return params_;
        }

        /*!
         * \brief Gets the Parameter set assoiciated with this node
         */
        const ParameterSet* getParameterSet() const {
            sparta_assert(params_ != 0);
            return params_;
        }

        /*!
         * \brief Gets the typename of the resource that this node will contain
         * once finalized, demangled
         * \return non-empty string.
         */
        virtual std::string getResourceType() const override {
            return res_fact_->getResourceType();
        }

        /*!
         * \brief Gets the typename of the resource that this node will contain
         *        once finalized.
         * \return non-empty string.
         */
        virtual std::string getResourceTypeRaw() const override {
            return res_fact_->getResourceTypeRaw();
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
            ss << "<" << getLocation() << " resource: \""
               << res_fact_->getResourceType() << "\">";
            return ss.str();
        }

        /*!
         * \brief Finalize this node and construct its resource
         * \see createResource_
         */
        void finalize() {
            if(getResource_() != nullptr){
                throw SpartaException("Cannot re-finalize this ResourceTreeNode: ")
                    << getLocation() << " because it already has a resource. ";
            }

            createResource_();
        }


        Resource* getResourceNow() noexcept {
            return getResource_();
        }

    protected:

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
         * \post Contained ArchData will be laid out.
         * \post Resource will be instantiated
         *
         * Validates parameters and throws exception if any fail.
         * Creates resource if all preconditions met and all parameters are
         * valid.
         */
        virtual void createResource_() override
        {
            if(getPhase() != TREE_FINALIZING){
                throw SpartaException("Tried to create resource on ")
                    << getLocation() << " but tree was not in TREE_FINALIZING phase";
            }

            if(getResource_() != nullptr){
                // Already has a resource. Nothing left to do
                sparta_assert(created_resource_,
                            "Resource was set in ResourceTreeNode " << getLocation()
                            << " but not by the ResourceTreeNode itself");
                //! \todo Move this check to the actual setting of the resources. ResourceTreeNode
                //! should hook into ResourceContainer's method to prevent setting of a resource externally
                return;
            }

            // Ensure that this node has a parent OR children.
            // Note that this:
            //    A) prohibits 1-node device trees. A 'top' node should exist anyway
            //    B) Cannot check for orphaned trees (yet)
            if(!isAttached()){
                throw SpartaException("Cannot create resource for TreeNode \"")
                    << getName() << "\"@" << (void*)this << " because it is not attached to a tree with a RootTreeNode";
            }

            // Check for valid configured values
            if(getResource_() != 0){
                throw SpartaException("Resource already created for TreeNode \"")
                    << getLocation() << "\". Cannot create again";
            }

            if(getClock() == nullptr){
                throw SpartaException("No clock associated with TreeNode ")
                    << getLocation() << " and no ancestor has an associated clock. All ResourceTreeNodes "
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

            // Note, parameters have been reset to 0 read-counts when the
            // parameter set was constructed

            Resource* r = res_fact_->createResource(this, params_);
            sparta_assert(r != nullptr); // Resource created must not be NULL

            if(getResource_() == nullptr){
                throw SpartaException("ResourceTreeNode ") << getLocation()
                    << " created a resource of type " << res_fact_->getResourceType()
                    << " but that resource did not register itself with this node. Ensure that "
                       "this resource class uses the proper sparta::Resource base-class constructor "
                       "which takes a ResourceContainer";
            }
            if(getResource_() != r){
                throw SpartaException("ResourceTreeNode ") << getLocation()
                    << " created a resource of type " << res_fact_->getResourceType()
                    << " but that resource was different than the resource registered with this "
                       "node.";
            }

            lockResource_(); // No changing resource until teardown

            created_resource_ = true; // Created by this ResourceTreeNode

            // Note, do not check all parameters right here because children may
            // be reading them. Instead, defer this to a tree-validation stage
            // Ensure each Parameter was read at least once by the consuming
            // component

            // Layout the contained ArchData
            adata_.layout();
        }

    private:

        // Prevent addition to the tree after the TREE_BUILDING phase
        void onSettingParent_(const TreeNode* parent) const override {
            (void) parent;
            if(isBuilt()){
                throw SpartaException("Cannot add ResourceTreeNode  \"")
                    << getName() << "\" as child of device tree node \""
                    << getLocation() << "\". This tree has exited the TREE_BUILDING "
                    << "phase and ResourceTreeNodes can no longer be added.";
            }
        }

        /*!
         * \brief When the framework is entering the configuration phase
         */
        void onConfiguring_() override {
            // Create subtree, which may want to look at parameters
            res_fact_->createSubtree(this);
        }

        /*!
         * \brief Called for each node during bindTreeEarly_ recursion
         */
        void onBindTreeEarly_() override {
            // This doesn't work due to a bug in Port's auto
            // precedence registration in SPARTA.  If the resource
            // factory does the binding early and connects ports
            // together, then when a sparta::Unit's onBindTreeEarly_
            // function is called, the ports are already bound and an
            // assertion will fire.  The fix should be a separate
            // state that allows the Unit to register events on ports
            // before ANY binding occurs
            //
            //res_fact_->bindEarly(this);
        }

        /*!
         * \brief Called for each node during bindTreeLate_ recursion
         */
        void onBindTreeLate_() override {
            res_fact_->bindLate(this);
        }

        // Validate the tree
        void validateNode_() const override {
            params_->verifyAllRead(); // Throws if not all read at least once
        }

        /*!
         * \brief Initializes configurable elements in this ResourceTreeNode
         * including parameters. Also allows ResourceFactory to create
         * subtree nodes through ResourceFactory::createSubtree
         * \post Resets read counters
         *
         * To be invoked by the ResourceTreeNode constructor(s).
         */
        void initConfigurables_() {
            // Create parameters first
            params_ = res_fact_->createParameters(this);
            sparta_assert(params_ != 0); // Params must not be NULL
            params_->resetReadCounts(); // Reset read-counts to 0

            std::string res_type = res_fact_->getResourceType();
            if(res_type == ""){
                throw SpartaException("resource type for ResourceFactory associated with ")
                    << getLocation() << " must not be empty string";
            }
        }

        bool created_resource_; //!< Did this factory actually create a resource. Used to catch other
        ResourceFactoryBase* const res_fact_; //!< ResourceFactory used to construct resources for this node
        ParameterSet* params_; //!< This node's parameters

        /*!
         * \brief Data space for this ResourceTreeNode because these nodes tend
         * to have children like counters and statistics
         * Immediate children (without IDs) can be placed in here for fast
         * checkpointing. This is accessible through the overloaded getArchData_
         * function.
         */
        ArchData adata_;
    };


} // namespace sparta

