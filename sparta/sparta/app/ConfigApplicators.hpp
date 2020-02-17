// <ConfigApplicators.h> -*- C++ -*-


/*!
 * \file ConfigApplicators.hpp
 * \brief Configuration Applicators
 */

#ifndef __CONFIG_APPLICATORS_H__
#define __CONFIG_APPLICATORS_H__

#include <cinttypes>
#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <ostream>

#include "sparta/parsers/ConfigParserYAML.hpp"
#include "sparta/simulation/ParameterTree.hpp"
#include "sparta/simulation/TreeNode.hpp"

namespace sparta {
namespace app {

/*!
 * \brief Base class for applying parameters or configuration files to the
 * simulator device tree. Contains a parameter or configuration file
 * "action" to be applied to a tree.
 *
 * This base class exists for app types of simulator configuration so that
 * they can be ordered in a single vector.
 *
 * \note Subclasses must be copy-constructable and copy-assignable..
 */
class ConfigApplicator
{
public:
    /*!
     * \brief Virtual destructor
     */
    virtual ~ConfigApplicator() {}

    /*!
     * \brief Type for dictating how tryApply should behave
     */
    enum class ApplySuccessCondition {
        ASC_DEFER = 0, //!< Defer to another layer in the parameter-application process to make the decision (i.e. do not override)
        ASC_MUST_ASSIGN = 1, //!< Must assign the parameter to success
        ASC_IGNORE = 2, //!< Ignore failures to assign the parameter
        MAX_ASC
    };

    /*!
     * \brief Type for dictating how to filter the parameter application.
     * (i.e. only apply to certain parts of the tree)
     */
    enum class LocationFilter {
        /*!
         * \brief Apply configuration to any nodes that match pattern and type (typical)
         */
        ALL = 0,

        /*!
         * \brief Apply configuration to any nodes that are or are below a given filter node.
         * Typically, this is used with ApplySuccessCondition::ASC_IGNORE to re-apply parameters
         * to a part of the tree
         */
        AT_OR_BELOW_NODE,

        /*!
         * \brief Apply configuration only to a given filter node if applicable. (unimplemented)
         */
        //AT_NODE,

        /*!
         * \brief Apply configuration to any nodes that are below a given filter node, excluding
         * the filter node itself. (unimplemneted)
         */
        //BELOW_NODE,

        /*!
         * \brief Enum maximum (invalid)
         */
        MAX_AF
    };

    /*!
     * \brief Represents a filter for applying parameters based on tree location.
     */
    struct ApplyFilter {
        /*!
         * \brief Construct a null-filter (filters nothing)
         */
        ApplyFilter()
            : locfilter_(LocationFilter::ALL),
              locfilter_node_(nullptr)
        {;}

        /*!
         * \brief Construct a location-filter
         * \param[in] locfilter Location-based filter policy
         * \param[in] filternode Node at which filter is applied
         */
        ApplyFilter(LocationFilter locfilter, const TreeNode* filternode)
            : locfilter_(locfilter),
              locfilter_node_(filternode)
        {
            sparta_assert(locfilter_node_,
                        "cannot create a location filter for parameter application with a null filter node");
        }

        ApplyFilter(const ApplyFilter&) = default;
        ApplyFilter& operator=(const ApplyFilter&) = default;

        /*!
         * \brief Test a node against this filter.
         * \param[in] n Node to test
         * \return true if the node (n) passes the filter, false if not.
         */
        bool test(const TreeNode* n) const {
            sparta_assert(n);
            switch(locfilter_)
            {
            case LocationFilter::ALL:
                return true;
                break;
            case LocationFilter::AT_OR_BELOW_NODE:
                return n->isDescendantOf(locfilter_node_);
                break;
            case LocationFilter::MAX_AF:
                // error out
                break;
            };
            sparta_assert(false,
                        "Unhandled ConfigApplciator::ApplyFilter location filter case: "
                        << static_cast<uint32_t>(locfilter_));
            return false;
        }

