// <ObjectManager> -*- C++ -*-

//SimDB headers
#include "simdb/ObjectManager.hpp"
#include "simdb/ObjectRef.hpp"
#include "simdb/TableRef.hpp"
#include "simdb/TableProxy.hpp"
#include "simdb/utils/ObjectQuery.hpp"
#include "simdb/utils/uuids.hpp"
#include "simdb/Errors.hpp"
#include "simdb/async/AsyncTaskEval.hpp"

//SQLite-specific headers
#include "simdb/impl/sqlite/TransactionUtils.hpp"
#include "simdb/impl/sqlite/Schema.hpp"
#include "simdb/impl/sqlite/SQLiteConnProxy.hpp"
#include <sqlite3.h>

//HDF5-specific headers
#include "simdb/impl/hdf5/HDF5ConnProxy.hpp"

//Standard headers
#include <map>
#include <set>

namespace simdb {

/*!
 * \brief Take all of the fully qualified table names an
 * ObjectManager has, split them around the table namespace
 * delimiter, and return a mapping from unqualified table
 * name to its associated namespace(s), and vice versa.
 */
static void parseTableNamespaces(
    const std::unordered_set<std::string> & full_table_names,
    std::unordered_map<std::string, std::set<std::string>> & tables_by_namespace,
    std::unordered_map<std::string, std::set<std::string>> & namespaces_by_table)
{
    for (const auto & full_table_name : full_table_names) {
        auto delim_idx = full_table_name.find(std::string(1, Table::NS_DELIM));
        if (delim_idx != std::string::npos && delim_idx + 1 < full_table_name.size()) {
            const std::string namespace_name = full_table_name.substr(0, delim_idx);
            const std::string unqualified_table_name = full_table_name.substr(delim_idx + 1);
            tables_by_namespace[namespace_name].insert(unqualified_table_name);
            namespaces_by_table[unqualified_table_name].insert(namespace_name);
        }
    }
}

//! RAII used for beginTransaction()/commitTransaction()
//! calls into the DbConnProxy class.
struct ScopedTransaction {
    ScopedTransaction(const DbConnProxy * db_proxy,
                      ObjectManager::TransactionFunc & transaction,
                      bool & in_transaction_flag) :
        db_proxy_(db_proxy),
        transaction_(transaction),
        in_transaction_flag_(in_transaction_flag)
    {
        in_transaction_flag_ = true;
        db_proxy_->beginAtomicTransaction();
        transaction_();
    }

    ~ScopedTransaction()
    {
        db_proxy_->commitAtomicTransaction();
        in_transaction_flag_ = false;
    }

private:
    //Open database connection
    const DbConnProxy * db_proxy_ = nullptr;

    //The caller's function they want inside BEGIN/COMMIT
    ObjectManager::TransactionFunc & transaction_;

