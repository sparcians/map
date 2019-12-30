// <Trigger> -*- C++ -*-

/**
 * \file SingleTrigger
 *
 */

#ifndef __SPARTA_SINGLE_TRIGGER__
#define __SPARTA_SINGLE_TRIGGER__

#include "sparta/trigger/ManagedTrigger.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/events/Scheduleable.hpp"
#include "sparta/events/Event.hpp"
#include "sparta/events/StartupEvent.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/kernel/DAG.hpp"

namespace sparta{
    namespace trigger{

/**
 * \class TriggerEvent
 */

class TriggerEvent : public Scheduleable
{
public:
    TriggerEvent(const std::string & name,
                 const SpartaHandler & consumer_event_handler,
                 const Clock * clk) :
        Scheduleable(consumer_event_handler, 0, SchedulingPhase::Trigger),
        name_(name)
    {
        local_clk_ = clk;

        if(clk) {
            scheduler_ = clk->getScheduler();
        }
        else {
            scheduler_ = Scheduler::getScheduler();
        }

        if(scheduler_->isFinalized()) {
            // Take on the GRP# of the GOP
            setGroupID_(scheduler_->getDAG()->getGOPoint("Trigger")->getGroupID());
        }

        // Prevent trigger events from keeping the simulation alive
        setContinuing(false);
        setLabel(name_.c_str());
    }

    /**
     * \brief No copy constructor
     */
    TriggerEvent(const TriggerEvent &) =delete;

    /**
     * \brief No move constructor
     */
    TriggerEvent(TriggerEvent &&) =delete;

private:
    std::string name_;
};

/**
 * \brief Single-Event Trigger Interface
 *
 * This method accepts a SpartaHandler to use as a callback to fire
 * when requested.
 * deactivate() can be called  to disable this trigger, and can
 * be called at any time to make the trigger meaningless.
 */
class SingleTrigger {
public:

    /*!
     * \brief Not default-constructable
     */
    SingleTrigger() = delete;

    /**
     * \brief Copy constructor
     */
    SingleTrigger(const SingleTrigger& rhp) :
        callback_(rhp.callback_),
        name_(rhp.name_),
        has_fired_(false)
    {;}

    /**
     * \brief Copy constructor
     */
    SingleTrigger& operator= (const SingleTrigger&) = default;

    SingleTrigger(const std::string& name, const SpartaHandler & callback) :
        callback_(callback),
        name_(name),
        has_fired_(false)
    {;}

    /**
     * \brief Virtual destructor
     */
    virtual ~SingleTrigger()
    {;}

    /*!
     * \brief Disable the trigger
     */
    virtual void deactivate() = 0;

    /*!
     * \brief Re-enable firing of the trigger based on the most recent prepping
     */
    virtual void set() = 0;

    /*!
     * \brief Is this trigger active (able to fire when condition is met)
     */
    virtual bool isActive() const = 0;

    /*!
     * \brief Has this trigger fired? Initially false, goes true when trigger
     * condition is met while isActive() is true;
     */
    bool hasFired() const
    {
        return has_fired_;
    }


    //! \name Configuration Interface
    //! @{
    ////////////////////////////////////////////////////////////////////////

    virtual void prepAbsolute(const Clock* clk, Clock::Cycle on_time)
    {
        (void) clk;
        (void) on_time;
    }

    virtual void prepRelative(const Clock* clk, Clock::Cycle rel)
    {
        (void) clk;
        (void) rel;
    }

    virtual void alignRelative(const bool align)
    {
        (void) align;
    }

    virtual void setAbsolute(const Clock* clk, Clock::Cycle on_time)
    {
        (void) clk;
        (void) on_time;
    }