        /*!
         * \brief Return the location filter policy
         */
        LocationFilter getLocationFilter() const { return locfilter_; }

        /*!
         * \brief RReturn the location filter node (may be none)
         */
        const TreeNode* getLocationFilterNode() const { return locfilter_node_; }

        /*!
         * \brief Prinit a configuration-application filter
         */
        friend std::ostream& operator<<(std::ostream& o,
                                        const ConfigApplicator::ApplyFilter* filter) {
            if(filter == nullptr){
                o << "<null config filter ptr>";
                return o;
            }

            return o << *filter;
        }

        /*!
         * \brief Prinit a configuration-application filter
         */
        friend std::ostream& operator<<(std::ostream& o, const ConfigApplicator::ApplyFilter& filter) {
            o << "<cfg-filter ";
            switch(filter.getLocationFilter()){
            case LocationFilter::ALL:
                o << "all_nodes";
                break;
            case LocationFilter::AT_OR_BELOW_NODE:
                o << "at_or_below_node " << filter.getLocationFilterNode()->getLocation();
                break;
            default:
                sparta_assert(0,
                            "Unhandled ConfigApplciator::ApplyFilter location filter case: " << static_cast<uint32_t>(filter.getLocationFilter()));
            };
            o << ">";
            return o;
        }

    private:
        LocationFilter locfilter_;
        const TreeNode* locfilter_node_;
    };

    /*!
     * \brief Apply the parameter contained  in this object to the unbound
     * (virtual) parameter tree \a ptree
     * \note This is done before device-tree-building so that parameters can
     * be used for defining topology
     */
    virtual void applyUnbound(sparta::ParameterTree& ptree, bool verbose=false) const = 0;

    /*!
     * \brief Apply the parameter contained in this object to the tree
     * starting at \a root
     * \param root Root of tree to apply parameter(s) too
     * \throw If parameter can not be set
     */
    void apply(sparta::TreeNode* root,
               ApplyFilter filter=ApplyFilter(),
               bool verbose=false) {
        tryApply(root, ApplySuccessCondition::ASC_DEFER, filter, verbose);
    }

    /*!
     * \brief Apply the parameter contained in this object to the tree
     * starting at \a root
     * \param root Root of tree to apply parameter(s) too
     */
    virtual void tryApply(sparta::TreeNode* root,
                          ApplySuccessCondition required,
                          ApplyFilter filter=ApplyFilter(),
                          bool verbose=false) const = 0;

    /*!
     * \brief Render this parameter action as a string
     */
    virtual std::string stringize() const = 0;
};

/*!
 * \brief Applies a value to a parameter node pattern
 */
class ParameterApplicator : public ConfigApplicator
{
    /*!
     * \brief Locations to apply parameter value to
     */
    const std::string loc_pattern_;

    /*!
     * \brief Value to apply to any parameter node matching loc_pattern
     */
    const std::string value_;

    /*!
     * \brief Default success condition. May be overridden by tryApply
     */
    const ApplySuccessCondition default_success_cond_;

    /*!
     * \brief Override or defer to default success condition with a given
     * success condition
     */
    ApplySuccessCondition getSuccessCondition(ApplySuccessCondition ovr) const {
        if(ovr == ApplySuccessCondition::ASC_DEFER){
            return default_success_cond_;
        }
        return ovr;
    }

public:
    ParameterApplicator(const std::string& loc_pattern,
                        const std::string& value,
                        ApplySuccessCondition required=ApplySuccessCondition::ASC_MUST_ASSIGN) :
        loc_pattern_(loc_pattern), value_(value), default_success_cond_(required)
    {;}

