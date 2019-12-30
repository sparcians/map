// <TableRef> -*- C++ -*-

#include "simdb/ObjectManager.hpp"
#include "simdb/ObjectRef.hpp"
#include "simdb/TableRef.hpp"
#include "simdb/DbConnProxy.hpp"
#include "simdb/utils/ObjectQuery.hpp"

namespace simdb {

//! Scoped object which ensures the TableRef's member
//! variables related to object inserts and updates
//! are cleared out at the end of the methods which
//! put one of these on the stack.
class OnCreateOrUpdateExit {
public:
    OnCreateOrUpdateExit(
           bool & is_in_update_statement,
           std::unique_ptr<TableRef::RecordFinder> & record_finder,
           std::vector<char> & raw_bytes,
           ColumnValueContainer & col_values) :
        is_in_update_statement_(is_in_update_statement),
        record_finder_(record_finder),
        raw_bytes_(raw_bytes),
        col_values_(col_values)
    {}

    ~OnCreateOrUpdateExit() {
        is_in_update_statement_ = false;
        record_finder_.reset();
        raw_bytes_.clear();
        col_values_.clear();
    }

private:
    bool & is_in_update_statement_;
    std::unique_ptr<TableRef::RecordFinder> & record_finder_;
    std::vector<char> & raw_bytes_;
    ColumnValueContainer & col_values_;
};

std::unique_ptr<ObjectRef> TableRef::createDefaultObject_()
{
    std::unique_ptr<ObjectRef> obj_ref;
    DatabaseID db_id = 0;

    obj_mgr_.safeTransaction([&]() {
        //Get ready for the database statement to be run.
        //Protect the TableRef member variables from exceptions.
        OnCreateOrUpdateExit scoped_exit(
            is_in_update_statement_,
            record_finder_for_update_,
            raw_bytes_for_obj_create_,
            col_values_);

        (void) scoped_exit;

        if (!raw_bytes_for_obj_create_.empty()) {
            db_id = fixed_size_record_factory_(
                db_proxy_.get(),
                table_name_,
                &raw_bytes_for_obj_create_[0],
                raw_bytes_for_obj_create_.size());

            return;
        }

        ColumnValues null_values;
        db_id = any_size_record_factory_(
            db_proxy_.get(), table_name_, null_values);
    });

    const bool return_object =
        explicit_return_object_ == ExplicitReturnObject::DEFAULT ||
        explicit_return_object_ == ExplicitReturnObject::ALWAYS_RETURN;

    if (db_id > 0 && return_object) {
        obj_ref.reset(new ObjectRef(obj_mgr_, table_name_, db_id));
    }
    return obj_ref;
}

//! Zero-argument object creation. All columns will take
//! their default values if any were specified in this
//! table's schema definition.
std::unique_ptr<ObjectRef> TableRef::createObject()
{
    return createDefaultObject_();
}

std::unique_ptr<ObjectRef> TableRef::finalizeCreationStatement_()
{
    //Defer to the zero-argument createObject() API
    //if we do not have any column values up front.
    //This could happen with a call site like this:
    //
    //   std::vector<double> empty_vec;
    //   auto obj = table->createObjectWithArgs(
    //       "MyBlob",
    //       empty_vec);
    if (col_values_.empty()) {
        return createDefaultObject_();
    }

    DatabaseID db_id = 0;
    obj_mgr_.safeTransaction([&]() {
        //Get ready for the database statement to be run.
        //Protect the TableRef member variables from exceptions.
        OnCreateOrUpdateExit scoped_exit(
            is_in_update_statement_,
            record_finder_for_update_,
            raw_bytes_for_obj_create_,
            col_values_);

        (void) scoped_exit;

        //Make sure the TableRef method updateRowValues() was
        //used like this:
        //
        //  table->updateRowValues("MyInt", 100).
        //         forRecordsWhere("MyInt", simdb::constraints::less, 85);
        //
        //And not like this:
        //
        //  auto & updater = table->updateRowValues("MyInt", 100);
        //  table->createObjectWithArgs("MyFloat", 3.14);
        //  updater.forRecordsWhere("MyInt", simdb::constraints::less, 85);
        //
        //The scoped OnCreateOrUpdateExit object above will prevent
        //resource leaks if we throw.
        if (is_in_update_statement_) {
            throw DBException(
                "You cannot make calls to RecordFinder::forRecordsWhere() "
                "at a different time (on a different line of code) than "
                "calls to TableRef::updateRowValues().");
        }

        db_id = any_size_record_factory_(
            db_proxy_.get(), table_name_, col_values_.getValues());
    });

    const bool return_object =
        explicit_return_object_ == ExplicitReturnObject::DEFAULT ||
        explicit_return_object_ == ExplicitReturnObject::ALWAYS_RETURN;

    if (!return_object) {
        return nullptr;
    }

    if (db_id <= 0) {
        throw DBException("Invalid database ID encountered while ")
            << "executing TableRef::createObjectWithArgs()";
    }

    return std::unique_ptr<ObjectRef>(
        new ObjectRef(obj_mgr_, table_name_, db_id));
}

void TableRef::finalizeDeletionStatement_()
{
    struct OnDeletionExit {
        OnDeletionExit(ColumnValueContainer & where_clauses) :
            where_clauses_(where_clauses)
        {}
        ~OnDeletionExit() {
            where_clauses_.clear();
        }
    private:
        ColumnValueContainer & where_clauses_;
    };

    obj_mgr_.safeTransaction([&]() {
        OnDeletionExit scoped_exit(delete_where_clauses_);
        (void) scoped_exit;

        db_proxy_->performDeletion(
            table_name_, delete_where_clauses_.getValues());
    });
}

size_t TableRef::finalizeUpdateStatement_()
{
    if (record_finder_for_update_ == nullptr) {
        return 0;
    }
    if (col_values_.empty()) {
        return 0;
    }

    size_t num_records_updated = 0;

    obj_mgr_.safeTransaction([&]() {
        //Get ready for the database statement to be run.
        //Protect the TableRef member variables from exceptions.
        OnCreateOrUpdateExit scoped_exit(
            is_in_update_statement_,
            record_finder_for_update_,
            raw_bytes_for_obj_create_,
            col_values_);

        (void) scoped_exit;

        const auto & where_clauses =
            record_finder_for_update_->update_where_clauses_;

        num_records_updated = db_proxy_->performUpdate(
            table_name_, col_values_.getValues(), where_clauses.getValues());
    });

    return num_records_updated;
}

struct ColumnValueCaster {
    ColumnValueCaster(const ColumnDataType dtype) :
        dtype_(dtype)
    {
        bytes_.resize(getFixedNumBytesForColumnDType(dtype_));
        value_ = &bytes_[0];
    }

