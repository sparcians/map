// <TreeNodeExtensions.hpp> -*- C++ -*-

/**
 * \file TreeNodeExtensions.hpp
 * \brief This file provides classes used to extend TreeNode's so they can
 * create and own hidden C++ objects without affecting simulator topology.
 * \see sparta/extensions/README.md
 */

#pragma once

#include "sparta/extensions/TreeNodeExtensionsSupport.hpp"

#include <memory>
#include <string>
#include <vector>

namespace sparta {

class ParameterBase;
class ParameterSet;
class TreeNode;

namespace detail {

/*!
 * \brief Base class used to extend TreeNode parameter sets
 */
class ExtensionsBase
{
public:
    ExtensionsBase();
    virtual ~ExtensionsBase();
    virtual std::string getClassName() const { return "unknown"; }
    virtual void setParameters(std::unique_ptr<ParameterSet> params) = 0;
    virtual ParameterSet * getParameters() const = 0;
    virtual ParameterSet * getYamlOnlyParameters() const = 0;
    virtual ParameterSet * getParameters() = 0;
    virtual ParameterSet * getYamlOnlyParameters() = 0;
    virtual void addParameter(std::unique_ptr<ParameterBase> param) = 0;
    virtual void postCreate() {}

    /*!
     * \brief All parameters are stored internally as a Parameter<std::string>.
     * Call this method templated on the known specific type to parse the string
     * as type T.
     * \throw Throws an exception if the string cannot be parsed into type T.
     * \note These are the ONLY parameter types supported by extensions:
     *
     *   getParameterValueAs<int8_t>
     *   getParameterValueAs<uint8_t>
     *
     *   getParameterValueAs<int16_t>
     *   getParameterValueAs<uint16_t>
     *
     *   getParameterValueAs<int32_t>
     *   getParameterValueAs<uint32_t>
     *
     *   getParameterValueAs<int64_t>
     *   getParameterValueAs<uint64_t>
     *
     *   getParameterValueAs<double>
     *
     *   getParameterValueAs<std::string>
     *
     *   getParameterValueAs<bool>
     *
     * As well as vectors of the above types:
     *
     *   getParameterValueAs<std::vector<double>>
     *   ...
     *
     * And nested vectors of the above types:
     *
     *   getParameterValueAs<std::vector<std::vector<uint32_t>>>
     *   ...
     */
    template <typename T>
    T getParameterValueAs(const std::string& param_name) {
        static_assert(extensions::is_supported<T>::value,
                        "ExtensionsBase::getParameterValueAs<T> called with "
                        "unsupported type T. See documentation for supported types.");
        return getParameterValueAs_<T>(param_name);
    }

    /*!
     * \brief UUID for testing purposes. NOT added to final config
     * (--write-final-config) output.
     */
    const std::string & getUUID() const {
        return uuid_;
    }

private:
    template <typename T>
    T getParameterValueAs_(const std::string& param_name);

    std::string uuid_;
};

} // namespace detail

/*!
 * \brief Helper class used to trivially extend TreeNode parameter sets (but not
 * any additional functionality beyond that)
 */
class ExtensionsParamsOnly : public detail::ExtensionsBase
{
public:
    explicit ExtensionsParamsOnly();
    virtual ~ExtensionsParamsOnly();
    virtual void setParameters(std::unique_ptr<ParameterSet> params) override final;
    virtual void addParameter(std::unique_ptr<ParameterBase> param) override final;
    virtual ParameterSet * getParameters() const override final;
    virtual ParameterSet * getYamlOnlyParameters() const override final;
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

typedef std::vector<std::unique_ptr<ExtensionDescriptor>> ExtensionDescriptorVec;

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

} // namespace sparta
