// <DatabaseInterface> -*- C++ -*-

#pragma once

#include "sparta/app/Simulation.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "simdb/schema/DatabaseRoot.hpp"
#include "simdb_fwd.hpp"

#include <memory>

namespace sparta {
namespace trigger {
    class ExpressionTrigger;
}
class SubContainer;

/*!
 * \class DatabaseAccessor
 * \brief There is a 1-to-1 mapping between a running simulation
 * and the database it is using. Some components in the simulator
 * may have database access, while others are not intended to use
 * the database. This is controlled via command line arguments, and
 * the simulation's DatabaseAccessor knows which components are DB-
 * enabled, and which are not.
 */
class DatabaseAccessor
{
public:
    //! Simulations typically will instantiate their DatabaseAccessor
    //! relative to the root tree node. During simulation, TreeNode's
    //! which ask the question "am I enabled for database access" will
    //! always be answered FALSE if they are not a child under this
    //! RootTreeNode.
    //!
    //! \param rtn RootTreeNode at the top of a device tree owned by
    //! an app::Simulation object
    explicit DatabaseAccessor(RootTreeNode * rtn) :
        root_(rtn)
    {
        if (all_simulation_accessors_.insert(rtn).second) {
            if (static_simdb_accessor_invoked_ &&
                all_simulation_accessors_.size() > 1)
            {
                throw SpartaException("More than one DatabaseAccessor has been ")
                    << "created, which indicates there may be more than one "
                    << "simulation running concurrently. When this is the case, "
                    << "the GET_DB_FROM_CURRENT_SIMULATION macro cannot be used.";
            }
        }
    }

    //! Check enabled status for any TreeNode
    //! \param tn TreeNode 'this' pointer at the call site where
    //! database access is being queried
    bool isEnabled(
        const std::string & db_namespace,
        const TreeNode * tn) const
    {
        if (!tn) {
            return false;
        }

        const utils::lowercase_string dbns = db_namespace;
        auto & enabled_components = enabled_components_[dbns];
        auto & implicitly_disabled_components = implicitly_disabled_components_[dbns];
        if (implicitly_disabled_components.count(tn) > 0) {
            return false;
        }

        auto mark_implicitly_disabled = [&implicitly_disabled_components, tn](const bool enabled) {
            if (!enabled) {
                implicitly_disabled_components.insert(tn);
            }
            return enabled;
        };

        TreeNode * child = nullptr;
        TreeNode * search_node = nullptr;
        const std::string loc = tn->getLocation();
        if (root_->hasChild(loc)) {
            child = root_->getChild(loc);
            search_node = root_;
        } else if (root_->getSearchScope()->hasChild(loc)) {
            child = root_->getSearchScope()->getChild(loc);
            search_node = root_->getSearchScope();
        } else {
            return mark_implicitly_disabled(false);
        }

        return mark_implicitly_disabled(
            expandEnabledComponentsWildcards_(
                search_node, child, enabled_components));
    }

    //! Check enabled status for any simulation
    //! \param sim Simulation 'this' pointer at the call site where
    //! database access is being queried
    bool isEnabled(const app::Simulation * sim) const {
        if (!sim) {
            return false;
        }
        if (root_ != sim->getRoot()) {
            return false;
        }
        return (sim->getDatabaseRoot() != nullptr);
    }

    //! Check enabled status from any call site which does
    //! not fit one of the other isEnabled() overloads.
    //! Returns the SimDB object if enabled, nullptr if
    //! not.
    static simdb::ObjectManager::ObjectDatabase * isEnabled(
        const std::string & db_namespace)
    {
        static_simdb_accessor_invoked_ = true;
        if (all_simulation_accessors_.size() == 1) {
            if (auto sim = (*all_simulation_accessors_.begin())->getSimulation()) {
                if (auto db_root = sim->getDatabaseRoot()) {
                    if (auto root_namespace = db_root->getNamespace(db_namespace)) {
                        return root_namespace->getDatabase();
                    }
                }
            }
        }
        return nullptr;
    }

private:
    //! Command line arguments pick and choose which components
    //! should be database-enabled, which results in calls into
    //! this method before simulation starts. This method is
    //! intended to only be callable by the Simulation object.
    //!
    //! \param loc Location of the TreeNode where database access
    //! has been enabled ("top.core0.rob", etc.)
    void enableComponentAtLocation_(
        const std::string & db_namespace,
        const std::string & loc)
    {
        std::string dbns_(db_namespace);
        boost::trim(dbns_);

        std::string loc_(loc);
        boost::trim(loc_);
        enabled_components_[dbns_].insert(loc_);
    }

