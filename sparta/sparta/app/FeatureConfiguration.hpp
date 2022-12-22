// <FeatureConfiguration.h> -*- C++ -*-


/*!
 * \file FeatureConfiguration.hpp
 * Class for configuring feature values. Used together
 * with the command-line "--feature <name> value" option.
 */

#pragma once

#include "sparta/simulation/ParameterTree.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/StringUtils.hpp"
#include "sparta/utils/MetaStructs.hpp"

#include <iostream>
#include <fstream>
#include <memory>

#include <boost/lexical_cast.hpp>

namespace sparta {
namespace app {

//! Collection of named feature values
class FeatureConfiguration
{
public:
    void setFeatureValue(const std::string & name, const unsigned int value) {
        feature_values_.create(name)->setValue(
            boost::lexical_cast<std::string>(value));
    }

    unsigned int getFeatureValue(const std::string & feature_name) const {
        auto feature = feature_values_.tryGet(feature_name);
        if (feature == nullptr || !feature->hasValue()) {
            return 0;
        }
        return std::stoi(feature->getValue());
    }

    bool isFeatureValueSet(const std::string & feature_name) const {
        return (feature_values_.tryGet(feature_name) != nullptr);
    }

    //! Feature options let you provide optional parameterization
    //! for any given feature. Typically, a feature value is either
    //! 0 or 1, but there may be more feature values:
    //!
    //!      Value 0 = featured off
    //!      Value 1 = featured on, foo parameters/configuration
    //!      Value 2 = featured on, bar parameters/configuration
    //!       ...
    //!
    //! For these scenarios, it may be easier to run the simulator
    //! with commands like:
    //!
    //!      <sim> -i 10k --feature my_feat 1 foo.yaml
    //!      <sim> -i 10k --feature my_feat 1 bar.yaml
    //!
    //! Where foo.yaml and bar.yaml are colon-separated name/value
    //! pairs such as:
    //!
    //!      foo.yaml                        bar.yaml
    //!     -----------------------         -----------------------
    //!      biz: 102                        bazA: hello
    //!      buz: 48.5                       bazB: world
    //!                                      buz: 45
    //!
    //! Say the above example yamls were for a feature called "MyCoolFeature".
    //! You could ask the FeatureConfiguration object for the feature value
    //! as you normally would:
    //!
    //!      if (feature_cfg->getFeatureValue("MyCoolFeature") == 1) {
    //!          //...featured on...
    //!      } else {
    //!          //...featured off...
    //!      }
    //!
    //! And you could also (optionally) ask the FeatureConfiguration for any
    //! options that were found in yaml file(s) at the command line, like so:
    //!
    //!      auto feature_opts = feature_cfg->getFeatureOptions("MyCoolFeature");
    //!      if (feature_opts) {
    //!          double biz_val = feature_opts->getOptionValue<double>("biz", 10);
    //!          //This would equal 102 for foo.yaml configuration, but it would
    //!          //equal 10 for bar.yaml configuration since there is no "biz"
    //!          //parameter found in bar.yaml
    //!
    //!          double buz_val = feature_opts->getOptionValue<double>("buz", 50);
    //!          //Equals 48.5 for foo.yaml configuration, and 45 for bar.yaml
    //!          //configuration.
    //!
    //!          std::string bazA_val = feature_opts->getOptionValue<std::string>("bazA", "fizz");
    //!          //Equals "fizz" for foo.yaml since this option does not exist
    //!          //in that file, and it equals "hello" for bar.yaml
    //!      }
    class FeatureOptions
    {
    public:
        //! Consume a yaml file that contains options in name-value format:
        //!     param1: value1
        //!     param2: value2
        //!       ...     ...
        void setOptionsFromYamlFile(const std::string & yaml_opts_fname)
        {
            std::ifstream fin(yaml_opts_fname);
            if (!fin) {
                std::cout << "  [feature] Warning: No options file named '"
                          << yaml_opts_fname << "' found." << std::endl;
                return;
            }

            std::vector<std::vector<std::string>> str_vectors;
            utils::split_lines_around_tokens(fin, str_vectors, ":");
            for (auto & strvec : str_vectors) {
                sparta_assert(strvec.size() == 2);
                std::string & option_name = strvec[0];
                std::string & option_value = strvec[1];
                option_name  = sparta::utils::strip_whitespace(option_name);
                option_value = sparta::utils::strip_whitespace(option_value);
                feature_options_.set(option_name, option_value, false);
            }
        }

        //! Get the value of a particular feature option by name.
        //! If this option could not be found, the default_value
        //! you provide will be returned. Here are ways this method
        //! can fail:
        //!
        //!          opts.yaml
        //!     -------------------
        //!      myFoo: 100
        //!      myBar: someString
        //!
        //! If you called the getOptionValue() method like this, it
        //! would fail and return the default value you pass in:
        //!
        //!   //Fails because myFizzBuzz is not a known parameter.
        //!   //Returns default value 4.
        //!   auto val1 = feature_opts->getOptionValue<double>("myFizzBuzz", 4);
        //!
        //!   //Fails because myBar is a string, not a double.
        //!   //Returns default value 5.
        //!   auto val2 = feature_opts->getOptionValue<double>("myBar", 5);
        template <typename T,
                  typename = typename std::enable_if<
                      std::is_trivial<T>::value, T>::type>
        T getOptionValue(
            const std::string & option_name,
            const T default_value) const
        {
            auto option = feature_options_.tryGet(option_name);
            if (option == nullptr || !option->hasValue()) {
                return default_value;
            }

            const std::string & opt_value_str = option->getValue();
            try {
                const double opt_value = boost::lexical_cast<double>(opt_value_str);
                return static_cast<T>(opt_value);
            } catch (const boost::bad_lexical_cast &) {
            }

            //We could get here if there was an option yaml file like this:
            //
            //    myFoo: 4.6
            //    myBar: helloWorld
            //
            //And the call site looked like this:
            //
            //    auto opt_value = feature_opts->getOptionValue<double>("myBar", 3.14);
            //
            //The myBar option data type is string, but the call site specified
            //a double template argument.
            return default_value;
        }

