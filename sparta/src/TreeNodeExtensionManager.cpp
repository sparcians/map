#include "sparta/extensions/TreeNodeExtensionManager.hpp"
#include "sparta/simulation/ParameterTree.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/simulation/Parameter.hpp"
#include "sparta/app/ConfigApplicators.hpp"

#include <boost/algorithm/string.hpp>

struct ScopedPTreeDumper
{
public:
    ScopedPTreeDumper(std::shared_ptr<sparta::ParameterTree> wildcard_config_ptree,
                      std::shared_ptr<sparta::ParameterTree> concrete_config_ptree,
                      std::shared_ptr<sparta::ParameterTree> write_final_config_ptree,
                      const std::string & func)
        : wildcard_config_ptree_(wildcard_config_ptree)
        , concrete_config_ptree_(concrete_config_ptree)
        , write_final_config_ptree_(write_final_config_ptree)
        , func_(func)
    {
        printPTrees_("Entering");
    }

    ~ScopedPTreeDumper()
    {
        printPTrees_("Exiting");
    }

private:
    void printPTrees_(const std::string & entering_exiting) {
        std::cout << "\n[debug] " << entering_exiting << " function: " << func_ << std::endl;

        std::cout << "wildcard_config_ptree:\n";
        wildcard_config_ptree_->recursePrint(std::cout);

        std::cout << "concrete_config_ptree:\n";
        concrete_config_ptree_->recursePrint(std::cout);

        std::cout << "write_final_config_ptree:\n";
        write_final_config_ptree_->recursePrint(std::cout);
    }

    std::shared_ptr<sparta::ParameterTree> wildcard_config_ptree_;
    std::shared_ptr<sparta::ParameterTree> concrete_config_ptree_;
    std::shared_ptr<sparta::ParameterTree> write_final_config_ptree_;
    std::string func_;
};

#define DEBUG_THIS_FUNCTION \
    [[maybe_unused]] ScopedPTreeDumper dumper(wildcard_config_ptree_, concrete_config_ptree_, write_final_config_ptree_, __FUNCTION__);

