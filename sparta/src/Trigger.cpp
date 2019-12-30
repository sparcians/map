

#include <cstdint>
#include <string>
#include <vector>

#include "sparta/trigger/SingleTrigger.hpp"
#include "sparta/trigger/TriggerManager.hpp"
#include "sparta/trigger/ExpressionTrigger.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/statistics/CounterBase.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/kernel/SpartaHandler.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/events/StartupEvent.hpp"
#include "sparta/trigger/ManagedTrigger.hpp"

//*// Static initializations //*//
std::vector<std::string> sparta::trigger::ExpressionTrigger::
    supported_comparisons_({ "==", "!=", ">=", "<=", ">", "<" });
//*////////////////////////////*//

namespace sparta {
    namespace trigger {

CounterTrigger::CounterTrigger(const std::string& name,
                               const SpartaHandler & callback,
                               const CounterBase* counter,
                               CounterBase::counter_type trigger_point) :
    SingleTrigger(name, callback),
    ManagedTrigger(name, counter->getClock()),
    counter_(counter),
    trigger_point_(trigger_point)
{
    sparta_assert(counter != nullptr,
                "Cannot construct CounterTrigger \"" << name << "\" with a null counter");
    sparta_assert(counter->getClock() != nullptr,
                "Cannot construct CounterTrigger \"" << name << "\" with counter \""
                << counter << "\" having a null clock");

    counter_wref_ = counter_->getWeakPtr();
}

CounterTrigger::CounterTrigger(const CounterTrigger& rhp) :
    SingleTrigger(rhp),
    ManagedTrigger(rhp),
    counter_(rhp.counter_),
    counter_wref_(rhp.counter_wref_),
    trigger_point_(rhp.trigger_point_)
{
}

CounterTrigger::~CounterTrigger()
{
}

CounterTrigger& CounterTrigger::operator= (const CounterTrigger& rhp)
{
    *static_cast<SingleTrigger*>(this) = *static_cast<const SingleTrigger*>(&rhp);
    *static_cast<ManagedTrigger*>(this) = *static_cast<const ManagedTrigger*>(&rhp);

    // Copy over data and re-register
    sparta_assert(rhp.counter_);
    counter_ = rhp.counter_;
    trigger_point_ = rhp.trigger_point_;

    return *this;
}

void CounterTrigger::deactivate()
{
    // No harm in repeating removal
    ManagedTrigger::deactivate_();
}

void CounterTrigger::set()
{
    const std::string & name = ManagedTrigger::getName();
    sparta_assert(!isActive(),
                "Trigger[ '" << name << "']: cannot be already set, "
                "only prepped");
    ManagedTrigger::registerSelf_();
}

bool CounterTrigger::isActive() const
{
    return ManagedTrigger::isActive_();
}

TimeTrigger::TimeTrigger(const std::string& name,
                         const SpartaHandler & callback,
                         uint64_t picoseconds,
                         const Clock * clk) :
    SingleTrigger(name, callback),
    event_("time_trigger_event", CREATE_SPARTA_HANDLER(TimeTrigger, invoke_), clk),
    schedule_for_ps_(picoseconds),
    clk_(clk)
{
    event_.setContinuing(false);

    // Make active immediately
    set_();
}

void ManagedTrigger::registerSelf_()
{
    auto scheduler = clk_->getScheduler();
    if (scheduler->isFinalized()) {
        TriggerManager::getTriggerManager().addTrigger(this);
        active_ = true;
    } else {
        StartupEvent(scheduler,
                     register_handler_);
    }
}

void ManagedTrigger::deregisterSelf_() const
{
    TriggerManager::getTriggerManager().removeTrigger(this);
}

void ManagedTrigger::deactivate_()
{
    deregisterSelf_();
    active_ = false;
}

bool ManagedTrigger::isActive_() const
{
    return active_;
}

    } // namespace trigger
} // namespace sparta
