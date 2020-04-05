// <Schema> -*- C++ -*-

#pragma once

#include "simdb/schema/ColumnMetaStructs.hpp"
#include "simdb/schema/ColumnTypedefs.hpp"
#include "simdb/schema/TableTypedefs.hpp"
#include "simdb/schema/TableSummaries.hpp"
#include "simdb/utils/Stringifiers.hpp"
#include "simdb/Errors.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace simdb {

//! This class is used to add columns to a fixed-size
//! schema Table (one whose columns are all fixed-size
//! POD's). It is intended to be used with the FOFFSET
//! macro below, for example:
//!
//!     struct MyStruct {
//!         int32_t x;
//!         double y;
//!     };
//!
//!     simdb::Schema schema;
//!     schema.addTable("MyStruct")
//!         .addColumn("x", dt::int32_t,  FOFFSET(MyStruct,x))
//!         .addColumn("y", dt::double_t, FOFFSET(MyStruct,y));
class FieldAdder
{
public:
    FieldAdder(const size_t byte_offset) :
        byte_offset_(byte_offset)
    {}

    FieldAdder(const FieldAdder &) = default;
    FieldAdder & operator=(const FieldAdder &) = default;

private:
    size_t byte_offset_ = 0;
    friend class Table;
};

//! This macro is used to create a FieldAdder object
//! that holds the byte offset to a field in a struct.
//! See the doxygen for FieldAdder for an example.
//!
//! The macro name stands for "Field Offset".
#define FOFFSET(s,f) simdb::FieldAdder(offsetof(s,f))

//! Compression enumeration specifying various levels
//! of compression which may be available in the SimDB
//! implementation.
enum class CompressionType : int8_t {
    NONE,
    DEFAULT_COMPRESSION,
    BEST_COMPRESSION_RATIO,
    BEST_COMPRESSION_SPEED
};

//! Column class used for creating SimDB tables
class Column
{
public:
    //! Construct a column with a name and one of the supported data types.
    //! The column name must not be empty.
    Column(const std::string & column_name, const ColumnDataType dt) :
        name_(column_name),
        dt_(dt)
    {
        if (name_.empty()) {
            throw DBException(
                "You cannot create a database column with no name");
        }
    }

    //! Default copies, default assignments
    Column(const Column &) = default;
    Column & operator=(const Column &) = default;

    //! Equivalence against another Column
    bool operator==(const Column & rhs) const {
        if (getName() != rhs.getName()) {
            return false;
        }
        if (getDataType() != rhs.getDataType()) {
            return false;
        }
        return true;
    }

    //! Equivalence against another Column
    bool operator!=(const Column & rhs) const {
        return !(*this == rhs);
    }

    //! Name of this table column
    const std::string & getName() const {
        return name_;
    }

    //! Data type of this table column
    ColumnDataType getDataType() const {
        return dt_;
    }

    //! Dimensions of this table column
    const std::vector<size_t> & getDimensions() const {
        return dims_;
    }

    //! See if this column is indexed, either by itself or
    //! indexed against other Columns too.
    bool isIndexed() const {
        return !indexed_properties_.empty();
    }

    //! If indexed, return the list of indexed Column names.
    //! Returns an empty vector if this Column is not indexed.
    const std::vector<Column*> & getIndexedProperties() const {
        return indexed_properties_;
    }

    //! See if this Column has a default value set or not.
    bool hasDefaultValue() const {
        return !default_val_string_.empty();
    }

    //! Get this Column's default value. These are returned as
    //! strings since the schema creation code is implementation-
    //! specific (SQLite builds table creation statements one way,
    //! HDF5 builds them another way, etc.) and default values
    //! are only allowed on simple column data types (any type
    //! that isn't a blob) so these strings can be lexical-casted
    //! back to their native form if needed ("123" -> 123, etc.)
    const std::string & getDefaultValueAsString() const {
        return default_val_string_;
    }

    //! Tables whose columns were populated using the addField()
    //! method will know their byte offset in each of the table's
    //! rows. If this column hasByteOffset(), it is part of a
    //! table with fixed-size records.
    bool hasByteOffset() const {
        return byte_offset_.second;
    }

