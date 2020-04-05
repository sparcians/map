// <TableRef> -*- C++ -*-

#pragma once

#include "simdb/schema/ColumnValueContainer.hpp"
#include "simdb/schema/ColumnTypedefs.hpp"
#include "simdb/ObjectFactory.hpp"
#include "simdb/utils/Stringifiers.hpp"
#include "simdb/Constraints.hpp"
#include "simdb/ObjectRef.hpp"
#include "simdb_fwd.hpp"

#include <memory>
#include <string>
#include <sstream>
#include <vector>
#include <tuple>
#include <functional>
#include <initializer_list>
#include <type_traits>

namespace simdb {

/*!
 * \brief Wrapper around a SimDB table
 */
class TableRef
{
public:
    //! Construct a TableRef for a SimDB table with the
    //! given name, and the ObjectManager it belongs to.
    //!
    //! Typically, you should get a TableRef object
    //! from an ObjectManager that you created first,
    //! instead of making a TableRef yourself manually.
    TableRef(const std::string & table_name,
             const ObjectManager & obj_mgr) :
        table_name_(table_name),
        obj_mgr_(obj_mgr)
    {}

    //! Create a new record in this table. Returns a
    //! wrapper around the new record.
    std::unique_ptr<ObjectRef> createObject();

    //! Intermediate class used in UPDATE statements:
    //!
    //!   table->updateRowValues("MyInt32", 100, "MyString", "bar").
    //!          forRecordsWhere("MyInt32", constraints::equal, 85);
    //!
    //! This would update a table with these records:
    //!
    //!      MyInt32    MyString    MyDouble
    //!     ---------  ----------  ----------
    //!      80         hello       3.45
    //!      85         world       4.56        <-- (match)
    //!      85         foo         5.67        <-- (match)
    //!      90         bar         6.78
    //!
    //! And the new table records would then be:
    //!
    //!      MyInt32    MyString    MyDouble
    //!     ---------  ----------  ----------
    //!      80         hello       3.45
    //!      100        bar         4.56        <-- (new values)
    //!      100        bar         5.67        <-- (new values)
    //!      90         bar         6.78
    class RecordFinder
    {
    public:
        //! WHERE clause in UPDATE statements for numeric
        //! column constraints.
        template <typename ColumnT>
        typename std::enable_if<
            std::is_fundamental<ColumnT>::value,
        size_t>::type
        forRecordsWhere(const char * col_name,
                        const constraints constraint,
                        const ColumnT col_val)
        {
            update_where_clauses_.add(col_name, col_val)
                ->setConstraint(constraint);
            return finalize_callback_();
        }

        //! WHERE clause in UPDATE statements for string
        //! column constraints.
        template <typename ColumnT>
        typename std::enable_if<
            std::is_same<typename std::decay<ColumnT>::type, const char*>::value,
        size_t>::type
        forRecordsWhere(const char * col_name,
                        const constraints constraint,
                        ColumnT col_val)
        {
            update_where_clauses_.add(col_name, col_val)
                ->setConstraint(constraint);
            return finalize_callback_();
        }

        //! WHERE clause in UPDATE statements for string
        //! column constraints.
        template <typename ColumnT>
        typename std::enable_if<
            std::is_same<ColumnT, std::string>::value,
        size_t>::type
        forRecordsWhere(const char * col_name,
                        const constraints constraint,
                        const ColumnT & col_val)
        {
            update_where_clauses_.add(col_name, col_val)
                ->setConstraint(constraint);
            return finalize_callback_();
        }

        //! WHERE clause in UPDATE statements for {num,num,...} and
        //! {string,string,...} column constraints ("IN SET" / "NOT
        //! IN SET").
        template <typename ColumnT>
        size_t forRecordsWhere(const char * col_name,
                               const constraints constraint,
                               const std::initializer_list<ColumnT> & col_val)
        {
            update_where_clauses_.add(col_name, col_val)
                ->setConstraint(constraint);
            return finalize_callback_();
        }

        //! WHERE clause in UPDATE statements for multi-column
        //! selection constraints.
        template <typename ColumnT, typename... Args>
        size_t forRecordsWhere(const char * col_name,
                               const constraints constraint,
                               const ColumnT & col_val,
                               Args &&... args)
        {
            update_where_clauses_.add(col_name, col_val)
                ->setConstraint(constraint);
            return forRecordsWhere(std::forward<Args>(args)...);
        }

