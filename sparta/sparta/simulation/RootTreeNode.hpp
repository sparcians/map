// <RootTreeNode> -*- C++ -*-

/*!
 * \file RootTreeNode.hpp
 * \brief TreeNode refinement representing the root (or "top") of a device tree
 */

#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/TreeNodeExtensions.hpp"
#include "sparta/simulation/GlobalTreeNode.hpp"
#include "sparta/app/ConfigApplicators.hpp"
#include "sparta/log/NotificationSource.hpp"
#include "sparta/simulation/TreeNodePrivateAttorney.hpp"
#include "sparta/simulation/ParameterTree.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/kernel/PhasedObject.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaException.hpp"

namespace sparta
{
    class ArchData;
    class Clock;

    namespace app {
        class Simulation;
    }

    namespace python {
        class PythonInterpreter;
    }

    /*!
     * \brief TreeNode which represents the root ("top") of a device tree.
     *
     * Has special behavior in that it is always attached. It provides an
     * interface for updating tree phases as well.
     *
     * Multiple roots cannot exist in the same tree since they can have no
     * parent. Therefore, it is safe for different tree to be in
     * different phases of construction. Multiple roots can, however, share
     * the same global search scope. See getSearchScope
     *
     * This class cannot be subclassed
     */
    class RootTreeNode : public TreeNode
    {
    public:

        /*!
         * \brief Type for notification source in this node which is posted when
         * a new descendent is attached.
         */
        typedef NotificationSource<TreeNode> NewDescendantNotiSrc;

        /*!
         * \brief Constructor
         * \param name Name of this node [default="top"]
         * \param desc Description of this node
         * \param sim Simulator owning this tree
         * \param search_scope This node can be shared between multiple roots
         * and allows searches to be performed on it that include this root node
         * and others in the search pattern (e.g. "top.x.y.z") by using the
         * sparta::TreeNode::getChild* and findChildren methods on the
         * search_scope node. If nullptr, the RootTreeNode will allocate its own
         * search node containing only this root, and delete it at destruction.
         * Generally, the global node can be shared but for testing it is
         * convenient to have a null global node.
         * \note search_scope will not be a parent of this instance, but this
         * instance will be a child of search_scope.
         */
        RootTreeNode(const std::string & name,
                     const std::string & desc,
                     app::Simulation* sim,
                     GlobalTreeNode* search_scope) :
            TreeNode(name, GROUP_NAME_NONE, GROUP_IDX_NONE, desc),
            sim_(sim),
            new_node_noti_(this,
                           "descendant_attached",
                           "Notification immediately after a node becomes a descendant of this "
                           "root at any distance. This new node may have children already attached "
                           "which will not receive their own descendant_attached notification",
                           "descendant_attached")
        {
            if(search_scope != nullptr){
                search_node_ = search_scope;
            }else{
                search_node_ = new GlobalTreeNode();
                alloc_search_node_.reset(search_node_);
            }
            search_node_->addChild(this);

            /* Define the default scope that all tree nodes will be in. This is
             * to ensure that getScopeRoot() never returns nullptr event when no
             * scope is explicitly defined. */
            setScopeRoot();
        }

        /*!
         * \brief Constructor with name, desc, and search scope
         */
        RootTreeNode(const std::string & name,
                     const std::string & desc,
                     GlobalTreeNode* search_scope) :
            RootTreeNode(name, desc, nullptr, search_scope)
        {
            // Delegated construction
        }

         /*!
         * \brief Constructor with name only
         */
        RootTreeNode(const std::string & name) :
            RootTreeNode(name, "Top of device tree", nullptr, nullptr)
        {
            // Delegated construction
        }

        /*!
         * \brief Consturctor with name, desc, and simulator
         */
        RootTreeNode(const std::string & name,
                     const std::string & desc,
                     app::Simulation* sim) :
            RootTreeNode(name, desc, sim, nullptr)
        {
            // Delegated construction
        }

        /*!
         * \brief Constructor with name and desc.
         * \note Uses null search scope and null simulator
         */
        RootTreeNode(const std::string & name,
                     const std::string & desc) :
            RootTreeNode(name, desc, nullptr, nullptr)
        {
            // Delegated construction
        }