    //! If hasByteOffset() returns true, ask for the byte offset
    //! value with this method. Throws if hasByteOffset() returns
    //! false.
    size_t getByteOffset() const {
        if (!hasByteOffset()) {
            throw DBException("Cannot call Column::getByteOffset() - ")
                << "check Column::hasByteOffset() beforehand";
        }
        return byte_offset_.first;
    }

    //! SimDB columns may or may not be able to be summarized,
    //! but for those that do support column summary, they still
    //! may have been explicitly removed from summarization:
    //!
    //!     simdb::Schema schema;
    //!     schema.addTable("MyInts")
    //!         .addColumn("A", dt::int32_t)
    //!         .addColumn("B", dt::int32_t)
    //!             ->noSummary()
    //!         .addColumn("C", dt::int32_t);
    //!
    bool isSummaryDisabled() const {
        return summary_disabled_;
    }

private:
    // Columns that are added to their table object via
    // the FOFFSET macro will be assigned their byte
    // offset value through this constructor.
    Column(const std::string & column_name,
           const ColumnDataType dt,
           const size_t byte_offset) :
        Column(column_name, dt)
    {
        byte_offset_ = std::make_pair(byte_offset, true);
    }

    // Optionally specify a default value for this Column.
    // Defaults for Blob data types are not allowed and will
    // throw if you attempt to set a Blob default value.
    // The DefaultValueT template type needs to be able to
    // be lexical-casted to a std::string, or this method
    // will throw.
    template <typename DefaultValueT>
    void setDefaultValue_(const DefaultValueT & val) {
        if (dt_ == ColumnDataType::blob_t) {
            throw DBException(
                "Cannot set default value for a database "
                "column with blob data type");
        }
        std::ostringstream ss;
        ss << val;
        default_val_string_ = ss.str();
        if (default_val_string_.empty()) {
            throw DBException("Unable to convert default value ")
                << val << " into a std::string";
        }
    }

    // You can tell the database to create indexes on specific
    // table columns for faster queries later on. For example:
    //
    //    using dt = simdb::ColumnDataType;
    //    simdb::Schema schema;
    //
    //    schema.addTable("Customers")
    //        .addColumn("Last", dt::string_t)
    //            ->index();
    //
    // This results in fast lookup performance for queries like this:
    //
    //    SELECT * FROM Customers WHERE Last = 'Smith'
    //
    // If you want to build indexes based on multiple columns'
    // values, pass in those other Column objects like this:
    //
    //    schema.addTable("Customers")
    //        .addColumn("First", dt::string_t)
    //        .addColumn("Last", dt::string_t)
    //            ->indexAgainst("First");
    //
    // This results in fast lookup performance for queries like this:
    //
    //    SELECT * FROM Customers WHERE Last = 'Smith' AND Age > 40
    //
    void setIsIndexed_(const std::vector<Column*> & indexed_columns = {})
    {
        indexed_properties_.clear();
        indexed_properties_.emplace_back(this);

        indexed_properties_.insert(
            indexed_properties_.end(),
            indexed_columns.begin(),
            indexed_columns.end());

        indexed_properties_.erase(
            std::unique(indexed_properties_.begin(), indexed_properties_.end()),
            indexed_properties_.end());
    }

    // Set dimensions for this column. Defaults to scalar
    // dimensions of {1}.
    void setDimensions_(const std::vector<size_t> & dims) {
        dims_ = dims;
    }

    std::string name_;
    ColumnDataType dt_;
    std::vector<size_t> dims_ = {1};
    std::string default_val_string_;
    std::vector<Column*> indexed_properties_;
    std::pair<size_t, bool> byte_offset_ = std::make_pair(0, false);
    bool summary_disabled_ = false;

    friend class Table;
};