    std::string stringize() const override {
        std::stringstream ss;
        ss << "Parameter \"" << loc_pattern_ << "\" <- value: \"" << value_ << "\"";
        switch(default_success_cond_){
        case ApplySuccessCondition::ASC_MUST_ASSIGN:
            break; // Ok
        case ApplySuccessCondition::ASC_IGNORE:
            ss << " [optional parameter]"; // Show
            break;
        case ApplySuccessCondition::ASC_DEFER:
            break; // Ok
        default:
            ss << " Unknown ApplySuccessCondition value: " << (int)default_success_cond_;
            break;
        }
        return ss.str();
    }

    void tryApply(sparta::TreeNode* root,
                  ApplySuccessCondition required,
                  ApplyFilter filter,
                  bool verbose) const override
    {
        const ApplySuccessCondition effective_asc = getSuccessCondition(required);

        assert(root);
        std::vector<sparta::TreeNode*> results;
        uint32_t found = root->findChildren(loc_pattern_, results);

        // Filter the found nodes by the given filter
        std::vector<sparta::TreeNode*> filtered_results;
        std::copy_if(results.begin(), results.end(),
                     std::back_inserter(filtered_results), [&](TreeNode* n)->bool{return filter.test(n);});

        if(0 == filtered_results.size()){
            // These parameters will be in the unbound list as well. Unresolved
            // parameters will be handled by the unbound tree so allow missing nodes
            // while parsing this for now. Later, unread unbound parameters which do not
            // correspond to sparta::Parameter Nodes will cause exceptions
            //! \todo Allow meta-data attached to virtual parameter trees
            switch(effective_asc){
            case ApplySuccessCondition::ASC_MUST_ASSIGN:
                throw sparta::SpartaException("Failed to find any nodes matching pattern \"")
                    << loc_pattern_ << "\" and filter " << filter << " for which to set parameter value \""
                    << value_ << "\"";
                break;
            case ApplySuccessCondition::ASC_IGNORE:
                break;
            case ApplySuccessCondition::ASC_DEFER:
                sparta_assert(0, "ParameterApplicator cannot have success policy of "
                            "ASC_DEFER. required=" << (int)required << " default="
                            << (int)default_success_cond_ << ". This is likely a bug in "
                            "sparta::app unless other code is creating ParameterApplicators");
            default:
                sparta_assert(0, "Unknown ApplySuccessCondition value: " << (int)required);
            };
            return;
        }
        uint32_t set = 0;
        for(sparta::TreeNode* node : filtered_results){
            sparta::ParameterBase* p = dynamic_cast<sparta::ParameterBase*>(node);
            if(nullptr != p){
                if(false == p->isVector()){
                    // Assign the string directly to this node.
                    // It is possible that this can just be parsed as YAML.
                    p->setValueFromString(value_);
                }else{
                    // Parse the string as YAML and assign it to this node
                    ParameterTree ptree;
                    sparta::ConfigParser::YAML::EventHandler handler("<command line>",
                                                                   {p},
                                                                   ptree,
                                                                   {},
                                                                   verbose);

                    // These parameters will be in the unbound list first
                    // and then applied later where it MUST match existing nodes
                    handler.allowMissingNodes(false);
                    std::stringstream input(value_);
                    YP::Parser parser(input);
                    while(parser.HandleNextDocument(*((YP::EventHandler*)&handler))) {}

                    if(handler.getErrors().size() != 0){
                        sparta::SpartaException ex("One or more errors detected while parsing "
                                               "command line parameter values.\n"
                                               "Attempting to intrerpret YAML value '");
                        ex << value_ << "' at " << p->getLocation() << "\n";
                        for(const std::string& es : handler.getErrors()){
                            ex << es << '\n';
                        }
                        throw  ex;
                    }
                }
                ++set;
            }
        }
        if(0 == set && effective_asc == ApplySuccessCondition::ASC_MUST_ASSIGN){
            throw sparta::SpartaException("Found ")
                << found << " nodes matching parameter pattern \""
                << loc_pattern_ << "\". " << filtered_results.size() << " matched filter "
                << filter << "too. But none of them were actually parameters";
        }
    }

