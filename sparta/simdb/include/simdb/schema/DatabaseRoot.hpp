// <DatabaseRoot> -*- C++ -*-

#ifndef __SIMDB_ROOT_DATABASE_H__
#define __SIMDB_ROOT_DATABASE_H__

#include "simdb/ObjectManager.hpp"
#include "simdb/schema/Schema.hpp"
#include "simdb/DbConnProxy.hpp"
#include "simdb/async/AsyncTaskEval.hpp"
#include "simdb/utils/StringUtils.hpp"
#include "simdb/Errors.hpp"

#include <functional>
#include <map>
#include <memory>
#include <string>

namespace simdb {

//! Signature of user-defined schema creation method.
using SchemaBuildFcn = std::function<void(simdb::Schema&)>;

//! Factory function which returns a DbConnProxy subclass.
using ProxyCreateFcn = std::function<DbConnProxy*()>;

/*!
 * \class DatabaseNamespace
 *
 * \brief Database files are organized as follows:
 *
 *     root                  (DatabaseRoot)
 *       -> namespace1       (DatabaseNamespace)
 *         --> table
 *         --> table
 *       -> namespace2       (DatabaseNamespace)
 *         --> table
 *         --> table
 *
 * This class is a container wrapping the namespace
 * nodes in the database hierarchy.
 */
class DatabaseNamespace
{
public:
    /*!
     * \brief Invoke registered schema builder methods.
     * This will be triggered when using the GET_DB_*
     * macros, but it also works if you want to inline
     * your schema creation code like so:
     *
     * \code
     *     DatabaseNamespace * db_namespace = ...
     *
     *     db_namespace->addToSchema([](simdb::Schema & schema) {
     *         schema.addTable("FizzBuzz")
     *             .addColumn("Fizz", simdb::ColumnDataType::int32_t)
     *             .addColumn("Buzz", simdb::ColumnDataType::int32_t);
     *     });
     * \endcode
     *
     * \param schema_builder Callable code which builds the
     * schema from the SchemaBuildFcn you provide.
     */
    void addToSchema(const SchemaBuildFcn & schema_builder)
    {
        simdb::Schema schema;
        schema_builder(schema);
        addSchema_(schema);
    }

    /*
     * \brief Check to see if there is a table in the underlying
     * database with this name, taking into account our namespace
     * as well.
     *
     * \param table_name Name of the target table you are looking for
     *
     * \return Returns true if found, false otherwise.
     */
    bool hasTableNamed(const std::string & table_name) const {
        return schema_.getTableNamed(
            db_namespace_ + Table::NS_DELIM + table_name) != nullptr;
    }

    /*!
     * \brief Get the table by the given name, if it exists.
     *
     * \param table_name Name of the target table you are looking for
     *
     * \note The table name you pass in ideally should not contain
     * the namespace delimiter '$' as it can lead to exceptions.
     * For example, say we had a namespace called "Stats", with
     * a table called "ReportTimeseries":
     *
     * \code
     *      DatabaseNamespace * ns = ...
     *
     *      //This will never throw
     *      if (auto table = ns->getTableNamed("ReportTimeseries")) {
     *          table->...
     *      }
     *
     *      //This *happens* to not throw, since the namespace
     *      //matched this object's namespace
     *      if (auto table = ns->getTableNamed("Stats$ReportTimeseries")) {
     *          table->...
     *      }
     *
     *      //This *would* throw, since the namespace is "Stats",
     *      //not "Statistics"
     *      if (auto table = ns->getTableNamed("Statistics$ReportTimeseries")) {
     *          table->...
     *      }
     * \endcode
     *
     * \note As long as you do not include the namespace delimiter
     * '$' in the table name you pass in, this method will never
     * throw; it returns null if the table is not found.
     */
    const Table * getTableNamed(const std::string & table_name) const {
        std::string requested_table_name;
        auto delim_idx = table_name.find(std::string(1, Table::NS_DELIM));
        if (delim_idx != std::string::npos) {
            const utils::lowercase_string requested_namespace_lower =
                table_name.substr(0, delim_idx);

            const std::string requested_namespace = requested_namespace_lower;

            requested_table_name =
                (delim_idx + 1 < table_name.size()) ?
                table_name.substr(delim_idx + 1) : "";

            if (!requested_namespace.empty() &&
                requested_namespace != db_namespace_)
            {
                throw DBException("Invalid namespace. This DatabaseNamespace ")
                    << "is named '" << db_namespace_ << "', but the requested namespace "
                    << "was '" << requested_namespace << "'";
            }
        } else {
            requested_table_name = table_name;
        }

        return schema_.getTableNamed(
            db_namespace_ + Table::NS_DELIM + requested_table_name);
    }