//! Table class used for creating SimDB schemas
class Table
{
public:
    //! \brief Construct a table with a name. The table name
    //! must not be empty.
    //!
    //! \param table_name Name of the database table
    //!
    //! \param compression Optionally request that the table
    //! contents be compressed in the database. Note that the
    //! specific implementation (DbConnProxy subclass) may not
    //! support compression, in which case this option would be
    //! ignored.
    Table(const std::string & table_name,
          const CompressionType compression = CompressionType::BEST_COMPRESSION_RATIO) :
        name_(table_name),
        compression_(compression)
    {
        if (name_.empty()) {
            throw DBException(
                "You cannot create a database table with no name");
        }
        if (name_.find(std::string(1, Table::NS_DELIM)) != std::string::npos) {
            throw DBException("Cannot call Table constructor with ")
                << "a table name that includes the '"
                << Table::NS_DELIM << "' character";
        }
    }

    //! Construct without a name.
    Table() = default;

    //! Default copies, default assignments
    Table(const Table &) = default;
    Table & operator=(const Table &) = default;

    //! Equivalence against another Table
    bool operator==(const Table & rhs) const {
        if (getName() != rhs.getName()) {
            return false;
        }
        if (columns_.size() != rhs.columns_.size()) {
            return false;
        }
        for (size_t idx = 0; idx < columns_.size(); ++idx) {
            if (*columns_[idx] != *rhs.columns_[idx]) {
                return false;
            }
        }
        return true;
    }

    //! Equivalence against another Table
    bool operator!=(const Table & rhs) const {
        return !(*this == rhs);
    }

    //! Delimiter used to concatenate table names with
    //! the database namespace each table is used in,
    //! for instance:
    //!
    //!     namespace: "stats"
    //!     table name: "Timeseries"
    //!       -> qualified table name: "stats$Timeseries"
    //!
    //! NS_DELIM stands for "namespace delimiter"
    static constexpr char NS_DELIM = '$';

    //! Get this Table's name
    std::string getName() const {
        if (name_prefix_.empty()) {
            return name_;
        }
        return name_prefix_ + NS_DELIM + name_;
    }

    //! \return Table's compression setting
    CompressionType getCompression() const {
        return compression_;
    }

    //! See if this table is composed only of columns
    //! of fixed size (POD's)
    bool isFixedSize() const {
        return is_fixed_size_;
    }

    //! Add a Column to this Table.
    Table & addColumn(
        const std::string & name,
        const ColumnDataType dt)
    {
        if (is_fixed_size_) {
            is_fixed_size_ = getColumnIsFixedSize(dt);
        }
        columns_.emplace_back(new Column(name, dt));
        columns_by_name_[name] = columns_.back();
        column_modifier_.reset();
        return *this;
    }

    //! If your table represents a contiguous struct of
    //! fixed-size data fields, you can define the table
    //! layout like this:
    //!
    //!    simdb::Schema schema;
    //!    schema.addTable("MyStruct")
    //!        .addField("a", dt::int8_t,   FOFFSET(s,a))
    //!        .addField("b", dt::double_t, FOFFSET(s,b))
    //!        .addField("c", dt::uint16_t, FOFFSET(s,c));
    //!
    //! In the above example, this would equate to the
    //! following C struct:
    //!
    //!    struct MyStruct {
    //!        int8_t a;
    //!        double b;
    //!        uint16_t c;
    //!    };
    Table & addField(
        const std::string & name,
        const ColumnDataType dt,
        const FieldAdder & adder)
    {
        if (!getColumnIsFixedSize(dt)) {
            throw DBException("Cannot call Table::addField() for a column ")
                << "whose data type is variable length";
        }
        columns_.emplace_back(new Column(name, dt, adder.byte_offset_));
        columns_by_name_[name] = columns_.back();
        column_modifier_.reset();
        return *this;
    }

    //! Iterator access
    std::vector<std::shared_ptr<Column>>::const_iterator begin() const {
        return columns_.begin();
    }

    //! Iterator access
    std::vector<std::shared_ptr<Column>>::const_iterator end() const {
        return columns_.end();
    }

    //! Ask if this Table has any Columns yet
    bool hasColumns() const {
        return !columns_.empty();
    }

