// <ObjectQuery> -*- C++ -*-

#pragma once

/*!
 * \file ObjectQuery.h
 * \brief This utility makes it easy to put together
 * database queries with or without constraints (WHERE
 * clauses), without explicitly writing SQL commands.
 * It also makes it easy to iterate over many records
 * that match your constraints without having to bring
 * them all into memory at once.
 */

#include "simdb/ObjectManager.hpp"
#include "simdb/ObjectRef.hpp"
#include "simdb/TableRef.hpp"
#include "simdb/DbConnProxy.hpp"
#include "simdb/Constraints.hpp"
#include "simdb/utils/Stringifiers.hpp"

#include <cstdint>
#include <iostream>
#include <vector>
#include <map>
#include <utility>
#include <initializer_list>

//SQLite-specific headers
#include "simdb/impl/sqlite/TransactionUtils.hpp"
#include <sqlite3.h>

namespace simdb {

//! ORDER BY ASC|DESC
enum class ColumnOrdering : int8_t {
    asc,
    desc,
    default_ordering
};

constexpr ColumnOrdering ASC = ColumnOrdering::asc;
constexpr ColumnOrdering DESC = ColumnOrdering::desc;

inline std::ostream & operator<<(std::ostream & os,
                                 const ColumnOrdering order)
{
    switch (order) {
        case ColumnOrdering::asc: {
            os << "ASC";
            break;
        }
        case ColumnOrdering::desc: {
            os << "DESC";
            break;
        }
        case ColumnOrdering::default_ordering: {
            simdb_throw("not handled")
        }
    }
    return os;
}

//! ORDER BY clause
class OrderBy {
public:
    OrderBy(const char * column_name,
            const ColumnOrdering column_order = ColumnOrdering::default_ordering) :
        col_name_(column_name),
        col_ordering_(column_order)
    {}

    OrderBy(const std::string & column_name,
            const ColumnOrdering column_order = ColumnOrdering::default_ordering) :
        col_name_(column_name),
        col_ordering_(column_order)
    {}

    OrderBy() = default;
    OrderBy(const OrderBy &) = default;

    OrderBy& operator=(const OrderBy&) = default;

    operator std::string() const {
        std::ostringstream oss;
        oss << (*this);
        return oss.str();
    }

private:
    std::string col_name_;
    ColumnOrdering col_ordering_ = ColumnOrdering::default_ordering;
    friend std::ostream & operator<<(std::ostream &, const OrderBy);
};

inline std::ostream & operator<<(std::ostream & os, const OrderBy order_by)
{
    if (order_by.col_name_.empty()) {
        return os;
    }

    os << " ORDER BY "
       << order_by.col_name_ << " "
       << order_by.col_ordering_ << " ";

    return os;
}

/*!
 * \brief This class is used together with ObjectQuery to be
 * able to execute an SQL query once, and then simply iterate
 * over the result set by calling the getNext() API in this class.
 */
class ResultIter
{
public:
    //! Advance this iterator to the next record in its underlying
    //! set of records. Returns true on success, false otherwise.
    //! If false, the SQL code will be written to the output variable
    //! 'sqlite_return_code', if provided. See 'sqlite3.h' for more
    //! details about what the specific error codes mean. This method
    //! will typically return false only when there are no more records
    //! to iterate over.
    bool getNext(int * sqlite_return_code = nullptr)
    {
        //Step our prepared statement forward
        const int step_result = sqlite3_step(prepared_stmt_);

        //SQLite returns SQLITE_ROW when we have another record
        //that matches the original query.
        if (step_result != SQLITE_ROW) {
            //We are either out of matching records, or something
            //else happened (Someone else has put a lock on our
            //database? This may need more investigation. For now,
            //we'll just return false and halt further iterations
            //for this query).
            if (sqlite_return_code) {
                *sqlite_return_code = step_result;
            }
            sqlite3_reset(prepared_stmt_);
            return false;
        }

        //Go out and memcpy the current matching record's column
        //values into the user's variables.
        writeCurrentResultToPtrs_();

        //In the event of a successful iterator advance, give
        //the SQLITE_ROW result code to the caller, if they
        //have asked for it.
        if (sqlite_return_code) {
            *sqlite_return_code = step_result;
        }

        return true;
    }

