// <ObjectManager> -*- C++ -*-

#pragma once

#include "simdb/schema/Schema.hpp"
#include "simdb/ObjectFactory.hpp"
#include "simdb/utils/StringUtils.hpp"
#include "simdb_fwd.hpp"

#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>

namespace simdb {

//! Warnings utility that will log any messages for
//! you into memory, and write them to file when the
//! object goes out of scope.
class WarningLogger
{
public:
    explicit WarningLogger(const std::string & warn_filename) :
        warn_filename_(warn_filename)
    {}

    ~WarningLogger()
    {
        const std::string warnings = msgs_.str();
        if (!warnings.empty()) {
            std::ofstream fout(warn_filename_);
            fout << warnings;
        }
    }

    template <typename MessageT>
    const WarningLogger & operator<<(const MessageT & msg) const
    {
        msgs_ << msg;
        return *this;
    }

    const WarningLogger & operator<<(const char * msg) const
    {
        msgs_ << msg;
        return *this;
    }

private:
    mutable std::ostringstream msgs_;
    const std::string warn_filename_;
};

/*!
 * \brief Database object manager. Used in order to create
 * databases with a user-specified schema, and connect to
 * existing databases that were previously created with
 * another ObjectManager.
 */
class ObjectManager
{
public:
    //! Construct an ObjectManager. This does not open any
    //! database connection or create any database just yet.
    //! The database path that you pass in is wherever you
    //! want the database to ultimately live.
    ObjectManager(const std::string & db_dir = ".");

    //! Using a Schema object which specifies the Tables,
    //! Columns, and Indexes for your database, construct
    //! the physical database file and open the connection.
    //!
    //! Returns true if successful, false otherwise.
    bool createDatabaseFromSchema(
        Schema & schema,
        std::unique_ptr<DbConnProxy> db_proxy);

    //! After calling createDatabaseFromSchema(), you may
    //! add additional tables with this method. If a table
    //! has the same name as an existing table in this database,
    //! all of the table columns need to match exactly as
    //! well, or this method will throw. If the columns
    //! match however, the table will be ignored as it
    //! already exists in the schema.
    //!
    //! Returns true if the provided schema's tables were
    //! successfuly added to this ObjectManager's schema,
    //! otherwise returns false.
    bool appendSchema(Schema & schema);

    //! Open a database connection to an existing database
    //! file. The 'db_file' that you pass in should be the
    //! full database path, including the file name and
    //! extension. For example, "/path/to/my/dir/statistics.db"
    //!
    //! This 'db_file' is typically one that was given to you
    //! from a previous call to ObjectManager::getDatabaseFile()
    //!
    //! Returns true if successful, false otherwise.
    bool connectToExistingDatabase(const std::string & db_file);

    //! Get the full database file name, including its path and
    //! file extension. If the database has not been opened or
    //! created yet, this will just return the database path.
    const std::string & getDatabaseFile() const;

    //! Get the internal database proxy. Will return nullptr
    //! if no database connection has been made yet.
    const DbConnProxy * getDbConn() const;

    //! Get this database connection's task queue. This
    //! object can be used to schedule database work to
    //! be executed on a background thread. This never
    //! returns null.
    AsyncTaskEval * getTaskQueue() const {
        return task_queue_.get();
    }

    //! Every ObjectManager has its own AsyncTaskEval
    //! which can be used to schedule database work
    //! on a background thread. If you have multiple
    //! of these ObjectManager's, you may want them
    //! all to share the same background thread. In
    //! that case, create an AsyncTaskController
    //! object, and tell each of your ObjectManager's
    //! to "addToTaskController()". All calls that
    //! forward work to this ObjectManager's task queue
    //! will get rerouted to the same work queue
    //! inside your AsyncTaskController.
    //!
    //!    auto controller = std::make_shared<AsyncTaskController>();
    //!    ObjectManager mgrA(".");
    //!    ObjectManager mgrB(".");
    //!
    //!    //... set up schemas, set up proxies, ...
    //!
    //!    mgrA.addToTaskController(controller);
    //!    mgrB.addToTaskController(controller);
    //!
    //!    //Schedule hypothetical WorkerTask objects
    //!    //on the shared controller's work thread:
    //!    mgrA.getTaskQueue()->addWorkerTask(
    //!        std::unique_ptr<WorkerTask>(new TaskA));
    //!
    //!    mgrB.getTaskQueue()->addWorkerTask(
    //!        std::unique_ptr<WorkerTask>(new TaskB));
    //!
    //!    //Typically, you would not immediately flush
    //!    //the worker thread, instead letting it consume
    //!    //your scheduled work in the background, but
    //!    //to illustrate use of one of the controller
    //!    //APIs, the TaskA and TaskB work requests would
    //!    //get completed with this call, if they aren't
    //!    //already underway in the background:
    //!    controller->flushQueue();
    //!
    void addToTaskController(
        AsyncTaskController * controller);