        //! Get the value of a particular feature option by name.
        //! If this option could not be found, the default_value
        //! you provide will be returned.
        template <typename T,
                  typename = typename std::enable_if<
                      std::is_same<T, std::string>::value, T>::type>
        std::string getOptionValue(
            const std::string & option_name,
            const T default_value) const
        {
            auto option = feature_options_.tryGet(option_name);
            if (option == nullptr || !option->hasValue()) {
                return default_value;
            }
            return option->getValue();
        }

    private:
        ParameterTree feature_options_;
    };

    //! Consume a feature options YAML file for a given feature name.
    //! This is typically done from CommandLineSimulator while parsing
    //! simulator options.
    void setFeatureOptionsFromFile(const std::string & feature_name,
                                   const std::string & yaml_opts_fname)
    {
        auto iter = feature_options_.find(feature_name);
        if (iter == feature_options_.end()) {
            std::unique_ptr<FeatureOptions> opts(new FeatureOptions);
            opts->setOptionsFromYamlFile(yaml_opts_fname);
            feature_options_[feature_name] = std::move(opts);
            return;
        }
        iter->second->setOptionsFromYamlFile(yaml_opts_fname);
    }

    //! Return the feature options data structure for the given
    //! feature name. If there are no options for this feature,
    //! this method still returns a non-null FeatureOptions pointer,
    //! though all of its option values will return the default
    //! value you pass in.
    //!
    //!       auto feat_opts = feat_cfg->getFeatureOptions("nonexistent");
    //!       sparta_assert(feat_opts != nullptr);
    //!
    //!       // "pi equals 3.14" (despite having no option values)
    //!       std::cout << feat_opts->getOptionValue<std::string>("nonexistent", "pi");
    //!       std::cout << " equals ";
    //!       std::cout << feat_opts->getOptionValue<double>("nonexistent", 3.14);
    const FeatureOptions * getFeatureOptions(const std::string & feature_name) const {
        auto iter = feature_options_.find(feature_name);
        if (iter != feature_options_.end()) {
            return iter->second.get();
        }
        return &null_feature_options_;
    }

private:
    ParameterTree feature_values_;
    std::unordered_map<std::string, std::unique_ptr<FeatureOptions>> feature_options_;
    FeatureOptions null_feature_options_;
};

} // namespace app

//! Utility in the sparta namespace that checks a FeatureConfiguration
//! for a specific feature's current value (typically 1=enabled, and
//! 0=disabled, but there could be more feature values too)
template <typename T>
typename std::enable_if<
    std::is_same<T, app::FeatureConfiguration>::value,
bool>::type
IsFeatureValueEqualTo(
    const T & cfg,
    const std::string & feature_name,
    const unsigned int feature_value)
{
    return cfg.getFeatureValue(feature_name) == feature_value;
}

//! Utility in the sparta namespace that checks for a FeatureConfiguration
//! value, with a FeatureConfiguration that may be null
template <typename T>
typename std::enable_if<
    MetaStruct::is_any_pointer<T>::value,
bool>::type
IsFeatureValueEqualTo(const T & cfg,
                      const std::string & feature_name,
                      const unsigned int feature_value)
{
    return cfg ? IsFeatureValueEqualTo(*cfg, feature_name, feature_value) :
                 (feature_value == 0);
}

//! Utility in the sparta namespace that checks if a feature value
//! has been set to any positive number (0=disabled, >0=enabled)
template <typename T>
typename std::enable_if<
    std::is_same<T, app::FeatureConfiguration>::value,
bool>::type
IsFeatureValueEnabled(
    const T & cfg,
    const std::string & feature_name)
{
    return cfg.getFeatureValue(feature_name) > 0;
}

//! Utility in the sparta namespace that asks a FeatureConfiguration
//! if the provided feature is enabled, for a FeatureConfiguration
//! that may be null
template <typename T>
typename std::enable_if<
    MetaStruct::is_any_pointer<T>::value,
bool>::type
IsFeatureValueEnabled(
    const T & cfg,
    const std::string & feature_name)
{
    return cfg ? IsFeatureValueEnabled(*cfg, feature_name) : false;
}

//! Utility in the sparta namespace that gets the named FeatureOptions
//! object from a FeatureConfiguration set
template <typename T>
typename std::enable_if<
    std::is_same<T, app::FeatureConfiguration>::value,
const app::FeatureConfiguration::FeatureOptions*>::type
GetFeatureOptions(const T & cfg,
                  const std::string & feature_name)
{
    return cfg.getFeatureOptions(feature_name);
}

//! Utility in the sparta namespace which gets the named FeatureOptions object
//! from a FeatureConfiguration which may be null
template <typename T>
typename std::enable_if<
    MetaStruct::is_any_pointer<T>::value,
const app::FeatureConfiguration::FeatureOptions*>::type
GetFeatureOptions(
    const T & cfg,
    const std::string & feature_name)
{
    return cfg ? GetFeatureOptions(*cfg, feature_name) : nullptr;
}

} // namespace sparta
