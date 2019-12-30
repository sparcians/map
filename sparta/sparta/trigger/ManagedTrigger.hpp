
#ifndef __SPARTA_MANAGED_TRIGGER_H__
#define __SPARTA_MANAGED_TRIGGER_H__

#include "sparta/kernel/Scheduler.hpp"
#include "sparta/kernel/SpartaHandler.hpp"

namespace sparta {

class Clock;

namespace trigger {

/*!
 * \brief Base class that works together with the TriggerManager
 * singleton to control when triggers are to be considered active.
 * When a subclass is created, it is added to the TriggerManager,
 * and removed when it is destroyed or when the subclass calls
 * the protected method 'deactivate_()'.
 *
 * While the trigger is active, its 'isTriggerReached_()' virtual
 * method will be called at every Scheduler tick. Once this method
 * returns true, the virtual method 'invokeTrigger_()' will be called,
 * and the trigger will be removed from the TriggerManager.
 */
class ManagedTrigger
{
public:
    ManagedTrigger(const ManagedTrigger & rhp) :
        name_(rhp.name_),
        clk_(rhp.clk_),
        active_(rhp.active_),
        register_handler_(rhp.register_handler_)
    {
        if (active_) {
            registerSelf_();
        }
    }

    ManagedTrigger & operator=(const ManagedTrigger & rhp) {
        // End current observation
        active_ = false;
        deregisterSelf_();

        name_ = rhp.name_;
        clk_ = rhp.clk_;
        active_ = rhp.active_;
        register_handler_ = rhp.register_handler_;

        // Reregister if active
        if (active_) {
            registerSelf_();
        }

        return *this;
    }

    virtual ~ManagedTrigger() {
        deregisterSelf_();
    }

    const Clock * getClock() const {
        return clk_;
    }

    void check() {
        sparta_assert(active_, "ManagedTrigger \"" << getName()
                    << "\" was 'checked' when not active.");

        if (isTriggerReached_()) {
            active_ = false;
            deregisterSelf_();
            invokeTrigger_();
        }
    }

    const std::string & getName() const {
        return name_;
    }

protected:
    ManagedTrigger(const std::string & name,
                   const Clock * clk) :
        name_(name),
        clk_(clk),
        register_handler_(
            CREATE_SPARTA_HANDLER(ManagedTrigger,
                                registerSelf_))
    {
        sparta_assert(clk_ != nullptr,
                    "Cannot instantiate a ManagedTrigger with a null clock");

        //Make active immediately
        registerSelf_();
    }

    void registerSelf_();

    void deactivate_();

    bool isActive_() const;

private:
    void deregisterSelf_() const;

    virtual bool isTriggerReached_() const = 0;
    virtual void invokeTrigger_() = 0;

    std::string name_;
    const Clock * clk_ = nullptr;
    bool active_ = false;
    SpartaHandler register_handler_;
    friend class ExpressionTrigger;
};

} // namespace trigger
} // namespace sparta

#endif