    //! Get a unique identifier for this database connection.
    //! Returns 0 if there is no open connection yet, which
    //! happens during a call to "createDatabaseFromSchema()"
    int32_t getId() const {
        return uuid_;
    }

    //! Warnings are printed to stdout. Disable warnings by
    //! calling this method. Warnings are enabled by default.
    void disableWarningMessages() {
        warnings_enabled_ = false;
    }

    //! Re-enable disabled warnings. Warnings are enabled
    //! by default.
    void enableWarningMessages() {
        warnings_enabled_ = true;
    }

    //! Open database connections will be closed when the
    //! destructor is called.
    ~ObjectManager();

    //! All API calls to ObjectManager, ObjectRef, and the
    //! other database classes will be executed inside "safe
    //! transactions" for exception safety and for better
    //! performance. Failed database writes/reads will be
    //! retried until successful. This will also improve
    //! performance - especially for DB writes - if you
    //! have several operations that you need to perform
    //! on the database, for example:
    //!
    //! \code
    //!     ObjectRef new_customer(...)
    //!     new_customer.setPropertyString("First", "Bob")
    //!     new_customer.setPropertyString("Last", "Smith")
    //!     new_customer.setPropertyInt32("Age", 41)
    //! \endcode
    //!
    //! That would normally be three individual transactions.
    //! But if you do this instead (assuming you have an
    //! ObjectManager 'obj_mgr' nearby):
    //!
    //! \code
    //!     obj_mgr.safeTransaction([&]() {
    //!         ObjectRef new_customer(...)
    //!         new_customer.setPropertyString("First", "Bob")
    //!         new_customer.setPropertyString("Last", "Smith")
    //!         new_customer.setPropertyInt32("Age", 41)
    //!     });
    //! \endcode
    //!
    //! That actually ends up being just *one* database
    //! transaction. Not only is this faster (in some
    //! scenarios it can be a very significant performance
    //! boost) but all three of these individual setProperty()
    //! calls would either be committed to the database, or
    //! they wouldn't, maybe due to an exception. But the
    //! "new_customer" object would not have the "First"
    //! property written to the database, while the "Last"
    //! and "Age" properties were left unwritten. "Half-
    //! written" database objects could result in difficult
    //! bugs to track down, or leave your data in an
    //! inconsistent state.
    typedef std::function<void()> TransactionFunc;
    void safeTransaction(TransactionFunc transaction) const;

    class ObjectDatabase {
    public:
        //! Get a wrapper to a Table in this database.
        //! Returns nullptr if the table name you provide
        //! is not in this database's schema. Call the
        //! 'getTableNames()' method to see the available
        //! tables (originally taken from the Schema that
        //! this ObjectManager used to create the database).
        std::unique_ptr<TableRef> getTable(
            const std::string & table_name) const;

        //! There are scenarios where a TableRef either cannot
        //! be returned by the above 'getTable()' method if the
        //! namespace schema has not yet been fully realized,
        //! or the schema is realized but the table is still
        //! not writable at this time for another reason.
        //!
        //! Tables can conditionally allow or disallow access
        //! in response to triggered events (such as specific
        //! conditions in a simulation).
        //!
        //! Using this method will never return null, and the
        //! returned proxy object has an 'isWritable()' method
        //! to check its accessibility state before proceeding.
        //! The proxy also has a 'getTable()' method of its own
        //! which returns the underlying TableRef pointer if
        //! the table / namespace is accessible at that time,
        //! or null if the table is still inaccessible.
        TableProxy * getConditionalTable(
            const std::string & table_name) const;

        //! Get a list of Table names in this database
        const std::unordered_set<std::string> & getTableNames() const;

        //! Find any database record by its table name and
        //! its unique database ID in that table.
        std::unique_ptr<ObjectRef> findObject(
            const std::string & table_name,
            const DatabaseID db_id) const;