    /*
     * \brief Check to see if our underlying schema has any tables at all.
     */
     bool hasSchema() const {
         return schema_.hasTables();
     }

    /*
     * \return Returns whether or not this database has an open
     * connection to a database file.
     */
    bool databaseConnectionEstablished() const;

    /*
     * \brief Get an object which has many of the same ObjectManager
     * APIs, and return an "intermediate" class object. This class
     * sits right between the calling code (SPARTA simulator, unit
     * tests, etc.) and the lower level objects that run SELECT /
     * UPDATE / etc. database commands.
     */
    ObjectManager::ObjectDatabase * getDatabase();

    ~DatabaseNamespace() = default;

private:
    DatabaseNamespace(const utils::lowercase_string & db_namespace,
                      DatabaseRoot * db_root,
                      AsyncTaskController * task_controller) :
        db_namespace_(db_namespace),
        db_root_(db_root),
        task_controller_(task_controller)
    {}

    void addSchema_(Schema & schema)
    {
        schema.setNamespace_(db_namespace_);
        for (auto & table : schema) {
            if (Table * existing_table = schema_.getTableNamed(table.getName())) {
                if (*existing_table != table) {
                    throw DBException("Invalid table added to schema. The table ")
                        << "has the same name as an existing schema table, but has "
                        << "a different column configuration. The offending table "
                        << "is '" << table.getName() << "'.";
                }
                continue;
            }
            schema_.addTable(table);
        }

        //If we already have a database connection open, we
        //need to forward these new schema tables to the
        //ObjectManager we are associated with. Note that
        //we also need to make the call to appendSchema()
        //before unsetting the schema's namespace below.
        if (db_root_) {
            appendSchemaToConnectionIfOpen_(*db_root_, schema);
        }

        schema.setNamespace_("");
    }

    void grantAccess_() {
        if (cached_db_) {
            cached_db_->grantAccess();
        }
        access_granted_ = true;
    }

    void revokeAccess_() {
        if (cached_db_) {
            cached_db_->revokeAccess();
        }
        access_granted_ = false;
    }

   void appendSchemaToConnectionIfOpen_(
        DatabaseRoot & db_root,
        Schema & schema);

    std::string db_namespace_;
    Schema schema_;
    DatabaseRoot * db_root_ = nullptr;
    AsyncTaskController * task_controller_ = nullptr;
    std::unique_ptr<ObjectManager::ObjectDatabase> cached_db_;
    bool access_granted_ = true;
    friend class DatabaseRoot;
};

/*!
 * \class DatabaseRoot
 *
 * \brief SimDB organizes its schema into namespaces. This class
 * is at the top of the database hierarchy; it is a collection of
 * SimDB namespaces.
 */
class DatabaseRoot
{
public:
    //! Construct with a database directory. All ObjectManager's
    //! created underneath this DatabaseRoot will put their database
    //! file(s)/artifacts in this directory.
    DatabaseRoot(const std::string & db_dir = ".") :
        db_dir_(db_dir),
        task_controller_(new AsyncTaskController)
    {}

    ~DatabaseRoot() = default;

    //! Access a SimDB namespace by the given name. The first time
    //! this is called for a particular namespace, it will be created
    //! for you. If a SchemaBuildFcn was registered with SimDB for
    //! this namespace, it will be invoked. Otherwise, you will have
    //! to populate the namespace schema yourself using the method
    //! "DatabaseNamespace::addToSchema()" on the returned object.
    DatabaseNamespace * getNamespace(const utils::lowercase_string & db_namespace) {
        auto ns_iter = namespaces_.find(db_namespace);
        if (ns_iter != namespaces_.end()) {
            return ns_iter->second.get();
        }

        auto type_iter = db_types_by_namespace_.find(db_namespace);
        if (type_iter == db_types_by_namespace_.end()) {
            throw DBException("Unable to get namespace named '")
                << db_namespace << "'. This namespace was not registered "
                << "with SimDB.";
        }

        std::unique_ptr<DatabaseNamespace> ns(new DatabaseNamespace(
            db_namespace, this, task_controller_.get()));

        auto build_iter = schema_builders_by_namespace_.find(db_namespace);
        if (build_iter != schema_builders_by_namespace_.end()) {
            for (auto & builder : build_iter->second) {
                ns->addToSchema(builder);
            }
        }

        auto ret = ns.get();
        namespaces_[db_namespace].reset(ns.release());
        return ret;
    }