    //! This class serves as a level of indirection which allows
    //! the schema creation call site to look something like this:
    //!
    //!     Schema schema;
    //!     using dt = simdb::ColumnDataType;
    //!
    //!     schema.addTable("Customers")
    //!       .addColumn("First", dt::string_t)
    //!          ->index()
    //!       .addColumn("Last", dt::string_t)
    //!          ->indexAgainst("First")
    //!       .addColumn("RewardsBal", dt::double_t)
    //!          ->setDefaultValue(50);
    //!
    //! And so on. The index(), indexAgainst() and setDefaultValue()
    //! calls will apply to the column that was just added on the
    //! line of code above it in addColumn().
    class ColumnPropsModifier {
    public:
        Table & index() {
            table_->setIndexedColumn_NO_THROW_(col_name_);
            return *table_;
        }

        Table & indexAgainst(const std::string & other_column) {
            table_->setCompoundIndexedColumn_NO_THROW_(col_name_, {other_column});
            return *table_;
        }

        Table & indexAgainst(const std::vector<std::string> & other_columns) {
            table_->setCompoundIndexedColumn_NO_THROW_(col_name_, other_columns);
            return *table_;
        }

        template <typename DefaultValueT>
        Table & setDefaultValue(const DefaultValueT & val) {
            table_->setDefaultValue_(col_name_, val);
            return *table_;
        }

        Table & setDimensions(const std::vector<size_t> & dims) {
            table_->setDimensions_(col_name_, dims);
            return *table_;
        }

        Table & noSummary() {
            table_->disableSummary_(col_name_);
            return *table_;
        }

    private:
        ColumnPropsModifier(Table * tbl, const std::string & col_name) :
            table_(tbl),
            col_name_(col_name)
        {}

        Table * table_ = nullptr;
        std::string col_name_;
        friend class Table;
    };

    //! Overloaded operator->() so we know when to switch
    //! into "column modification" mode during a call that
    //! looks like this:
    //!
    //!    schema.addTable("Customers")
    //!        .addColumn("FirstName", dt::string_t)
    //!        .addColumn("LastName", dt::string_t)
    //!            ->indexAgainst("FirstName")
    //!        .addColumn(...)
    //!            ... <continue with the schema creation> ...
    //!            ...........................................
    //!
    //! The call to '->indexAgainst("First")' actually means:
    //!   "Go back to the previous column 'LastName', and create a
    //!    compound index for it together with the 'FirstName' column."
    //!
    //! The subsequent call to addColumn() will tip off the
    //! Table object that column modification mode is done,
    //! at least until operator->() gets called again.
    //!
    //! These modification APIs can be strung together too, like this:
    //!
    //!    schema.addTable("RewardsAccounts")
    //!        .addColumn("FirstName", dt::string_t)
    //!        .addColumn("LastName", dt::string_t)
    //!        .addColumn("Amount", dt::double_t)
    //!            ->indexAgainst("LastName")
    //!            ->setDefaultValue(100)
    //!        .addColumn("DaysToExpiration", dt::int32_t)
    //!            ->setDefaultValue(365);
    //!
    ColumnPropsModifier * operator->() {
        if (columns_.empty()) {
            throw DBException("Invalid use of the schema creation utility. ")
                << "An attempt was made to modify a table's column indexing or "
                << "default values, but the table does not have any columns to "
                << "modify. The offending table was '" << getName() << "'.";
        }

        column_modifier_.reset(new ColumnPropsModifier(
            this, columns_.back()->getName()));

        return column_modifier_.get();
    }

private:
    // Optionally specify a default value for the given Column
    template <typename DefaultValueT>
    void setDefaultValue_(const std::string & column_name,
                          const DefaultValueT & val)
    {
        auto iter = columns_by_name_.find(column_name);
        if (iter == columns_by_name_.end()) {
            throw DBException("Table::setDefaultValue_() called with ")
                << "a column name that does not exist: '"
                << column_name << "'";
        }
        iter->second->setDefaultValue_(val);
    }

    // Set the column by the given name to be indexed
    void setIndexedColumn_(const std::string & column_name) {
        auto iter = columns_by_name_.find(column_name);
        if (iter == columns_by_name_.end()) {
            throw DBException("Table::setIndexedColumn_() called with ")
                << "a column name that does not exist: '"
                << column_name << "'";
        }
        iter->second->setIsIndexed_();
    }