    //The caller's "in transaction flag" - in case they
    //need to know whether *their code* is already in
    //an ongoing transaction:
    //
    //  void MyObj::callSomeSQL(DbConnProxy * db_proxy) {
    //      if (!already_in_transaction_) {
    //          ScopedTransaction(db_proxy,
    //              [&](){ eval_sql(db_proxy, "INSERT INTO Customers ..."); },
    //              already_in_transaction_);
    //
    //          //Now call another method which MIGHT
    //          //call this "callSomeSQL()" method again:
    //          callFooBarFunction_();
    //      } else {
    //          eval_sql(db_proxy, "INSERT INTO Customers ...");
    //      }
    //  }
    //
    //The use of this flag lets functions like MyObj::callSomeSQL()
    //be safely called recursively. Without it, "BEGIN TRANSACTION"
    //could get called a second time like this:
    //
    //     BEGIN TRANSACTION
    //     INSERT INTO Customers ...
    //     BEGIN TRANSACTION            <-- SQLite will error!
    //                          (was expecting COMMIT TRANSACTION before
    //                                   seeing this again)
    bool & in_transaction_flag_;
};

//Helper to get rid of 'unused variable' errors and keep the code tidy
#define LOCAL_TRANSACTION(db_proxy, transaction, transaction_flag)   \
    ScopedTransaction raii(db_proxy, transaction, transaction_flag); \
    (void) raii;

//! Database files are currently given a random file name, like:
//!     345l34-gu345lkj-234lsdf-kjh892y.db
//!
//! Users only have control over the directory where the database
//! should live, but not the file name.
static std::string generateRandomDatabaseFilename(
    const std::string & extension)
{
    return generateUUID() + extension;
}

ObjectManager::ObjectManager(const std::string & db_dir) :
    db_dir_(db_dir),
    task_queue_(new AsyncTaskEval),
    warning_log_(db_dir + "/database.warn")
{
}

void ObjectManager::addToTaskController(
    AsyncTaskController * controller)
{
    task_queue_->setSimulationDatabase(this);
    task_queue_->addToTaskController(controller);
    task_controller_ = controller;
}

void ObjectManager::captureTableSummaries()
{
    if (db_proxy_) {
        safeTransaction([&]() {
            const auto & summary_source_tables = schema_.summary_query_info_structs_.source_tables;
            for (const auto & summary_table : summary_source_tables) {
                const std::string & source_table_name = summary_table.table_name;
                if (!schema_.shouldSummarizeTable_(source_table_name)) {
                    continue;
                }
                getTable(source_table_name)->captureSummary();
            }
        });
    }
}

ObjectManager::~ObjectManager()
{
}

void ObjectManager::openDatabaseWithoutSchema_()
{
    assertNoDatabaseConnectionOpen_();
    const std::string extension = db_proxy_->getDatabaseFileExtension();
    const std::string db_file = generateRandomDatabaseFilename(extension);
    openDbFile_(db_file, true);
}

void ObjectManager::assertNoDatabaseConnectionOpen_() const
{
    if (db_proxy_ == nullptr) {
        return;
    }

    //For now, we only allow one ObjectManager owning
    //one SimDB connection. This method is called in
    //several places where we need to make sure a user
    //isn't accidentally trying to open a new connection
    //when we already have one opened.
    if (db_proxy_->isValid()) {
        throw DBException(
            "A database connection has already been "
            "made for this ObjectManager");
    }
}

//Ask the proxy object to give us the table names in
//the database. These are cached in memory after that
//for performance reasons.
void ObjectManager::getDatabaseTableNames_() const
{
    if (!table_names_.empty()) {
        return;
    }
    db_proxy_->getTableNames(table_names_);

    if (table_names_.empty()) {
        table_names_ = default_table_names_;
        default_table_names_.clear();
    }
}

bool ObjectManager::connectToExistingDatabase(const std::string & db_file)
{
    assertNoDatabaseConnectionOpen_();

    bool connected = false;
    auto slash = db_file.find_last_of(".");

    if (slash < db_file.size()) {
        //Use the file extension to take a best guess on the
        //database format. The DbConnProxy subclasses will
        //verify the file for us.
        const std::string extension = db_file.substr(slash);
        if (extension == ".db") {
            db_proxy_.reset(new SQLiteConnProxy);
            connected = db_proxy_->connectToExistingDatabase(db_file);
        }
        if (!connected && extension == ".h5") {
            db_proxy_.reset(new HDF5ConnProxy);
            connected = db_proxy_->connectToExistingDatabase(db_file);
        }
        if (!connected && extension == ".todo") {
            //todo (other implementations get added here)
        }
    } else {
        //There is no file extension. Go through each database
        //implementation that SimDB supports until we find one
        //that verifies the file format.
        db_proxy_.reset(new SQLiteConnProxy);
        connected = db_proxy_->connectToExistingDatabase(db_file);

        if (!connected) {
            db_proxy_.reset(new HDF5ConnProxy);
            connected = db_proxy_->connectToExistingDatabase(db_file);
        }
        if (!connected) {
            //todo (other implementations get added here)
        }
    }

    if (!connected) {
        db_proxy_.reset();
        db_full_filename_.clear();
        return false;
    }

    db_full_filename_ = db_proxy_->getDatabaseFullFilename();
    return true;
}

const std::string & ObjectManager::getDatabaseFile() const
{
    return !db_full_filename_.empty() ? db_full_filename_ : db_dir_;
}

//! This mutex is also guarding the 'is_in_transaction_' variable,
//! which is not protected against recursive calls in the same
//! way that ScopedTransaction is. This needs to be a recursive
//! mutex since safeTransaction() often gets called recursively.
//!
//! As long as this mutex is here and everyone is going through
//! safeTransaction() to make database calls, we will put as much
//! of the database work that we can through the worker thread /
//! simdb::AsyncTaskEval so the vast majority of the database calls
//! don't have to wait on this mutex. The only forced synchronous
//! flush we need today is at simulation end, and to do that we
//! will just put an interrupt task in the queue and wait for the
//! interrupt to be issued... after all of our pending database
//! queries / inserts have already run.
static std::recursive_mutex obj_mgr_transaction_mutex;

void ObjectManager::safeTransaction(TransactionFunc transaction) const
{
    const DbConnProxy * db_proxy = getDbConn();

    //There are "normal" or "acceptable" SQLite errors that
    //we trap: SQLITE_BUSY (the database file is locked), and
    //SQLITE_LOCKED (a table in the database is locked). These
    //can occur when SQLite is used in concurrent systems, and
    //are not necessarily "real" errors.
    //
    //If these *specific* types of errors occur, we will catch
    //them and keep retrying the transaction until successful.
    //This is part of what is meant by a "safe" transaction.
    //Database transactions will not fail due to concurrent
    //access errors that are not always obvious from a SPARTA
    //user/developer's perspective.

    while (true) {
        try {
            //More thought needs to go into thread safety of the
            //database writes/reads. Let's be super lazy and grab
            //a mutex right here for the time being.
            std::lock_guard<std::recursive_mutex> lock(obj_mgr_transaction_mutex);

            //Check to see if we are already in a transaction, in which
            //case we simply call the transaction function. We cannot
            //call "BEGIN TRANSACTION" recursively.
            if (is_in_transaction_ || !db_proxy->supportsAtomicTransactions()) {
                transaction();
            } else {
                LOCAL_TRANSACTION(db_proxy, transaction, is_in_transaction_)
            }

            //We got this far without an exception, which means
            //that the proxy's commitAtomicTransaction() method
            //has been called (if it supports atomic transactions).
            break;

        //Retry transaction due to database access errors
        } catch (const DBAccessException & ex) {
            warning_log_ << ex.what() << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            continue;
        }

        //Note that other std::exceptions are still being thrown,
        //and may abort the simulation
    }
}

bool ObjectManager::createDatabaseFromSchema(
    Schema & schema,
    std::unique_ptr<DbConnProxy> db_proxy)
{
    if (db_proxy == nullptr) {
        return false;
    }

    schema.finalizeSchema_();
    db_proxy->validateSchema(schema);
    db_proxy_ = std::move(db_proxy);
    schema_ = schema;

    for (const auto & table : schema) {
        default_table_names_.insert(table.getName());
        if (table.isFixedSize()) {
            fixed_size_tables_.insert(table.getName());
        }
    }

    openDatabaseWithoutSchema_();
    db_proxy_->realizeSchema(schema, *this);

    if (db_proxy_->isValid()) {
        getAndStoreDatabaseID_();
        return true;
    }
    return false;
}

bool ObjectManager::appendSchema(Schema & schema)
{
    if (db_proxy_ == nullptr) {
        return false;
    } else if (!db_proxy_->isValid()) {
        throw DBException("Attempt to append schema tables to ")
            << "an ObjectManager that does not have a valid "
            << "database connection";
    }

    schema.finalizeSchema_();
    db_proxy_->validateSchema(schema);

    for (const auto & table : schema) {
        const std::string table_name = table.getName();
        if (!table_names_.empty()) {
            table_names_.insert(table_name);
        }
        default_table_names_.insert(table_name);
        if (table.isFixedSize()) {
            fixed_size_tables_.insert(table_name);
        }
    }

    db_proxy_->realizeSchema(schema, *this);
    return true;
}

std::string ObjectManager::getStatsTableName_(
    const std::string & table_name) const
{
    auto qualified_table_name = getQualifiedTableName(table_name);
    if (qualified_table_name.empty()) {
        qualified_table_name = getQualifiedTableName(table_name, "Stats");
    }
    return qualified_table_name;
}

std::string ObjectManager::getQualifiedTableName(
    const std::string & table_name,
    const utils::lowercase_string & namespace_hint) const
{
    {
        auto outer_iter = cached_qualified_table_names_.find(table_name);
        if (outer_iter != cached_qualified_table_names_.end()) {
            auto inner_iter = outer_iter->second.find(namespace_hint);
            if (inner_iter != outer_iter->second.end()) {
                return inner_iter->second;
            }
        }
    }

    auto cache_qualified_table_name = [&](const std::string & qualified) {
        auto & ret = cached_qualified_table_names_[table_name][namespace_hint];
        ret = qualified;
        return ret;
    };

    const auto & table_names = getTableNames();
    const std::string & table_name_as_is = table_name;
    if (table_names.count(table_name_as_is)) {
        return cache_qualified_table_name(table_name_as_is);
    }

    std::unordered_map<std::string, std::set<std::string>> tables_by_namespace;
    std::unordered_map<std::string, std::set<std::string>> namespaces_by_table;
    parseTableNamespaces(table_names, tables_by_namespace, namespaces_by_table);

    if (namespace_hint.empty()) {
        auto iter = namespaces_by_table.find(table_name);
        if (iter == namespaces_by_table.end()) {
            return "";
        }
        if (iter->second.size() == 1) {
            auto possible = *(iter->second.begin()) + Table::NS_DELIM + table_name;
            if (table_names.count(possible)) {
                return cache_qualified_table_name(possible);
            }
        }
        return "";
    }

    std::string hint = namespace_hint;
    hint += std::string(1, Table::NS_DELIM);
    hint += table_name;
    if (table_names.count(hint)) {
        return hint;
    }

    return "";
}

std::unique_ptr<TableRef> ObjectManager::ObjectDatabase::getTable(
    const std::string & table_name) const
{
    if (db_namespace_.empty()) {
        return sim_db_->getTable(table_name);
    }

    return sim_db_->getTable(
        db_namespace_ + Table::NS_DELIM + table_name);
}

TableProxy * ObjectManager::ObjectDatabase::getConditionalTable(
    const std::string & table_name) const
{
    const std::string name =
        db_namespace_.empty() ? table_name :
        db_namespace_ + Table::NS_DELIM + table_name;

    auto iter = table_proxies_.find(name);
    if (iter != table_proxies_.end()) {
        return iter->second.get();
    }

    std::shared_ptr<TableProxy> proxy(new TableProxy(
        name, *sim_db_, db_namespace_obj_));
    table_proxies_[name] = proxy;
    return proxy.get();
}

void ObjectManager::ObjectDatabase::grantAccess() {
    for (auto & proxy : table_proxies_) {
        proxy.second->grantAccess();
    }
    access_granted_ = true;
}

void ObjectManager::ObjectDatabase::revokeAccess() {
    for (auto & proxy : table_proxies_) {
        proxy.second->revokeAccess();
    }
    access_granted_ = false;
}

std::unique_ptr<TableRef> ObjectManager::getTable_(
    const std::string & table_name) const
{
    if (table_name.empty()) {
        return nullptr;
    }

    //Ask the database for its table names, and
    //cache them in memory
    getDatabaseTableNames_();

    //Return null if this is not a table in this database
    auto iter = table_names_.find(table_name);
    if (iter == table_names_.end()) {
        return nullptr;
    }

    //Table name is valid. Return a wrapper around this table.
    AnySizeObjectFactory any_size_factory;
    FixedSizeObjectFactory fixed_size_factory;

    auto fixed_iter = fixed_size_tables_.find(table_name);
    if (fixed_iter != fixed_size_tables_.end()) {
        auto fixed_size_factory_iter = fixed_size_record_factories_.find(table_name);
        if (fixed_size_factory_iter == fixed_size_record_factories_.end()) {
            fixed_size_factory = db_proxy_->
                getFixedSizeObjectFactoryForTable(table_name);

            if (!fixed_size_factory) {
                fixed_size_tables_.erase(table_name);
            } else {
                fixed_size_record_factories_[table_name] = fixed_size_factory;
            }
        } else {
            fixed_size_factory = fixed_size_factory_iter->second;
        }
    }

    auto any_iter = any_size_record_factories_.find(table_name);
    if (any_iter == any_size_record_factories_.end()) {
        any_size_factory = db_proxy_->
            getObjectFactoryForTable(table_name);

        if (any_size_factory) {
            any_size_record_factories_[table_name] = any_size_factory;
        }
    } else {
        any_size_factory = any_iter->second;
    }

    NamedSummaryFunctions summary_fcns;
    std::vector<ColumnDescriptor> col_metadata;
    for (const auto & tbl : schema_.summary_query_info_structs_.source_tables) {
        if (tbl.table_name == table_name) {
            col_metadata = tbl.table_columns;
            summary_fcns = schema_.summary_query_info_structs_.summary_fcns;
            break;
        }
    }

    return std::unique_ptr<TableRef>(new TableRef(
        table_name, *this, db_proxy_,
        col_metadata,
        summary_fcns,
        any_size_factory,
        fixed_size_factory));
}

const std::unordered_set<std::string> &
    ObjectManager::ObjectDatabase::getTableNames() const
{
    if (!table_names_.empty()) {
        return table_names_;
    }

    if (db_namespace_.empty()) {
        table_names_.clear();
        return table_names_;
    }

    table_names_.clear();
    auto table_names = sim_db_->getTableNames_();
    const std::string target_str = db_namespace_ + Table::NS_DELIM;

    for (const auto & table_name : table_names) {
        if (table_name.find(target_str) == 0) {
            table_names_.insert(table_name.substr(target_str.size()));
        }
    }
    return table_names_;
}

const std::unordered_set<std::string> & ObjectManager::getTableNames_() const
{
    getDatabaseTableNames_();
    return table_names_;
}

void ObjectManager::getAndStoreDatabaseID_()
{
    if (uuid_ > 0) {
        return;
    }
    if (db_proxy_ == nullptr) {
        return;
    }

    if (!db_proxy_->isValid()) {
        throw DBException("There is no database connection yet. ")
            << "The ObjectManager::getAndStoreDatabaseID_() method "
            << "cannot be called.";
    }

    safeTransaction([&]() {
        //TODO: For custom-defined schemas, this table probably
        //will not exist. We should think about whether we can
        //safely add this table to these custom schemas. For now,
        //this UUID is only being used for SI/report-related
        //database work, i.e. using the default provided schema.
        //We should be able to safely warn and early return.
        auto obj_mgr_table_name = getQualifiedTableName(
            "ObjectManagersInDatabase", "Stats");
        auto obj_mgr_uuids_tbl = getTable_(obj_mgr_table_name);
        if (obj_mgr_uuids_tbl == nullptr) {
            if (warnings_enabled_) {
                std::cout << "Custom SimDB schema detected. You will not "
                          << "be able to make use of the ObjectManager::getId() "
                          << "method for anything useful; all ObjectManager "
                          << "connections made to this schema will return "
                          << "0 if getId() is called."
                          << std::endl;
            }
            return;
        }

        simdb::ObjectQuery query(*this, "ObjectManagersInDatabase");

        int32_t obj_mgr_id = 0;
        query.writeResultIterationsTo("ObjMgrID", &obj_mgr_id);

        //We are looking for the max ObjMgrID in this database,
        //and we'll take the ID that is 1 greater than it.
        query.orderBy(OrderBy("ObjMgrID", DESC));
        query.setLimit(1);
        auto result_iter = query.executeQuery();
        result_iter->getNext();

        //Just increment the maximum existing UUID by 1 and add
        //an entry to this table accordingly.
        uuid_ = obj_mgr_id + 1;

        obj_mgr_uuids_tbl->createObjectWithArgs("ObjMgrID", uuid_);
    });
}

std::unique_ptr<ObjectRef> ObjectManager::ObjectDatabase::findObject(
    const std::string & table_name,
    const DatabaseID db_id) const
{
    if (db_namespace_.empty()) {
        return sim_db_->findObject(table_name, db_id);
    }
    return sim_db_->findObject(
        db_namespace_ + Table::NS_DELIM + table_name, db_id);
}

std::unique_ptr<ObjectRef> ObjectManager::findObject_(
    const std::string & table_name,
    const DatabaseID db_id) const
{
    if (db_proxy_ == nullptr) {
        return nullptr;
    }

    if (!db_proxy_->supportsObjectQuery_()) {
        if (db_proxy_->hasObject_(table_name, db_id)) {
            return std::unique_ptr<ObjectRef>(
                new ObjectRef(*this, table_name, db_id));
        } else {
            return nullptr;
        }
    }

    //We *could* first check if the 'table_name' is even
    //in our set of known tables. We could return null in
    //that case. But an object should really be unfound
    //if the *database ID* was not found, NOT because the
    //table name wasn't even legit. Let's not take the
    //small performance hit of the map/set lookup, and
    //just let SQLite hard error if the table name is
    //bad. This is probably a bug anyway.

    //Try to find the record in that table whose 'Id'
    //(primary key) is the one we're looking for.
    ObjectQuery query(*this, table_name);
    query.addConstraints("Id", constraints::equal, db_id);

    //This is only considered a "found" record if we found
    //exactly one record with this Id. Since this is a primary
    //key, we could also assert that it is either 0 (not found)
    //or 1 (found).
    std::unique_ptr<ObjectRef> obj_ref;
    if (query.countMatches() == 1) {
        obj_ref.reset(new ObjectRef(*this, table_name, db_id));
    }
    return obj_ref;
}

void ObjectManager::ObjectDatabase::findObjects(
    const std::string & table_name,
    const std::vector<DatabaseID> & db_ids,
    std::vector<std::unique_ptr<ObjectRef>> & obj_refs) const
{
    if (db_namespace_.empty()) {
        sim_db_->findObjects(table_name, db_ids, obj_refs);
    } else {
        sim_db_->findObjects(
            db_namespace_ + Table::NS_DELIM + table_name, db_ids, obj_refs);
    }
}

void ObjectManager::findObjects_(
    const std::string & table_name,
    const std::vector<DatabaseID> & db_ids,
    std::vector<std::unique_ptr<ObjectRef>> & obj_refs) const
{
    obj_refs.clear();

    if (db_proxy_ == nullptr) {
        return;
    }

    ObjectQuery query(*this, table_name);
    if (!db_ids.empty()) {
        query.addConstraints("Id", constraints::in_set, db_ids);
    }

    DatabaseID found_id;
    query.writeResultIterationsTo("Id", &found_id);

    std::set<DatabaseID> found_ids;
    auto result_iter = query.executeQuery();
    while (result_iter->getNext()) {
        found_ids.insert(found_id);
    }

    if (!db_ids.empty()) {
        obj_refs.reserve(db_ids.size());
        for (const auto db_id : db_ids) {
            if (found_ids.count(db_id) > 0) {
                obj_refs.emplace_back(new ObjectRef(*this, table_name, db_id));
            } else {
                obj_refs.emplace_back(nullptr);
            }
        }
    } else {
        obj_refs.reserve(found_ids.size());
        for (const auto db_id : found_ids) {
            obj_refs.emplace_back(new ObjectRef(*this, table_name, db_id));
        }
    }
}

std::unique_ptr<ObjectQuery> ObjectManager::ObjectDatabase::
    createObjectQueryForTable(const std::string & table_name) const
{
    auto sim_db = getObjectManager();
    if (!sim_db) {
        return nullptr;
    }

    auto qualified_table_name = sim_db->
        getQualifiedTableName(table_name, db_namespace_);

    if (qualified_table_name.empty()) {
        return nullptr;
    }

    return std::unique_ptr<ObjectQuery>(new ObjectQuery(
        *sim_db, qualified_table_name));
}

AsyncTaskEval* ObjectManager::ObjectDatabase::
    getTaskQueue() const
{
    auto sim_db = getObjectManager();
    if (!sim_db) {
        return nullptr;
    }
    return sim_db->getTaskQueue();
}

const DbConnProxy * ObjectManager::getDbConn() const
{
    return db_proxy_.get();
}

bool ObjectManager::openDbFile_(const std::string & db_file,
                                const bool create_file)
{
    if (db_proxy_ == nullptr) {
        return false;
    }

    const std::string db_proxy_filename =
        db_proxy_->openDbFile_(db_dir_, db_file, create_file);

    if (!db_proxy_filename.empty()) {
        //File opened without issue. Store the full DB filename.
        db_full_filename_ = db_proxy_filename;
        return true;
    }
    return false;
}

//! ------ DEPRECATED. -----------------------------------------
//! For backwards compatibility only.
//! May be removed in a future release.
//! ------------------------------------------------------------
std::unique_ptr<TableRef> ObjectManager::getTable(
    const std::string & table_name) const
{
    std::string qualified_table_name =
        getQualifiedTableName(table_name);

    if (qualified_table_name.empty()) {
        qualified_table_name = getQualifiedTableName(table_name, "Stats");
    }
    if (qualified_table_name.empty()) {
        return nullptr;
    }
    return getTable_(qualified_table_name);
}

//! -------------------- (... deprecated ...) --------------------
const std::unordered_set<std::string> & ObjectManager::getTableNames() const
{
    return getTableNames_();
}

//! -------------------- (... deprecated ...) --------------------
std::unique_ptr<ObjectRef> ObjectManager::findObject(
    const std::string & table_name,
    const DatabaseID db_id) const
{
    if (!db_proxy_->supportsObjectQuery_()) {
        if (db_proxy_->hasObject_(table_name, db_id)) {
            return std::unique_ptr<ObjectRef>(
                new ObjectRef(*this, table_name, db_id));
        }
        return nullptr;
    }

    auto table = getTable_(table_name);
    if (!table) {
        table = getTable_(getStatsTableName_(table_name));
        if (!table) {
            return nullptr;
        }
        return findObject_(getStatsTableName_(table_name), db_id);
    }
    return findObject_(table_name, db_id);
}

//! -------------------- (... deprecated ...) --------------------
void ObjectManager::findObjects(
    const std::string & table_name,
    const std::vector<DatabaseID> & db_ids,
    std::vector<std::unique_ptr<ObjectRef>> & obj_refs) const
{
    auto table = getTable_(table_name);
    if (!table) {
        table = getTable_(getStatsTableName_(table_name));
        if (!table) {
            return;
        }
        findObjects_(getStatsTableName_(table_name), db_ids, obj_refs);
    }
    findObjects_(table_name, db_ids, obj_refs);
}
//! ---------------------- (end DEPRECATED) ----------------------

} // namespace simdb