    AsyncTaskController * getTaskController() const {
        return task_controller_.get();
    }

    //! Let SimDB know the database type that should be used to
    //! instantiate the schema for the given namespace.
    //!
    //! \param db_namespace Namespace to register. Case insenstive.
    //!
    //! \param db_type Database type for this namespace. Examples
    //! include "sqlite" and "hdf5". Case insensitive.
    //!
    //! \warning This will throw if a namespace is passed in that
    //! has already been registered *with a different db_type*.
    //!
    //! \note It is recommended that you use the macros found at the
    //! bottom of this file instead of calling this method directly.
    //! See the macro comments for details.
    static void registerDatabaseNamespace(
        const utils::lowercase_string & db_namespace,
        const utils::lowercase_string & db_type)
    {
        auto iter = db_types_by_namespace_.find(db_namespace);
        if (iter != db_types_by_namespace_.end()) {
            if (iter->second != db_type) {
                throw DBException("SimDB has already been registered with a ")
                    << "conflicting database type. Namespace '" << db_namespace << "' "
                    << "is registered for database type '" << iter->second << "', "
                    << "which conflicts with the new type '" << db_type << "'.";
            }
        }
        db_types_by_namespace_[db_namespace] = db_type;
    }

    //! Optionally give one of the SimDB namespaces a schema
    //! build function/callback. When this namespace is accessed
    //! for the first time, this callback will be invoked to
    //! populate the namespace schema with the appropriate
    //! empty tables.
    //!
    //! \param db_namespace Namespace of the schema build function
    //! you are registering. Case insenstive.
    //!
    //! \param build_fcn Schema build function to register for
    //! this namespace.
    //!
    //! \warning This will throw if the provided namespace already
    //! has a schema build function registered for it, *and the new
    //! build function is different than the existing build function*.
    //! In order to determine if the new and existing schema builders
    //! are identical, the build function will be invoked with a
    //! blank Schema object, and the resulting built schema will be
    //! compared against the existing schema.
    //!
    //! \note If you do not provide a schema build function for
    //! your namespace, you have to call the DatabaseNamespace
    //! "addToSchema()" method manually before you can start
    //! writing any records into that namespace. Registering
    //! your schema build callback is just a convenience so you
    //! don't have to make the call yourself.
    //!
    //! \note It is recommended that you use the macros found at the
    //! bottom of this file instead of calling this method directly.
    //! See the macro comments for details.
    static void registerSchemaBuilderForNamespace(
        const utils::lowercase_string & db_namespace,
        const SchemaBuildFcn & build_fcn)
    {
        schema_builders_by_namespace_[db_namespace].emplace_back(build_fcn);
    }

    //! Give SimDB a DbConnProxy factory function for the given
    //! database type.
    //!
    //! \param db_type Database type for this proxy factory.
    //! Examples include "sqlite" and "hdf5". Case insensitive.
    //!
    //! \param create_fcn Schema builder callback function. Note
    //! that this builder code may also be inlined with a lambda
    //! at the call site.
    //!
    //! \note If there is already a proxy factory registered for
    //! the provided database type, it will be overwritten. A warning
    //! will be printed to stdout, but it will not reject it or throw.
    //!
    //! \note It is recommended that you use the macros found at the
    //! bottom of this file instead of calling this method directly.
    //! See the macro comments for details.
    static void registerProxyCreatorForDatabaseType(
        const utils::lowercase_string & db_type,
        const ProxyCreateFcn & create_fcn)
    {
        if (proxy_creators_by_db_type_.find(db_type) !=
            proxy_creators_by_db_type_.end())
        {
            std::cout << "  [simdb] Warning: Database type '" << db_type << "' "
                      << "already has a DbConnProxy factory registered for it. "
                      << "The registered factory is being overwritten with the "
                      << "new factory." << std::endl;
        }
        proxy_creators_by_db_type_[db_type] = create_fcn;
    }

private:
    // Return the database type for the given namespace. Database
    // types are registered with SimDB using the registration
    // macros at the bottom of this file. Examples of these strings
    // include "SQLite" and "HDF5".
    const std::string & getDatabaseTypeForNamespace_(
        const utils::lowercase_string & db_namespace) const
    {
        auto type_iter = db_types_by_namespace_.find(db_namespace);
        if (type_iter == db_types_by_namespace_.end()) {
            throw DBException("No registered database type found ")
                << "for namespace '" << db_namespace << "'";
        }
        return type_iter->second;
    }

