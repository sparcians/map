// <StatInstRowIterator> -*- C++ -*-

#pragma once

#include "simdb/ObjectManager.hpp"
#include "simdb/utils/ObjectQuery.hpp"
#include "simdb/schema/Schema.hpp"
#include "sparta/utils/ValidValue.hpp"

namespace sparta {

/*!
 * \brief This class wraps an ObjectQuery which is
 * positioned to loop over one or more rows of SI
 * blob data.
 */
class StatInstRowIterator
{
public:
    //! This class can be used standalone as an ObjectQuery
    //! wrapper, using the getNext() method to iterate over
    //! an SI dataset.
    //!
    //! However, this class is also used together with
    //! StatInstValueLookup objects, which are given to
    //! all SI's in a SimDB report. We do not want to
    //! expose public API's like getNext() to every SI
    //! in a report, since a call to that method has an
    //! irreversible effect on all StatInstValueLookup's
    //! that are pointing to this row iterator.
    //!
    //! This nested class overcomes this, and only provides
    //! SI's a getter method returning a const reference to
    //! the underlying SI double vector.
    class RowAccessor
    {
    public:
        //! Construct with an SI values vector. This accessor
        //! holds a reference to that vector.
        explicit RowAccessor(const std::vector<double> & row_ref) :
            row_(row_ref)
        {}

        //! Return the row of SI values this accessor is tied to.
        const std::vector<double> & getCurrentRow() const {
            return row_;
        }

    private:
        const std::vector<double> & row_;
    };

    using RowAccessorPtr = std::shared_ptr<RowAccessor>;

    //! Construct a row iterator for a root-level report node
    //! in the given database. You can find available root-level
    //! nodes in the ReportNodeHierarchy table where the column
    //! ParentNodeID equals 0.
    StatInstRowIterator(const simdb::DatabaseID report_root_node_id,
                        const simdb::ObjectManager & obj_mgr) :
        row_accessor_(new RowAccessor(raw_si_values_))
    {
        simdb::ObjectQuery query(obj_mgr, "SingleUpdateStatInstValues");

        query.addConstraints(
            "RootReportNodeID",
            simdb::constraints::equal,
            report_root_node_id);

        query.writeResultIterationsTo(
            "RawBytes", &raw_si_bytes_,
            "NumPts", &raw_si_num_pts_,
            "WasCompressed", &raw_si_was_compressed_);

        result_iter_ = query.executeQuery();
        if (result_iter_ == nullptr) {
            throw SpartaException("Unable to use StatInstRowIterator. The ")
                << "database query failed.";
        }
    }

    //! Get a "row accessor" object which can give you a const
    //! reference to this row iterator's current SI row values.
    virtual const RowAccessorPtr & getRowAccessor() const {
        return row_accessor_;
    }

    //! Advance this iterator to the next row of SI values.
    //! Returns true on success, false otherwise. This will
    //! typically return false only when there are no more
    //! SI values in this data set.
    //!
    //! If this method returns false for any reason, consider
    //! the accompanying RowAccessor to be invalidated. Once
    //! this iterator is out of data to loop over, calls to
    //! RowAccessor::getCurrentRow() are undefined.
    virtual bool getNext() {
        //Advance the ObjectQuery
        if (!result_iter_->getNext()) {
            return false;
        }

        return getCurrentRowDoubles_();
    }

    virtual ~StatInstRowIterator() = default;

    //! This class can be "partially constructed" if you
    //! only have a *non* root-level report node ID on
    //! hand, and cannot retrieve its root-level node ID
    //! right away for some reason. In that case, create a
    //! placeholders::StatInstRowIterator object with the
    //! non-root-level report node ID, and later call its
    //! realizePlaceholder() method when you are ready.
    //! The placeholder subclass will find the root-level
    //! report node ID for you at that time.
    virtual sparta::StatInstRowIterator * realizePlaceholder() {
        return this;
    }

protected:
    StatInstRowIterator() = default;

private:
    //! Turn the vector of char's we are holding onto
    //! into a vector double's. This takes into account
    //! compression if it was performed on the current
    //! SI row. Returns true on success.
    bool getCurrentRowDoubles_();

    int raw_si_num_pts_;
    int raw_si_was_compressed_;
    RowAccessorPtr row_accessor_;
    std::vector<double> raw_si_values_;
    std::vector<char> raw_si_bytes_;
    std::unique_ptr<simdb::ResultIter> result_iter_;
};

} // namespace sparta

namespace placeholders {

//! This placeholder is used when you want to ultimately
//! create a sparta::StatInstRowIterator object, but you
//! only have a non-root-level report node ID. It can
//! be somewhat expensive to get the root-level node ID
//! for the report node that you have on hand, and
//! you may wish to delay that expensive database query
//! until later for some reason.
//!
//! These "placeholder" objects are later turned into
//! "realized" objects when you call the realizePlaceholder()
//! method.
//!
//! IMPORTANT: All public API's in the base class are
//! off limits until you first call realizePlaceholder().
//! The *returned* finalized, realized row iterator object
//! is the only one who can touch those API's. Attempts to
//! call base class API's on an unrealized placeholder
//! will throw an exception.
class StatInstRowIterator : public sparta::StatInstRowIterator
{
public:
    //! Construct one of these placeholders with a non-root-level
    //! report node ID. These would be any Id's for records in the
    //! ReportNodeHierarchy table whose ParentNodeID field is not
    //! zero.
    //!
    //! These placeholders can also be given a root-level report
    //! node ID, though you may incur unnecessary overhead if you
    //! use a placeholder when you don't need to. You'll typically
    //! give the base class your root-level ID directly, and skip
    //! the step of having a placeholder at all.
    StatInstRowIterator(const simdb::DatabaseID report_hier_node_id,
                        const simdb::ObjectManager * obj_mgr) :
        sparta::StatInstRowIterator(),
        report_hier_node_id_(report_hier_node_id),
        obj_mgr_(obj_mgr)
    {}

    virtual sparta::StatInstRowIterator * realizePlaceholder() override;

private:
    //! Override base class public API's so we can throw if
    //! anyone tries to use a placeholder like it was a finalized
    //! sparta::StatInstRowIterator
    using RowAccessorPtr = sparta::StatInstRowIterator::RowAccessorPtr;
    virtual const RowAccessorPtr & getRowAccessor() const override final {
        throw sparta::SpartaException("StatInstRowIterator::getRowAccessor() called ")
            << "on a placeholder object that has not yet been realized!";
    }
    virtual bool getNext() override final {
        throw sparta::SpartaException("StatInstRowIterator::getNext() called ")
            << "on a placeholder object that has not yet been realized!";
    }

    const simdb::DatabaseID report_hier_node_id_;
    const simdb::ObjectManager *const obj_mgr_;
};

} // namespace placeholders