    //! Expand any simdb-enabled-components that were given
    //! with wildcards in them. For example, say we had the
    //! following:
    //!
    //!     \code
    //!         TreeNode * search_node = _global
    //!         TreeNode * requesting_node = top.cpu.core0.rob
    //!         enabled_components = { "top.cpu.core0.r*" }
    //!     \endcode
    //!
    //! Strictly using string comparison would not find any
    //! matches, since "top.cpu.core0.rob" != "top.cpu.core0.r*"
    //! but if we ask the search node to find all of its
    //! children (not necessarily *immediate* children)
    //! for anything matching "top.cpu.core0.r*" we would
    //! end up with this (at the time of this writing,
    //! sparta_core_example only had these matches):
    //!
    //!     \code
    //!         enabled_components = {
    //!             "top.cpu.core0.regs",
    //!             "top.cpu.core0.rename",
    //!             "top.cpu.core0.rob"
    //!         }
    //!     \endcode
    //!
    //! Let's update the simdb-enabled-components list to
    //! account for any wildcards now.
    bool expandEnabledComponentsWildcards_(
        TreeNode * search_node,
        TreeNode * requesting_node,
        std::unordered_set<std::string> & enabled_components) const
    {
        if (!requesting_node) {
            return false;
        }

        if (enabled_components.count(requesting_node->getLocation())) {
            return true;
        }

        if (!search_node) {
            return false;
        }

        std::vector<TreeNode*> all_matching_nodes;
        for (const std::string & component : enabled_components) {
            std::vector<TreeNode*> matching_nodes;
            search_node->findChildren(component, matching_nodes);

            all_matching_nodes.insert(all_matching_nodes.end(),
                                      matching_nodes.begin(),
                                      matching_nodes.end());
        }

        const std::unordered_set<TreeNode*> unique_matching_nodes(
            all_matching_nodes.begin(),
            all_matching_nodes.end());

        std::unordered_set<std::string> expanded_components;
        for (auto tn : unique_matching_nodes) {
            expanded_components.insert(tn->getLocation());
        }

        std::swap(expanded_components, enabled_components);
        return enabled_components.count(requesting_node->getLocation()) > 0;
    }

    //! \class AccessTrigger
    //! \brief Utility class which turns trigger expressions
    //! into invocable SpartaHandler's, and informs the owning
    //! DatabaseAccessor object when a schema namespace has
    //! just become available or unavailable for reads and
    //! writes via the TableProxy class objects.
    class AccessTrigger
    {
    public:
        AccessTrigger(
            DatabaseAccessor * db_accessor,
            const std::string & db_namespace,
            const std::string & start_expr,
            const std::string & stop_expr,
            RootTreeNode * rtn,
            std::shared_ptr<SubContainer> & sub_container);

    private:
        void grantAccess_() {
            db_accessor_->grantAccess_(db_namespace_);
        }

        void revokeAccess_() {
            db_accessor_->revokeAccess_(db_namespace_);
        }

        std::shared_ptr<trigger::ExpressionTrigger> start_;
        std::shared_ptr<trigger::ExpressionTrigger> stop_;
        DatabaseAccessor * db_accessor_ = nullptr;
        std::string db_namespace_;
    };

    //! This method is called when a SimDB namespace has
    //! just become available for reads and writes via
    //! the TableProxy class objects.
    void grantAccess_(const std::string & db_namespace) {
        if (auto db = isEnabled(db_namespace)) {
            db->grantAccess();
        }
    }

    //! This method is called when a SimDB namespace has
    //! just become unavailable for reads and writes via
    //! the TableProxy class objects.
    void revokeAccess_(const std::string & db_namespace) {
        if (auto db = isEnabled(db_namespace)) {
            db->revokeAccess();
        }
    }

