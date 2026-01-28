// <TreeNodeExtensions.cpp> -*- C++ -*-


#include "sparta/simulation/TreeNodeExtensions.hpp"

#include <yaml-cpp/parser.h>
#include <ostream>
#include <stack>
#include <unordered_map>
#include <utility>
#include <vector>
#include <algorithm>

#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/Parameter.hpp"
#include "sparta/parsers/YAMLTreeEventHandler.hpp"
#include "sparta/utils/KeyValue.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/Utils.hpp"

namespace YAML {
class EventHandler;
}  // namespace YAML

namespace sparta {

/*!
 * \brief ExtensionParamsOnly::Impl class implementation
 */
class ExtensionsParamsOnly::Impl
{
public:
    Impl() {}
    ~Impl() {}

    void setParameters(std::unique_ptr<ParameterSet> params)
    {
        parameters_ = std::move(params);

        yaml_parameter_set_.reset(new ParameterSet(nullptr));
        auto param_names = parameters_->getNames();
        ParameterSet * ps = yaml_parameter_set_.get();

        for (const auto & param_name : param_names) {
            yaml_parameters_.emplace_back(new Parameter<std::string>(
                param_name, parameters_->getParameter(param_name)->getValueAsString(),
                "Parameter '" + param_name + "' from YAML file", ps));
        }
    }

    ParameterSet * getParameters() const
    {
        return parameters_.get();
    }

    ParameterSet * getYamlOnlyParameters() const
    {
        return yaml_parameter_set_.get();
    }

    void addParameter(std::unique_ptr<ParameterBase> param)
    {
        user_parameters_.emplace_back(std::move(param));
    }

private:
    std::unique_ptr<ParameterSet> parameters_;
    std::unique_ptr<ParameterSet> yaml_parameter_set_;
    std::vector<std::unique_ptr<Parameter<std::string>>> yaml_parameters_;
    std::vector<std::unique_ptr<ParameterBase>> user_parameters_;
};

ExtensionsParamsOnly::ExtensionsParamsOnly() :
    impl_(new ExtensionsParamsOnly::Impl)
{
}

ExtensionsParamsOnly::~ExtensionsParamsOnly()
{
}

void ExtensionsParamsOnly::setParameters(std::unique_ptr<ParameterSet> params)
{
    impl_->setParameters(std::move(params));
}

ParameterSet * ExtensionsParamsOnly::getParameters() const
{
    return impl_->getParameters();
}

ParameterSet * ExtensionsParamsOnly::getYamlOnlyParameters() const
{
    return impl_->getYamlOnlyParameters();
}

void ExtensionsParamsOnly::addParameter(std::unique_ptr<ParameterBase> param)
{
    impl_->addParameter(std::move(param));
}

/*!
 * \brief ExtensionDescriptor::Impl class implementation
 */
class ExtensionDescriptor::Impl
{
public:
    Impl() {}
    ~Impl() {}

    void setNodeLocation(const std::string & location)
    {
        node_location_ = location;
    }

    void setName(const std::string & name)
    {
        name_ = name;
    }

    void addParameterAsString(const std::string & prm_name,
                              const std::string & prm_value)
    {
        if (parameters_.find(prm_name) != parameters_.end()) {
            throw SpartaException("Parameter named '") << prm_name <<
                                "' already exists in this descriptor";
        }
        parameters_[prm_name] = prm_value;
    }

    const std::string & getNodeLocation() const
    {
        return node_location_;
    }

    const std::string & getName() const
    {
        return name_;
    }