    ~ResultIter()
    {
        sqlite3_finalize(prepared_stmt_);
    }

private:
    //! Pair of the void* and the data type of a variable we were
    //! given to write into during each call to getNext()
    typedef std::pair<void*, ColumnDataType> ResultColumn;

    //! Mapping of variable name to its ResultColumn
    typedef std::map<std::string, ResultColumn> NamedResultColumns;

    //! Private constructor only ObjectQuery has access to
    ResultIter(const NamedResultColumns & result_cols,
               sqlite3_stmt * stmt) :
        dest_ptrs_(result_cols),
        prepared_stmt_(stmt)
    {
        assert(prepared_stmt_);
        if (dest_ptrs_.empty()) {
            std::cout << "simdb::ResultIter object created without "
                         "result iteration pointers. This is probably "
                         "a mistake. The query can be stepped forward "
                         "with calls to getNext(), but nobody is listening "
                         "for those result data values." << std::endl;
        }
    }

    //! During each call to getNext(), write the column values into
    //! the variables that were given to ObjectQuery by the user/caller.
    void writeCurrentResultToPtrs_()
    {
        auto iter = dest_ptrs_.begin();
        for (size_t col_idx = 0; col_idx < dest_ptrs_.size(); ++col_idx) {
            void * dest = iter->second.first;

            //Now we have the void* of the variable that is supposed to
            //be written into during each getNext() iteration. How many
            //bytes and/or the method we use to populate that void*
            //only depends on the data type of the column in question.
            switch (iter->second.second) {
                case ColumnDataType::fkey_t:
                case ColumnDataType::char_t:
                case ColumnDataType::int8_t:
                case ColumnDataType::uint8_t:
                case ColumnDataType::int16_t:
                case ColumnDataType::uint16_t:
                case ColumnDataType::int32_t:
                case ColumnDataType::uint32_t: {
                    const int val = sqlite3_column_int(prepared_stmt_, static_cast<int>(col_idx));
                    if (!memcpyInteger32_(val, dest, iter->second.second)) {
                        throw DBException("Unable to convert integer value ")
                            << "to the requested type";
                    }
                    break;
                }

                case ColumnDataType::int64_t:
                case ColumnDataType::uint64_t: {
                    const sqlite3_int64 val = sqlite3_column_int64(prepared_stmt_, static_cast<int>(col_idx));
                    if (!memcpyInteger64_(val, dest, iter->second.second)) {
                        throw DBException("Unable to convert integer value ")
                            << "to the requested type";
                    }
                    break;
                }

                case ColumnDataType::float_t:
                case ColumnDataType::double_t: {
                    const double val = sqlite3_column_double(prepared_stmt_, static_cast<int>(col_idx));
                    if (!memcpyFloatingPoint_(val, dest, iter->second.second)) {
                        throw DBException("Unable to convert integer value ")
                            << "to the requested type";
                    }
                    break;
                }

                case ColumnDataType::string_t: {
                    std::string tmp = std::string(reinterpret_cast<const char*>(
                        sqlite3_column_text(prepared_stmt_, static_cast<int>(col_idx))));
                    std::swap(tmp, *static_cast<std::string*>(dest));
                    break;
                }

                case ColumnDataType::blob_t: {
                    const void * blob_ptr = sqlite3_column_blob(prepared_stmt_, static_cast<int>(col_idx));
                    const size_t num_bytes = sqlite3_column_bytes(prepared_stmt_, static_cast<int>(col_idx));
                    std::vector<char> & dest_vector = *(static_cast<std::vector<char>*>(dest));
                    dest_vector.resize(num_bytes);
                    memcpy(&dest_vector[0], blob_ptr, num_bytes);
                    break;
                }

                default:
                    throw DBException("Unrecognized column data type encountered");
            }
            ++iter;
        }
    }