        //! Find a group of database records in a given table
        //! by their unique database ID's in that table. This
        //! will not verify that the 'db_ids' you pass in are
        //! in fact unique. The returned 'obj_refs' will always
        //! be equal in length to the 'db_ids' that you pass in.
        //! Database ID's that did not have a record in this table
        //! will have a nullptr in that ObjectRef's spot in 'obj_refs'.
        //!
        //! \code
        //!     std::vector<uptr<ObjectRef>> obj_refs;
        //!     obj_mgr.findObjects("Customers", {44, 68, 92}, obj_refs);
        //!
        //!     if (obj_refs[0]) {
        //!         std::cout << "Customer with id 44 found" << std::endl;
        //!     } else {
        //!         std::cout << "Customer with id 44 was NOT found" << std::endl;
        //!     }
        //!     //...
        //!
        //!     assert(obj_refs.size() == 3,
        //!            "No matter what, ObjectManager::findObjects() should "
        //!            "return a vector of ObjectRef's that is the same length "
        //!                 "as the vector of database IDs that you pass in.");
        //! \endcode
        //!
        //! If you want to get *all* records in the given table, pass in {}
        //! for the 'db_ids' input.
        void findObjects(
            const std::string & table_name,
            const std::vector<DatabaseID> & db_ids,
            std::vector<std::unique_ptr<ObjectRef>> & obj_refs) const;

        //! Get the underlying data file location. This is the full path
        //! to the file, the file stem, and the file extension.
        const std::string & getDatabaseFile() const {
            return sim_db_->getDatabaseFile();
        }

        //! Perform constrained queries against the underlying database
        //! to find records that match a specific set of criteria. See
        //! simdb/include/simdb/utils/ObjectQuery.h for more details
        //! on how to use this class.
        //!
        //! \warning ObjectQuery is currently only supported for SQLite
        //! database connections, and will throw if you attempt to use
        //! it with another database format such as HDF5.
        std::unique_ptr<ObjectQuery> createObjectQueryForTable(
            const std::string & table_name) const;

        //! Get the task queue associated with the underlying ObjectManager.
        //! The WorkerTask's you give to the task queue will be invoked
        //! on a background thread, inside of ObjectManager::safeTransaction()
        //! calls.
        AsyncTaskEval * getTaskQueue() const;

        //! Get the underlying database. This is not necessarily unique
        //! to this one ObjectDatabase; it may be shared among many DB
        //! namespaces.
        //!
        //! \warning This method may be removed in a future release.
        //! It is currently here primarily for backwards compatibility.
        ObjectManager * getObjectManager() const {
            return sim_db_;
        }

        //! This method is called when a SimDB namespace has
        //! just become available for reads and writes via
        //! the TableProxy class objects.
        void grantAccess();

        //! This method is called when a SimDB namespace has
        //! just become unavailable for reads and writes via
        //! the TableProxy class objects.
        void revokeAccess();

    private:
        ObjectDatabase(ObjectManager * sim_db,
                       const std::string & db_namespace,
                       DatabaseNamespace * db_namespace_obj = nullptr) :
            sim_db_(sim_db),
            db_namespace_(db_namespace),
            db_namespace_obj_(db_namespace_obj)
        {
            if (!sim_db_) {
                throw DBException("Null ObjectManager given to a ObjectDatabase");
            }
        }

        ObjectManager * sim_db_ = nullptr;
        std::string db_namespace_;
        DatabaseNamespace * db_namespace_obj_ = nullptr;
        mutable std::unordered_set<std::string> table_names_;
        mutable std::map<std::string, std::shared_ptr<TableProxy>> table_proxies_;
        bool access_granted_ = true;
        friend class DatabaseRoot;
        friend class DatabaseNamespace;
        friend class ObjectManager;
    };