        /*!
         * \brief Constructor with only a simulator
         */
        RootTreeNode(app::Simulation* sim) :
            RootTreeNode("top", "Top of device tree", sim, nullptr)
        {
            // Delegated construction
        }

        /*!
         * \brief Constructor with only a search scope
         */
        RootTreeNode(GlobalTreeNode* search_scope) :
            RootTreeNode("top", "Top of device tree", nullptr, search_scope)
        {
            // Delegated construction
        }

        /*!
         * \brief Constructor with only a simulator and search scope
         */
        RootTreeNode(app::Simulation* sim, GlobalTreeNode* search_scope) :
            RootTreeNode("top", "Top of device tree", sim, search_scope)
        {
            // Delegated construction
        }

        /*!
         * \brief Default constuctor
         */
        RootTreeNode() :
            RootTreeNode("top", "Top of device tree", nullptr, nullptr)
        {
            // Delegated construction
        }

        /*!
         * \brief Destructor
         *
         * Must not place the Tree into teardown mode. Tree content could have
         * been removed before this and walking those deleted nodes here would
         * be an error.
         */
        virtual ~RootTreeNode() {
            // Inform Search Scope node since it will not be notified by
            // ~TreeNode, which does not recognize the search scope as a parent
            if(search_node_){
                removeFromParentForTeardown_(search_node_);
            }

            // No need to alert children, because ~TreeNode will do this next
        }

        // Returns true. Tree root is always considered "attached"
        virtual bool isAttached() const override final {
            return true;
        }

        // Returns 0. Tree root will never have a parent
        virtual TreeNode* getParent() override final { return nullptr; }

        // Returns 0. Tree root will never have a parent
        virtual const TreeNode* getParent() const override final { return nullptr; }

        // Override TreeNode::setClock() so we can pass that clock to the search scope
        virtual void setClock(const Clock * clk) override final {
            if (search_node_) {
                search_node_->setClock(clk);
            }
            TreeNode::setClock(clk);
        }

        /*!
         * \brief Gets the simulator (if any) associated with this Root node
         */
        app::Simulation* getSimulator() const {
            return sim_;
        }

        /*!
         * \brief Gets the search node "parent" of this root node which is
         * suitable for performing searches that include the name of this root
         * node (e.g. "top.x.y.z").
         * \return Global search node. Guaranteed not to be nullptr.
         */
        GlobalTreeNode* getSearchScope(){
            sparta_assert(search_node_);
            return search_node_;
        }

        /*!
         * \brief Public method to crystalize the tree structure and begin
         * configuring.
         * \pre Tree must be in TREE_BUILDING phase. Note that subtrees below
         * the root can still be in earlier phases.
         *
         * This can only be invoked at the root of a tree
         */
        void enterConfiguring(){
            if(getPhase() != TREE_BUILDING){
                throw SpartaException("Device tree with root \"")
                    << getLocation()<< "\" not currently in the TREE_BUILDING phase, so it cannot "
                                       "enter TREE_CONFIGURING";
            }

            enterConfig_(); // Enter the next phase (cannot throw)
        }

        /*!
         * \brief Public method for recursive tree finalization. Places tree
         * temporarily into TREE_FINALIZING phase before finalizing, then places
         * tree into TREE_FINALIZED
         * \throw SpartaException on error. Can be called again if an exception
         * is thrown.
         * \pre Tree must be in TREE_CONFIGURING phase. Note that subtrees below
         * the root can still be in earlier phases.
         * \post Tree will be in TREE_FINALIZED phase if successful
         * \see sparta::TreeNode::isConfiguring
         * \see sparta::TreeNode::isFinalizing
         * \see sparta::TreeNode::isFinalized
         *
         * This can only be invoked at the root of a tree
         */
        void enterFinalized(sparta::python::PythonInterpreter* pyshell = nullptr);