    virtual void setRelative(const Clock* clk, Clock::Cycle delay)
    {
        (void) clk;
        (void) delay;
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

    /*!
     * \brief Return the clock associated with this trigger
     * \note Some triggers are not associated with a clock
     */
    virtual const Clock* getClock() const = 0;

    /**
     * \brief Returns the name of this trigger
     */
    std::string getName() const
    {
        return name_;
    }

    /*!
     * \brief Let subclasses access this trigger's scheduler, if it has one
     */
    Scheduler * getScheduler() const {
        auto clk = getClock();
        // TimeTrigger's will ultimately be tied to a clock, but to
        // prevent downstream teams from having to change their code
        // until sparta_v1.7, let's allow those code bases to just
        // fall back on the singleton scheduler. It's the scheduler
        // their simulators are using anyway.
        if (!clk) {
            return Scheduler::getScheduler();
        }
        auto scheduler = clk->getScheduler();
        sparta_assert(scheduler, "Trigger had a valid clock, but that clock did not have a scheduler");
        return scheduler;
    }

protected:

    /*!
     * \brief Invoke the callback
     * \post hasFired() will be true after the callback returns
     */
    void invokeCallback_()
    {
        callback_();
        has_fired_ = true;
    }

    /*!
     * \brief Get the callback handler
     */
    const SpartaHandler& getCallback_() const
    {
        return callback_;
    }

private:

    /**
     * \brief Owner's callback that is executed when this trigger is fired
     * (*by invoke_()).
     */
    SpartaHandler callback_;

    /**
     * \brief Name of this trigger
     */
    std::string name_;

    /*!
     * \brief Has this trigger fired yet
     */
    bool has_fired_;
};

/**
 * \brief A class that acts is responsible for invoking a callback after a
 * particular counter reaches a certain value
 *
 * see other methods for activating and scheduling the trigger.
 */
class CounterTrigger : public SingleTrigger, public ManagedTrigger
{
public:

     /**
     * \brief Not default-constructable
     */
    CounterTrigger() = delete;

    /**
     * \brief Construct with name and callback
     * \param name Name of the trigger
     * \param callback Function to call when the condition is reached
     * \param counter Counter to observe. Must not be nullptr
     * \param trigger_point Value of counter at or above which this trigger will
     * fire.
     */
    CounterTrigger(const std::string& name,
                   const SpartaHandler & callback,
                   const CounterBase* counter,
                   CounterBase::counter_type trigger_point);

    /**
     * \brief Copy constructor
     */
    CounterTrigger(const CounterTrigger& rhp);

    /**
     * \brief Not move-constructable
     */
    CounterTrigger(CounterTrigger&&) = delete;

    /**
     * \brief Destructor
     */
    ~CounterTrigger();

    /**
     * \brief Assignment
     */
    CounterTrigger& operator= (const CounterTrigger& rhp);

    void deactivate() override;

    void set() override;

    bool isActive() const override;

    void prepAbsolute(const Clock* clk, Clock::Cycle on_time) override
    {
        (void) clk;
        (void) on_time;
        sparta_assert(false, "prepAbsolute is currently unsupported for CounterTrigger");
    }

    void prepRelative(const Clock* clk, Clock::Cycle rel) override
    {
        (void) clk;
        (void) rel;
        sparta_assert(false, "prepRelative is currently unsupported for CounterTrigger");
    }

    void alignRelative(const bool align) override
    {
        (void) align;
        sparta_assert(false, "alignRelative is currently unsupported for CounterTrigger");
    }

    void setAbsolute(const Clock* clk, Clock::Cycle on_time) override
    {
        (void) clk;
        (void) on_time;
        sparta_assert(false, "setAbsolute is currently unsupported for CounterTrigger");
    }

    void setRelative(const Clock* clk, Clock::Cycle delay) override
    {
        (void) clk;
        (void) delay;
        sparta_assert(false, "setRelative is currently unsupported for CounterTrigger");
    }

    /*!
     * \brief Change the trigger point and activate the trigger.
     * Note that the trigger is activated by default upon construction and needn't be reactivated
     * here unless the trigger point needs to be updated. If the trigger has fired and must simple
     * be re-enabled, use set() instead.
     */
    void resetAbsolute(CounterBase::counter_type trigger_point) {
        ManagedTrigger::deactivate_(); // No harm if already inactive
        trigger_point_ = trigger_point;
        ManagedTrigger::registerSelf_();
    }

    /*!
     * \brief Returns the clock associated with this trigger, which is the clock
     * associated with the counter being observed. Will not be nullptr
     */
    const Clock* getClock() const override
    {
        // Return cached clock in case counter_ has been destructed (can be
        // checked with counter_wref_.expired()_
        return ManagedTrigger::getClock();
    }

    /*!
     * \brief Return the counter associated with this trigger. Will not be nullptr
     */
    virtual const CounterBase* getCounter() const
    {
        sparta_assert(false == counter_wref_.expired(),
                    "Cannot getCounter on a CounterTrigger because the referenced counter "
                    "has expired");
        return counter_;
    }