    // Return the DbConnProxy factory for the given namespace.
    // Throws if the namespace was not registered with SimDB
    // beforehand.
    const ProxyCreateFcn & getProxyCreatorForNamespace_(
        const utils::lowercase_string & db_namespace) const
    {
        const std::string & db_type = getDatabaseTypeForNamespace_(db_namespace);
        auto creation_iter = proxy_creators_by_db_type_.find(db_type);
        if (creation_iter == proxy_creators_by_db_type_.end()) {
            throw DBException("No registered DbConnProxy factory ")
                << "found for namespace '" << db_namespace << "'";
        }
        return creation_iter->second;
    }

    // Return the ObjectManager associated with the given namespace.
    // The first time this is called for a particular namespace, the
    // ObjectManager will be created, the appropriate DbConnProxy will
    // be instantiated, and its namespace schema will be realized with
    // empty tables.
    ObjectManager * getObjectManagerForNamespace_(
        const utils::lowercase_string & db_namespace,
        simdb::Schema & namespace_schema)
    {
        ObjectManager * ret = nullptr;
        if (!hasObjectManagerForNamespace_(db_namespace)) {
            std::unique_ptr<ObjectManager> sim_db(new ObjectManager(db_dir_));
            ret = sim_db.get();
            const ProxyCreateFcn & proxy_creator =
                getProxyCreatorForNamespace_(db_namespace);

            namespace_schema.setNamespace_(db_namespace);
            std::unique_ptr<DbConnProxy> db_proxy(proxy_creator());
            ret->createDatabaseFromSchema(namespace_schema, std::move(db_proxy));
            namespace_schema.setNamespace_("");

            sim_dbs_by_db_type_[db_types_by_namespace_[db_namespace]] = std::move(sim_db);
        } else {
            ret = sim_dbs_by_db_type_[db_types_by_namespace_[db_namespace]].get();

            simdb::Schema combined_schema;
            combined_schema.setNamespace_(db_namespace);
            namespace_schema.setNamespace_(db_namespace);

            auto build_iter = schema_builders_by_namespace_.find(db_namespace);
            if (build_iter != schema_builders_by_namespace_.end()) {
                for (auto & builder : build_iter->second) {
                    builder(combined_schema);
                }
            }
            combined_schema += namespace_schema;

            simdb::Schema pruned_schema;
            pruned_schema.setNamespace_(db_namespace);
            const auto & curr_realized_tables = ret->getTableNames();
            for (const auto & new_table : combined_schema) {
                if (!curr_realized_tables.count(new_table.getName())) {
                    pruned_schema.addTable(new_table);
                }
            }

            if (pruned_schema.hasTables()) {
                ret->appendSchema(pruned_schema);
            }
            namespace_schema.setNamespace_("");
        }
        return ret;
    }

    // See if the given namespace already has an ObjectManager created for it.
    bool hasObjectManagerForNamespace_(const utils::lowercase_string & db_namespace) const {
        auto type_iter = db_types_by_namespace_.find(db_namespace);
        if (type_iter == db_types_by_namespace_.end()) {
            throw DBException("No registered database type found for ")
                << "namespace '" << db_namespace << "'";
        }

        auto db_iter = sim_dbs_by_db_type_.find(type_iter->second);
        if (db_iter == sim_dbs_by_db_type_.end()) {
            return false;
        }

        auto sim_db = db_iter->second.get();
        if (sim_db == nullptr) {
            throw DBException("Unexpectedly found a null ObjectManager");
        }
        if (sim_db->getDbConn() == nullptr) {
            throw DBException(
                "Unexpectedly found an ObjectManager with a null DbConnProxy");
        }
        return true;
    }