    void applyUnbound(sparta::ParameterTree& ptree, bool verbose=false) const override {
        (void) verbose;
        const bool required = default_success_cond_ == ApplySuccessCondition::ASC_MUST_ASSIGN;
        ptree.set(loc_pattern_, value_, required, "command line");
    }
};

/*!
 * \brief Applies a new default value to a parameter node pattern
 */
class ParameterDefaultApplicator : public ConfigApplicator
{
    /*!
     * \brief Locations to apply parameter value to
     */
    const std::string loc_pattern_;

    /*!
     * \brief Value to apply to any parameter node matching loc_pattern
     */
    const std::string value_;

    /*!
     * \brief Default success condition. May be overridden by tryApply
     */
    const ApplySuccessCondition default_success_cond_;

    /*!
     * \brief Override or defer to default success condition with a given
     * success condition
     */
    ApplySuccessCondition getSuccessCondition(ApplySuccessCondition ovr) const {
        if(ovr == ApplySuccessCondition::ASC_DEFER){
            return default_success_cond_;
        }
        return ovr;
    }

public:
    ParameterDefaultApplicator(const std::string& loc_pattern,
                               const std::string& value,
                               ApplySuccessCondition required=ApplySuccessCondition::ASC_MUST_ASSIGN) :
        loc_pattern_(loc_pattern), value_(value), default_success_cond_(required)
    {;}

    std::string stringize() const override {
        std::stringstream ss;
        ss << "Parameter \"" << loc_pattern_ << "\" <- arch value: \"" << value_ << "\"";
        switch(default_success_cond_){
        case ApplySuccessCondition::ASC_MUST_ASSIGN:
            break; // Ok
        case ApplySuccessCondition::ASC_IGNORE:
            ss << " [optional parameter]"; // Show
            break;
        case ApplySuccessCondition::ASC_DEFER:
            break; // Ok
        default:
            ss << " Unknown ApplySuccessCondition value: " << (int)default_success_cond_;
            break;
        }
        return ss.str();
    }

    void tryApply(sparta::TreeNode* root,
                  ApplySuccessCondition required,
                  ApplyFilter filter=ApplyFilter(),
                  bool verbose=false) const override
    {
        const ApplySuccessCondition effective_asc = getSuccessCondition(required);

        assert(root);
        std::vector<sparta::TreeNode*> results;
        uint32_t found = root->findChildren(loc_pattern_, results);

        // Filter the found nodes by the given filter
        std::vector<sparta::TreeNode*> filtered_results;
        std::copy_if(results.begin(), results.end(),
                     std::back_inserter(filtered_results), [&](TreeNode* n)->bool{return filter.test(n);});

        if(0 == filtered_results.size()){
            // These parameters will be in the unbound list as well. Unresolved
            // parameters will be handled by the unbound tree so allow missing nodes
            // while parsing this for now. Later, unread unbound parameters which do not
            // correspond to sparta::Parameter Nodes will cause exceptions
            //! \todo Allow meta-data attached to virtual parameter trees
            switch(effective_asc){
            case ApplySuccessCondition::ASC_MUST_ASSIGN:
                throw sparta::SpartaException("Failed to find any nodes matching pattern \"")
                    << loc_pattern_ << "\" and filter " << filter << " for which to set parameter value \""
                    << value_ << "\"";
                break;
            case ApplySuccessCondition::ASC_IGNORE:
                break;
            case ApplySuccessCondition::ASC_DEFER:
                sparta_assert(0, "ParameterDefaultApplicator cannot have success policy of "
                            "ASC_DEFER. required=" << (int)required << " default="
                            << (int)default_success_cond_ << ". This is likely a bug in "
                            "sparta::app unless other code is creating ParameterApplicators");
            default:
                sparta_assert(0, "Unknown ApplySuccessCondition value: " << (int)required);
            };
            return;
        }
        uint32_t set = 0;
        for(sparta::TreeNode* node : filtered_results){
            sparta::ParameterBase* p = dynamic_cast<sparta::ParameterBase*>(node);
            if(nullptr != p){
                if(false == p->isVector()){
                    // Assign the string directly to this node.
                    // It is possible that this can just be parsed as YAML.
                    p->overrideDefaultFromString(value_);
                }else{
                    // Parse the string as YAML and assign it to this node
                    ParameterTree ptree;
                    sparta::ConfigParser::YAML::EventHandler handler("<command line>",
                                                                   {p},
                                                                   ptree,
                                                                   {},
                                                                   verbose);
                    // These parameters will be in the unbound list first
                    // and then applied later where it MUST match existing nodes
                    handler.allowMissingNodes(false);
                    handler.writeToDefault(true); // Overwrite DEFAULT value, not value
                    std::stringstream input(value_);
                    YP::Parser parser(input);
                    while(parser.HandleNextDocument(*((YP::EventHandler*)&handler))) {}

                    if(handler.getErrors().size() != 0){
                        sparta::SpartaException ex("One or more errors detected while parsing "
                                               "command line parameter values.\n"
                                               "Attempting to intrerpret YAML value '");
                        ex << value_ << "' at " << p->getLocation() << "\n";
                        for(const std::string& es : handler.getErrors()){
                            ex << es << '\n';
                        }
                        throw  ex;
                    }
                }
                ++set;
            }
        }
        if(0 == set && effective_asc == ApplySuccessCondition::ASC_MUST_ASSIGN){
            throw sparta::SpartaException("Found ")
                << found << " nodes matching parameter pattern \""
                << loc_pattern_ << "\" and filter " << filter << " but none of them were actually parameters";
        }
    }