    // Set the column by the given name to be indexed
    // together with one or more other columns. This
    // is used to enable fast queries that look like
    // this:
    //
    //   SELECT * FROM Customers WHERE Last='Smith' AND Age>50
    void setCompoundIndexedColumn_(
        const std::string & primary_column_name,
        const std::vector<std::string> & other_indexed_column_names)
    {
        auto primary_iter = columns_by_name_.find(primary_column_name);
        if (primary_iter == columns_by_name_.end()) {
            throw DBException("Column '") << primary_column_name
                << "' does not exist in table '" << getName() << "'";
        }

        std::vector<Column*> other_indexed_columns;
        for (const auto & other_col_name : other_indexed_column_names) {
            auto other_iter = columns_by_name_.find(other_col_name);
            if (other_iter == columns_by_name_.end()) {
                throw DBException("Column '") << primary_column_name
                    << "' does not exist in table '" << getName() << "'";
            }
            other_indexed_columns.emplace_back(other_iter->second.get());
        }

        primary_iter->second->setIsIndexed_(other_indexed_columns);
    }

    // Column dimensionality defaults to scalar. Some database
    // implementations such as SQLite do not support non-scalar
    // data types like double{3,4} but others such as HDF5 do
    // support N-D columns. Set the dimensions with this API,
    // or use the Schema object like so:
    //
    //     Schema schema;
    //     using dt = simdb::ColumnDataType;
    //
    //     schema.addTable("Numbers")
    //         .addColumn("MyMatrix", dt::double_t)
    //             ->setDimensions({3,4});
    //
    void setDimensions_(
        const std::string & column_name,
        const std::vector<size_t> & dims)
    {
        auto column_iter = columns_by_name_.find(column_name);
        if (column_iter == columns_by_name_.end()) {
            throw DBException("Column '") << column_name
                << "' does not exist in table '" << getName() << "'";
        }

        column_iter->second->setDimensions_(dims);
    }

    // The ColumnPropsModifier class may need to inform us if a schema
    // call site looks like this:
    //
    //     Schema schema;
    //     using dt = simdb::ColumnDataType;
    //
    //     schema.addTable("Customers")
    //         .addColumn("LastName", dt::string_t)
    //             ->indexAgainst("FirstName")
    //         .addColumn("FirstName", dt::string_t);
    //
    // The indexAgainst("FirstName") call would be made before the
    // column "FirstName" was even added to the table. It would not
    // be very user-friendly to throw an exception and enforce the
    // addColumn() calls be ordered in a specific way.
    //
    // Aside from user-unfriendly behavior, it would also prevent
    // indexes like the following completely:
    //
    //     schema.addTable("Sales")
    //         .addColumn("Amount", dt::double_t)
    //             ->indexAgainst("LastName")
    //         .addColumn("LastName", dt::string_t)
    //             ->indexAgainst("FirstName")
    //         .addColumn("FirstName", dt::string_t)
    //             ->indexAgainst("Amount");
    //
    void setIndexedColumn_NO_THROW_(const std::string & column_name)
    {
        if (columns_by_name_.find(column_name) == columns_by_name_.end()) {
            unresolved_column_idxs_[column_name] = {};
            return;
        } else {
            setIndexedColumn_(column_name);
        }
    }

