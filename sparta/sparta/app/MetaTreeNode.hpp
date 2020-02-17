// <Simulation.h> -*- C++ -*-


/*!
 * \file Simulation.hpp
 * \brief Simulation setup base class
 */

#ifndef __SPARTA_METATREENODE_H__
#define __SPARTA_METATREENODE_H__

#include <memory>
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/Parameter.hpp"
#include "sparta/simulation/ParameterSet.hpp"


namespace sparta {
namespace app {

/*!
 * \brief ParameterTemplate providing factory
 * implementation for each data type
 *
 * Example
 * \verbatim
 * new ParameterTemplate("paramfoo", (const std::string&)"default", "test parameter");
 * \endverbatim
 */
class ParameterTemplate
{
    /*!
     * \brief Internal Parameter factory interface
     */
    class FactoryIF
    {
    public:
        virtual ParameterBase* create() const = 0;

        virtual ~FactoryIF()
        {;}
    };

    /*!
     * \brief Templated factory implementation
     */
    template <typename T>
    class Factory : public FactoryIF
    {
        std::string name_;
        T def_val_;
        std::string docstring_;

    public:
        Factory(const std::string& name, const T& def_val, const std::string& docstring)
          : name_(name),
            def_val_(def_val),
            docstring_(docstring)
        {;}

        /*!
         * \brief Create the parameter
         */
        ParameterBase* create() const override { return new Parameter<T>(name_, def_val_, docstring_); }
    };

    /*!
     * \brief Shared pointer to underlying templated factory implementation.
     *
     * Shared for easy in copy and move construction
     */
    std::shared_ptr<FactoryIF> fact_;

public:

    /*!
     * \brief Create the template. Specify the type of def_val explicitly when invoking this function
     */
    template <typename T>
    ParameterTemplate(const std::string& name, const T& def_val, const std::string& docstring)
      : fact_(new Factory<T>(name, def_val, docstring))
    {;}

    ParameterTemplate(const std::string& name, const char* def_val, const std::string& docstring)
      : fact_(new Factory<std::string>(name, def_val, docstring))
    {;}

    ParameterTemplate(const ParameterTemplate&) = default;
    ParameterTemplate(ParameterTemplate&&) = default;

    ParameterTemplate& operator=(const ParameterTemplate&) = default;

    /*!
     * \brief Crete an instance of the parameter base on this template using "new".
     * \note Caller must delete it using "delete"
     */
    ParameterBase* create() const { return fact_->create(); }
};

class MetaTreeNode : public RootTreeNode
{

    class ParameterSet : public sparta::ParameterSet
    {
        std::vector<ParameterBase*> owned_params_;

    public:

        /*!
         * \brief Create ParameterSet and populate with parameters based on a list of templates
         */
        ParameterSet(TreeNode *n, const std::list<ParameterTemplate>& plist)
          : sparta::ParameterSet(n)
        {
            for(auto& pt: plist){
                ParameterBase* pb = pt.create();
                addParameter_(pb);
                owned_params_.push_back(pb);
            }
        }

        /*!
         * \brief Create ParameterSet with only defaults
         */
        ParameterSet(TreeNode *n)
          : sparta::ParameterSet(n)
        {;}

        ~ParameterSet()
        {
            for(ParameterBase*& p : owned_params_){
                delete p;
                p = nullptr;
            }
        }

        // Built-in parameters

        PARAMETER (std::string, architecture, "NONE", "Name of architecture being simulated")
        PARAMETER (bool, is_final_config, false, "True if this config was generated using --write-final-config. "
                   "This value is checked during --read-final-config to validate we are "
                   "loading a full config.")

    } params_;

public:

    /*!
     * \brief Constructor with only a simulator and search scope
     * \param[in] sim Simulator owning the node
     * \param[in] search_scope Global scope in which to search for this node (e.g. as a sibling of "top")
     * \param[in] pset Custom MetaTreeNode::ParameterSet subclass. If nullptr, creates a default set
     */
    MetaTreeNode(app::Simulation* sim, GlobalTreeNode* search_scope, const std::list<ParameterTemplate>& plist)
        : RootTreeNode("meta", "Meta-Data gloabl node", sim, search_scope),
          params_(this, plist)
    {
    }
};

} // namespace app
} // naemspace sparta

#endif // #ifndef __SPARTA_METATREENODE_H__