    /*!
     * \brief Returns the most recently configured trigger point
     */
    CounterBase::counter_type getTriggerPoint() const { return trigger_point_; }

protected:
    /**
     * \brief Allow subclasses to construct the base with the name
     * of the trigger, the callback to invoke when triggered, and
     * the clock object to which this trigger belongs.
     */
    CounterTrigger(const std::string & name,
                   const SpartaHandler & callback,
                   const Clock * clk) :
        SingleTrigger(name, callback),
        ManagedTrigger(name, clk),
        counter_(nullptr),
        trigger_point_(0)
    {
    }

private:

    /**
     * \brief Checks to see whether this trigger is reached
     * \return true if current counter value has reached or exceeded trigger
     * point
     * \pre Referenced counter must not have been destructed
     */
    virtual bool isTriggerReached_() const override
    {
        // Assume counter has no expired
        return counter_->get() >= trigger_point_;
    }

    /**
     * \brief When the "isTriggerReached_()" evaluation returns
     * true, the TriggerManager will call this method to invoke
     * the client's callback.
     */
    virtual void invokeTrigger_() override {
        SingleTrigger::invokeCallback_();
    }

    /**
     * \brief Counter to oberve
     */
    const CounterBase* counter_;

    /**
     * \brief Weak reference to counter to oberve
     */
    TreeNode::ConstWeakPtr counter_wref_;

    /**
     * \brief Point at or above which the trigger will fire
     */
    CounterBase::counter_type trigger_point_;
};

/**
 * \brief A class that acts is responsible for firing an event
 * after a number of cycles
 *
 * see other methods for activating and scheduling the trigger.
 */
class CycleTrigger : public SingleTrigger
{
public:

    /**
     * \brief Not default-constructable
     */
    CycleTrigger() = delete;

    /**
     * \brief Construct with name and callback
     */
    CycleTrigger(const std::string& name, const SpartaHandler & callback, const Clock * clk) :
        SingleTrigger(name, callback),
        event_("cycle_trigger_event", CREATE_SPARTA_HANDLER(CycleTrigger, invoke_), clk)
    {
    }

    /**
     * \brief Copy constructor
     */
    CycleTrigger(const CycleTrigger&) = delete;

    /**
     * \brief Not move-constructable
     */
    CycleTrigger(CycleTrigger&&) = delete;

    /**
     * \brief Not copy assignable
     */
    CycleTrigger& operator= (const CycleTrigger& rhp) = delete;

    //! \name Activation Control
    //! @{
    ////////////////////////////////////////////////////////////////////////

    /**
     * \brief disable any current action scheduled on this SingleTrigger.
     * Any previous scheduled event will be ignored.
     * You may not set the trigger again until a previous scheduled event
     * has passed though, even though it will now be ignored.
     */
    void deactivate() override
    {
        trigger_set_ = false;
    }

    /**
     * \brief Go ahead and set a pre-prepped state of the trigger.
     * At this time set() will schedule events. This means that if
     * the time from an absolute prep has already passed, the event
     * will never occur.
     * This also means that relative preps will be scheduled relatively
     * according to the time set() is called.
     *
     */
    void set() override
    {
        sparta_assert(!trigger_set_, "Trigger[ '" << getName()
                          << "']: cannot be already set, only prepped");
        sparta_assert((prepped_relative_ | prepped_absolute_), "Trigger[ '" << getName()
                          << "']: cannot set a trigger that has not been prepped first");

        sparta_assert((prepped_relative_ ^ prepped_absolute_), "Trigger[ '" << getName()
                          << "']: cannot prepRelative and prepAbsolute");

        set_();
    }