    // No-throw API for setting a compound index. Called by ColumnPropsModifier.
    void setCompoundIndexedColumn_NO_THROW_(
        const std::string & primary_column_name,
        const std::vector<std::string> & other_indexed_column_names)
    {
        // Note that we only allow the indexed *against* columns to
        // be unresolved, not the primary column. This is valid:
        //
        //    using dt = simdb::ColumnDataType;
        //
        //    schema.addTable("Customers")
        //        .addColumn("LastName", dt::string_t)
        //            ->indexAgainst({"FirstName", "AccountActive"}
        //        .addColumn("FirstName", dt::string_t)
        //        .addColumn("AccountActive", dt::int32_t);
        //
        // Here, the primary column name would be "LastName", which the
        // Table object (this) explicitly gave to the ColumnPropsModifier
        // ahead of time when Table::operator->() was called.
        //
        // There is no way ColumnPropsModifier would pass in a primary
        // column name that the Table did not already have, unless it
        // ignored the column name we just gave it, and turned around
        // and gave us another column name instead.
        //
        // The *secondary* columns in the compound index are allowed to
        // be unresolved until the Schema is given to an ObjectManager.
        // In the above example, "FirstName" and "AccountActive" are the
        // secondary columns in the index.
        //
        //    SELECT * FROM Customers WHERE
        //    LastName='Smith' AND FirstName='Bob' AND AccountActive=1
        //
        //    ----------------     -----------------------------------
        //       (primary)                     (secondary)
        //
        bool all_resolved = true;
        for (const auto & secondary_col_name : other_indexed_column_names) {
            if (columns_by_name_.find(secondary_col_name) == columns_by_name_.end()) {
                all_resolved = false;
                break;
            }
        }

        if (!all_resolved) {
            unresolved_column_idxs_[primary_column_name] = other_indexed_column_names;
            return;
        } else {
            setCompoundIndexedColumn_(
                primary_column_name, other_indexed_column_names);
        }
    }

    // This method gets called during schema construction
    // for columns that are explicitly removed from table
    // summarization:
    //
    //      simdb::Schema schema;
    //      schema.addTable("MyInts")
    //          .addColumn("A", dt::int32_t)
    //          .addColumn("B", dt::int32_t)
    //              ->noSummary()
    //          .addColumn("C", dt::int32_t);
    //
    void disableSummary_(const std::string & column_name) {
        auto iter = columns_by_name_.find(column_name);
        if (iter != columns_by_name_.end()) {
            iter->second->summary_disabled_ = true;
        }
    }

    // Called by the Schema object this Table belongs to when
    // the Schema is given to an ObjectManager for database
    // instantiation. Schema is a friend of this class to
    // make this private call.
    void finalizeTable_() {
        std::unordered_map<Column*, std::vector<Column*>> resolved_column_idxs;

        for (const auto & unresolved : unresolved_column_idxs_) {
            auto primary_column_iter = columns_by_name_.find(unresolved.first);

            if (primary_column_iter == columns_by_name_.end()) {
                throw DBException("Unrecognized column '") <<
                    unresolved.first << "' encountered in the " <<
                    "SimDB table '" << getName() << "'.";
            }

            Column * primary_column_obj = primary_column_iter->second.get();
            resolved_column_idxs[primary_column_obj] = {};

            for (const auto & unresolved_secondary : unresolved.second) {
                auto secondary_column_iter = columns_by_name_.find(
                    unresolved_secondary);

                if (secondary_column_iter == columns_by_name_.end()) {
                    throw DBException("Unrecognized column '") <<
                        unresolved_secondary << "' encountered in the " <<
                        "SimDB table '" << getName() << "'.";
                }

                resolved_column_idxs[primary_column_obj].emplace_back(
                    secondary_column_iter->second.get());
            }
        }

        // If we got this far, the resolved column indexes map has
        // all the objects we need to finalize the schema. The keys
        // in this map are the primary columns, and the values they
        // point to are the secondary columns that go with it to
        // make the compound index.
        for (auto & resolved : resolved_column_idxs) {
            resolved.first->setIsIndexed_(resolved.second);
        }
        unresolved_column_idxs_.clear();
    }

    bool is_fixed_size_ = true;
    std::string name_;
    std::string name_prefix_;
    CompressionType compression_ = CompressionType::BEST_COMPRESSION_RATIO;
    std::vector<std::shared_ptr<Column>> columns_;
    std::shared_ptr<ColumnPropsModifier> column_modifier_;
    std::unordered_map<std::string, std::shared_ptr<Column>> columns_by_name_;
    std::unordered_map<std::string, std::vector<std::string>> unresolved_column_idxs_;

    friend class ColumnPropsModifier;
    friend class Schema;
};

//! Schema class used for creating SimDB databases
class Schema
{
public:
    Schema() = default;