    /*!
     * \brief ObjectManager's can have tables inside the database file
     * separated into different namespaces. Here is an example of a
     * database schema with four tables in two namespaces, using the
     * equivalent C++ pseudo-code to illustrate:
     *
     *         namespace gold {
     *             struct CustomerInfo {
     *                 string First;
     *                 string Last;
     *                 long AccountNumber;
     *             };
     *             struct RewardsMembers {
     *                 long AccountNumber;
     *                 int YearsActive;
     *                 short RewardsBalance;
     *             };
     *         }
     *
     *         namespace platinum {
     *             struct CustomerInfo {
     *                 string First;
     *                 string Middle;
     *                 string Last;
     *                 string Occupation;
     *                 long RewardsBalance;
     *             };
     *         }
     *
     * Say you want to access the RewardsMembers table, but you
     * are not sure which namespace it belongs to. You can call
     * this method in several ways to get the full table name in
     * this database, if it exists:
     *
     *   1) You don't know the exact namespace, but you do know that
     *      the RewardsMembers table only exists in one namespace.
     *
     *        auto qualified_table_name =
     *            obj_mgr.getQualifiedTableName("RewardsMembers");
     *
     *      In this case, qualified_table_name would be something
     *      like "gold$RewardsMembers" - a delimiter separates the
     *      namespace from the table name, but you should not rely
     *      on this delimiter being any specific character/string.
     *      The delimiter in use is implementation detail.
     *
     *   2) You are sure the namespace is "gold", but you don't
     *      want to hard code the '$' character in your code.
     *      You want to keep your code robust.
     *
     *        auto qualified_table_name =
     *            obj_mgr.getQualifiedTableName("RewardsMembers", "gold");
     *
     *      For this example, and assuming that the delimiter is
     *      '$', this would return "gold$RewardsMembers"
     *
     *   3) The table you *really* want is RewardsMembers in the
     *      'platinum' namespace. But your schemas are dynamically
     *      generated and this table sometimes does not exist in
     *      the 'platinum' namespace.
     *
     *        auto qualified_table_name =
     *            obj_mgr.getQualifiedTableName("RewardsMembers", "platinum");
     *
     *      For the above schema, this would return an empty string.
     *      It would NOT return "gold$RewardsMembers", as that is a
     *      completely different table, with a different column
     *      arrangement.
     *
     * Now let's say that you were looking for the "CustomerInfo"
     * table. This table appears in two namespaces. Here are some
     * return values for this example:
     *
     *   1) You are looking for CustomerInfo in the "gold" namespace.
     *
     *        auto qualified_table_name =
     *            obj_mgr.getQualifiedTableName("CustomerInfo", "gold");
     *
     *      Assuming a '$' delimiter, this would return "gold$CustomerInfo"
     *
     *   2) You are looking for CustomerInfo, and you think it only
     *      exists in one of the database namespaces, but you don't
     *      know the name of that namespace.
     *
     *        auto qualified_table_name =
     *            obj_mgr.getQualifiedTableName("CustomerInfo");
     *
     *      This would return an empty string. There were matches
     *      found in two namespaces, but since you did not provide
     *      any namespace hint, ObjectManager cannot know which
     *      qualified table name to return.
     */
    std::string getQualifiedTableName(
        const std::string & table_name,
        const utils::lowercase_string & namespace_hint = "") const;

    //! Generate table summary snapshot. Columns that support
    //! summaries (numeric scalars, and possibly other data types)
    //! will have their data values' min/max/average captured and
    //! put into separate summary tables, along with any other
    //! custom summary/aggregation calculations that were given
    //! to the schema's TableSummaries object.
    void captureTableSummaries();

    //! Get the schema this ObjectManager is using.
    Schema & getSchema() {
        return schema_;
    }

    //! Get the schema this ObjectManager is using.
    const Schema & getSchema() const {
        return schema_;
    }

    //! ------- DEPRECATED. Use the ObjectDatabase class above. -------
    std::unique_ptr<TableRef> getTable(
        const std::string & table_name) const;

    const std::unordered_set<std::string> & getTableNames() const;

    std::unique_ptr<ObjectRef> findObject(
        const std::string & table_name,
        const DatabaseID db_id) const;

    void findObjects(
        const std::string & table_name,
        const std::vector<DatabaseID> & db_ids,
        std::vector<std::unique_ptr<ObjectRef>> & obj_refs) const;
    //! ---------------------- (end DEPRECATED) -------------------------

private:
    //Complements ObjectDatabase::getTable() - the table name
    //passed in will be fully qualified ("<db_namespace>$<table_name>")
    std::unique_ptr<TableRef> getTable_(
        const std::string & table_name) const;

    //Complements ObjectDatabase::getTableNames() - the table name
    //passed in will be fully qualified ("<db_namespace>$<table_name>")
    const std::unordered_set<std::string> & getTableNames_() const;