        //! Unconstrained UPDATE statements terminate the call to
        //! TableRef::updateRowValues() with this method:
        //!
        //!    table->updateRowValues("MyString", "foobar", "MyDouble", 5.6).
        //!           forAllRecords();
        size_t forAllRecords()
        {
            return finalize_callback_();
        }

    private:
        //Maintain a list of WHERE clauses for the UPDATE.
        ColumnValueContainer update_where_clauses_;

        //Only to be used by TableRef. Cannot be created outside
        //of an UPDATE action.
        RecordFinder(const std::function<size_t()> & finalize_callback) :
            finalize_callback_(finalize_callback)
        {}
        friend class TableRef;

        //Callback set by TableRef when the forRecordsWhere()
        //parameter pack has been unrolled.
        std::function<size_t()> finalize_callback_;
    };

    //! UPDATE value clause for integer and floating-point columns.
    template <typename ColumnT>
    typename std::enable_if<
        std::is_fundamental<ColumnT>::value,
    RecordFinder&>::type
    updateRowValues(const char * col_name,
                    const ColumnT col_val)
    {
        col_values_.add(col_name, col_val);
        return makeRecordFinder_();
    }

    //! UPDATE value clause for string columns.
    template <typename ColumnT>
    typename std::enable_if<
        std::is_same<typename std::decay<ColumnT>::type, const char*>::value,
    RecordFinder&>::type
    updateRowValues(const char * col_name,
                    ColumnT col_val)
    {
        col_values_.add(col_name, col_val);
        return makeRecordFinder_();
    }

    //! UPDATE value clause for string columns.
    template <typename ColumnT>
    typename std::enable_if<
        std::is_same<ColumnT, std::string>::value,
    RecordFinder&>::type
    updateRowValues(const char * col_name,
                    const ColumnT & col_val)
    {
        return updateRowValues(col_name, col_val.c_str());
    }

    //! UPDATE value clause for blob columns (as an STL container)
    template <typename ColumnT>
    typename std::enable_if<
        is_container<ColumnT>::value and is_contiguous<ColumnT>::value,
    RecordFinder&>::type
    updateRowValues(const char * col_name,
                    const ColumnT & col_val)
    {
        col_values_.add(col_name, col_val);
        return makeRecordFinder_();
    }

    //! UPDATE value clause for blob columns (as a struct holding
    //! the void* and number of bytes for the raw data)
    template <typename ColumnT>
    typename std::enable_if<
        std::is_same<ColumnT, Blob>::value,
    RecordFinder&>::type
    updateRowValues(const char * col_name,
                    const ColumnT & col_val)
    {
        col_values_.add(col_name, col_val);
        return makeRecordFinder_();
    }

    //! UPDATE value clause for overwriting multiple column
    //! values in the same statement.
    template <typename ColumnT, typename... Args>
    RecordFinder & updateRowValues(const char * col_name,
                                   const ColumnT & col_val,
                                   Args &&... args)
    {
        col_values_.add(col_name, col_val);
        return updateRowValues(std::forward<Args>(args)...);
    }

    template <typename ColumnT>
    typename std::enable_if<
        std::is_arithmetic<ColumnT>::value,
    std::unique_ptr<ObjectRef>>::type
    createObjectWithVals(const ColumnT col_val)
    {
        appendRawValue_(col_val);
        return finalizeCreationStatement_();
    }

    template <typename ColumnT, typename... Args>
    typename std::enable_if<
        std::is_arithmetic<ColumnT>::value,
    std::unique_ptr<ObjectRef>>::type
    createObjectWithVals(
        const ColumnT col_val,
        Args &&... args)
    {
        appendRawValue_(col_val);
        return createObjectWithVals(std::forward<Args>(args)...);
    }

    template <typename ColumnT>
    typename std::enable_if<
        std::is_trivially_constructible<ColumnT>::value,
    std::unique_ptr<ObjectRef>>::type
    createObjectFromStruct(const ColumnT & structure)
    {
        appendRawValue_(structure);
        return finalizeCreationStatement_();
    }