    std::unique_ptr<ParameterSet> cloneParameters() const
    {
        sparta_assert(alive_parameters_.empty());
        std::unique_ptr<ParameterSet> parameters(new ParameterSet(nullptr));

        std::vector<std::unique_ptr<Parameter<std::string>>> alive_parameters;
        alive_parameters.reserve(parameters_.size());

        for (const auto & param_kv : parameters_) {
            alive_parameters.emplace_back(new Parameter<std::string>(
                param_kv.first,
                param_kv.second,
                param_kv.first + " (extension)",
                parameters.get()));
        }

        alive_parameters_.swap(alive_parameters);
        return parameters;
    }

private:
    std::string node_location_;
    std::string name_;
    std::unordered_map<std::string, std::string> parameters_;
    mutable std::vector<std::unique_ptr<Parameter<std::string>>> alive_parameters_;
};

/*!
 * \brief ExtensionDescriptor class implementation
 */
ExtensionDescriptor::ExtensionDescriptor() :
    impl_(new ExtensionDescriptor::Impl)
{
}

ExtensionDescriptor::~ExtensionDescriptor()
{
}

void ExtensionDescriptor::setNodeLocation(const std::string & location)
{
    impl_->setNodeLocation(location);
}

void ExtensionDescriptor::setName(const std::string & name)
{
    impl_->setName(name);
}

void ExtensionDescriptor::addParameterAsString(
    const std::string & prm_name,
    const std::string & prm_value)
{
    impl_->addParameterAsString(prm_name, prm_value);
}

const std::string & ExtensionDescriptor::getNodeLocation() const
{
    return impl_->getNodeLocation();
}

const std::string & ExtensionDescriptor::getName() const
{
    return impl_->getName();
}

std::unique_ptr<ParameterSet> ExtensionDescriptor::cloneParameters() const
{
    return impl_->cloneParameters();
}

/*!
 * \brief TreeNode::ExtensionsBase class implementation
 */
TreeNode::ExtensionsBase::ExtensionsBase()
    : uuid_(generateUUID())
{
}

TreeNode::ExtensionsBase::~ExtensionsBase()
{
}

/*!
 * \brief YAML parser class to turn multi-extension definition files:
 *
 *   content:
 *
 *     extension:
 *       node:       top.core0.lsu
 *       name:       foo
 *       params:
 *         <key>:    <value>
 *         <key>:    <value>
 *           :         :
 *     extension:
 *       node:       top.core0.lsu
 *       name:       bar
 *       params:
 *         <key>:    <value>
 *         <key>:    <value>
 *           :         :
 *
 * Into a vector of ExtensionDescriptor objects. This class does not
 * bind any descriptors / extensions to tree nodes - that is left up
 * to the caller to do.
 */
class TreeNodeExtensionFileParserYAML
{
    /*!
     * \brief Event handler for YAML parser
     */
    class TreeNodeExtensionFileEventHandlerYAML :
        public sparta::YAMLTreeEventHandler
    {
    private:
        std::stack<bool> in_extension_stack_;
        bool in_parameters_section_ = false;
        ExtensionDescriptorVec completed_descriptors_;

        std::string node_location_;
        std::string extension_name_;
        std::unordered_map<std::string, std::string> parameters_as_strings_;

        /*!
         * \brief Reserved keywords for this parser's dictionary
         */
        static constexpr char KEY_CONTENT[]     = "content";
        static constexpr char KEY_EXTENSION[]   = "extension";
        static constexpr char KEY_NODE[]        = "node";
        static constexpr char KEY_NAME[]        = "name";
        static constexpr char KEY_PARAMS[]      = "params";

        virtual bool handleEnterMap_(
            const std::string & key,
            NavVector & context) override final
        {
            (void) context;

            if (key == KEY_CONTENT) {
                return false;
            }

            if (key == KEY_EXTENSION) {
                if (!in_extension_stack_.empty()) {
                    throw SpartaException("Nested extension definitions are not supported");
                }
                this->prepareForNextDescriptor_();
                in_extension_stack_.push(true);
                return false;
            }

            if (key == KEY_PARAMS) {
                if (in_parameters_section_) {
                    throw SpartaException("Nested extension parameters are not supported");
                }
                in_parameters_section_ = true;
                return false;
            }

            if (!key.empty()) {
                throw SpartaException("Unrecognized key found in definition file: ") << key;
            }
            return false;
        }

        virtual void handleLeafScalar_(
            TreeNode * n,
            const std::string & value,
            const std::string & assoc_key,
            const std::vector<std::string> & captures,
            node_uid_t uid) override final
        {
            (void) n;
            (void) captures;
            (void) uid;

            if (in_parameters_section_) {
                parameters_as_strings_[assoc_key] = value;
            } else {
                if (assoc_key == KEY_NODE) {
                    node_location_ = value;
                } else if (assoc_key == KEY_NAME) {
                    extension_name_ = value;
                } else {
                    std::ostringstream oss;
                    oss << "Unrecognized key in extension definition "
                           "file: '" << assoc_key << "'";
                    throw SpartaException(oss.str());
                }
            }
        }

        virtual bool handleExitMap_(
            const std::string & key,
            const NavVector & context) override final
        {
            (void) context;

            if (key == KEY_EXTENSION) {
                if (extension_name_.empty()) {
                    throw SpartaException(
                        "Each extension section must contain a 'name' entry");
                }

                sparta_assert(!node_location_.empty());
                in_extension_stack_.pop();

                std::unique_ptr<ExtensionDescriptor> descriptor(new ExtensionDescriptor);
                descriptor->setNodeLocation(node_location_);
                descriptor->setName(extension_name_);
                for (const auto & pv : parameters_as_strings_) {
                    descriptor->addParameterAsString(pv.first, pv.second);
                }
                completed_descriptors_.emplace_back(descriptor.release());

            } else if (key == KEY_PARAMS) {
                in_parameters_section_ = false;
            }

            return false;
        }

        virtual bool isReservedKey_(
            const std::string & key) const override final
        {
            //If we are inside a parameters block, every key is "reserved" (allowed)
            if (in_parameters_section_) {
                return true;
            }
            return (key == KEY_CONTENT      ||
                    key == KEY_EXTENSION    ||
                    key == KEY_NODE         ||
                    key == KEY_NAME         ||
                    key == KEY_PARAMS);
        }

        void prepareForNextDescriptor_()
        {
            node_location_ = "top";
            extension_name_.clear();
            parameters_as_strings_.clear();
        }

    public:
        TreeNodeExtensionFileEventHandlerYAML(
            const std::string & def_file,
            NavVector device_trees) :
                sparta::YAMLTreeEventHandler(def_file, device_trees, false)
        {
        }

        ExtensionDescriptorVec getDescriptors()
        {
            return std::move(completed_descriptors_);
        }
    };

public:
    explicit TreeNodeExtensionFileParserYAML(const std::string & def_file) :
        fin_(def_file),
        parser_(fin_),
        def_file_(def_file)
    {
        if (!fin_) {
            throw SpartaException(
                "Failed to open tree node extension file for read \"")
                << def_file_ << "\"";
        }
    }