    //! \brief Create a new Table in this Schema with the given name
    //!
    //! \param table_name Name of the database table
    //!
    //! \param compression Table-specific compression setting.
    //! This setting will be ignored if the DbConnProxy subclass
    //! being used does not support compression.
    //!
    //! \warning Throws if the provided table name contains the
    //! character '$' - this is a reserved delimiter.
    //!
    //! \return Reference to the added table
    Table & addTable(
        const std::string & table_name,
        const CompressionType compression = CompressionType::BEST_COMPRESSION_RATIO)
    {
        if (table_name.find(std::string(1, Table::NS_DELIM)) != std::string::npos) {
            throw DBException("Cannot call Schema::addTable() ")
                << "with a table name that includes the '"
                << Table::NS_DELIM << "' character";
        }
        tables_.emplace_back(table_name, compression);
        if (!pending_namespace_.empty()) {
            tables_.back().name_prefix_ = pending_namespace_;
        }
        return tables_.back();
    }

    //! \brief Create a new Table in this Schema, copied from the
    //! Table object passed in.
    //!
    //! \param rhs Source table object intended to be added to the schema
    //!
    //! \warning Throws if the provided table name contains the
    //! character '$' - this is a reserved delimiter.
    //!
    //! \return Returns a reference to the newly created table, or
    //! returns a reference to an existing table in this schema that
    //! matched the incoming table (same table name, same colum names,
    //! and the same column data types).
    Table & addTable(const Table & rhs) {
        if (Table * existing_table = getTableNamed(rhs.getName())) {
            if (*existing_table != rhs) {
                throw DBException("Cannot add table '")
                    << rhs.getName() << "' to schema. A table "
                    << "with that name already exists.";
            }
            return *existing_table;
        }

        tables_.emplace_back(rhs);
        if (!pending_namespace_.empty()) {
            tables_.back().name_prefix_ = pending_namespace_;
        }
        return tables_.back();
    }

    /*!
     * \brief Combine this schema with the tables from another
     * schema. Any clashes between the new table(s) and the
     * existing table(s) will throw an exception:
     *
     *   1) Added schema has table called "Customers". The
     *      existing schema already has a table by the same
     *      name. However, the column configuration for both
     *      tables is identical. Will NOT throw. The table
     *      would be ignored.
     *
     *   2) Added schema has table called "Customers". The
     *      existing schema already has a table by the same
     *      name. The column configurations of the two tables
     *      are different. WILL throw. Columns are considered
     *      different if they have a different name (case-
     *      sensitive) and/or a different ColumnDataType.
     */
    Schema & operator+=(const Schema & rhs) {
        for (const auto & table : rhs) {
            addTable(table);
        }
        return *this;
    }

    /*!
     * \brief Get a pointer to the schema table with the
     * given name
     *
     * \param table_name Name of the table in this schema
     * you want to access
     *
     * \return Pointer to the table with the given name.
     * Returns null if no table by that name exists.
     */
    Table * getTableNamed(const std::string & table_name) {
        for (auto & lhs : tables_) {
            if (lhs.getName() == table_name) {
                return &lhs;
            }
        }
        return nullptr;
    }

    /*!
     * \brief Get a pointer to the schema table with the
     * given name
     *
     * \param table_name Name of the table in this schema
     * you want to access
     *
     * \return Pointer to the table with the given name.
     * Returns null if no table by that name exists.
     */
    const Table * getTableNamed(const std::string & table_name) const {
        for (auto & lhs : tables_) {
            if (lhs.getName() == table_name) {
                return &lhs;
            }
        }
        return nullptr;
    }

    //! Iterator access
    std::deque<Table>::const_iterator begin() const {
        return tables_.begin();
    }

    //! Iterator access
    std::deque<Table>::const_iterator end() const {
        return tables_.end();
    }

    //! Ask if this Schema has any Tables yet
    bool hasTables() const {
        return !tables_.empty();
    }

    //! Equivalence check
    bool operator==(const Schema & rhs) const {
        if (tables_.size() != rhs.tables_.size()) {
            return false;
        }
        for (size_t idx = 0; idx < tables_.size(); ++idx) {
            if (tables_[idx] != rhs.tables_[idx]) {
                return false;
            }
        }
        return true;
    }

    //! Equivalence check
    bool operator!=(const Schema & rhs) const {
        return !(*this == rhs);
    }