    std::map<std::string, std::unique_ptr<DatabaseNamespace>> namespaces_;
    std::string db_dir_;
    std::map<std::string, std::unique_ptr<ObjectManager>> sim_dbs_by_db_type_;
    std::unique_ptr<AsyncTaskController> task_controller_;
    static std::map<std::string, std::string> db_types_by_namespace_;
    static std::map<std::string, std::vector<SchemaBuildFcn>> schema_builders_by_namespace_;
    static std::map<std::string, ProxyCreateFcn> proxy_creators_by_db_type_;
    friend class DatabaseNamespace;
};

inline bool DatabaseNamespace::databaseConnectionEstablished() const
{
    if (db_root_) {
        return db_root_->hasObjectManagerForNamespace_(db_namespace_);
    }
    return false;
}

inline ObjectManager::ObjectDatabase * DatabaseNamespace::getDatabase()
{
    if (cached_db_) {
        if (access_granted_) {
            cached_db_->grantAccess();
        } else {
            cached_db_->revokeAccess();
        }
        return cached_db_.get();
    }

    if (db_root_) {
        auto sim_db = db_root_->getObjectManagerForNamespace_(db_namespace_, schema_);
        sim_db->addToTaskController(task_controller_);
        cached_db_.reset(new ObjectManager::ObjectDatabase(sim_db, db_namespace_, this));

        if (access_granted_) {
            cached_db_->grantAccess();
        } else {
            cached_db_->revokeAccess();
        }
    }

    return cached_db_.get();
}

inline void DatabaseNamespace::appendSchemaToConnectionIfOpen_(
    DatabaseRoot & db_root,
    Schema & schema)
{
    if (db_root.hasObjectManagerForNamespace_(db_namespace_)) {
        Schema unused_schema;
        auto sim_db = db_root.getObjectManagerForNamespace_(
            db_namespace_, unused_schema);

        Schema pruned_schema;
        schema.setNamespace_("");
        for (const auto & table : schema) {
            if (sim_db->getQualifiedTableName(table.getName(), db_namespace_).empty()) {
                pruned_schema.addTable(table);
            }
        }
        pruned_schema.setNamespace_(db_namespace_);
        sim_db->appendSchema(pruned_schema);
    }
}

//! Let SimDB know about your database namespace, and the type
//! of the database that goes with it.
//!
//! \param db_namespace Name of the registered namespace. Case
//! insensitive.
//!
//! \param db_type Database type for this namespace. Examples
//! include "sqlite" and "hdf5". Case insensitive.
#define REGISTER_SIMDB_NAMESPACE(db_namespace, db_type) \
  simdb::DatabaseRoot::registerDatabaseNamespace(#db_namespace, #db_type)

//! Register a DbConnProxy factory for the given database type.
//!
//! \param db_type Database type for this proxy factory.
//! Examples include "sqlite" and "hdf5". Case insensitive.
//!
//! \param create_fcn Schema builder callback function. Note
//! that this builder code may also be inlined with a lambda
//! at the call site.
#define REGISTER_SIMDB_PROXY_CREATE_FUNCTION(db_type, proxy_creator)    \
  simdb::DatabaseRoot::registerProxyCreatorForDatabaseType(#db_type, proxy_creator)

//! Optionally provide SimDB with a schema build function
//! for the given namespace. Example:
//!
//!     \code
//!         void buildStatsSchema(simdb::Schema & schema)
//!         {
//!             schema.addTable("ReportHeader")
//!                 .addColumn(...)
//!                 .addColumn(...);
//!             schema.addTable("ReportTimeseries")
//!                 .addColumn(...)
//!                 .addColumn(...);
//!         }
//!         REGISTER_SIMDB_SCHEMA_BUILDER(Stats, buildStatsSchema);
//!     \endcode
//!
//! This method would be invoked the first time we request
//! the "Stats" namespace from SimDB:
//!
//!     \code
//!         DatabaseRoot * db_root = ...
//!         DatabaseNamespace * stats_ns = db_root->getNamespace("Stats");
//!     \endcode
//!
//! In this example, we only provided a schema build function
//! for the "Stats" namespace. We can still manually add tables
//! to namespaces:
//!
//!     \code
//!         DatabaseRoot * db_root = ...
//!         DatabaseNamespace * pipeline_ns = db_root->getNamespace("Pipeline");
//!         if (!pipeline_ns->hasSchema()) {
//!             pipeline_ns->addToSchema([](simdb::Schema & schema) {
//!                 schema.addTable(...)
//!                     .addColumn(...)
//!                     .addColumn(...);
//!             });
//!         }
//!     \endcode
//!
//! You can also specify part of the namespace schema using
//! a regisered build callback, and add more tables manually:
//!
//!     \code
//!         DatabaseRoot * db_root = ...
//!         DatabaseNamespace * stats_ns = db_root->getNamespace("Stats");
//!         assert(stats_ns->hasTableNamed("ReportHeader"));
//!         assert(stats_ns->hasTableNamed("ReportTimeseries"));
//!
//!         assert(!stats_ns->hasTableNamed("ReportVerifySummary"));
//!         if (report_verif_enabled_) {
//!             stats_ns->addToSchema([](simdb::Schema & schema) {
//!                 schema.addTable("ReportVerifySummary")
//!                     .addColumn(...)
//!                     .addColumn(...);
//!             });
//!         }
//!     \endcode
#define REGISTER_SIMDB_SCHEMA_BUILDER(db_namespace, schema_builder)     \
  simdb::DatabaseRoot::registerSchemaBuilderForNamespace(#db_namespace, schema_builder)

}

#endif
