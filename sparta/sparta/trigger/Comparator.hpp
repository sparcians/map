// <Comparator> -*- C++ -*-

/**
 * \file Comparator.hpp
 *
 */

#ifndef __SPARTA_COMPARATOR_H__
#define __SPARTA_COMPARATOR_H__

/*!
 * \brief Generic classes for comparing one dynamic value against
 * a static value: ==, !=, <, >, <=, >=
 */

#include <functional>
#include <cinttypes>
#include <memory>
#include <map>

namespace sparta {
namespace trigger {

template <typename DataT>
class ComparatorBase
{
public:
    virtual ~ComparatorBase() {}
    virtual bool eval(const DataT & compareTo) const = 0;
    virtual ComparatorBase<DataT> * clone(const DataT & val) const = 0;
};

template <typename DataT, class CompareFunctor>
class Comparator : public ComparatorBase<DataT>
{
public:
    bool eval(const DataT & compareTo) const override final {
        return evaluator_(compareTo, val_);
    }

    ComparatorBase<DataT> * clone(const DataT & val) const override {
        auto comp = new Comparator<DataT, CompareFunctor>(*this);
        comp->val_ = val;
        return comp;
    }

    CompareFunctor evaluator_;
    DataT val_;
};

template <class DataT>
std::map<std::string, std::unique_ptr<ComparatorBase<DataT>>> createMap()
{
    std::map<std::string, std::unique_ptr<ComparatorBase<DataT>>> cases;

    cases["=="] = std::unique_ptr<ComparatorBase<DataT>>(new Comparator<DataT, std::equal_to<DataT>>);
    cases["!="] = std::unique_ptr<ComparatorBase<DataT>>(new Comparator<DataT, std::not_equal_to<DataT>>);
    cases[">="] = std::unique_ptr<ComparatorBase<DataT>>(new Comparator<DataT, std::greater_equal<DataT>>);
    cases["<="] = std::unique_ptr<ComparatorBase<DataT>>(new Comparator<DataT, std::less_equal<DataT>>);
    cases[">" ] = std::unique_ptr<ComparatorBase<DataT>>(new Comparator<DataT, std::greater<DataT>>);
    cases["<" ] = std::unique_ptr<ComparatorBase<DataT>>(new Comparator<DataT, std::less<DataT>>);

    return cases;
}

inline std::map<std::string, std::string> getNegatedComparatorMap()
{
    std::map<std::string, std::string> cases;

    cases["=="] = "!=";
    cases["!="] = "==";
    cases[">="] = "<";
    cases["<="] = ">";
    cases[">" ] = "<=";
    cases["<" ] = ">=";

    return cases;
}

/*!
 * \brief Utility factory to turn a logical operator (string)
 * into a Comparator (object)
 */
template <typename DataT>
std::unique_ptr<ComparatorBase<DataT>> createComparator(const std::string & op, const DataT & val)
{
    static auto cases = createMap<DataT>();

    auto iter = cases.find(op);
    if (iter == cases.end()) {
        return nullptr;
    }

    return std::unique_ptr<ComparatorBase<DataT>>(iter->second->clone(val));
}

// namespace trigger
}
// namespace sparta
}

// __SPARTA_COMPARATOR_H__
#endif