    //! Give this schema a TableSummaries object it can
    //! use to enable the TableRef::captureSummary() method
    //! for the finalized database. This method should be
    //! called prior to giving this schema to an ObjectManager
    //! via the createDatabaseFromSchema() method.
    void setTableSummaryConfig(TableSummaries && summary_config) {
        summary_config_ = std::move(summary_config);
    }

private:
    std::deque<Table> tables_;
    std::string pending_namespace_;

    //Prepend a namespace to this schema. This is called by
    //friend classes DatabaseRoot and DatabaseNamespace.
    void setNamespace_(const std::string & namespace_name) {
        if (tables_.empty()) {
            pending_namespace_ = namespace_name;
        } else {
            for (auto & tbl : tables_) {
                tbl.name_prefix_ = namespace_name;
            }
        }
    }
    TableSummaries summary_config_;

    struct TableSummaryQueryInfo {
        struct SourceTableInfo {
            std::string table_name;
            std::vector<ColumnDescriptor> table_columns;
        };

        std::vector<SourceTableInfo> source_tables;
        NamedSummaryFunctions summary_fcns;
    };
    TableSummaryQueryInfo summary_query_info_structs_;

    void getSummarizeableColumnsForTable_(
        std::vector<ColumnDescriptor> & summarizeable_cols,
        const Table & table) const
    {
        for (const auto & col : table) {
            switch (col->getDataType()) {
                case ColumnDataType::fkey_t:
                case ColumnDataType::string_t:
                case ColumnDataType::blob_t: {
                    continue;
                }
                default: {
                    if (!col->isSummaryDisabled()) {
                        summarizeable_cols.emplace_back(
                            col->getName(), col->getDataType());
                    }
                }
            }
        }
    }

    //When this Schema is given to an ObjectManager, it will call back
    //into this method to give us a chance to finalize the Schema and
    //throw any exceptions if we need to.
    void finalizeSchema_() {
        std::vector<std::pair<
            std::string,                     //Table name
            std::vector<ColumnDescriptor>    //Table columns
        >> summarizeable_columns_by_table;

        const NamedSummaryFunctions * summary_fcns = nullptr;
        if (!summary_config_.named_summary_fcns_.empty()) {
            summary_fcns = &summary_config_.named_summary_fcns_;
        }

        for (auto & tbl : tables_) {
            if (!pending_namespace_.empty()) {
                tbl.name_prefix_ = pending_namespace_;
            }
            tbl.finalizeTable_();

            if (summary_fcns) {
                std::vector<ColumnDescriptor> summarizeable_columns;
                summarizeable_columns_by_table.emplace_back(
                    std::make_pair(tbl.getName(), summarizeable_columns));

                getSummarizeableColumnsForTable_(
                    summarizeable_columns_by_table.back().second, tbl);

                if (summarizeable_columns_by_table.back().second.empty()) {
                    summarizeable_columns_by_table.pop_back();
                }
            }
        }

        for (const auto & summarizeable : summarizeable_columns_by_table) {
            Table summary_table(summarizeable.first + "_Summary");
            for (const auto & summarizeable_column : summarizeable.second) {
                for (const auto & summary_fcn : *summary_fcns) {
                    summary_table.addColumn(
                        summarizeable_column.first + "_" + summary_fcn.first,
                        ColumnDataType::double_t);
                }
            }

            if (summary_table.hasColumns()) {
                tables_.emplace_back(summary_table);
                TableSummaryQueryInfo::SourceTableInfo source_table;
                source_table.table_name = summarizeable.first;
                source_table.table_columns = summarizeable.second;
                summary_query_info_structs_.source_tables.emplace_back(source_table);
            }
        }

        if (!summary_query_info_structs_.source_tables.empty()) {
            summary_query_info_structs_.summary_fcns = *summary_fcns;
        }
        pending_namespace_.clear();
    }

    bool shouldSummarizeTable_(const std::string & table_name) const {
        return summary_config_.excluded_tables_.find(table_name) ==
               summary_config_.excluded_tables_.end();
    }

    friend class ObjectManager;
    friend class DatabaseRoot;
    friend class DatabaseNamespace;
};

} // namespace simdb