    //! When iterating over ObjectQuery result sets, we need
    //! to copy an integer value that SQLite gave us from the
    //! database into the variable given at the call site:
    //!
    //!     ObjectQuery query(...);
    //!
    //!     int8_t x;
    //!     uint16_t y;
    //!     query.writeResultIterationsTo("MyInt8", &x, "MyUint16", &y);
    //!      ...
    //!
    //! This method performs the memcpy into variables like
    //! x and y above.
    bool memcpyInteger32_(
        const int val, void * dest,
        const ColumnDataType dest_type) const
    {
        using dt = ColumnDataType;

        switch (dest_type) {
            case dt::int8_t: {
                const auto casted = static_cast<int8_t>(val);
                memcpy(dest, &casted, sizeof(int8_t));
                break;
            }

            case dt::uint8_t: {
                const auto casted = static_cast<uint8_t>(val);
                memcpy(dest, &casted, sizeof(uint8_t));
                break;
            }

            case dt::int16_t: {
                const auto casted = static_cast<int16_t>(val);
                memcpy(dest, &casted, sizeof(int16_t));
                break;
            }

            case dt::uint16_t: {
                const auto casted = static_cast<uint16_t>(val);
                memcpy(dest, &casted, sizeof(uint16_t));
                break;
            }

            case dt::int32_t: {
                const auto casted = static_cast<int32_t>(val);
                memcpy(dest, &casted, sizeof(int32_t));
                break;
            }

            case dt::uint32_t: {
                const auto casted = static_cast<uint32_t>(val);
                memcpy(dest, &casted, sizeof(uint32_t));
                break;
            }

            case dt::char_t: {
                const auto casted = static_cast<char>(val);
                memcpy(dest, &casted, sizeof(char));
                break;
            }

            default:
                return false;
        }

        return true;
    }

    //! Similar to memcpyInteger32, but this is for 64-bit
    //! integer data types.
    bool memcpyInteger64_(
        const int64_t val, void * dest,
        const ColumnDataType dest_type) const
    {
        using dt = ColumnDataType;

        if (dest_type == dt::int64_t || dest_type == dt::uint64_t) {
            memcpy(dest, &val, sizeof(int64_t));
            return true;
        }
        return false;
    }

    //! Similar to memcpyInteger32, but this is for floats
    //! and doubles.
    bool memcpyFloatingPoint_(
        const double val, void * dest,
        const ColumnDataType dest_type) const
    {
        using dt = ColumnDataType;

        if (dest_type == dt::float_t) {
            const float casted = static_cast<float>(val);
            memcpy(dest, &casted, sizeof(float));
            return true;
        } else if (dest_type == dt::double_t) {
            memcpy(dest, &val, sizeof(double));
            return true;
        }
        return false;
    }

    NamedResultColumns dest_ptrs_;
    sqlite3_stmt * prepared_stmt_ = nullptr;

    //This class goes hand in hand with ObjectQuery.
    //Nobody else can create one of these.
    friend class ObjectQuery;
};

/*
 * \brief This class lets you put together SELECT statements
 * without having to explicitly write SQL commands yourself.
 * It supports WHERE clauses of the form:
 *
 *   Col1 constraint Val1 AND Col2 constraint Val2 AND...
 *
 * See the top of this file for the constraints enum. Some
 * limitations of this class currently include:
 *
 *   - No support for WHERE clauses that include logical *OR*
 *   - No support for WHERE clauses against blob columns
 *
 * Here are examples / pseudo-sql statements that are not
 * supported, per the above limitations:
 *
 *   SELECT * FROM Employees WHERE LastName='Smith' OR Position='Mgr'
 *   SELECT * FROM Employees WHERE EmployeeID=[1,5,3,5,2,8]
 *
 * The first is no good since it uses logical OR, and the
 * second is no good since it has a column constraint with
 * a blob value data type.
 *
 * One note on the logical OR limitation. If you need a
 * query that looks something like this:
 *
 *   SELECT * FROM Employees WHERE EmployeeID=104 OR EmployeeID=398
 *   SELECT * FROM Employees WHERE LastName='Smith' OR LastName='Thompson'
 *
 * You can achieve this with the 'in_set' constraint like so:
 *
 *   ObjectQuery query1(obj_mgr, "Employees");
 *   query1.addConstraints(
 *       "EmployeeID", simdb::constraints::in_set, {104,398});
 *   ...
 *
 *   ObjectQuery query2(obj_mgr, "Employees");
 *   query2.addConstraints(
 *       "LastName", simdb::constraints::in_set, {"Smith","Thompson"});
 *   ...
 */
class ObjectQuery
{
public:
    ObjectQuery(const ObjectManager & obj_mgr,
                const std::string & tbl_name) :
        obj_mgr_(obj_mgr),
        tbl_name_(tbl_name)
    {}

