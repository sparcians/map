// <TreeNodeExtensions.h> -*- C++ -*-

/**
 * \file TreeNodeExtensions.hpp
 *
 */

#ifndef __SPARTA_TREE_NODE_EXTENSIONS_H__
#define __SPARTA_TREE_NODE_EXTENSIONS_H__

#include <memory>
#include <string>

#include "sparta/simulation/TreeNode.hpp"

namespace sparta {

class ParameterSet;

/*!
 * \brief Helper class used to trivially extend TreeNode parameter sets (but not
 * any additional functionality beyond that)
 */
class ExtensionsParamsOnly : public TreeNode::ExtensionsBase
{
public:
    explicit ExtensionsParamsOnly();
    virtual ~ExtensionsParamsOnly();
    virtual void setParameters(std::unique_ptr<ParameterSet> params) override final;
    virtual ParameterSet * getParameters() override final;
    virtual ParameterSet * getYamlOnlyParameters() override final;

private:
    class Impl;

    std::shared_ptr<Impl> impl_;
};

/*!
 * \brief Descriptor class which provides basic information about an extended
 * tree node: device tree location, extension name, and parameter name-values.
 */
class ExtensionDescriptor
{
public:
    ExtensionDescriptor();
    ~ExtensionDescriptor();

    void setNodeLocation(const std::string & location);

    void setName(const std::string & name);

    void addParameterAsString(const std::string & prm_name,
                              const std::string & prm_value);

    const std::string & getNodeLocation() const;

    const std::string & getName() const;

    std::unique_ptr<ParameterSet> cloneParameters() const;

private:
    class Impl;
    std::shared_ptr<Impl> impl_;
};

/*!
 * \brief Given a tree node extension YAML file, parse it out into individual
 * descriptors, one for each extension defined in the file.
 */
ExtensionDescriptorVec createExtensionDescriptorsFromFile(
    const std::string & def_file,
    TreeNode * context);

/*!
 * \brief Given a tree node extension definition string, parse it out into
 * individual descriptors.
 */
ExtensionDescriptorVec createExtensionDescriptorsFromDefinitionString(
    const std::string & def_string,
    TreeNode * context);

}

// __SPARTA_TREE_NODE_EXTENSIONS_H__
#endif
