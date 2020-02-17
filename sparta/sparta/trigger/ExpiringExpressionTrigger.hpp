// <ExpiringExpressionTrigger> -*- C++ -*-

/**
 * \file ExpiringExpressionTrigger.hpp
 *
 */

#ifndef __SPARTA_EXPIRING_EXPRESSION_TRIGGER_H__
#define __SPARTA_EXPIRING_EXPRESSION_TRIGGER_H__

#include "sparta/trigger/ExpressionTrigger.hpp"

namespace sparta {
namespace trigger {

/*!
 * \class ExpiringExpressionTrigger
 *
 * \brief ExpressionTrigger's reuse classes such as SingleTrigger
 * (CounterTrigger, CycleTrigger, TimeTrigger) and others such as
 * NotificationTrigger under the hood. Some of these other classes
 * cannot safely be deleted during simulation as they can leave
 * dangling this/back-pointers in the TriggerManager singleton.
 * ExpiringExpressionTrigger is used in order to allow these
 * ExpressionTrigger's to be "destroyed" during simulation, but
 * instead of actually being deallocated, they are simply removed
 * (deactivated) from the TriggerManager and cleared out of any
 * other data structures that use trigger back pointers in a
 * similar way to what TriggerManager does.
 *
 * Here is example code which causes problems during simulation,
 * notably valgrind failures:
 *
 *     \code
 *         unique_ptr<ExpressionTrigger> trig(new ...)
 *             --- sim loop running ---
 *         trig.reset(new ...)
 *             --- sim loop still running ---
 *     \endcode
 *
 * Here is code which allows the same thing, but safely removes
 * the trigger from the TriggerManager / other data structures
 * without actually deallocating the ExpressionTrigger:
 *
 *     \code
 *         ExpiringExpressionTrigger trig(new ...)
 *             --- sim loop running ---
 *         trig.reset(new ...)
 *             --- sim loop still running ---
 *     \endcode
 */
class ExpiringExpressionTrigger
{
public:
    //! Instantiate with an ExpressionTrigger.
    //! \note The 'trig' passed in must have been allocated
    //! with 'new'. It will be owned outright by this class.
    explicit ExpiringExpressionTrigger(ExpressionTrigger * trig = nullptr) {
        reset(trig);
    }

    //! Set or reset the ExpressionTrigger owned by this object.
    void reset(ExpressionTrigger * trig = nullptr) {
        if (trigger_ != nullptr) {
            trigger_->deactivateAllInternals_();
            expired_triggers_.emplace_back(trigger_.release());
        }
        trigger_.reset(trig);
    }

    //! Underlying trigger access.
    ExpressionTrigger * operator->() {
        return trigger_.get();
    }

    //! Underlying trigger access.
    const ExpressionTrigger * operator->() const {
        return trigger_.get();
    }

    //! Underlying trigger access.
    ExpressionTrigger * get() {
        return trigger_.get();
    }

    //! Underlying trigger access.
    const ExpressionTrigger * get() const {
        return trigger_.get();
    }

    //! Compare trigger pointers.
    bool operator==(const ExpressionTrigger * trig) const {
        return trigger_.get() == trig;
    }

    //! Compare trigger pointers.
    bool operator!=(const ExpressionTrigger * trig) const {
        return trigger_.get() != trig;
    }

    //! Check for null with implicit cast.
    operator bool() const {
        return trigger_.get() != nullptr;
    }

private:
    std::unique_ptr<ExpressionTrigger> trigger_;
    std::vector<std::unique_ptr<ExpressionTrigger>> expired_triggers_;
};

} // namespace trigger
} // namespace sparta

#endif