    void * getValuePtr() {
        return value_;
    }

    double getDoubleValue() const {
        using dt = ColumnDataType;

        switch (dtype_) {
            case dt::char_t: {
                const char casted = *reinterpret_cast<char*>(value_);
                return static_cast<double>(casted);
            }
            case dt::int8_t: {
                const int8_t casted = *reinterpret_cast<int8_t*>(value_);
                return static_cast<double>(casted);
            }
            case dt::uint8_t: {
                const uint8_t casted = *reinterpret_cast<uint8_t*>(value_);
                return static_cast<double>(casted);
            }
            case dt::int16_t: {
                const int16_t casted = *reinterpret_cast<int16_t*>(value_);
                return static_cast<double>(casted);
            }
            case dt::uint16_t: {
                const uint16_t casted = *reinterpret_cast<uint16_t*>(value_);
                return static_cast<double>(casted);
            }
            case dt::int32_t: {
                const int32_t casted = *reinterpret_cast<int32_t*>(value_);
                return static_cast<double>(casted);
            }
            case dt::uint32_t: {
                const uint32_t casted = *reinterpret_cast<uint32_t*>(value_);
                return static_cast<double>(casted);
            }
            case dt::int64_t: {
                const int64_t casted = *reinterpret_cast<int64_t*>(value_);
                return static_cast<double>(casted);
            }
            case dt::uint64_t: {
                const uint64_t casted = *reinterpret_cast<uint64_t*>(value_);
                return static_cast<double>(casted);
            }
            case dt::float_t: {
                const float casted = *reinterpret_cast<float*>(value_);
                return static_cast<double>(casted);
            }
            case dt::double_t: {
                return *static_cast<double*>(value_);
            }
            default:
                return 0;
        }
    }