    //! Single-contraint query. Also used as the base function
    //! for the variadic function below. Specific to integral
    //! and string target values.
    template <typename ColumnT>
    typename std::enable_if<
        not std::is_floating_point<ColumnT>::value,
    void>::type
    addConstraints(const char * col_name,
                   const constraints constraint,
                   const ColumnT & col_val)
    {
        //Chain multiple constraints with logical AND
        if (has_query_constraints_) {
            query_constraints_oss_ << " AND ";
        }
        has_query_constraints_ = true;

        //Note that the stringify<T>() method will take
        //care of any special formatting this data type
        //might need in order to form a valid SQL string,
        //such as enclosing string constraint values in
        //single quotes.
        query_constraints_oss_ << col_name
                               << constraint
                               << stringify(col_val);
    }

    //! Single-contraint query. Also used as the base function
    //! for the variadic function below. Specific to floating-
    //! point target values.
    template <typename ColumnT>
    typename std::enable_if<
        std::is_floating_point<ColumnT>::value,
    void>::type
    addConstraints(const char * col_name,
                   const constraints constraint,
                   const ColumnT col_val)
    {
        //Chain multiple constraints with logical AND
        if (has_query_constraints_) {
            query_constraints_oss_ << " AND ";
        }
        has_query_constraints_ = true;

        //For queries that want to compare floating-point
        //column values against a target value to match
        //*exactly*, we will allow a tolerance of machine
        //epsilon to consider the comparison a match. We
        //perform the comparison ourselves in the withinTol()
        //function we registered with SQLite.
        if (constraint == constraints::equal) {
            query_constraints_oss_
                << "withinTol(" << col_name << ","
                << std::setprecision(std::numeric_limits<ColumnT>::max_digits10)
                << col_val << "," << std::numeric_limits<ColumnT>::epsilon() << ")";
        } else {
            //Note that the stringify<T>() method will take
            //care of any special formatting this data type
            //might need in order to form a valid SQL string,
            //such as enclosing string constraint values in
            //single quotes.
            query_constraints_oss_ << col_name
                                   << constraint
                                   << stringify(col_val);
        }
    }

    //! Initializer list constraints. Supports queries like:
    //!
    //!     query.addConstraints(
    //!         "EmployeeID",
    //!         constraints::in_set,
    //!         {100,106,107,598,678});
    template <typename ColumnT>
    void addConstraints(const char * col_name,
                        const constraints constraint,
                        std::initializer_list<ColumnT> col_val)
    {
        //Chain multiple constraints with logical AND
        if (has_query_constraints_) {
            query_constraints_oss_ << " AND ";
        }
        has_query_constraints_ = true;

        //See note about stringify<T>() in the above method
        query_constraints_oss_ << col_name
                               << constraint
                               << stringify(col_val);
    }

    //! Multi-constraint query. Call site might look like this:
    //!
    //!    ObjectQuery query(obj_mgr, "ReportHeader");
    //!
    //!    query.addConstraints(
    //!        "WarmupInsts", constraints::greater_than, 12800,
    //!        "NumStatInsts", constraints::less_than_or_equal_to, 300,
    //!        "ReportName", constraints::in_set, {"mysim","sparta_core_example"});
    //!
    template <typename ColumnT, typename... Args>
    void addConstraints(const char * col_name,
                        const constraints constraint,
                        const ColumnT & col_val,
                        Args &&... args)
    {
        //Process one constraint.
        addConstraints(col_name, constraint, col_val);

        //Peel off the rest of the constraints and keep going.
        addConstraints(std::forward<Args>(args)...);
    }

    //! Single-column value memcpy. You will typically want to
    //! read one or more column values for each of the records
    //! that match your query. Call this method to have the
    //! records' column values written directly into your own
    //! variables. For example:
    //!
    //!    ObjectQuery query(obj_mgr, "ReportHeader");
    //!    query.addConstraints(...); //optional
    //!
    //!    std::string report_name;
    //!    query.writeResultIterationsTo("ReportName", &report_name);
    //!
    //!    std::unique_ptr<simdb::ResultIter> db_iter = query.executeQuery();
    //!    while (db_iter->getNext()) {
    //!        std::cout << "Found another record! Its report name is '"
    //!                  << report_name << "'" << std::endl;
    //!    }
    //!
    //! This method is also used as the base function for the
    //! variadic function below.
    template <typename ColumnT>
    void writeResultIterationsTo(const char * col_name,
                                 ColumnT * result_ptr)
    {
        static_assert(
            !is_container<ColumnT>::value ||
            (is_contiguous<ColumnT>::value &&
             std::is_trivial<typename is_container<ColumnT>::value_type>::value),
            "ObjectQuery::writeResultIterationsTo(name, &var) can "
            "be used to extract blobs from the database directly "
            "into your own variable, but that variable must be a "
            "std::vector<T>, where the type T is scalar numeric.");

        result_iter_dest_ptrs_[col_name] = std::make_pair(
            result_ptr, column_info<ColumnT>::data_type());
    }