    //! Create a new record in this table, setting
    //! one of the record's column values at the time
    //! of creation.
    //!
    //! This method is also used as the base function in
    //! the N-argument creation method below.
    template <typename ColumnT>
    typename std::enable_if<
        std::is_fundamental<ColumnT>::value,
    std::unique_ptr<ObjectRef>>::type
    createObjectWithArgs(const char * col_name,
                         const ColumnT col_val)
    {
        col_values_.add(col_name, col_val);
        return finalizeCreationStatement_();
    }

    //! Same as single-argument creation method above, but
    //! specific to const char* column values.
    template <typename ColumnT>
    typename std::enable_if<
        std::is_same<typename std::decay<ColumnT>::type, const char*>::value,
    std::unique_ptr<ObjectRef>>::type
    createObjectWithArgs(const char * col_name,
                         ColumnT col_val)
    {
        col_values_.add(col_name, col_val);
        return finalizeCreationStatement_();
    }

    //! Same as single-argument creation method above, but
    //! specific to std::string column values.
    template <typename ColumnT>
    typename std::enable_if<
        std::is_same<ColumnT, std::string>::value,
    std::unique_ptr<ObjectRef>>::type
    createObjectWithArgs(const char * col_name,
                         const ColumnT & col_val)
    {
        return createObjectWithArgs(col_name, col_val.c_str());
    }

    //! Same as single-argument creation mentioned above, but
    //! specific to std::vector column values.
    template <typename ColumnT>
    typename std::enable_if<
        is_container<ColumnT>::value and is_contiguous<ColumnT>::value,
    std::unique_ptr<ObjectRef>>::type
    createObjectWithArgs(const char * col_name,
                         const ColumnT & col_val)
    {
        col_values_.add(col_name, col_val);
        return finalizeCreationStatement_();
    }

    //! Create a new record in this table, setting one
    //! or more column values at the time of creation.
    template <typename ColumnT, typename... Args>
    std::unique_ptr<ObjectRef> createObjectWithArgs(
        const char * col_name,
        const ColumnT & col_val,
        Args &&... args)
    {
        col_values_.add(col_name, col_val);
        return createObjectWithArgs(std::forward<Args>(args)...);
    }

    //! Delete one or more records from this table which
    //! match the provided constraint.
    //!
    //! This method is also used as the base function in
    //! the N-argument deletion method below.
    template <typename ColumnT>
    typename std::enable_if<
        std::is_fundamental<ColumnT>::value and
        !std::is_same<ColumnT, std::string>::value,
    void>::type
    deleteObjectsWhere(const char * col_name,
                       const constraints constraint,
                       const ColumnT col_val)
    {
        delete_where_clauses_.add(col_name, col_val)
            ->setConstraint(constraint);
        finalizeDeletionStatement_();
    }

    //! Same as single-argument record deletion method above,
    //! but specific to const char* constraint values.
    template <typename ColumnT>
    typename std::enable_if<
        std::is_same<typename std::decay<ColumnT>::type, const char*>::value,
    void>::type
    deleteObjectsWhere(const char * col_name,
                       const constraints constraint,
                       ColumnT col_val)
    {
        delete_where_clauses_.add(col_name, col_val)
            ->setConstraint(constraint);
        finalizeDeletionStatement_();
    }

    //! Same as single-argument record deletion method above,
    //! but specific to std::string constraint values.
    template <typename ColumnT>
    typename std::enable_if<
        std::is_same<ColumnT, std::string>::value,
    void>::type
    deleteObjectsWhere(const char * col_name,
                       const constraints constraint,
                       const ColumnT & col_val)
    {
        delete_where_clauses_.add(col_name, col_val)
            ->setConstraint(constraint);
        finalizeDeletionStatement_();
    }

    //! Same as single-argument record deletion method above,
    //! but specific to initializer list constraints. Supports
    //! constraints like {10,24,26} (integer) and {"a","b","c"}
    //! (string literals).
    template <typename ColumnT>
    void deleteObjectsWhere(const char * col_name,
                            const constraints constraint,
                            const std::initializer_list<ColumnT> & col_val)
    {
        delete_where_clauses_.add(col_name, col_val)
            ->setConstraint(constraint);
        finalizeDeletionStatement_();
    }

    //! Delete one or more records from this table which
    //! match the provided constraints.
    template <typename ColumnT, typename... Args>
    void deleteObjectsWhere(const char * col_name,
                            const constraints constraint,
                            const ColumnT & col_val,
                            Args &&... args)
    {
        delete_where_clauses_.add(col_name, col_val)
            ->setConstraint(constraint);
        deleteObjectsWhere(std::forward<Args>(args)...);
    }

