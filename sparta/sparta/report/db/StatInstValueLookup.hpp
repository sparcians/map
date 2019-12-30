// <StatInstValueLookup> -*- C++ -*-

#ifndef __SPARTA_STAT_INST_VALUE_LOOKUP_H__
#define __SPARTA_STAT_INST_VALUE_LOOKUP_H__

#include "sparta/report/db/StatInstRowIterator.hpp"

namespace sparta {

/*
 * \brief This class holds a shared StatInstRowIterator
 * object which owns a vector of SI values. This class
 * also has an index value which tells it which element
 * in the row iterator's vector belongs to this direct-
 * lookup object.
 *
 * More specifically, it owns a StatInstRowIterator's
 * "RowAccessor" object, which has just one read-only
 * API to ask for the current SI row's double vector
 * by const reference.
 */
class StatInstValueLookup {
public:
    using RowAccessorPtr =
        std::shared_ptr<StatInstRowIterator::RowAccessor>;

    //! Construct with a shared RowAccessor object (obtained
    //! from a StatInstRowIterator), and an SI index which
    //! tells us where our SI value can be found in the
    //! row accessor's underlying double vector.
    StatInstValueLookup(const RowAccessorPtr & row_accessor,
                        const size_t si_index) :
        StatInstValueLookup(row_accessor, si_index, true)
    {
        //Constructor delegation. Pass in true for third argument
        //so we know to perform argument validation.
    }

    //! Verify that this lookup object's SI index is within
    //! the range of the underlying RowAccessor's SI values
    //! vector. You may want to call this just once after
    //! calling StatInstRowIterator::getNext() for the first
    //! time, or call it once after every call to getNext().
    //! Or never call it at all for best performance, if you
    //! are willing to give up safety checks. This would be
    //! the same decision as calling std::vector<T>::operator[]
    //! versus std::vector<T>::at()
    virtual bool isIndexValidForCurrentRow() const {
        return si_index_ < row_accessor_->getCurrentRow().size();
    }

    //! Get this lookup object's SI value for the SI row
    //! its RowAccessor is currently pointing to.
    virtual double getCurrentValue() const {
        return row_accessor_->getCurrentRow()[si_index_];
    }

    //! This class can be "partially constructed" if you
    //! only have the SI index value on hand, but not the
    //! RowAccessor object that goes with it. Create a
    //! placeholders::StatInstValueLookup object with the
    //! SI index value, and call its realizePlaceholder()
    //! method when you later get the RowAccessor.
    virtual sparta::StatInstValueLookup * realizePlaceholder(
        const RowAccessorPtr & row_accessor)
    {
        (void) row_accessor;
        return this;
    }

    virtual ~StatInstValueLookup() = default;

protected:
    StatInstValueLookup() :
        StatInstValueLookup(nullptr, 0, false)
    {
        //Constructor delegation. Pass in false for third argument
        //so we do not perform argument validation.
    }

private:
    //! The public constructor meant for non-placeholder or realized
    //! placeholder objects calls this private constructor. And the
    //! protected constructor meant for for non-realized placeholders
    //! calls here too. The only difference is if we validate the
    //! constructor arguments.
    StatInstValueLookup(const RowAccessorPtr & row_accessor,
                        const size_t si_index,
                        const bool validate) :
        row_accessor_(row_accessor),
        si_index_(si_index)
    {
        if (validate and !row_accessor_) {
            throw SpartaException("Null StatInstRowIterator::RowAccessor ")
                << "given to a StatInstValueLookup constructor";
        }
        //We can't validate the SI index right now. The row
        //accessor could still be holding a reference to an
        //empty vector. This would be the case if the owning
        //StatInstRowIterator::getNext() method hasn't been
        //called yet.
    }

    RowAccessorPtr row_accessor_;
    const size_t si_index_;
};

} // namespace sparta

namespace placeholders {

//! This placeholder is used when you want to ultimately
//! create a sparta::StatInstValueLookup object, but you
//! only have the leaf SI index value on hand.
//!
//! These "placeholder" objects are later turned into
//! "realized" objects when you call the realizePlaceholder()
//! method.
//!
//! IMPORTANT: All public API's in the base class are
//! off limits until you first call realizePlaceholder().
//! The *returned* finalized, realized value lookup object
//! is the only one who can touch those API's. Attemps to
//! call base class API's on an unrealized placeholder
//! will throw an exception.
class StatInstValueLookup : public sparta::StatInstValueLookup
{
public:
    //! Construct with the leaf SI index that you want to
    //! later give to a sparta::StatInstValueLookup.
    explicit StatInstValueLookup(const size_t si_index) :
        sparta::StatInstValueLookup(),
        si_index_(si_index)
    {}

    //! Combine the leaf SI index you gave our constructor
    //! with a RowAccessor object to "realize" this placeholder
    //! into a finalized, usable, StatInstValueLookup object.
    virtual sparta::StatInstValueLookup * realizePlaceholder(
        const sparta::StatInstValueLookup::RowAccessorPtr & row_accessor) override;

private:
    //! Override base class public API's so we can throw if
    //! anyone tries to use a placeholder like it was a finalized
    //! sparta::StatInstValueLookup
    virtual bool isIndexValidForCurrentRow() const override final {
        throw sparta::SpartaException("StatInstValueLookup::isIndexValidForCurrentRow() ")
            << "called on a placeholder object that has not yet "
            << "been realized!";
    }
    virtual double getCurrentValue() const override final {
        throw sparta::SpartaException("StatInstValueLookup::getCurrentValue() ")
            << "called on a placeholder object that has not yet "
            << "been realized!";
    }

    const size_t si_index_;
};

} // namespace placeholders

#endif