    ColumnDataType getDataType() const {
        return dtype_;
    }

    ColumnValueCaster(const ColumnValueCaster &) = delete;
    ColumnValueCaster(ColumnValueCaster &&) = delete;

private:
    const ColumnDataType dtype_;
    void * value_ = nullptr;
    std::vector<char> bytes_;
};

bool TableRef::captureSummary()
{
    ObjectQuery query(obj_mgr_, table_name_);
    std::vector<double> source_column_values;

    std::unique_ptr<TableRef> summary_table_ref =
        obj_mgr_.getTable(table_name_ + "_Summary");

    if (!summary_table_ref) {
        return false;
    }

    std::unique_ptr<ObjectRef> summary_table_record;

    for (const auto & col : col_descriptors_) {
        ColumnValueCaster caster(col.second);

        using dt = ColumnDataType;

        switch (caster.getDataType()) {
            case dt::char_t: {
                query.writeResultIterationsTo(
                    col.first.c_str(), static_cast<char*>(caster.getValuePtr()));
                break;
            }
            case dt::int8_t: {
                query.writeResultIterationsTo(
                    col.first.c_str(), static_cast<int8_t*>(caster.getValuePtr()));
                break;
            }
            case dt::uint8_t: {
                query.writeResultIterationsTo(
                    col.first.c_str(), static_cast<uint8_t*>(caster.getValuePtr()));
                break;
            }
            case dt::int16_t: {
                query.writeResultIterationsTo(
                    col.first.c_str(), static_cast<int16_t*>(caster.getValuePtr()));
                break;
            }
            case dt::uint16_t: {
                query.writeResultIterationsTo(
                    col.first.c_str(), static_cast<uint16_t*>(caster.getValuePtr()));
                break;
            }
            case dt::int32_t: {
                query.writeResultIterationsTo(
                    col.first.c_str(), static_cast<int32_t*>(caster.getValuePtr()));
                break;
            }
            case dt::uint32_t: {
                query.writeResultIterationsTo(
                    col.first.c_str(), static_cast<uint32_t*>(caster.getValuePtr()));
                break;
            }
            case dt::int64_t: {
                query.writeResultIterationsTo(
                    col.first.c_str(), static_cast<int64_t*>(caster.getValuePtr()));
                break;
            }
            case dt::uint64_t: {
                query.writeResultIterationsTo(
                    col.first.c_str(), static_cast<uint64_t*>(caster.getValuePtr()));
                break;
            }
            case dt::float_t: {
                query.writeResultIterationsTo(
                    col.first.c_str(), static_cast<float*>(caster.getValuePtr()));
                break;
            }
            case dt::double_t: {
                query.writeResultIterationsTo(
                    col.first.c_str(), static_cast<double*>(caster.getValuePtr()));
                break;
            }
            default:
                continue;
        }

        source_column_values.clear();
        const size_t matches = query.countMatches();
        if (matches == 0) {
            continue;
        }
        source_column_values.reserve(matches);

        if (summary_table_record == nullptr) {
            summary_table_record = summary_table_ref->createObject();
        }

        auto result_iter = query.executeQuery();
        while (result_iter->getNext()) {
            source_column_values.emplace_back(caster.getDoubleValue());
        }

        for (const auto & summary_fcn : summary_fcns_) {
            const std::string summary_table_column_name = col.first + "_" + summary_fcn.first;
            const double summarized_value = summary_fcn.second(source_column_values);

            summary_table_record->setPropertyDouble(
                summary_table_column_name,
                summarized_value);
        }
    }

    return summary_table_record != nullptr;
}

} // namespace simdb