    //Complements ObjectDatabase::findObject() - the table name
    //passed in will be fully qualified ("<db_namespace>$<table_name>")
    std::unique_ptr<ObjectRef> findObject_(
        const std::string & table_name,
        const DatabaseID db_id) const;

    //Complements ObjectDatabase::findObjects() - the table name
    //passed in will be fully qualified ("<db_namespace>$<table_name>")
    void findObjects_(
        const std::string & table_name,
        const std::vector<DatabaseID> & db_ids,
        std::vector<std::unique_ptr<ObjectRef>> & obj_refs) const;

    //Open the given database file. If the connection is
    //successful, this file will be the ObjectManager's
    //"db_full_filename_" value.
    bool openDbFile_(const std::string & db_file,
                     const bool create_file);

    //Try to just open an empty database file. This is
    //similar to fopen().
    void openDatabaseWithoutSchema_();

    //This class does not currently allow one ObjectManager
    //to be simultaneously connected to multiple databases.
    void assertNoDatabaseConnectionOpen_() const;

    //Ask the database what its table names are, and cache
    //them in memory for later.
    void getDatabaseTableNames_() const;
    mutable std::unordered_set<std::string> table_names_;
    mutable std::unordered_set<std::string> default_table_names_;

    //Helper method to get the full name of the provided
    //table in the "Stats" namespace. This is for backwards
    //compatibility only and may be removed in a future
    //release.
    std::string getStatsTableName_(
        const std::string & table_name) const;

    //Cached qualified table names. Used to boost performance
    //of ObjectManager::getQualifiedTableName()
    mutable std::unordered_map<
        std::string,           //Unqualified table name
        std::unordered_map<
            std::string,       //Namespace
            std::string        //Qualified table name
          >> cached_qualified_table_names_;

    //UUID for this database connection
    void getAndStoreDatabaseID_();
    int32_t uuid_ = 0;

    //Warnings are enabled by default. There are APIs in this
    //class for disabling/re-enabling warnings.
    bool warnings_enabled_ = true;

    //Physical database proxy. Commands (INSERT, UPDATE, etc.)
    //are executed against this proxy, not against the lower-
    //level database APIs directly.
    std::shared_ptr<DbConnProxy> db_proxy_;

    //Registered object factories by table name.
    mutable std::unordered_map<std::string, AnySizeObjectFactory> any_size_record_factories_;
    mutable std::unordered_map<std::string, FixedSizeObjectFactory> fixed_size_record_factories_;

    //Keep a list of fixed-size tables. For these tables, all of
    //their columns are POD's, and the DbConnProxy subclasses may
    //have an optimized object factory implementation for fixed-
    //size record creation.
    mutable std::unordered_set<std::string> fixed_size_tables_;

    //Copy of the schema that was given to the ObjectManager's
    //createDatabaseFromSchema() method.
    Schema schema_;

    //Location where this database lives, e.g. the tempdir
    const std::string db_dir_;

    //Task queue associated with this database connection.
    //It is instantiated from our constructor, but won't
    //have any effect unless its addWorkerTask() method
    //is called. That method starts a background thread
    //to begin consuming work packets.
    std::unique_ptr<AsyncTaskEval> task_queue_;

    //When this task controller is in use, any WorkerTask
    //that gets added to our AsyncTaskEval work queue will
    //be rerouted into this controller's own internal work
    //queue. This is used to enable many individual database
    //connections to write to the same database file using
    //just one worker thread.
    AsyncTaskController * task_controller_ = nullptr;

    //Full database file name, including the database path
    //and file extension
    std::string db_full_filename_;

    //Flag used in RAII safeTransaction() calls. This is
    //needed to we know whether to tell SQL to "BEGIN
    //TRANSACTION" or not (i.e. if we're already in the
    //middle of another safeTransaction).
    //
    //This allows users to freely do something like this:
    //
    //    obj_mgr_.safeTransaction([&]() {
    //        writeReportHeader_(report);
    //    });
    //
    //Even if their writeReportHeader_() code does the
    //same thing:
    //
    //    void CSV::writeReportHeader_(sparta::Report * r) {
    //        obj_mgr_.safeTransaction([&]() {
    //            writeReportName_(r);
    //            writeSimulationMetadata_(sim_);
    //        });
    //    }
    mutable bool is_in_transaction_ = false;

    //Logging utility to capture any database warnings or
    //other useful messages to a "database.warn" log file
    WarningLogger warning_log_;

    friend class TableProxy;
};

} // namespace simdb