    //! Multi-column value memcpy. Call site might look like this:
    //!
    //!    ObjectQuery query(obj_mgr, "ReportHeader");
    //!    query.addConstraints(...); //optional
    //!
    //!    std::string report_name,
    //!    uint64_t start_time;
    //!    uint64_t end_time;
    //!
    //!    query.writeResultIterationsTo(
    //!        "ReportName", &report_name,
    //!        "StartTime", &start_time,
    //!        "EndTime", &end_time);
    //!
    //!    std::unique_ptr<simdb::ResultIter> db_iter = query.executeQuery();
    //!    while (db_iter->getNext()) {
    //!        std::cout << "Found another record! Its report name is '"
    //!                  << report_name << "', its start time is "
    //!                  << start_time << ", and its end time is "
    //!                  << end_time << std::endl;
    //!    }
    template <typename ColumnT, typename...Args>
    void writeResultIterationsTo(const char * col_name,
                                 ColumnT * result_ptr,
                                 Args &&... args)
    {
        writeResultIterationsTo(col_name, result_ptr);
        writeResultIterationsTo(std::forward<Args>(args)...);
    }

    //! Optionally apply an ORDER BY clause to your query
    void orderBy(OrderBy order_by)
    {
        std::swap(order_by_, order_by);
    }

    //! Optionally apply a LIMIT clause to your query
    void setLimit(const uint32_t limit)
    {
        if (limit == 0) {
            std::cout << "Encountered ObjectQuery::setLimit() call with "
                      << "LIMIT 0. This will be ignored." << std::endl;
            return;
        }
        limit_ = limit;
    }

    //! Call this method to see how many records match your
    //! current constraints:
    //!
    //!    auto query(obj_mgr, "ReportHeader");
    //!    query.addConstraints("ReportName", constraints::equal, "foo.json");
    //!    std::cout << "There are " << query.countMatches() << " reports "
    //!              << "with the name 'foo.json'";
    //!
    //!    query.addConstraints("StartTime", constraints::greater, 5000);
    //!    std::cout << "And of those, " << query.countMatches() << " have "
    //!              << "a start time greater than 5000.";
    size_t countMatches() {
        auto cur_has_query_constraints = has_query_constraints_;
        auto cur_query_constraints_oss_str = query_constraints_oss_.str();
        auto cur_result_iter_dest_ptrs = result_iter_dest_ptrs_;
        auto cur_limit = limit_;
        auto cur_order_by = order_by_;

        if (!has_query_constraints_) {
            addConstraints("Id", constraints::not_equal, 0);
        }

        DatabaseID unused_id;
        if (result_iter_dest_ptrs_.empty()) {
            writeResultIterationsTo("Id", &unused_id);
        }

        size_t count = 0;
        auto result_iter = executeQuery();
        while (result_iter->getNext()) {
            ++count;
        }

        has_query_constraints_ = cur_has_query_constraints;
        query_constraints_oss_ << cur_query_constraints_oss_str;
        result_iter_dest_ptrs_ = cur_result_iter_dest_ptrs;
        limit_ = cur_limit;
        order_by_ = cur_order_by;

        return count;
    }

