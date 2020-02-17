// <StatInstCalculator> -*- C++ -*-

/**
 * \file StatInstCalculator.hpp
 *
 */

#ifndef __SPARTA_STAT_INST_CALCULATOR_H__
#define __SPARTA_STAT_INST_CALCULATOR_H__

#include "sparta/kernel/SpartaHandler.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace sparta {

class TreeNode;

/*
 * \brief This class is a wrapper around user-implemented
 * code (SpartaHandler's) that calculate the value of a SPARTA
 * statistic on demand. There are cases where expressing
 * a statistic equation in a single std::string is not
 * very easy to do - if it were easily written out in
 * a single string, you could use StatisticDef's to get
 * the statistic values in reports as usual.
 *
 * StatisticInstance's have a constructor overload which
 * takes one of these StatInstCalculator objects, and
 * when the reporting infrastructure calls 'SI::getValue()'
 * it will invoke the user's SpartaHandler to retrieve the
 * value.
 */
class StatInstCalculator {
public:
    //! Give the calculator a SpartaHandler to your own
    //! code which performs the calculation, as well as
    //! a variable in your code which holds the calculated
    //! value. For example:
    //!
    //!   class MyFoo {
    //!   public:
    //!     MyFoo() {
    //!       auto cb = CREATE_SPARTA_HANDLER(MyFoo, randomNum_);
    //!       std::shared_ptr<StatInstCalculator> calc(
    //!         new StatInstCalculator(cb, my_random_num_));
    //!       //Do whatever you want with this 'calc' object,
    //!       //such as creating a StatisticInstance out of
    //!       //it for evaluation (in reports, etc.)
    //!     }
    //!   private:
    //!     void randomNum_() {
    //!       my_random_num_ = (double)rand();
    //!     }
    //!     double my_random_num_ = 0;
    //!   };
    //!
    //! \note The SpartaHandler (callback) must have the function
    //! signature 'void fcnName(void)'
    //! \note The member variable where you store the calculated
    //! value must be a double scalar
    StatInstCalculator(const SpartaHandler & handler,
                       const double & aggregated_value) :
        handler_(handler),
        aggregated_value_(aggregated_value)
    {}

    //! Invoke the user's SpartaHandler to perform the calculation,
    //! and return the result.
    double getCurrentValue() const {
        handler_();
        return aggregated_value_;
    }

    //! If you want to give this StatInstCalculator to a
    //! StatisticInstance (for getting these calculated values
    //! into a SPARTA report for instance) then you need to give
    //! this object the TreeNode to which it belongs.
    void setNode(const TreeNode * node) {
        sparta_assert(node_ == nullptr || node_ == node);
        sparta_assert(node != nullptr);
        node_ = node;
    }

    //! Return the TreeNode to which this calculator belongs
    //! \note Will return null if 'setNode()' was never called
    const TreeNode * getNode() const {
        return node_;
    }

private:
    SpartaHandler handler_;
    const double & aggregated_value_;
    const TreeNode * node_ = nullptr;
};

}

#endif