        /*!
         * \brief Public method for recursively giving all resources and nodes a
         * chance to bind ports locally. Recurses depth first by order of
         * construction of each child in each node encountered.
         * \note This should be done by a simulator before allowing top-level
         * binding within that simulator.
         * \warning This should not be called once the simulation is running
         * \todo Prevent this from being called when the simulation is running
         * \throw SpartaException on error. Can be called again if an exception
         * is thrown.
         * \pre Tree must be in TREE_FINALIZED phase (i.e. after enterFinalized
         * is called successfully)
         * \see bindTreeLate
         * This can only be invoked at the root of a tree
         */
        void bindTreeEarly() {
            if(getPhase() != TREE_FINALIZED){
                throw SpartaException("Device tree with root \"")
                    << getLocation()
                    << "\" not in the TREE_FINALIZED phase, so it cannot be bound (bindTreeEarly)";
            }

            bindTreeEarly_();
        }

        /*!
         * \brief Public method for recursively giving all resources and nodes a
         * chance to bind ports locally. Recurses depth first by order of
         * construction of each child in each node encountered.
         * \note This should be done by a simulator after allowing top-level
         * binding within that simulator.
         * \pre Tree must be in TREE_FINALIZED phase (i.e. after enterFinalized
         * is called successfully)
         * \see bindTreeEarly
         */
        void bindTreeLate() {
            if(getPhase() != TREE_FINALIZED){
                throw SpartaException("Device tree with root \"")
                    << getLocation()
                    << "\" not in the TREE_FINALIZED phase, so it cannot be bound (bindTreeLate)";
            }

            bindTreeLate_();
        }

        /*!
         * \brief Called after simulation has stopped, but before statistic/report generation
         *
         * This method is called by the Simulation class after it has
         * detected that simulation has stopped, but before reports or
         * statistics are generated/collected.
         *
         * As a sparta::Resource or sparta::Unit, the developer can
         * override the private method simulationTerminating_() and
         * perform last minute cleanup of their component.
         */
        void simulationTerminating() {
            simulationTerminating_();
        }

        /*!
         * \brief Validate the entire tree immediately prior to running.
         * All ports should be bound ,etc. Does not change phase
         * \pre Tree must be in phase TREE_FINALIZED
         * \note Uses validateTree_
         */
        void validatePreRun() {
            if(getPhase() != TREE_FINALIZED){
                throw SpartaException("Device tree with root \"")
                    << getLocation()
                    << "\" not in the TREE_FINALIZED phase, so it cannot be pre-run validated";
            }

            validateTree_(); // Validate all nodes, which may throw
        }


        /*!
         * \brief Validate all resources in the simulator, throwing exceptions
         * if invalid state is detected. Exceptions are allowed to propogate out
         * of this method.
         * \throw Any Exception
         */
        void validatePostRun();

        /*!
         * \brief Dump all debug content from each resource in the tree to an
         * ostream
         * \param out Output stream to which all debug content is written
         * \todo Replace this method with a method that places all output into
         * different files within a subfolder
         * \throw Must not throw
         * \note This is typically called at shutdown on error, but simulators
         * may use it differently
         */
        void dumpDebugContent(std::ostream& out) noexcept {
            dumpDebugContent_(out);
        }

        /*!
         * \brief Places this tree into TREE_TEARDOWN phase so that nodes may be
         * deleted without errors.
         * \pre Tree can be in ANY phase
         * \throw SpartaException on error. Can be called again if an exception
         * is thrown.
         * \see sparta::TreeNode::isTearingDown
         *
         * This can only be invoked at the root of a tree
         */
        void enterTeardown() {
            enterTeardown_(); // Enter the next phase
        }

        /*!
         * \brief Render description of this RootTreeNode as a string
         * \return String description of the form
         * \verbatim
         * <location (root)>
         * \endverbatim
         */
        virtual std::string stringize(bool pretty=false) const override {
            (void) pretty;
            std::stringstream ss;
            ss << "<" << getLocation() << " (root)>";
            return ss.str();
        }