    void applyUnbound(sparta::ParameterTree& ptree, bool verbose=false) const override {
        (void) verbose;
        const bool required = default_success_cond_ == ApplySuccessCondition::ASC_MUST_ASSIGN;
        ptree.set(loc_pattern_, value_, required, "command line");
    }
};


/*!
 * \brief Applies a configuration file to a node pattern
 */
class NodeConfigFileApplicator : public ConfigApplicator
{
    /*!
     * \brief Locations to apply parameter value to
     */
    std::string loc_pattern_;

    /*!
     * \brief Filename of configuration file to apply at any nodes matching
     * loc_pattern
     */
    std::string filename_;

    /*!
     * \brief The include paths to look for configurations using include directive
     */
    const std::vector<std::string> include_paths_;

    /*!
     * \brief Print with verbose formatting
     */
    bool verbose_;

public:

    NodeConfigFileApplicator(const std::string& loc_pattern,
                             const std::string& filename,
                             const std::vector<std::string>& include_paths,
                             bool verbose=false) :
        loc_pattern_(loc_pattern), filename_(filename),
        include_paths_(include_paths), verbose_(verbose)
    {(void)verbose_;}

    std::string stringize() const override {
        std::stringstream ss;
        ss << "Node \"" << loc_pattern_ << "\" <- file: \"" << filename_ << "\"";
        return ss.str();
    }

    void tryApply(sparta::TreeNode* root,
                  ApplySuccessCondition asc,
                  ApplyFilter filter=ApplyFilter(),
                  bool verbose=false) const override
    {
        sparta_assert(asc != ApplySuccessCondition::ASC_DEFER,
                    "NodeConfigFileApplicator cannot have success policy of "
                    "ASC_DEFER. This is likely a bug in sparta::app unless other code "
                    "is creating ParameterApplicators");

        assert(root);
        std::vector<sparta::TreeNode*> results;
        root->findChildren(loc_pattern_, results);

        // Filter the found nodes by the given filter
        std::vector<sparta::TreeNode*> filtered_results;
        std::copy_if(results.begin(), results.end(),
                     std::back_inserter(filtered_results), [&](TreeNode* n)->bool{return filter.test(n);});

        if(0 == filtered_results.size() && asc == ApplySuccessCondition::ASC_MUST_ASSIGN){
            throw sparta::SpartaException("Failed to find any nodes matching pattern \"")
                << loc_pattern_ << " and filter " << filter << " for which to apply configuration file \""
                << filename_ << "\"";
        }
        sparta::ConfigParser::YAML param_file(filename_, include_paths_);
        // These parameters will be in the unbound list as well. Unresolved parameters will be
        // handled by the unbound tree so allow missing nodes while parsing this for now.
        // Later, unread unbound parameters which do not correspond to sparta::Parameter Nodes
        // will cause exceptions
        //! \todo Allow meta-data attached to virtual parameter trees
        //! \todo Support tracking of set parameters and error on 0-paramerters set based on
        //!       ASC_ policy.
        param_file.allowMissingNodes(asc != ApplySuccessCondition::ASC_MUST_ASSIGN);
        param_file.setParameterApplyFilter(std::bind(&ApplyFilter::test, &filter, std::placeholders::_1));
        for(sparta::TreeNode* node : filtered_results){
            param_file.consumeParameters(node, verbose);
        }
    }