    //! Delete **ALL** records in this table. This operation
    //! cannot be undone!
    void deleteAllObjects()
    {
        finalizeDeletionStatement_();
    }

    //! The various create*() methods will return ObjectRef
    //! wrappers around the newly created record by default.
    //! Disable that behavior with a call to this method.
    //! The create*() methods will return nullptr until
    //! this TableRef object is told to do otherwise.
    void neverReturnObjectRefsOnCreate() {
        explicit_return_object_ = ExplicitReturnObject::NEVER_RETURN;
    }

    //! Make a call to this method if you want the various
    //! create*() method calls to return ObjectRef wrappers
    //! around the newly created records. This is the default
    //! behavior. The create*() methods will return ObjectRef
    //! wrappers until this TableRef object is told to do
    //! otherwise.
    void alwaysReturnObjectRefsOnCreate() {
        explicit_return_object_ = ExplicitReturnObject::ALWAYS_RETURN;
    }

    //! Some tables are configured to be able to capture
    //! summaries of their record value(s). Some columns
    //! do not support summaries, such as blobs and strings,
    //! but numeric columns may have summary support. This
    //! method returns true if the summary snapshot was
    //! successful, false otherwise. Note that returning
    //! false does not mean that an error occurred, but
    //! that this table did not have any columns that were
    //! able to be summarized.
    //!
    //! Also note that tables only have at most one summary
    //! record associated with them. Calling this method
    //! on a table that had previously been summarized will
    //! overwrite the summary record with the updated values.
    bool captureSummary();

private:
    //! At the end of a TableRef::updateRowValues() function
    //! call, this method returns a RecordFinder which is bound
    //! to 'this' TableRef:
    //!
    //!     table->updateRowValues("MyString", "helloWorld").
    //!            forRecordsWhere("MyDouble", simdb::constraints::equal, 4.5);
    //!
    //! The RecordFinder class provides TableRef access through
    //! friendship to its constructor, and a public set of API's
    //! that let the outside world build the WHERE clause of the
    //! UPDATE statement.
    RecordFinder & makeRecordFinder_()
    {
        std::function<size_t()> finalize_callback = [&]() {
            return finalizeUpdateStatement_();
        };
        record_finder_for_update_.reset(new RecordFinder(finalize_callback));
        is_in_update_statement_ = true;
        return *record_finder_for_update_;
    }

    //! When using the createObjectWithArgs() API's, this
    //! finalize method gets called when all creation args
    //! in the parameter pack have been unrolled.
    std::unique_ptr<ObjectRef> finalizeCreationStatement_();

    //! When using the deleteObjectsWhere() API's, this finalize
    //! method gets called when all deletion args/clauses in the
    //! parameter pack have been unrolled.
    void finalizeDeletionStatement_();

    //! When using the updateRowValues() API's, this finalize
    //! method gets called when all update args/clauses in the
    //! parameter pack have been unrolled.
    size_t finalizeUpdateStatement_();
    std::unique_ptr<RecordFinder> record_finder_for_update_;

    //! Object creation method used by createObject() and
    //! createObjectWithArgs() public APIs.
    std::unique_ptr<ObjectRef> createDefaultObject_();

    //! ObjectRef's are returned from the various create*()
    //! methods by default. There are separate APIs which
    //! lets the caller say whether record wrappers should
    //! always be returned, or never returned. Those APIs
    //! set this member variable.
    enum class ExplicitReturnObject {
        ALWAYS_RETURN,
        NEVER_RETURN,
        DEFAULT
    };
    ExplicitReturnObject explicit_return_object_ =
        ExplicitReturnObject::DEFAULT;

    //! Calls to updateRowValues() use the same member variables
    //! as createObjectWithArgs() do, but calls to these API's cannot
    //! be mixed. Here are separate UPDATE and INSERT calls done the
    //! correct way:
    //!
    //!    table->updateRowValues("MyString", "foo").
    //!           forRecordsWhere("MyDouble", constraints::equal, 50);
    //!
    //!    table->createObjectWithArgs("MyString", "bar",
    //!                                "MyDouble", 45.89);
    //!
    //! Those two calls would not affect each other. However, here
    //! are calls that would result in incorrect data values in the
    //! end:
    //!
    //!    auto & updater = table->updateRowValues("MyString", "foo");
    //!
    //!    table->createObjectWithArgs("MyString", "bar",
    //!                                "MyDouble", 45.89);
    //!
    //!    updater.forRecordsWhere("MyDouble", constraints::equal, 50);
    //!
    //! This is invalid because a createObjectWithArgs() function
    //! call is made when the UPDATE statement was being built.
    //! This flag lets us throw in the case where UPDATE/INSERT
    //! API's are mixed.
    bool is_in_update_statement_ = false;