        //! \name Observation
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Returns the post-write notification-source node for this
         * register which can be used to observe writes to this register.
         * This notification is posted immediately after the register has
         * been written and populates a sparta::Register::PostWriteAccess object
         * with the results.
         *
         * Refer to PostWriteNotiSrc (sparta::NotificationSource) for more
         * details. Use the sparta::NotificationSource::registerForThis and
         * sparta::NotificationSource::deregisterForThis methods.
         */
        NewDescendantNotiSrc& getNodeAttachedNotification() {
            return new_node_noti_;
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Diagnostics
        //!
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Gets vectors of all the ArchData instances in existance now
         * categorized into several groups. Inputs are cleared before appending
         * \param ad_this_tree ArchDatas associated with this tree
         * \param ad_no_assoc ArchDatas having no association
         * \param ad_not_attached ArchDatas associated with nodes that are not
         * part of ANY tree
         * \param ad_other_tree ArchDatas associated with nodes that are pat of
         * a different tree
         * \note All inputs are cleared before appending
         */
        void getArchDataAssociations(std::vector<const ArchData*>& ad_this_tree,
                                     std::vector<const ArchData*>& ad_no_assoc,
                                     std::vector<const ArchData*>& ad_not_attached,
                                     std::vector<const ArchData*>& ad_other_tree) const noexcept;

        /*!
         * \brief Gets the results
         * \throw SpartaException if there are any ArchDatas found with no
         * association or any ArchDatas not attached to a tree root.
         *
         * ArchDatas associated with this tree and part of other trees
         * are always acceptable. ArchDatas without a clear tree association
         * are dangerous and it is the responsibility of the caller to determine
         * if that is expected behavior
         */
        void validateArchDataAssociations() const;

        /*!
         * \brief Debugging tool which checks all ArchDatas in existence to see
         * if they are associated with this tree. Prints all ArchDatas in
         * several categories (see getArchDataAssociations)
         * \param o Ostream to which results will be printed
         *
         * If should be regarded as an error in an actual production simulation
         * if any ArchDatas are found in to be unassociated or associated
         * without being part of a tree.
         */
        void dumpArchDataAssociations(std::ostream& o) const noexcept;

        /*!
         * \brief Dumps the mix of tree node concrete types to the ostream
         */
        void dumpTypeMix(std::ostream& o) const;

        /*!
         * \brief Register a tree node extension factory.
         */
        template <typename ExtensionT>
        static void registerExtensionClass() {
            static_assert(std::is_base_of<ExtensionsBase, ExtensionT>::value);
            extensionFactories_()[ExtensionT::NAME] = []() { return new ExtensionT; };
        }

        /*!
         * \brief Register a tree node extension factory.
         */
        static void registerExtensionFactory(const std::string & extension_name,
                                             std::function<ExtensionsBase*()> factory)
        {
            extensionFactories_()[extension_name] = factory;
        }

        /*!
         * \brief Get a tree node extension factory, if available. Check "operator bool()"
         * before using.
         */
        static std::function<ExtensionsBase*()> & getExtensionFactory(const std::string & extension_name)
        {
            auto it = extensionFactories_().find(extension_name);
            if (it != extensionFactories_().end()) {
                return it->second;
            }

            static std::function<ExtensionsBase*()> none;
            return none;
        }

        /*!
         * \brief Get all tree node extension factories.
         */
        static std::map<std::string, std::function<ExtensionsBase*()>> & getExtensionFactories() {
            return extensionFactories_();
        }

        /*!
         * \brief Returns a ParameterTree containing an unbound set of
         * named tree node extensions and their parameter value(s).
         */
        ParameterTree& getExtensionsUnboundParameterTree() {
            return extensions_ptree_;
        }

        /*!
         * \brief Returns a ParameterTree (const version) containing an
         * unbound set of named tree node extensions and their parameter
         * value(s).
         */
        const ParameterTree& getExtensionsUnboundParameterTree() const {
            return extensions_ptree_;
        }

        /*!
         * \brief Create or update tree node extension(s) from the given YAML file.
         */
        void createExtensions(const std::string & yaml_file,
                              const std::vector<std::string> & config_search_paths = {},
                              const bool verbose_cfg = false)
        {
            app::NodeConfigFileApplicator applicator("", yaml_file, config_search_paths);

            ParameterTree ptree;
            applicator.applyUnbound(ptree, verbose_cfg);

            std::vector<ParameterTree::Node*> nodes;
            ptree.getUnreadValueNodes(&nodes);
            for (auto node : nodes) {
                node->unrequire();
            }

            if (verbose_cfg) {
                std::cout << "Parsed extension file '" << yaml_file << "' into tree:\n";
                constexpr bool print_user_data = false; // Already printed from "extension" nodes
                ptree.recursePrint(std::cout, print_user_data);
            }

            // Find all ParameterTree::Node's named "extension"
            std::vector<ParameterTree::Node*> extension_nodes;
            std::function<void(ParameterTree::Node*)> recurse_find_ext_nodes;

            recurse_find_ext_nodes = [&](ParameterTree::Node* n)
            {
                if (n->getName() == "extension") {
                    extension_nodes.push_back(n);
                } else {
                    for (auto child : n->getChildren()) {
                        recurse_find_ext_nodes(child);
                    }
                }
            };

            recurse_find_ext_nodes(ptree.getRoot());

            // For every extension node, extract:
            //   - path (unbound TreeNode that gets the extension)
            //   - extension name
            //   - name/value pairs for all extension parameters
            struct ExtensionDesc {
                std::string path;
                std::string ext_name;
                std::vector<std::pair<std::string, std::string>> params;
            };

            std::vector<ExtensionDesc> ext_descriptors;
            for (auto n : extension_nodes) {
                // Create one extension descriptor for each child of this node.
                for (auto child : n->getChildren()) {
                    ExtensionDesc desc;

                    // Path e.g. "top.node1" from "top.node1.extension.foo"
                    desc.path = n->getPath();
                    auto idx = desc.path.rfind(".extension");
                    sparta_assert(idx != std::string::npos);
                    desc.path = desc.path.substr(0, idx);

                    // Extension name e.g. "foo" from "top.node1.extension.foo"
                    desc.ext_name = child->getName();

                    // Param key-value pairs e.g. [bar:"2", baz:"blah"] from:
                    //   top.node1.extension.foo:
                    //     bar: 2
                    //     baz: "blah"
                    //
                    // We extract the param values as strings, and later we will
                    // convert to specific Parameter<T> params.
                    auto param_nodes = child->getChildren();
                    for (auto p : param_nodes) {
                        auto p_name = p->getName();
                        auto p_value = p->getValue();
                        auto kvp = std::make_pair(p_name, p_value);
                        desc.params.push_back(kvp);
                    }

                    ext_descriptors.emplace_back(std::move(desc));
                }
            }

            // Create each extension using its registered factory if we have it,
            // else we have to create a ExtensionsParamsOnly object.
            struct NodeExtension {
                std::string path;
                std::string ext_name;
                std::shared_ptr<TreeNode::ExtensionsBase> extension;
            };

            std::vector<std::unique_ptr<NodeExtension>> node_extensions;
            for (const auto & desc : ext_descriptors) {
                auto node_extension = std::make_unique<NodeExtension>();
                node_extension->path = desc.path;
                node_extension->ext_name = desc.ext_name;

                auto factory = getExtensionFactory(desc.ext_name);
                if (factory) {
                    node_extension->extension.reset(factory());
                } else {
                    node_extension->extension = std::make_shared<ExtensionsParamsOnly>();
                }

                node_extension->extension->setParameters(std::make_unique<ParameterSet>(nullptr));
                auto ps = node_extension->extension->getParameters();

                for (const auto & kvp : desc.params) {
                    // All parameters are created as Parameter<std::string>, and users
                    // can later call ExtensionsBase::getParameterAs<T> which will attempt
                    // to parse the string as T, and throw if it fails.
                    const auto & p_name = kvp.first;
                    const auto & p_value = kvp.second;
                    const auto & p_desc = p_name;
                    auto param = std::make_unique<Parameter<std::string>>(p_name, p_value, p_desc, ps);
                    node_extension->extension->addParameter(std::move(param));
                }

                node_extension->extension->postCreate();
                node_extensions.emplace_back(std::move(node_extension));
            }

            // Store the extensions in the std::any map owned by the ParameterTree::Node's
            extensions_ptree_.merge(ptree);
            if (verbose_cfg) {
                std::cout << "After merging extension file '" << yaml_file << "', parameter tree contains:\n";
                constexpr bool print_user_data = false; // Already printed from "extension" nodes
                extensions_ptree_.recursePrint(std::cout, print_user_data);
            }

            for (auto & extension : node_extensions) {
                constexpr bool must_be_leaf = false;
                auto n = extensions_ptree_.tryGet(extension->path, must_be_leaf);
                n->setUserData(extension->ext_name, extension->extension);
            }
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

    private:

        /*!
         * \brief Get static extension factories. Wrapped in a method to prevent static
         * initialization order fiasco.
         */
        static std::map<std::string, std::function<ExtensionsBase*()>> & extensionFactories_() {
            static std::map<std::string, std::function<ExtensionsBase*()>> factories;
            return factories;
        }

        //! Unbound (pre-application) Extensions Tree
        ParameterTree extensions_ptree_;

        // No effect on root
        virtual void createResource_() override {};

        /*!
         * \brief Disallow assigning a parent to this node except for the
         * GlobalTreeNode.
         * \note No parent is stored or set, but values of parent other than
         * GlobalTreeNode() will cause this method to throw.
         *
         * The root of the tree can have no parent, but is a child of the
         * GlobalTreeNode
         */
        virtual void onSettingParent_(const TreeNode* parent) const override final {
            if(parent != search_node_){
                throw SpartaException("This RootTreeNode \"")
                    << "\" cannot be a child of any other node except its constructed "
                       "GlobalTreeNode";
            }
        }

        /*!
         * \brief Disallow assigning a parent to this node except for the
         * GlobalTreeNode. Should be caught by onSettingParent_. If this
         * function is reached with a different node, it is a critical error.
         * \note This method is protected from being called improperly by
         * onSettingParent_ which throws when an inappropriate parent is about
         * to be set.
         */
        virtual void setParent_(TreeNode* parent, bool) override final {
            if(parent != search_node_){
                throw SpartaCriticalError("This RootTreeNode \"")
                    << "\" cannot be a child of any other node except its constructed "
                       "GlobalTreeNode";
            }

            // NOTE: Do not store parent because the root should act as if there is no parent
        }

        virtual void onDescendentSubtreeAdded_(TreeNode* des) noexcept override final {
            // Broadcast notification for self coming into existence
            try{
                new_node_noti_.postNotification(*des);

                // Iterate subtree headed by des and notify observers of these
                for(TreeNode* child : TreeNodePrivateAttorney::getAllChildren(des)){
                    onDescendentSubtreeAdded_(child);
                }
            }catch(SpartaException& ex){
                std::stringstream msg;
                msg << ex.what() <<
                    "\nThis exception within onDescendentSubtreeAdded_, which is not exception-"
                    "safe. The integrity of this sparta tree (" << getRoot() << ") is compromised. "
                    "Aborting. ";
                sparta_abort(false,msg.str());
            }
        }

        /*!
         * \brief GlobalTreeNode allocated by this node as a parent if a null
         * global was specified at construction.
         */
        std::unique_ptr<GlobalTreeNode> alloc_search_node_;

        /*!
         * \brief Global node for this root. May be shared with other
         * RootTreeNodes.
         */
        GlobalTreeNode* search_node_;

        /*!
         * \brief Simulator associated with this tree
         */
        app::Simulation* sim_;

        /*!
         * \brief Notification the be posted when a descendent is attached to
         * this tree
         */
        NewDescendantNotiSrc new_node_noti_;
    };

    template <typename ExtensionT>
    class ExtensionRegistration
    {
    public:
        ExtensionRegistration()
        {
            sparta::RootTreeNode::registerExtensionClass<ExtensionT>();
        }
    };

} // namespace sparta

#define SPARTA_CONCATENATE_DETAIL(x, y) x##y
#define SPARTA_CONCATENATE(x, y) SPARTA_CONCATENATE_DETAIL(x, y)

#define REGISTER_TREE_NODE_EXTENSION(ExtensionClass) \
    sparta::ExtensionRegistration<ExtensionClass> SPARTA_CONCATENATE(__extension_registration_, __COUNTER__)