    explicit TreeNodeExtensionFileParserYAML(std::istream & content) :
        fin_(),
        parser_(content),
        def_file_("<istream>")
    {
    }

    ExtensionDescriptorVec parseIntoDescriptors(TreeNode * context)
    {
        std::shared_ptr<YAMLTreeEventHandler::NavNode> scope(
            new YAMLTreeEventHandler::NavNode({
                nullptr, context, {}, 0}));

        TreeNodeExtensionFileEventHandlerYAML handler(def_file_, {scope});

        while (parser_.HandleNextDocument(*((YP::EventHandler*)&handler))) {}

        return handler.getDescriptors();
    }

private:
    std::ifstream fin_;
    YP::Parser parser_;
    std::string def_file_;
};

/*!
 * \brief Given a tree node extension YAML file, parse it out into individual
 * descriptors, one for each extension defined in the file.
 */
ExtensionDescriptorVec createExtensionDescriptorsFromFile(
    const std::string & def_file,
    TreeNode * context)
{
    TreeNodeExtensionFileParserYAML parser(def_file);
    return parser.parseIntoDescriptors(context);
}

/*!
 * \brief Given a tree node extension definition string, parse it out into
 * individual descriptors.
 */
ExtensionDescriptorVec createExtensionDescriptorsFromDefinitionString(
    const std::string & def_string,
    TreeNode * context)
{
    std::stringstream ss(def_string);
    TreeNodeExtensionFileParserYAML parser(ss);
    return parser.parseIntoDescriptors(context);
}

}