    void applyUnbound(sparta::ParameterTree& ptree, bool verbose=false) const override {
        // Parse YAML file without actual device tree and extract parameter tree
        TreeNode dummy("dummy", "dummy");
        sparta::ConfigParser::YAML param_file(filename_, include_paths_);
        param_file.allowMissingNodes(true);
        param_file.consumeParameters(&dummy, verbose);
        //param_file.getParameterTree().recursPrint(std::cout);
        ptree.create(loc_pattern_, false)->appendTree(param_file.getParameterTree().getRoot()); // Apply to existing ptree
    }
};

/*!
 * \brief Applies an architectural configuration (parameter defaults) to a
 * virtual parameter tree. This does not support applying, only applyUnbound.
 */
class ArchNodeConfigFileApplicator : public ConfigApplicator
{
    /*!
     * \brief Locations to apply parameter value to
     */
    const std::string loc_pattern_;

    /*!
     * \brief Filename of configuration file to apply at any nodes matching
     * loc_pattern
     */
    const std::string filename_;

    /*!
     * \brief The include paths to look for arch include definitions
     */
    const std::vector<std::string> include_paths_;

public:

    ArchNodeConfigFileApplicator(const std::string& loc_pattern,
                                 const std::string& filename,
                                 const std::vector<std::string> & include_paths) :
        loc_pattern_(loc_pattern), filename_(filename),
        include_paths_(include_paths)
    {;}

    std::string stringize() const override {
        std::stringstream ss;
        ss << "ArchCfg Node \"" << loc_pattern_ << "\" <- file: \"" << filename_ << "\"";
        return ss.str();
    }

    void tryApply(sparta::TreeNode* root,
                  ApplySuccessCondition asc,
                  ApplyFilter filter=ApplyFilter(),
                  bool verbose=false) const override
    {
        (void) root;
        (void) filter;
        (void) asc;
        (void) verbose;
        throw SpartaException("Cannot \"apply\" ArchNodeConfigFileApplicator - it can only be "
                            "applied to a virtual parameter tree (applyUnbound). It is a bug "
                            "in SPARTA that this function is being called.");
    }

    void applyUnbound(sparta::ParameterTree& ptree, bool verbose=false) const override {
        // Parse YAML file without actual device tree and extract parameter tree
        TreeNode dummy("dummy", "dummy");
        sparta::ConfigParser::YAML param_file(filename_, include_paths_);
        param_file.allowMissingNodes(true);
        param_file.consumeParameters(&dummy, verbose);
        //param_file.getParameterTree().recursPrint(std::cout);
        ptree.create(loc_pattern_, false)->appendTree(param_file.getParameterTree().getRoot()); // Apply to existing ptree
    }
};


typedef std::vector<std::pair<std::string, std::string>> StringPairVec;
typedef std::vector<std::unique_ptr<ConfigApplicator>>   ConfigVec;


} // namespace app
} // namespace sparta


// __CONFIG_APPLICATORS_H__
#endif