namespace sparta
{

using ExtensionsBase = detail::ExtensionsBase;
using ExtensionsPtr = std::shared_ptr<ExtensionsBase>;

//! \brief Helper to validate parameters before continuing
//! \return If validated, returns parameter [name,value] pair
//! \throw Throws if the "optional" keyword was misused
std::pair<std::string, std::string> validateParam(const ParameterTree::Node* p)
{
    auto p_name = p->getName();
    auto p_value = p->peekValue();

    if (p_name == "optional") {
        try {
            size_t end_pos;
            utils::smartLexicalCast<bool>(p_value, end_pos);

            // Don't print a whole bunch of identical messages
            static std::set<std::string> msg_extensions;
            const auto & extension_name = notNull(p->getParent())->getValue();
            if (msg_extensions.insert(extension_name).second) {
                std::cout << "Ignoring reserved keyword 'optional' in extensions YAML definition. "
                          << "This is not a parameter - it tells the simulation that this YAML does "
                          << "not have to be read before finalizeTree(). This was encountered for "
                          << "extension '" << extension_name << "'." << std::endl;
            }
        } catch (const SpartaException &) {
            throw SpartaException("Extension YAML 'optional' keyword must only be set to ")
                << "true or false, not '" << p_value << "'. Error parsing parameter '"
                << p->getPath() << "'.";
        }
    }

    return {p_name, p_value};
}

TreeNodeExtensionManager::TreeNodeExtensionManager()
    : wildcard_config_ptree_(std::make_shared<ParameterTree>())
    , concrete_config_ptree_(std::make_shared<ParameterTree>())
    , write_final_config_ptree_(std::make_shared<ParameterTree>())
{
}

void TreeNodeExtensionManager::setRoot(RootTreeNode* root)
{
    sparta_assert(root_ == nullptr || root_ == root);
    root->setExtensionManager(this);
    root_ = root;
}

ExtensionsBase * TreeNodeExtensionManager::getExtension(
    const std::string & loc,
    const std::string & extension_name,
    bool no_factory_ok)
{
    // First check if we already have this extension.
    if (auto ext = getLiveExtension_(loc, extension_name)) {
        return ext;
    }

    // If this tree node location was never in any of the
    // input YAML files, always return null.
    if (!inYamlConfig_(loc + ".extension." + extension_name)) {
        return nullptr;
    }

    // We will only create extensions on demand if there is a registered
    // factory, or if no_factory_ok=true.
    if (!no_factory_ok && !TreeNodeExtensionManager::hasExtensionFactory(extension_name)) {
        return nullptr;
    }

    // Go ahead and create/cache the extension.
    return createExtension(loc, extension_name);
}

const ExtensionsBase * TreeNodeExtensionManager::getExtension(
    const std::string & loc,
    const std::string & extension_name) const
{
    return getLiveExtension_(loc, extension_name);
}

ExtensionsBase * TreeNodeExtensionManager::createExtension(
    const std::string & loc,
    const std::string & extension_name,
    bool replace)
{
    sparta_assert(notNull(root_)->getPhase() >= PhasedObject::TREE_CONFIGURING);

    // Throw away the live extension if we have one and are replacing it.
    if (replace && getLiveExtension_(loc, extension_name) != nullptr) {
        auto success = removeExtension(loc, extension_name);
        sparta_assert(success);
    }

    // Create new extension, with or without a factory.
    ExtensionsPtr extension;
    if (auto& factory = getExtensionFactory_(extension_name)) {
        extension.reset(factory());
    } else {
        extension.reset(new ExtensionsParamsOnly());
    }

    extension->setParameters(std::make_unique<ParameterSet>(nullptr));
    auto ps = extension->getParameters();
    sparta_assert(ps != nullptr);

    // Call postCreate before adding parameters found in the ptree. The extension
    // subclass created from the factory might have parameters with default values
    // that are overridden by those found in the YAML file, and we can't add any
    // parameters to the ParameterSet with the same name.
    extension->postCreate();

    // In order to show default parameter values in the final config YAML, we
    // need to add any parameters created in postCreate() to our concrete config
    // parameter tree.
    std::vector<ParameterBase*> post_create_params;
    ps->getChildrenOfType<ParameterBase>(post_create_params);
    auto ext_path = loc + ".extension." + extension_name;

    if (!post_create_params.empty()) {
        auto concrete_post_create_node = concrete_config_ptree_->create(ext_path);
        auto wildcard_post_create_node = wildcard_config_ptree_->tryGet(ext_path, false /*not a leaf*/);
        for (auto p : post_create_params) {
            auto p_name = p->getName();
            std::string p_value;

            if (concrete_post_create_node) {
                auto param_node = concrete_post_create_node->getChild(p_name);
                if (param_node) {
                    p_value = param_node->peekValue();
                }
            } 
            
            if (wildcard_post_create_node && p_value.empty()) {
                auto param_node = wildcard_post_create_node->getChild(p_name);
                if (param_node) {
                    p_value = param_node->peekValue();
                }
            }

            auto ext_param = ps->getParameter(p_name);
            sparta_assert(ext_param != nullptr);
            if (!p_value.empty()) {
                ext_param->setValueFromString(p_value);
            } else {
                p_value = ext_param->getValueAsString();
            }

            auto p_node = concrete_post_create_node->create(p_name, false /*unrequired*/);
            p_node->setValue(p_value, false /*unrequired*/, "postCreate" /*origin*/);
            p_node->setUserData("user_visible", true);

            if (wildcard_post_create_node) {
                p_node = wildcard_post_create_node->create(p_name, false /*unrequired*/);
                p_node->setValue(p_value, false /*unrequired*/, "postCreate" /*origin*/);
                p_node->setUserData("user_visible", true);
            }
        }
    }

    // Apply all parameters we were given in the arch/config/extension YAML
    // files to the extension params in its ParameterSet.
    auto ext_node = concrete_config_ptree_->tryGet(ext_path, false /*not a leaf*/);
    if (ext_node) {
        for (auto p : ext_node->getChildren()) {
            const auto & [p_name, p_value] = validateParam(p);

            // If this parameter was already created during postCreate(), just
            // set its value.
            if (auto p = ps->getParameter(p_name, false)) {
                app::ParameterApplicator pa("", p_value);
                pa.apply(p);
                continue;
            }

            // Otherwise, create a new parameter, defaulting to string type.
            const auto & p_desc = p_name;
            auto param = std::make_unique<Parameter<std::string>>(p_name, p_value, p_desc, ps);
            extension->addParameter(std::move(param));
        }
    }

    // Since extensions can be created on the fly, validate this extension's parameters.
    // This already occurred for any extensions accessed/created before finalizeTree().
    std::string errs;
    auto tn = notNull(root_)->getSearchScope()->getChild(loc);
    if (!extension->getParameters()->validateDependencies(tn, errs)) {
        throw SpartaException("Parameter validation callbacks indicated invalid parameters: ")
            << errs;
    }

    // Cache the extension and return it.
    live_extensions_[loc][extension_name] = extension;
    return extension.get();
}

ExtensionsBase * TreeNodeExtensionManager::createExtension(
    const std::string & loc,
    bool replace)
{
    sparta_assert(notNull(root_)->getPhase() >= PhasedObject::TREE_CONFIGURING);
    std::set<std::string> known_extension_names = getAllConfigExtensionNames(loc);

    // Don't have any extension names? No extensions.
    if (known_extension_names.empty()) {
        return nullptr;
    }

    // More than one unique extension name? Exception.
    else if (known_extension_names.size() > 1) {
        std::ostringstream oss;
        oss << "TreeNode::createExtension() overload called without any specific \n"
            << "named extension requested. However, more than one extension was \n"
            << "found. Applies to '" << loc << "'\nHere are the extension names \n"
            << "found at this node:\n";
        for (const auto & ext : known_extension_names) {
            oss << "\t" << ext << "\n";
        }
        throw SpartaException(oss.str());
    }

    // Get the one named extension.
    return createExtension(loc, *known_extension_names.begin(), replace);
}

ExtensionsBase * TreeNodeExtensionManager::createExtension(
    const std::string & loc,
    const char* extension_name,
    bool replace)
{
    return createExtension(loc, std::string(extension_name), replace);
}

void TreeNodeExtensionManager::addExtensions(
    const std::string & yaml_file,
    const std::vector<std::string> & search_dirs)
{
    app::NodeConfigFileApplicator applicator("", yaml_file, search_dirs);

    ParameterTree ptree;
    applicator.applyUnbound(ptree);
    addExtensions(ptree);

    std::cout << "  [in] Extensions: " << applicator.stringize() << std::endl;
}

void TreeNodeExtensionManager::addExtensions(
    const ParameterTree & ptree)
{
    sparta_assert(root_ == nullptr || root_->getPhase() == PhasedObject::TREE_BUILDING);
    sparta_assert(!hasExtensions());

    std::vector<const ParameterTree::Node*> ext_nodes;
    ptree.getRoot()->recursFindPTreeNodesNamed("extension", ext_nodes);
    if (ext_nodes.empty()) {
        return;
    }

    ptree.visitLeaves([&](const ParameterTree::Node* leaf) {
        auto parent = leaf->getParent();
        auto grandparent = parent ? parent->getParent() : nullptr;
        if (!grandparent || grandparent->getName() != "extension") {
            return true; // keep going
        }

        auto path = leaf->getPath();
        auto& dst_ptree = TreeNode::hasWildcardCharacters(path) ?
            *wildcard_config_ptree_ : *concrete_config_ptree_;

        auto n = dst_ptree.create(path);
        n->setValue(leaf->getValue(), false /*unrequired*/, leaf->getOrigin());

        // Differentiate between auto-expanded nodes and user-visible nodes.
        // This is needed to keep the --write-final-config YAML file looking
        // like this:
        //
        //   top.cpu.core*.lsu.foobar.foo: 4
        //
        // Instead of this:
        //
        //   top.cpu.core0.lsu.foobar.foo: 4
        //   top.cpu.core1.lsu.foobar.foo: 4
        //
        // We want the final config YAML file to match what users provided
        // in the input YAML files.
        n->setUserData("user_visible", true);

        return true; // keep going
    });
}

bool TreeNodeExtensionManager::removeExtension(
    const std::string & loc,
    const std::string & extension_name)
{
    if (getLiveExtension_(loc, extension_name) == nullptr) {
        return false;
    }

    live_extensions_[loc].erase(extension_name);
    if (live_extensions_[loc].empty()) {
        live_extensions_.erase(loc);
    }

    return true;
}

bool TreeNodeExtensionManager::hasExtension(
    const std::string & loc,
    const std::string & extension_name) const
{
    return getLiveExtension_(loc, extension_name) != nullptr;
}

bool TreeNodeExtensionManager::hasExtensions() const
{
    return !live_extensions_.empty();
}

std::set<std::string> TreeNodeExtensionManager::getAllInstantiatedExtensionNames(
    const std::string & loc) const
{
    auto outer_it = live_extensions_.find(loc);
    if (outer_it == live_extensions_.end()) {
        return {};
    }

    std::set<std::string> ext_names;
    for (const auto & [ext_name, _] : outer_it->second) {
        ext_names.insert(ext_name);
    }

    return ext_names;
}

std::set<std::string> TreeNodeExtensionManager::getAllConfigExtensionNames(
    const std::string & loc) const
{
    std::set<std::string> ext_names;
    for (auto ptree : {wildcard_config_ptree_, concrete_config_ptree_}) {
        if (auto ext_node = ptree->tryGet(loc + ".extension", false /*not a leaf*/)) {
            for (auto child : ext_node->getChildren()) {
                ext_names.insert(child->getName());
            }
        }
    }
    return ext_names;
}

size_t TreeNodeExtensionManager::getNumExtensions(
    const std::string & loc) const
{
    return getAllExtensions(loc).size();
}

std::map<std::string, const ExtensionsBase*> TreeNodeExtensionManager::getAllExtensions(
    const std::string & loc) const
{
    auto outer_it = live_extensions_.find(loc);
    if (outer_it == live_extensions_.end()) {
        return {};
    }

    std::map<std::string, const ExtensionsBase*> map;
    for (const auto & [ext_name, ext] : outer_it->second) {
        map[ext_name] = ext.get();
    }

    return map;
}

std::map<std::string, std::map<std::string, const ExtensionsBase *>>
TreeNodeExtensionManager::getAllExtensions() const
{
    std::map<std::string, std::map<std::string, const ExtensionsBase *>> map;
    for (const auto & [loc, exts] : live_extensions_) {
        for (const auto & [ext_name, ext] : exts) {
            map[loc][ext_name] = ext.get();
        }
    }
    return map;
}

const ParameterTree * TreeNodeExtensionManager::getFinalConfigPTree()
{
    sparta_assert(notNull(root_)->getPhase() == PhasedObject::TREE_FINALIZED);
    write_final_config_ptree_->clear();

    struct ExtensionParam {
        std::string p_value;   // parameter value
        std::string p_origin;  // parameter origin
        bool operator==(const ExtensionParam & other) const {
            // Only compare values
            return p_value == other.p_value;
        }
    };

    using NamedExtensionParams = std::map<
        std::string,           // parameter name
        ExtensionParam         // parameter value/origin
    >;

    using NamedExtensions = std::map<
        std::string,           // extension name
        NamedExtensionParams
    >;

    using PTreeExtensions = std::map<
        std::string,           // ptree node path
        NamedExtensions        // extensions by name
    >;

    auto extract_all_extensions_info = [](const ParameterTree & ptree, PTreeExtensions & ptree_extensions)
    {
        std::vector<const ParameterTree::Node*> extension_root_nodes;
        ptree.getRoot()->recursFindPTreeNodesNamed("extension", extension_root_nodes);

        for (auto ext_root_node : extension_root_nodes) {
            auto tn_loc = notNull(ext_root_node->getParent())->getPath();
            NamedExtensions & extensions_at_this_path = ptree_extensions[tn_loc];
            for (auto ext_node : ext_root_node->getChildren()) {
                const auto & ext_name = ext_node->getName();
                NamedExtensionParams & extension_params_at_this_extension = extensions_at_this_path[ext_name];
                for (auto param_node : ext_node->getChildren()) {
                    const auto & p_name = param_node->getName();
                    const auto & p_value = param_node->peekValue();
                    const auto & p_origin = param_node->getOrigin();
                    extension_params_at_this_extension[p_name] = ExtensionParam{p_value, p_origin};
                }
            }
        }
    };

    PTreeExtensions wildcard_extensions;
    extract_all_extensions_info(*wildcard_config_ptree_, wildcard_extensions);

    PTreeExtensions concrete_extensions;
    extract_all_extensions_info(*concrete_config_ptree_, concrete_extensions);

    PTreeExtensions final_extensions;
    for (const auto & [wildcard_loc, /*NamedExtensions*/wildcard_ext_infos] : wildcard_extensions) {
        std::vector<TreeNode*> matching_tns;
        notNull(root_)->getSearchScope()->findChildren(wildcard_loc, matching_tns);

        // See if this wildcard location's expanded TreeNodes all have the same extensions
        bool use_wildcard_path = true;
        NamedExtensions concrete_ext_infos;
        for (auto tn : matching_tns) {
            auto concrete_ext_path = tn->getLocation();
            const NamedExtensions & concrete_ext_infos = concrete_extensions.at(concrete_ext_path);
            if (concrete_ext_infos != wildcard_ext_infos) {
                use_wildcard_path = false;
                break;
            }
        }

        if (use_wildcard_path) {
            for (const auto & [ext_name, /*NamedExtensionParams*/wildcard_ext_params] : wildcard_ext_infos) {
                auto wildcard_ext_node = write_final_config_ptree_->create(wildcard_loc + ".extension." + ext_name);
                for (const auto & [p_name, p_val_and_origin] : wildcard_ext_params) {
                    auto p = wildcard_ext_node->create(p_name, false /*unrequired*/);
                    const auto & p_value = p_val_and_origin.p_value;
                    const auto & p_origin = p_val_and_origin.p_origin;
                    p->setValue(p_value, false /*unrequired*/, p_origin);
                    p->setUserData("user_visible", true);
                }
            }
            continue;
        }

        for (const auto & [ext_name, /*NamedExtensionParams*/wildcard_ext_params] : wildcard_ext_infos) {
            // See if this extension has identical parameter values in all
            // concrete extensions. If so, we will add the extension to the
            // final config ptree using the wildcard location to save YAML
            // file space.
            use_wildcard_path = true;
            for (auto tn : matching_tns) {
                auto concrete_ext_path = tn->getLocation() + ".extension." + ext_name;
                const auto & concrete_ext_node =
                    *notNull(concrete_config_ptree_->tryGet(concrete_ext_path, false /*not a leaf*/));

                NamedExtensionParams concrete_ext_params;
                for (auto concrete_param_node : concrete_ext_node.getChildren()) {
                    const auto & p_name = concrete_param_node->getName();
                    const auto & p_value = concrete_param_node->peekValue();
                    const auto & p_origin = concrete_param_node->getOrigin();
                    concrete_ext_params[p_name] = ExtensionParam{p_value, p_origin};
                }

                if (concrete_ext_params != wildcard_ext_params) {
                    use_wildcard_path = false;
                    break;
                }
            }

            if (use_wildcard_path) {
                auto wildcard_ext_path = wildcard_loc + ".extension." + ext_name;
                auto wildcard_ext_node = write_final_config_ptree_->create(wildcard_ext_path);
                for (const auto & [p_name, p_val_and_origin] : wildcard_ext_params) {
                    auto p = wildcard_ext_node->create(p_name, false /*unrequired*/);
                    const auto & p_value = p_val_and_origin.p_value;
                    const auto & p_origin = p_val_and_origin.p_origin;
                    p->setValue(p_value, false /*unrequired*/, p_origin);
                    p->setUserData("user_visible", true);
                }
            } else {
                for (auto tn : matching_tns) {
                    auto concrete_ext_path = tn->getLocation() + ".extension." + ext_name;
                    const auto & concrete_ext_node =
                        *notNull(concrete_config_ptree_->tryGet(concrete_ext_path, false /*not a leaf*/));
                    const auto & wildcard_ext_node =
                        *notNull(wildcard_config_ptree_->tryGet(concrete_ext_path, false /*not a leaf*/));

                    NamedExtensionParams concrete_ext_params;
                    for (auto wildcard_ext_param : wildcard_ext_node.getChildren()) {
                        const auto & p_name = wildcard_ext_param->getName();
                        if (auto concrete_ext_param = concrete_ext_node.getChild(p_name)) {
                            const auto & p_value = concrete_ext_param->peekValue();
                            const auto & p_origin = concrete_ext_param->getOrigin();
                            concrete_ext_params[p_name] = ExtensionParam{p_value, p_origin};
                        } else {
                            const auto & p_value = wildcard_ext_param->peekValue();
                            const auto & p_origin = wildcard_ext_param->getOrigin();
                            concrete_ext_params[p_name] = ExtensionParam{p_value, p_origin};
                        }
                    }

                    auto final_ext_node = write_final_config_ptree_->create(concrete_ext_path, false /*unrequired*/);
                    for (const auto & [p_name, p_val_and_origin] : concrete_ext_params) {
                        auto p = final_ext_node->create(p_name, false /*unrequired*/);
                        const auto & p_value = p_val_and_origin.p_value;
                        const auto & p_origin = p_val_and_origin.p_origin;
                        p->setValue(p_value, false /*unrequired*/, p_origin);
                        p->setUserData("user_visible", true);
                    }
                }
            }
        }
    }

    // Update all the read counts.
    write_final_config_ptree_->visitLeaves([&](const ParameterTree::Node* leaf) {
        if (leaf->getName().empty()) {
            // If the root is a leaf, there are no extensions in this ptree.
            return false; // true or false, we cannot recurse as there are no children
        }

        auto parent = notNull(leaf->getParent());
        auto grandparent = notNull(parent->getParent());
        sparta_assert(grandparent->getName() == "extension");
        sparta_assert(*leaf->tryGetUserData<bool>("user_visible"));

        const auto & [tn_loc, ext_name, ext_param] =
            parseExtensionParamPath_(leaf->getPath());

        std::vector<TreeNode*> matching_tns;
        notNull(root_)->getSearchScope()->findChildren(tn_loc, matching_tns);
        sparta_assert(!matching_tns.empty());

        for (auto tn : matching_tns) {
            if (auto ext = const_cast<const TreeNode*>(tn)->getExtension(ext_name)) {
                auto ps = ext->getParameters();
                auto param = ps->getChildAs<ParameterBase>(ext_param);

                auto concrete_param_path = tn->getLocation() + ".extension." + ext_name + "." + ext_param;
                auto & final_param_node = write_final_config_ptree_->get(concrete_param_path);
                while (final_param_node.getReadCount() < param->getReadCount()) {
                    final_param_node.incrementReadCount();
                }
            }
        }

        return true; // keep going
    });

    return write_final_config_ptree_.get();
}

void TreeNodeExtensionManager::postBuildTree()
{
    sparta_assert(!hasExtensions());
    if (wildcard_config_ptree_->getRoot()->getChildren().empty()) {
        // No wildcard extensions to expand.
        return;
    }

    // Expand all wildcard config paths to match the built device tree.
    // Merge into the "live" extensions config ptree.
    wildcard_config_ptree_->visitLeaves([&](const ParameterTree::Node* leaf) {
        auto parent = notNull(leaf->getParent());
        auto grandparent = notNull(parent->getParent());
        sparta_assert(grandparent->getName() == "extension");

        // Full path: top.cpu.core0.extension.foobar.foo
        //            top.cpu.core*.extension.foobar.foo
        auto path = leaf->getPath();

        // Regex path:           top.cpu.core0
        //                       top.cpu.core*
        // Extension name:       foobar
        // Extension param name: foo
        const auto & [regex_path, ext_name, ext_param_name] =
            parseExtensionParamPath_(path);

        // Extension param relative path: foobar.foo
        auto ext_rel_path = ext_name + "." + ext_param_name;

        std::vector<std::string> expanded_param_paths;
        if (TreeNode::hasWildcardCharacters(regex_path)) {
            std::vector<TreeNode*> matching_tns;
            notNull(root_)->getSearchScope()->findChildren(regex_path, matching_tns);
            for (auto tn : matching_tns) {
                auto expanded_path = tn->getLocation() + ".extension." + ext_rel_path;
                expanded_param_paths.push_back(expanded_path);
            }
        } else {
            expanded_param_paths.push_back(path);
        }

        for (const auto & param_path : expanded_param_paths) {
            // Do not overwrite existing concrete parameter values supplied
            // in the YAML files.
            auto p = concrete_config_ptree_->tryGet(param_path);
            if (p) {
                sparta_assert(*p->tryGetUserData<bool>("user_visible", true));
                continue;
            }

            p = concrete_config_ptree_->create(param_path);
            p->setValue(leaf->getValue(), false /*unrequired*/, leaf->getOrigin());
            if (const bool * user_visible = leaf->tryGetUserData<bool>("user_visible")) {
                p->setUserData("user_visible", *user_visible);
            }
        }

        return true; // keep going
    });
}

void TreeNodeExtensionManager::getUnreadExtensionParams(
    ParameterTree & unread_ptree)
{
    sparta_assert(notNull(root_)->getPhase() == PhasedObject::TREE_FINALIZED);
    sparta_assert(unread_ptree.getRoot()->getChildren().empty());

    auto final_config_ptree = getFinalConfigPTree();
    if (final_config_ptree->getRoot()->getChildren().empty()) {
        // No extensions for final config file.
        return;
    }

    // For every parameter node in our config ptree, we need to check the "equivalent"
    // ParameterBase's read count. If those are 0 (unread), then add the ptree node to
    // the unread ptree.
    final_config_ptree->visitLeaves([&](const ParameterTree::Node* leaf) {
        // Ensure we are only writing user-visible nodes to the final config.
        sparta_assert(*leaf->tryGetUserData<bool>("user_visible", true));

        // Verify that this is an extension parameter.
        auto parent = notNull(leaf->getParent());
        auto grandparent = notNull(parent->getParent());
        sparta_assert(grandparent->getName() == "extension");

        // Full path: top.cpu.core0.extension.foobar.foo
        auto path = leaf->getPath();

        // TreeNode location: top.cpu.core0
        // Extension param path: foobar.foo
        const auto & [tn_loc, ext_name, ext_param_name] =
            parseExtensionParamPath_(path);

        std::vector<TreeNode*> matching_tns;
        notNull(root_)->getSearchScope()->findChildren(tn_loc, matching_tns);
        for (auto tn : matching_tns) {
            // The incoming leaf node can have wildcards. Get the concrete leaf path.
            auto concrete_param_path = tn->getLocation() + ".extension." + ext_name + "." + ext_param_name;

            // Use const version of getExtension() so we don't accidentally
            // read the extension by virtue of creating it.
            auto const_tn = const_cast<const TreeNode*>(tn);
            if (auto tn_ext = const_tn->getExtension(ext_name)) {
                auto ext_param_set = tn_ext->getParameters();
                sparta_assert(ext_param_set != nullptr);

                auto ext_param = ext_param_set->getChildAs<ParameterBase>(ext_param_name);
                auto read_count = ext_param->getReadCount();
                if (read_count == 0) {
                    auto n = unread_ptree.create(concrete_param_path);
                    n->setValue(leaf->peekValue(), true /*required to force errors*/, leaf->getOrigin());
                }
            } else {
                auto n = unread_ptree.create(parent->getPath());
                n->setValue(leaf->peekValue(), true /*required to force errors*/, leaf->getOrigin());
            }
        }

        return true; // keep going
    });
}

bool TreeNodeExtensionManager::inYamlConfig_(
    const std::string & loc) const
{
    constexpr auto must_be_leaf = false;
    if (wildcard_config_ptree_->tryGet(loc, must_be_leaf)) {
        return true;
    }
    if (concrete_config_ptree_->tryGet(loc, must_be_leaf)) {
        return true;
    }
    return false;
}

std::tuple<std::string, std::string, std::string> TreeNodeExtensionManager::parseExtensionParamPath_(
    const std::string & path) const
{
    // Full path: top.cpu.core0.extension.foobar.foo
    constexpr std::string_view marker = ".extension.";
    auto pos = path.find(marker);
    sparta_assert(pos != std::string::npos);

    // TreeNode location: top.cpu.core0
    // Extension param path: foobar.foo
    std::string tn_loc = path.substr(0, pos);
    std::string ext_param_path = path.substr(pos + marker.size());

    std::vector<std::string> parts;
    boost::split(parts, ext_param_path, boost::is_any_of("."));
    sparta_assert(parts.size() == 2);

    // Extension name: foobar
    // Extension param name: foo
    const auto& ext_name = parts[0];
    const auto& ext_param_name = parts[1];

    return {tn_loc, ext_name, ext_param_name};
}

} // namespace sparta