    //! DELETE clauses we hold onto while unrolling a
    //! parameter pack for deleteObjectsWhere() calls.
    ColumnValueContainer delete_where_clauses_;

    //! Common table member variables for all INSERT,
    //! UPDATE, and DELETE actions.
    const std::string table_name_;
    const ObjectManager & obj_mgr_;

    //! Proxy back-pointer that was given to us by the
    //! ObjectManager.
    std::shared_ptr<DbConnProxy> db_proxy_;

    //! List of column names and their data types
    std::vector<ColumnDescriptor> col_descriptors_;

    //! Map of table summary calculation functions
    //! by summary function name. For instance:
    //!
    //!    {{ "min", [](const std::vector<double> & vals) {
    //!                  return vals.empty() ? NAN :
    //!                    *std::min_element(vals.begin(), vals.end());
    //!                },
    //!     { "max", [](const std::vector<double> & vals) {
    //!                  return vals.empty() ? NAN :
    //!                    *std::max_element(vals.begin(), vals.end());
    //!                }}
    NamedSummaryFunctions summary_fcns_;

    //! Record factory that was given to us by the
    //! ObjectManager.
    AnySizeObjectFactory any_size_record_factory_;
    FixedSizeObjectFactory fixed_size_record_factory_;

    //! This container holds onto column values and an
    //! enumeration which gives the column data type.
    //! Values are accessible via the ColumnValue's
    //! getAs<T>() method.
    ColumnValueContainer col_values_;

    //! To support data writes using only raw bytes,
    //! without cluttering the APIs with column names
    //! that we don't strictly need to use, we will
    //! hold onto the raw bytes in this char vector.
    std::vector<char> raw_bytes_for_obj_create_;

    //! Append to the 'raw bytes' member variable during
    //! calls to createObjectWithVals()
    template <typename ColumnT>
    typename std::enable_if<
        std::is_arithmetic<ColumnT>::value,
    void>::type
    appendRawValue_(const ColumnT col_val) {
        const size_t cur_num_bytes = raw_bytes_for_obj_create_.size();
        const size_t new_num_bytes = cur_num_bytes + sizeof(ColumnT);
        raw_bytes_for_obj_create_.resize(new_num_bytes);

        memcpy(&raw_bytes_for_obj_create_[cur_num_bytes],
               &col_val, sizeof(ColumnT));
    }

    //! Append to the 'raw bytes' member variable during
    //! calls to createObjectWithVals()
    template <typename ColumnT>
    typename std::enable_if<
        std::is_trivially_constructible<ColumnT>::value and
        not std::is_arithmetic<ColumnT>::value,
    void>::type
    appendRawValue_(const ColumnT & structure) {
        const size_t cur_num_bytes = raw_bytes_for_obj_create_.size();
        const size_t new_num_bytes = cur_num_bytes + sizeof(ColumnT);
        raw_bytes_for_obj_create_.resize(new_num_bytes);

        memcpy(&raw_bytes_for_obj_create_[cur_num_bytes],
               &structure, sizeof(ColumnT));
    }

    //! Private constructor, just for ObjectManager to call.
    TableRef(const std::string & table_name,
             const ObjectManager & obj_mgr,
             const std::shared_ptr<DbConnProxy> & db_proxy,
             const std::vector<ColumnDescriptor> & col_descriptors,
             const NamedSummaryFunctions & summary_fcns,
             AnySizeObjectFactory any_size_record_factory,
             FixedSizeObjectFactory fixed_size_record_factory) :
        table_name_(table_name),
        obj_mgr_(obj_mgr),
        db_proxy_(db_proxy),
        col_descriptors_(col_descriptors),
        summary_fcns_(summary_fcns),
        any_size_record_factory_(any_size_record_factory),
        fixed_size_record_factory_(fixed_size_record_factory)
    {}

    friend class ObjectManager;
};

} // namespace simdb