    //! Once your constraints (WHERE, addConstraint()) and your
    //! destination variables (SELECT, writeResultIterationsTo())
    //! have been set, finalize the query with a call to this method.
    //! The returned object is "bound" to the SQL query and your
    //! destination variable(s). Use the ResultIter object to loop
    //! over all records that matched your query.
    std::unique_ptr<ResultIter> executeQuery()
    {
        if (result_iter_dest_ptrs_.empty()) {
            //We should consider warning. This is an empty SQL query
            //without any SELECT clause.
            return nullptr;
        }

        //ObjectQuery is not supported by all SimDB implementations
        //at this time. Return null if this is the case.
        if (!obj_mgr_.getDbConn()->supportsObjectQuery_()) {
            return nullptr;
        }

        auto int2ascii = [](const uint32_t i) {
            std::ostringstream oss;
            oss << i;
            return oss.str();
        };

        const std::string tbl_name = obj_mgr_.getQualifiedTableName(tbl_name_, "Stats");

        //Build up the final SQL command
        const std::string command =
             "SELECT " + getResultIterColumnNames_() +
             " FROM " + tbl_name +
            (has_query_constraints_ ?
            (" WHERE " + query_constraints_oss_.str()) : "") +
            std::string(order_by_) +
            (limit_ > 0 ? (" LIMIT " + int2ascii(limit_)) : "");

        const DbConnProxy * db_proxy = obj_mgr_.getDbConn();

        //Create the prepared statement
        sqlite3_stmt * stmt_retrieve = nullptr;
        void * statement = nullptr;

        db_proxy->prepareStatement_(command, &statement);
        assert(statement != nullptr);
        stmt_retrieve = static_cast<sqlite3_stmt*>(statement);

        //Prepared statement should be good to go
        assert(stmt_retrieve != nullptr);

        std::unique_ptr<ResultIter> result(new ResultIter(
            result_iter_dest_ptrs_, stmt_retrieve));

        //Clear out our internals before returning. This needs
        //further design/discussion about what we do with these
        //ObjectQuery's that have already been used. They may
        //be useful to keep around for performance gains in
        //a use case like this:
        //
        //    ObjectQuery query(obj_mgr, "Employees");
        //    std::string first_name;
        //    int age;
        //
        //    query.writeResultIterationsTo(
        //        "FirstName", &first_name,
        //        "Age", &age);
        //
        //    //Say we want to find all first names and ages for
        //    //those employees with last name 'Smith'.
        //    query.addConstraint("LastName", constraints::equal, "Smith");
        //    auto db_iter = query.executeQuery();
        //    while (db_iter->getNext()) {
        //        //Process the employees with last name 'Smith'
        //    }
        //
        //    //Now do the same thing, but for all employees with
        //    //last name 'Thompson'. Nothing else has changed
        //    //regarding our destination variables 'first_name'
        //    //and 'age', so it would be nice to be able to reuse
        //    //the ObjectQuery and keep going:
        //    query.resetConstraint("LastName", constraints::equal, "Thompson");
        //    db_iter = query.executeQuery();
        //    while (db_iter->getNext()) {
        //        //Process the employees with last name 'Thompson'
        //    }
        //
        //For right now, reset all ObjectQuery internals until
        //this part of the design / DB requirements are better
        //understood.
        has_query_constraints_ = false;
        query_constraints_oss_.str(std::string());
        query_constraints_oss_.clear();
        result_iter_dest_ptrs_.clear();

        OrderBy empty_order_by;
        orderBy(empty_order_by);
        limit_ = 0;

        return result;
    }

private:
    //Put together the SELECT clause for this query. Examples:
    //
    //    SELECT * FROM Customers
    //    SELECT LastName,FirstName FROM Employees
    //    SELECT PhoneNumber FROM Contacts
    //    ...
    std::string getResultIterColumnNames_()
    {
        //We should have short-circuited this function call
        //if we didn't have any SELECT column(s)
        if (result_iter_dest_ptrs_.empty()) {
            throw DBException("ObjectQuery::executeQuery() called ")
                << "before the query was ready. This is a bug.";
        }

        //Single-column SELECT's are returned as-is
        auto iter = result_iter_dest_ptrs_.begin();
        if (result_iter_dest_ptrs_.size() == 1) {
            return iter->first;
        }

        //Multi-column SELECT's are comma-separated
        std::ostringstream oss;
        for (size_t idx = 0; idx < result_iter_dest_ptrs_.size()-1; ++idx) {
            oss << iter->first << ",";
            ++iter;
        }
        oss << iter->first;
        return oss.str();
    }

    const ObjectManager & obj_mgr_;
    const std::string tbl_name_;
    bool has_query_constraints_ = false;
    std::ostringstream query_constraints_oss_;
    OrderBy order_by_;
    uint32_t limit_ = 0;

    //Mapping from a column name to a pair<void*,ColumnDataType>
    //  - The ResultColumn pair holds the address of the user's
    //    object (&std::string, &int, etc.) as well as the column
    //    data type enum (String, Blob, etc.) and we use both of
    //    these pieces of info to write a record's column values
    //    into the caller's own variables.
    std::map<std::string, ResultIter::ResultColumn> result_iter_dest_ptrs_;
};

} // namespace simdb
