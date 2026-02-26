#include "sparta/extensions/TreeNodeExtensionManager.hpp"
#include "sparta/simulation/ParameterTree.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/simulation/Parameter.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/app/ConfigApplicators.hpp"

#include <boost/algorithm/string.hpp>

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
            const auto & extension_name = notNull(p->getParent())->getName();
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
    , post_create_params_ptree_(std::make_shared<ParameterTree>())
{
}

void TreeNodeExtensionManager::setRoot(RootTreeNode* root)
{
    sparta_assert(root_ == nullptr || root_ == root);
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
    // Throw away the live extension if we have one and are replacing it.
    auto live_ext = getLiveExtension_(loc, extension_name);
    if (replace && live_ext != nullptr) {
        auto success = removeExtension(loc, extension_name);
        sparta_assert(success);
    }

    // Return existing extension if we are not replacing it.
    else if (!replace && live_ext != nullptr) {
        return live_ext;
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
    // need to add any parameters created in postCreate() to our postCreate
    // default params ptree.
    std::vector<ParameterBase*> post_create_params;
    ps->getChildrenOfType<ParameterBase>(post_create_params);
    auto ext_path = loc + ".extension." + extension_name;

    auto & ordered_params = ordered_ext_params_[extension_name];
    if (!post_create_params.empty()) {
        for (auto p : post_create_params) {
            auto p_name = p->getName();
            ordered_params.insert(p_name);
            std::string p_value;

            // First see if we have a parameter value in the wildcard ptree.
            // The parameter could be in the wildcard ptree and concrete ptree,
            // which is inferred to be:
            //   wildcard: default param value
            //   concrete: override param value
            auto p_path = ext_path + "." + p_name;
            if (auto wildcard_ptree_node = wildcard_config_ptree_->tryGet(p_path)) {
                auto value = wildcard_ptree_node->peekValue();
                sparta_assert(!value.empty());
                p_value = value;
            }

            // Now see if we should overwrite the param value.
            if (auto concrete_ptree_node = concrete_config_ptree_->tryGet(p_path)) {
                p_value = concrete_ptree_node->peekValue();
                sparta_assert(!p_value.empty());
            }

            auto ext_param = ps->getParameter(p_name);
            sparta_assert(ext_param != nullptr);

            // The param value can still be empty if we have this YAML:
            //
            //   top.cpu.core*.lsu.extension.foobar:
            //     foo: 4
            //     bar: 5
            //   top.cpu.core0.lsu.extension.foobar:
            //     foo: 8
            //
            // And the "foobar" extension has a registered factory, thus
            // the postCreate() method can add more parameters with the
            // default values. Imagine the postCreate() call adds params
            // "fiz" and "buz". There is no mention of those parameters
            // in the YAML, so in this case the "p_value" variable will
            // still be empty.
            //
            // If empty:     Add to the post_create_params_ptree_ ptree. We will add
            //               the default values to the final config YAML file.
            // If not empty: We have a non-default param value in the YAML.
            //               Apply it to the sparta::Parameter and update the
            //               param value in the post_create_params_ptree_ ptree.
            if (p_value.empty()) {
                auto n = post_create_params_ptree_->create(p_path, false /*unrequired*/);
                n->setValue(ext_param->getValueAsString(), false /*unrequired*/, "postCreate()" /*origin*/);
            } else {
                app::ParameterApplicator applicator("", p_value);
                applicator.apply(ext_param);

                auto n = post_create_params_ptree_->create(p_path, false /*unrequired*/);
                n->setValue(p_value, false /*unrequired*/, "postCreate()" /*origin*/);
            }
        }
    }

    // Apply all parameters we were given in the arch/config/extension YAML
    // files to the extension params in its ParameterSet.
    for (auto ptree : {wildcard_config_ptree_, concrete_config_ptree_}) {
        auto ext_node = ptree->tryGet(ext_path, false /*not a leaf*/);
        if (ext_node) {
            for (auto p : ext_node->getChildren()) {
                const auto & [p_name, p_value] = validateParam(p);

                // If this sparta::Parameter already exists, update its value.
                // This handles the parameter value overrides for postCreate()
                // parameters. It also handles non-postCreate() parameters
                // that appeared in both the wildcard and concrete ptrees,
                // i.e. we created/added a string parameter in this loop's
                // first iteration (wildcard/default), then we are setting
                // the value again in the second iteration (concrete/override).
                if (auto p = ps->getParameter(p_name, false)) {
                    app::ParameterApplicator applicator("", p_value);
                    applicator.apply(p);
                    continue;
                }

                // Otherwise, create a new parameter, defaulting to string type.
                const auto & p_desc = p_name;
                auto param = std::make_unique<Parameter<std::string>>(p_name, p_value, p_desc, ps);
                extension->addParameter(std::move(param));
            }
        }
    }

    // Since extensions can be created on the fly, validate this extension's parameters.
    // This already occurred for any extensions accessed/created before finalizeTree().
    // We have to check if this location exists in the device tree since extensions
    // may be created during buildTree() before the node actually exists.
    std::string errs;
    if (auto tn = getRoot_()->getSearchScope()->getChild(loc, false)) {
        if (!extension->getParameters()->validateDependencies(tn, errs)) {
            throw SpartaException("Parameter validation callbacks indicated invalid parameters: ")
                << errs;
        }
    }

    // Cache the extension and return it.
    live_extensions_[loc][extension_name] = extension;
    return extension.get();
}

ExtensionsBase * TreeNodeExtensionManager::createExtension(
    const std::string & loc,
    bool replace)
{
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
    // Most of the time, we will not have a RootTreeNode, i.e. when using CommandLineSimulator.
    // These api calls are made while command line options are being parsed, which is before
    // the CommandLineSimulator has the Simulation object.
    //
    // But there is nothing wrong with users calling this api themselves; it just has to
    // be during or before buildTree().
    sparta_assert(root_ == nullptr || root_->getPhase() == PhasedObject::TREE_BUILDING);

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

        auto ext_name = parent->getName();
        auto & ordered_params = ordered_ext_params_[ext_name];
        ordered_params.insert(leaf->getName());

        auto path = leaf->getPath();
        auto& dst_ptree = TreeNode::hasWildcardCharacters(path) ?
            *wildcard_config_ptree_ : *concrete_config_ptree_;

        // Note that unlike most places in the code, we use getValue()
        // instead of peekValue() here to silence "unread unbound parameter"
        // errors coming from the arch/config YAML params that are specific
        // to extensions.
        auto n = dst_ptree.create(path);
        n->setValue(leaf->getValue(), false /*unrequired*/, leaf->getOrigin());
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
    sparta_assert(getRoot_()->getPhase() == PhasedObject::TREE_FINALIZED);
    write_final_config_ptree_->clear();

    // Helper to update the read count for the given extension parameter
    auto update_read_count = [&](ParameterTree::Node* leaf) {
        sparta_assert(leaf->getChildren().empty());
        sparta_assert(!TreeNode::hasWildcardCharacters(leaf->getPath()));
        leaf->resetReadCount();

        auto parent = leaf->getParent();
        auto grandparent = notNull(parent)->getParent();
        sparta_assert(notNull(grandparent)->getName() == "extension");

        auto ext_name = parent->getName();
        auto tn_loc = notNull(grandparent->getParent())->getPath();
        auto tn = getRoot_()->getSearchScope()->getChild(tn_loc);

        auto ext = const_cast<const TreeNode*>(tn)->getExtension(ext_name);
        if (ext) {
            auto param_name = leaf->getName();
            auto ps = ext->getParameters();
            auto p = ps->getChildAs<ParameterBase>(param_name);
            while (leaf->getReadCount() < p->getReadCount()) {
                leaf->incrementReadCount();
            }
        }

        // Force errors unless suppressed as warnings.
        if (leaf->getReadCount() == 0 && leaf->getRequiredCount() == 0) {
            leaf->incRequired();
        }
    };

    // Walk the entire wildcard ptree and expand * nodes
    wildcard_config_ptree_->visitLeaves([&](const ParameterTree::Node* leaf) {
        // Path will be empty if we have no wildcard config.
        auto path = leaf->getPath();
        if (path.empty()) {
            return false; // don't recurse (true or false, same result)
        }

        const auto & [loc_pattern, ext_name, ext_param_name] = parseExtensionParamPath_(path);
        sparta_assert(TreeNode::hasWildcardCharacters(loc_pattern));

        std::vector<TreeNode*> matching_tns;
        getRoot_()->getSearchScope()->findChildren(loc_pattern, matching_tns);

        for (auto tn : matching_tns) {
            auto concrete_path = tn->getLocation() + ".extension." + ext_name + "." + ext_param_name;
            auto n = write_final_config_ptree_->create(concrete_path, false /*unrequired*/);
            n->setValue(leaf->peekValue(), false /*unrequired*/, leaf->getOrigin());
            update_read_count(n);
        }

        return true; // keep going
    });

    // Walk the entire concrete ptree and add / update nodes
    concrete_config_ptree_->visitLeaves([&](const ParameterTree::Node* leaf) {
        // Path will be empty if we have no concrete config.
        auto path = leaf->getPath();
        if (path.empty()) {
            return false; // don't recurse (true or false, same result)
        }

        sparta_assert(!TreeNode::hasWildcardCharacters(path));
        auto n = write_final_config_ptree_->create(path, false /*unrequired*/);

        // For error reporting purposes, give all param origins e.g.
        //   base_extensions.yaml:15 col:7, override_extensions.yaml:9 col:8
        std::string origin;
        if (n->hasValue() && n->getOrigin() != leaf->getOrigin()) {
            // "n" origin came from wildcard ptree
            origin = n->getOrigin() + ", " + leaf->getOrigin();
        } else {
            origin = leaf->getOrigin();
        }

        n->setValue(leaf->peekValue(), false /*unrequired*/, origin);
        update_read_count(n);
        return true; // keep going
    });

    // Walk the entire postCreate ptree and add / update nodes
    post_create_params_ptree_->visitLeaves([&](const ParameterTree::Node* leaf) {
        // Path will be empty if we have no post create params.
        auto path = leaf->getPath();
        if (path.empty()) {
            return false; // don't recurse (true or false, same result)
        }

        sparta_assert(!TreeNode::hasWildcardCharacters(path));
        auto n = write_final_config_ptree_->create(path, false /*unrequired*/);
        n->setValue(leaf->peekValue(), false /*unrequired*/, leaf->getOrigin());

        // postCreate() parameters are the only ones that can be defined in
        // YAML files and NOT read, so we shouldn't issue errors/warnings
        // about those "unread unbound parameters".
        n->incrementReadCount();
        return true; // keep going
    });

    return write_final_config_ptree_.get();
}

void TreeNodeExtensionManager::checkAllYamlExtensionsCreated(
    bool suppress_exceptions)
{
    sparta_assert(getRoot_()->getPhase() == PhasedObject::TREE_FINALIZED);

    // Keep track of errors already added to the error list. The key is "<loc>-<ext_name>".
    std::unordered_set<std::string> handled_err_keys;
    std::vector<std::string> err_list;

    auto get_error = [](const std::string & loc, const std::string & ext_name)
    {
        std::ostringstream err;
        err << loc << " never instantiated extension \"" << ext_name << "\"";
        return err.str();
    };

    auto check_optional = [&](const ParameterTree & ptree,
                              const std::string & loc,
                              const std::string & ext_name) -> utils::ValidValue<bool>
    {
        utils::ValidValue<bool> is_optional;

        auto ext_root_node = ptree.tryGet(loc, false /*not a leaf*/);
        if (ext_root_node) {
            auto path = ext_root_node->getPath() + ".extension." + ext_name + ".optional";
            ptree.visitNodes([&](const ParameterTree::Node* node) {
                if (node->getPath() == path) {
                    auto value = node->peekValue();
                    if (value == "true") {
                        is_optional = true;
                    } else if (value == "false") {
                        is_optional = false;
                    } else {
                        throw SpartaException("The 'optional' value must be 'true' or 'false', not '")
                            << value << "'.";
                    }
                    return false; // stop visiting nodes; we have our answer
                }
                return true; // keep going
            });
        }

        return is_optional;
    };

    auto extension_optional = [&](const std::string & loc, const std::string & ext_name)
    {
        auto optional_from_wildcard = check_optional(*wildcard_config_ptree_, loc, ext_name);
        auto optional_from_concrete = check_optional(*concrete_config_ptree_, loc, ext_name);

        if (optional_from_wildcard.isValid() && optional_from_concrete.isValid()) {
            // Defer to the concrete extension config. It is considered an override.
            return optional_from_concrete.getValue();
        } else if (optional_from_wildcard.isValid()) {
            return optional_from_wildcard.getValue();
        } else if (optional_from_concrete.isValid()) {
            return optional_from_concrete.getValue();
        } else {
            return false;
        }
    };

    std::vector<const ParameterTree::Node*> wildcard_ext_nodes;
    wildcard_config_ptree_->getRoot()->recursFindPTreeNodesNamed("extension", wildcard_ext_nodes);
    for (auto node : wildcard_ext_nodes) {
        auto loc_pattern = notNull(node->getParent())->getPath();
        std::vector<TreeNode*> matching_tns;
        getRoot_()->getSearchScope()->findChildren(loc_pattern, matching_tns);

        for (auto tn : matching_tns) {
            auto loc = tn->getLocation();
            auto config_ext_names = getAllConfigExtensionNames(loc);
            auto instantiated_ext_names = getAllInstantiatedExtensionNames(loc);

            for (const auto & cfg_ext_name : config_ext_names) {
                bool is_error = false;
                // Not instantiated?
                if (instantiated_ext_names.count(cfg_ext_name) == 0) {
                    // No error if extension is optional.
                    is_error = !extension_optional(loc, cfg_ext_name);
                }

                if (is_error) {
                    auto err = get_error(loc, cfg_ext_name);
                    err_list.push_back(err);
                    handled_err_keys.insert(loc + "-" + cfg_ext_name);
                }
            }
        }
    }

    // Now go through the concrete extension ptree and look for more
    // uninstantiated extensions. The YAML could have been something
    // like this:
    //
    //   top.cpu.core*.lsu.extension.foobar
    //   top.cpu.core0.lsu.extension.fizbuz
    //
    // And we haven't checked the "fizbuz" extension creation yet
    // since that path has no wildcards, i.e. it would not have
    // been in the wildcard extension ptree at all.
    std::vector<const ParameterTree::Node*> concrete_ext_nodes;
    concrete_config_ptree_->getRoot()->recursFindPTreeNodesNamed("extension", concrete_ext_nodes);
    for (auto node : concrete_ext_nodes) {
        auto loc = notNull(node->getParent())->getPath();
        auto config_ext_names = getAllConfigExtensionNames(loc);
        auto instantiated_ext_names = getAllInstantiatedExtensionNames(loc);

        for (const auto & cfg_ext_name : config_ext_names) {
            bool is_error = false;
            // Not instantiated?
            if (instantiated_ext_names.count(cfg_ext_name) == 0) {
                // No error if extension is optional.
                is_error = !extension_optional(loc, cfg_ext_name);
            }

            if (is_error) {
                // Don't accidentally include identical errors/warnings in the
                // event that the YAML files were:
                //
                //   top.cpu.core*.lsu.extension.foobar        (goes to wildcard ptree)
                //   top.cpu.core0.lsu.extension.foobar        (goes to concrete ptree)
                //
                // That is not an invalid use case. The wildcard version might be
                // in a larger YAML file full of default extensions, and the user
                // provided a smaller "override" extension YAML file with different
                // extension parameters. Without checking "handled_err_keys", we would
                // issue redundant errors/warnings.
                if (handled_err_keys.insert(loc + "-" + cfg_ext_name).second) {
                    auto err = get_error(loc, cfg_ext_name);
                    err_list.push_back(err);
                }
            }
        }
    }

    if (!err_list.empty()) {
        std::ostringstream err_oss;
        for (const auto & err_msg : err_list) {
            err_oss << "    ";
            err_oss << (suppress_exceptions ? "NOTE: " : "ERROR: ");
            err_oss << err_msg << "\n";
        }

        if (suppress_exceptions) {
            std::cerr << err_oss.str() << std::endl;
        } else {
            throw SpartaException(err_oss.str());
        }
    }
}

void TreeNodeExtensionManager::doPostCreate_(ExtensionsBase* extension) const
{
    extension->setParameters(std::make_unique<ParameterSet>(nullptr));
    extension->postCreate();
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

const RootTreeNode* TreeNodeExtensionManager::getRoot_() const
{
    return notNull(root_);
}

RootTreeNode* TreeNodeExtensionManager::getRoot_()
{
    return notNull(root_);
}

} // namespace sparta