    //! Set various access options from a YAML file specifying:
    //!   <namespace>:
    //!     components:
    //!       <component>
    //!       <component>
    //!        ...
    //!   <namespace>:
    //!     components:
    //!       <component>
    //!       <component>
    //!        ...
    //!     start: <trigger expression>
    //!     stop:  <trigger expression>
    void setAccessOptsFromFile_(const std::string & opt_file);

    friend class AccessTrigger;
    friend class app::Simulation;
    RootTreeNode * root_ = nullptr;

    //Member variables for isEnabled() calls from component
    //code which has a 'this' pointer compatible with the
    //various isEnabled() overloads.
    mutable std::unordered_map<
        std::string,
        std::unordered_set<std::string>> enabled_components_;
    mutable std::unordered_map<
        std::string,
        std::unordered_set<const sparta::TreeNode*>> implicitly_disabled_components_;
    std::vector<std::unique_ptr<AccessTrigger>> access_triggers_;
    std::shared_ptr<SubContainer> sub_container_;

    //Member variables for isEnabled() calls from component
    //code which does not have a 'this' pointer matching one
    //of the isEnabled() overloads.
    static std::unordered_set<const RootTreeNode*> all_simulation_accessors_;
    static bool static_simdb_accessor_invoked_;
};

/*!
 * \brief Base struct for DatabaseInterface template class
 */
template <typename T, typename Enable = void>
struct DatabaseInterface;

/*!
 * \brief All TreeNode objects or TreeNode subclass objects use
 * this method to determine if they are database-enabled.
 */
template <typename T>
struct DatabaseInterface<T,
    typename std::enable_if<std::is_base_of<TreeNode, T>::value>::type>
{
    static simdb::ObjectManager::ObjectDatabase * dbEnabled(
        const std::string & db_namespace,
        T * tn)
    {
        if (!tn) {
            return nullptr;
        }
        if (auto sim = tn->getSimulation()) {
            if (auto db_accessor = sim->getSimulationDatabaseAccessor()) {
                if (db_accessor->isEnabled(db_namespace, tn)) {
                    if (auto db_root = sim->getDatabaseRoot()) {
                        if (auto root_namespace = db_root->getNamespace(db_namespace)) {
                            return root_namespace->getDatabase();
                        }
                    }
                }
            }
        }
        return nullptr;
    }
};

/*!
 * \brief All Simulation objects and Simulation subclass objects
 * use this method to determine if they are database-enabled.
 */
template <typename T>
struct DatabaseInterface<T,
    typename std::enable_if<std::is_base_of<app::Simulation, T>::value>::type>
{
    static simdb::ObjectManager::ObjectDatabase * dbEnabled(
        const std::string & db_namespace,
        T * sim)
    {
        if (!sim) {
            return nullptr;
        }
        if (auto db_accessor = sim->getSimulationDatabaseAccessor()) {
            if (db_accessor->isEnabled(sim)) {
                if (auto db_root = sim->getDatabaseRoot()) {
                    if (auto root_namespace = db_root->getNamespace(db_namespace)) {
                        return root_namespace->getDatabase();
                    }
                }
            }
        }
        return nullptr;
    }
};

//! This macro lets callers request the SimDB object from a variety
//! of simulation contexts.
#define GET_DB_FOR_COMPONENT(db_namespace, thisptr)     \
  sparta::DatabaseInterface<typename std::remove_pointer<decltype(thisptr)>::type>::dbEnabled(#db_namespace, thisptr)

//! This is similar to the macro above, but is intended to be used
//! from a simulation context that does not have an appropriate 'this'
//! pointer for the GET_DB_FOR_COMPONENT macro. It can also be used
//! in code that *does* subclass from TreeNode/etc. but for that
//! particular call site, you *always* want SimDB access.
//!
//! This is effectively singleton access, with safety checks in place
//! to ensure that the static getter can be called safely.
#define GET_DB_FROM_CURRENT_SIMULATION(db_namespace)    \
  sparta::DatabaseAccessor::isEnabled(#db_namespace)

}