    bool isActive() const override
    {
        return trigger_set_;
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

    //! \name Configuration Interface
    //! @{
    ////////////////////////////////////////////////////////////////////////

    /**
     * \brief prep an event to happen at an absolute time
     * in the clock domain of clk.
     *
     * set() must be called after a prep call in order
     * for any events to occure.
     */
    void prepAbsolute(const Clock* clk, Clock::Cycle on_time) override
    {
        check_safe();
        clk_ = clk;
        schedule_for_cycle_ = on_time;
        prepped_absolute_ = true;
    }

    /**
     * \brief prep an event to occure relative to the time from
     * which set() is called. see set()
     *
     * set() must be called after a prep call in order for any
     * events to actually occure .
     */
    void prepRelative(const Clock* clk, Clock::Cycle rel) override
    {
        check_safe();
        clk_ = clk;
        relative_delay_ = rel;
        prepped_relative_ = true;
    }

    /**
     * \brief should we align when relatively scheduling
     * events such that each event occures on a
     * time % rel_time = 0 always when align = true;
     */
    void alignRelative(const bool align) override
    {
        check_safe();
        align_relative_ = align;
    }

    /**
     * \brief schedule an event that calls the callback at a particular
     * cycle. This is an absolute cycle in time, not relative to anything.
     * \param clk the clock domain to use during scheduling.
     * \param on_time the time used to schedule. This is an ABSOLUTE time.
     */
    void setAbsolute(const Clock* clk, Clock::Cycle on_time) override
    {
        check_safe();
        prepAbsolute(clk, on_time);
        set_();
    }

    /**
     * \brief set an event to occure in the relative future of the
     * current time.
     */
    void setRelative(const Clock* clk, Clock::Cycle delay) override
    {
        check_safe();
        prepRelative(clk, delay);
        set_();
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

    /**
     * \brief get the clock that was used for setting this trigger.
     */
    const Clock* getClock() const override
    {
        return clk_;
    }

private:

    /**
     * \brief make sure this trigger is in a state that is
     * legal to modify options or set state.
     */
    void check_safe()
    {
        sparta_assert(!(trigger_set_ | scheduled_), "Trigger ['" << getName() << "']:  Cannot modify the state of the trigger after it has already been set or scheduled.");
    }
    /**
     * \brief a method that is responsible for scheduling a callback for
     * this SingleTrigger. This can be called prescheduler finilization and
     * post finalization, and will still take appropriate action to make sure
     * that set events are schedule.
     */
    void set_()
    {
        trigger_set_ = true;
        auto scheduler = getScheduler();
        if(scheduler->isFinalized())
        {
            if(prepped_relative_){

                Clock::Cycle rel_time = relative_delay_;
                if(align_relative_){
                    rel_time = (rel_time - (clk_->currentCycle() % rel_time));
                }
                event_.schedule(rel_time, clk_);
            }
            else if(prepped_absolute_){
                event_.schedule(schedule_for_cycle_ - clk_->currentCycle(), clk_);
            }
            else{
                //This is not reachable.
                sparta_assert(false);
            }
            scheduled_ = true;
        }
        else {

            //schedule an event post dag that will do the scheduling.
            StartupEvent(scheduler,
                         CREATE_SPARTA_HANDLER(CycleTrigger, postDAGFinalized_));
        }

    }
    /**
     * \brief a method used as a callback to recall set_ post scheduler
     * finalization
     */
    void postDAGFinalized_()
    {
        set_();
    }

    /**
     * \brief a method used as a callback for the trigger. It calls the
     * users callback that was set in the constructor.
     */
    void invoke_()
    {
        if(trigger_set_)
        {
            trigger_set_ = false;
            invokeCallback_();
        }
        scheduled_ = false;

    }

    /** A schedulable event for our Trigger to call invoke_() */
    TriggerEvent event_;
    bool trigger_set_ = false;
    bool scheduled_ = false;
    bool prepped_relative_ = false;
    bool align_relative_ = false;
    bool prepped_absolute_ = false;
    /** The clock that will be used to schedule a trigger event.*/
    const Clock* clk_ = nullptr;
    /** The absolute cycle that this subtrigger should fire on */
    Clock::Cycle schedule_for_cycle_ = 0;
    Clock::Cycle relative_delay_ = 0;
};


/**
 * \brief A class that acts is responsible for firing an event after a fixed
 * amount of simulated time
 */
class TimeTrigger : public SingleTrigger
{
public:

    /**
     * \brief Not default-constructable
     */
    TimeTrigger() = delete;

    /**
     * \brief Construct with name and callback
     * \param name Name of the trigger
     * \param callback Callback handler for when this trigger is fired
     * \param picoseconds
     * \param clk Clock which this trigger aligns itself on (it will
     *        fire when the clock's scheduler hits the number of
     *        simulated picoseconds)
     */
    TimeTrigger(const std::string& name,
                const SpartaHandler & callback,
                uint64_t picoseconds,
                const Clock * clk = nullptr);

    /**
     * \brief Copy constructor
     */
    TimeTrigger(const TimeTrigger&) = delete;

    /**
     * \brief No move constructor
     */
    TimeTrigger(TimeTrigger&&) = delete;

    /**
     * \brief No copy assignment
     */
    TimeTrigger& operator= (const TimeTrigger& rhp) = delete;


    //! \name Activation Control
    //! @{
    ////////////////////////////////////////////////////////////////////////

    /**
     * \brief disable any current action scheduled on this SingleTrigger.
     * Any previous scheduled event will be ignored.
     * You may not set the trigger again until a previous scheduled event
     * has passed though, even though it will now be ignored.
     */
    void deactivate() override
    {
        trigger_set_ = false;
    }

    /**
     * \brief Go ahead and set a pre-prepped state of the trigger.
     * At this time set() will schedule events. This means that if
     * the time from an absolute prep has already passed, the event
     * will never occur.
     * This also means that relative preps will be scheduled relatively
     * according to the time set() is called.
     *
     */
    void set() override
    {
        sparta_assert(!trigger_set_, "Trigger[ '" << getName()
                          << "']:  Already set. Cannot set() again");

        set_();
    }

    bool isActive() const override
    {
        return trigger_set_;
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

    /**
     * \brief get the clock that was used for setting this trigger.
     */
    const Clock* getClock() const override
    {
        return clk_;
    }

private:

    /**
     * \brief make sure this trigger is in a state that is
     * legal to modify options or set state.
     */
    void check_safe()
    {
        sparta_assert(!(trigger_set_ | scheduled_),
                          "Trigger ['" << getName() << "']:  Cannot modify the state of the trigger "
                          " after it has already been set or scheduled.");
    }

    /**
     * \brief a method that is responsible for scheduling a callback for
     * this Trigger. This can be called prescheduler finilization and
     * post finalization, and will still take appropriate action to make sure
     * that set events are schedule.
     */
    void set_()
    {
        trigger_set_ = true;
        auto scheduler = getScheduler();
        if(scheduler->isFinalized())
        {
            // Schedule relative. Convert from PS to ticks
            uint64_t f = scheduler->getFrequency();
            uint64_t ps;
            if(f == PS_PER_SECOND){
                ps = schedule_for_ps_;
            }else if(f > PS_PER_SECOND){
                sparta_assert(f / float(PS_PER_SECOND) == static_cast<uint64_t>(f / PS_PER_SECOND),
                                  "Cannot schedule a picosecond trigger because the scheduler "
                                  "frequency is not an even multiple of picoseconds-per-second (or vise versa)");
                ps = scheduled_ * static_cast<uint64_t>(f / PS_PER_SECOND);
            }else{
                sparta_assert(float(PS_PER_SECOND) / f == static_cast<uint64_t>(PS_PER_SECOND / f),
                                  "Cannot schedule a picosecond trigger because the scheduler "
                                  "frequency is not an even divisor of picoseconds-per-second (or vise versa)");
                ps = scheduled_ * static_cast<uint64_t>(PS_PER_SECOND / f);
            }

            event_.scheduleRelativeTick(ps, scheduler);
            scheduled_ = true;
        }
        else {

            //schedule an event post dag that will do the scheduling.
            StartupEvent(scheduler,
                         CREATE_SPARTA_HANDLER(TimeTrigger, postDAGFinalized_));
        }
    }

    /**
     * \brief a method used as a callback to recall set_ post scheduler
     * finalization
     */
    void postDAGFinalized_()
    {
        set_();
    }

    /**
     * \brief a method used as a callback for the trigger. It calls the
     * users callback that was set in the constructor.
     */
    void invoke_()
    {
        if(trigger_set_)
        {
            trigger_set_ = false;
            invokeCallback_();
        }
        scheduled_ = false;

    }

    /** A schedulable event for our Trigger to call invoke_() */
    TriggerEvent event_;
    bool trigger_set_ = false;
    bool scheduled_ = false;

    /** Picosecond schedule time (relative) */
    uint64_t schedule_for_ps_;
    const Clock *const clk_;
};

    }//namespace trigger
}//namespace sparta

#endif //__SPARTA_SINLE_TRIGGER__
