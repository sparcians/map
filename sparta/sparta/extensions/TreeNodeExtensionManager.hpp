#pragma once

#include "sparta/extensions/TreeNodeExtensions.hpp"
#include "sparta/utils/SpartaException.hpp"

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace sparta {

class RootTreeNode;
class ParameterTree;
class TreeNode;

namespace ConfigEmitter {
    class YAML;
}

//! \class TreeNodeExtensionManager
//! \brief This class maintains all TreeNode extensions.
class TreeNodeExtensionManager
{
public:
    using ExtensionsBase = detail::ExtensionsBase;

    /*!
     * \brief Constructor
     */
    TreeNodeExtensionManager();

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
     * \brief Check if we have an extension factory for the given extension name.
     */
    static bool hasExtensionFactory(const std::string & extension_name)
    {
        return extensionFactories_().count(extension_name) > 0;
    }

    /*!
     * \brief Set the RootTreeNode. This is required before using any other
     * non-static apis.
     */
    void setRoot(RootTreeNode* root);

    /*!
     * \brief Get an extension by its tree node location.
     * \see TreeNode::getExtension()
     */
    ExtensionsBase * getExtension(const std::string & loc,
                                  const std::string & extension_name,
                                  bool no_factory_ok=false);

    /*!
     * \brief Get an extension by its tree node location.
     * \see TreeNode::getExtension()
     */
    const ExtensionsBase * getExtension(const std::string & loc,
                                        const std::string & extension_name) const;

    /*!
     * \brief Get an extension by its tree node location and extension
     * name, downcast to the given type.
     * \throw Throws an exception if the extension exists, but the downcast failed.
     * \see TreeNode::getExtensionAs()
     */
    template <typename ExtensionT>
    ExtensionT* getExtensionAs(const std::string & loc,
                               const std::string & extension_name)
    {
        auto ext = getExtension(loc, extension_name);
        if (!ext) {
            return nullptr;
        }

        auto ret = dynamic_cast<ExtensionT*>(ext);
        if (!ret) {
            throw SpartaException("Could not downcast extension '")
                << extension_name << "' to " << typeid(ExtensionT).name() << ". "
                << "Actual extension type is " << ext->getClassName() << ".";
        }

        return ret;
    }

    /*!
     * \brief Get an extension by its tree node location and extension
     * name, downcast to the given type.
     * \throw Throws an exception if the extension exists, but the downcast failed.
     * \see TreeNode::getExtensionAs()
     */
    template <typename ExtensionT>
    const ExtensionT* getExtensionAs(const std::string & loc,
                                     const std::string & extension_name) const
    {
        static_assert(std::is_base_of<ExtensionsBase, ExtensionT>::value);
        auto ext = getExtension(loc, extension_name);
        if (!ext) {
            return nullptr;
        }

        auto ret = dynamic_cast<const ExtensionT*>(ext);
        if (!ret) {
            throw SpartaException("Could not downcast extension '")
                << extension_name << "' to " << typeid(ExtensionT).name() << ". "
                << "Actual extension type is " << ext->getClassName() << ".";
        }

        return ret;
    }

    /*!
     * \brief Create an extension on demand at the given tree node location.
     * \see TreeNode::createExtension()
     */
    ExtensionsBase * createExtension(const std::string & loc,
                                     const std::string & extension_name,
                                     bool replace=false);

    /*!
     * \brief Create an extension on demand at the given tree node location
     * without needing to specify any particular extension name.
     * \see TreeNode::createExtension()
     */
    ExtensionsBase * createExtension(const std::string & loc,
                                     bool replace=false);

    /*!
     * \see createExtension(loc, extension_name, replace)
     */
    ExtensionsBase * createExtension(const std::string & loc,
                                     const char* extension_name,
                                     bool replace = false);

    /*!
     * \brief Add tree node extension(s) from the given YAML file.
     * \note This method simply adds the extension information to
     * our underlying ParameterTree. The extension(s) will not be
     * instantiated until you call TreeNode::getExtension().
     * \note ONLY the extensions will be taken. The input YAML file can
     * be an arch/config file too, but irrelevant nodes will not
     * be merged.
     */
    void addExtensions(const std::string & yaml_file,
                       const std::vector<std::string> & search_dirs = {});

    /*!
     * \brief Extract all extenstions from the given ParameterTree.
     * \note This method simply adds the extension information to
     * our underlying ParameterTree. The extension(s) will not be
     * instantiated until you call TreeNode::getExtension().
     * \note ONLY the extensions will be taken. The input ptree can
     * be an arch/config ParameterTree too, but irrelevant nodes will
     * not be merged.
     */
    void addExtensions(const ParameterTree & ptree);

    /*!
     * \brief Add an extension at the given tree node location, specifying
     * the ExtensionsBase subclass type. Forward any arguments needed to your
     * subclass extension's constructor.
     * \throw Throws if the tree node already has an extension by the given
     * name. Use replaceExtension().
     * \see TreeNode::addExtension()
     */
    template <typename ExtensionT, typename... Args>
    ExtensionT * addExtension(const std::string & loc,
                              Args&&... args)
    {
        static_assert(std::is_base_of<ExtensionsBase, ExtensionT>::value);
        if (hasExtension(loc, ExtensionT::NAME)) {
            throw SpartaException("Extension already exists: ") << ExtensionT::NAME;
        }

        std::shared_ptr<ExtensionsBase> ext(new ExtensionT(std::forward<Args>(args)...));
        ext->setParameters(std::make_unique<ParameterSet>(nullptr));
        ext->postCreate();

        addExtension_(loc, ExtensionT::NAME, ext);
        return dynamic_cast<ExtensionT*>(ext.get());
    }

    /*!
     * \brief Replace an extension at the given tree node location, specifying
     * the ExtensionsBase subclass type.
     * \see TreeNode::replaceExtension()
     */
    template <typename ExtensionT, typename... Args>
    ExtensionT * replaceExtension(const std::string & loc,
                                  Args&&... args)
    {
        static_assert(std::is_base_of<ExtensionsBase, ExtensionT>::value);
        removeExtension(loc, ExtensionT::NAME);
        return addExtension<ExtensionT, Args...>(loc, std::forward<Args>(args)...);
    }

    /*!
     * \brief Remove an extension by its name at the given tree node location.
     * Returns true if successful, false if the extension was not found.
     * \see TreeNode::replaceExtension()
     */
    bool removeExtension(const std::string & loc,
                         const std::string & extension_name);

    /*!
     * \brief Check if there is an existing extension at the given tree node
     * location with this extension name.
     * \see TreeNode::hasExtension()
     */
    bool hasExtension(const std::string & loc,
                      const std::string & extension_name) const;

    /*!
     * \brief Check if there is an extension of concrete type at the given
     * tree node location with this extension name.
     * \see TreeNode::hasExtensionOfType()
     */
    template <typename ExtensionT>
    bool hasExtensionOfType(const std::string & loc,
                            const std::string & extension_name) const
    {
        auto ext = getExtension(loc, extension_name);
        if (!ext) {
            return false;
        }
        return dynamic_cast<const ExtensionT*>(ext) != nullptr;
    }

    /*!
     * \brief Check if we have any extensions on any tree node.
     */
    bool hasExtensions() const;

    /*!
     * \brief Get a list of extension names for all **instantiated**
     * extensions at the given tree node location.
     * \see TreeNode::getAllInstantiatedExtensionNames()
     */
    std::set<std::string> getAllInstantiatedExtensionNames(const std::string & loc) const;

    /*!
     * \brief Get a list of extension names known by the TreeNodeExtensionManager
     * for the tree node at the given location.
     * \see TreeNode::getAllConfigExtensionNames()
     */
    std::set<std::string> getAllConfigExtensionNames(const std::string & loc) const;

    /*!
     * \brief Get the number of instantiated extensions at the given
     * tree node location.
     * \see TreeNode::getNumExtensions()
     */
    size_t getNumExtensions(const std::string & loc) const;

    /*!
     * \brief Get a map of extensions for the given tree node location.
     */
    std::map<std::string, const ExtensionsBase*> getAllExtensions(const std::string & loc) const;

    /*!
     * \brief Get a map of extensions for all tree nodes.
     */
    std::map<std::string,                      // TreeNode location
             std::map<std::string,             // Extension name
                      const ExtensionsBase *>>
    getAllExtensions() const;

    /*!
     * \brief Get read-only access to our ParamterTree which holds
     * extension parameters. This is for --write-final-config.
     */
    const ParameterTree * getFinalConfigPTree();

    /*!
     * \brief Throw or warn if there were any extensions provided
     * in the YAML input files, but those extensions were never
     * created.
     * \note Must be called after the tree is finalized.
     */
    void checkAllYamlExtensionsCreated(bool suppress_exceptions = false);

private:

    /*!
     * \brief Get static extension factories. Wrapped in a method to prevent static
     * initialization order fiasco.
     */
    static std::map<std::string, std::function<ExtensionsBase*()>> & extensionFactories_() {
        static std::map<std::string, std::function<ExtensionsBase*()>> factories;
        return factories;
    }

    /*!
     * \brief Get a factory for the given extension name.
     */
    static std::function<ExtensionsBase*()> & getExtensionFactory_(
        const std::string & extension_name)
    {
        auto it = extensionFactories_().find(extension_name);
        if (it != extensionFactories_().end()) {
            return it->second;
        }

        static std::function<ExtensionsBase*()> no_factory;
        return no_factory;
    }

    /*!
     * \brief Try to get an extension by its location and extension name.
     */
    ExtensionsBase * getLiveExtension_(
        const std::string & loc,
        const std::string & extension_name)
    {
        auto outer_it = live_extensions_.find(loc);
        if (outer_it == live_extensions_.end()) {
            return nullptr;
        }

        auto inner_it = outer_it->second.find(extension_name);
        if (inner_it == outer_it->second.end()) {
            return nullptr;
        }

        return inner_it->second.get();
    }

    /*!
     * \brief Try to get an extension by its location and extension name.
     */
    const ExtensionsBase * getLiveExtension_(
        const std::string & loc,
        const std::string & extension_name) const
    {
        auto outer_it = live_extensions_.find(loc);
        if (outer_it == live_extensions_.end()) {
            return nullptr;
        }

        auto inner_it = outer_it->second.find(extension_name);
        if (inner_it == outer_it->second.end()) {
            return nullptr;
        }

        return inner_it->second.get();
    }

    /*!
     * \brief Check if the given tree node location was configured
     * for extensions in any of the input YAML files.
     */
    bool inYamlConfig_(const std::string & loc) const;

    /*!
     * \brief Helper which parses "top.cpu.core0.extension.foobar.foo"
     * into its relavent parts:
     *   - tree node loc:   top.cpu.core0
     *   - extension name:  foobar
     *   - extension param: foo
     */
    std::tuple<std::string, std::string, std::string> parseExtensionParamPath_(
        const std::string & path) const;

    /*!
     * \brief Get the root tree node with safety checks.
     */
    const RootTreeNode* getRoot_() const;

    /*!
     * \brief Get the root tree node with safety checks.
     */
    RootTreeNode* getRoot_();

    /*!
     * \brief ParameterTree which holds onto all extension parameter
     * nodes with wildcards in their path. This specifically holds
     * only extension configs explicitly given to us in calls to
     * addExtensions() prior to buildTree().
     */
    std::shared_ptr<ParameterTree> wildcard_config_ptree_;

    /*!
     * \brief ParameterTree which holds onto all extension parameter
     * nodes without wildcards in their path. This specifically holds
     * only extension configs explicitly given to us in calls to
     * addExtensions() prior to buildTree().
     */
    std::shared_ptr<ParameterTree> concrete_config_ptree_;

    /*!
     * \brief Dedicated ParameterTree just for --write-final-config.
     * Regenerated every time we are asked for the final config ptree.
     */
    std::shared_ptr<ParameterTree> write_final_config_ptree_;

    /*!
     * \brief Dedicated ptree to hold onto all parameter values for
     * those params created in postCreate() calls to extensions with
     * registered factories. The values at the time of finalizeTree()
     * will be added to the final config YAML file.
     */
    std::shared_ptr<ParameterTree> post_create_params_;

    /*!
     * \brief Cached extensions for getExtension() performance.
     * \note Uses weak pointers since the extensions (shared_ptr) can be
     * removed from the ParameterTree without our knowledge.
     */
    std::unordered_map<
        std::string,              // TreeNode location
        std::unordered_map<
            std::string,          // Extension name
            std::shared_ptr<ExtensionsBase>>> live_extensions_;

    /*!
     * \brief Simulation's RootTreeNode.
     */
    RootTreeNode* root_ = nullptr;

    /*!
     * \brief Keep track of the original extension param order
     * as seen in the input YAML files.
     */
    class OrderedParams
    {
    public:
        void insert(const std::string & param_name) {
            auto it = std::find(ordered_params_.begin(),
                                ordered_params_.end(),
                                param_name);
            if (it == ordered_params_.end()) {
                ordered_params_.push_back(param_name);
            }
        }

        size_t getPosition(const std::string & param_name) const {
            auto it = std::find(ordered_params_.begin(),
                                ordered_params_.end(),
                                param_name);
            if (it == ordered_params_.end()) {
                throw SpartaException("Param name not found: ") << param_name;
            }
            return std::distance(ordered_params_.begin(), it);
        }

    private:
        std::vector<std::string> ordered_params_;
    };

    std::map<std::string, OrderedParams> ordered_ext_params_;
};

template <typename ExtensionT>
class ExtensionRegistration
{
public:
    ExtensionRegistration()
    {
        TreeNodeExtensionManager::registerExtensionClass<ExtensionT>();
    }
};

} // namespace sparta

#define SPARTA_CONCATENATE_DETAIL(x, y) x##y
#define SPARTA_CONCATENATE(x, y) SPARTA_CONCATENATE_DETAIL(x, y)

#define REGISTER_TREE_NODE_EXTENSION(ExtensionClass) \
    sparta::ExtensionRegistration<ExtensionClass> SPARTA_CONCATENATE(__extension_registration_, __COUNTER__)
