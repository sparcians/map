// <Trigger.hpp> -*- C++ -*-

/**
 * \file Trigger.hpp
 * \brief Class used to "trigger" one time functionality
 */
#ifndef __SPARTA_TRIGGER_H__
#define __SPARTA_TRIGGER_H__

#include "sparta/trigger/Triggerable.hpp"
#include "sparta/trigger/SingleTrigger.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/events/Event.hpp"
#include "sparta/utils/Colors.hpp"
#include <array>

namespace sparta
{
    namespace trigger
    {

/**
 * \class Trigger
 * \brief A class that can be used to set scheduled callback
 *        events, such as an event to trigger logging or pipeline collection
 *        on the scheduler.
 *
 */
class Trigger
{
    /**
     * \brief an enumeration used for indexing into an array of
     * SingleTriggers that are manipulated to make this Trigger function.
     */
    enum SubTriggers {
        START,
        STOP,
        REPEAT
    };


public:
    Trigger(const std::string& name, const Clock * clk = nullptr) :
        name_(name)
    {
        // No initial start trigger
        triggers_[STOP]  .reset(new CycleTrigger(name, CREATE_SPARTA_HANDLER(Trigger, onStopTrigger_), clk));
        triggers_[REPEAT].reset(new CycleTrigger(name, CREATE_SPARTA_HANDLER(Trigger, onRepeatTrigger_), clk));
    }

    ~Trigger(){}

    /**
     * \brief set some alignment options used during periodic trigger rescheduling.
     *
     * \param align_period when the value is true this trigger will trigger ONLY
     * at modulo times of the period length, An example is that a trigger that starts
     * on cycle 500 but has a period of 1000 will only fire on 1000, 2000, etc when true.
     * If false it will fire on 1500, 2500, 3500 etc.
     */
    void setPeriodAlignmentOptions(bool align_period)
    {
        check_safe_(1);
        aligned_period_ = align_period;
        //Tell our repeat trigger to align itself.
        triggers_[REPEAT]->alignRelative(align_period);
    }

    /**
     * \brief set the trigger with an absolute desired start time based off a particular clock.
     * \param on_clock the clock used for scheduling the start event
     * \param on_time the absolute cycle that you'd like the trigger to fire a start() event.
     */
    void setTriggerStartAbsolute(Clock* on_clock, Clock::Cycle on_time)
    {
        check_safe_(on_time);
        triggers_[START].reset(new CycleTrigger(name_ + "_start", CREATE_SPARTA_HANDLER(Trigger, onStartTrigger_), on_clock));
        triggers_[START]->setAbsolute(on_clock, on_time);
    }

    /**
     * \brief set the trigger with an absolute desired start time from a particular counter
     * \param crr Counter to observe
     * \param val Value to trigger at
     */
    void setTriggerStartAbsolute(const CounterBase* ctr, Counter::counter_type val)
    {
        sparta_assert(ctr, "Cannot setTriggerStartAbsolute on Trigger \"" << name_
                               << "\" with null ctr");
        check_safe_(val);
        triggers_[START].reset(new CounterTrigger(name_ + "_start",
                                                  CREATE_SPARTA_HANDLER(Trigger, onStartTrigger_),
                                                  ctr,
                                                  val
                                                  ));
    }

    /**
     * \brief set the trigger to start at a relative time IN THE FUTURE based off the clock on_clock,
     * this is NOT some sort of triggering relative to another trigger event, only relative to the current time.
     * \param on_clock the clock used for scheduling.
     * \param on_rel the relative number of ticks in the future you'd like to fire the start() event.
     */
    void setTriggerStartRelative(Clock* on_clock, Clock::Cycle on_rel)
    {
        setTriggerStartAbsolute(on_clock, on_rel + on_clock->currentCycle());
    }

    /**
     * \brief set the absolute Cycle that this Trigger should fire a stop event.
     * \param off_clock the clock domain to schedule the stop event.
     * \param off_time the absolute time in ticks to stop the trigger.
     *
     * \note Stop events are always scheduled at the time the start event fires.
     * This means that if the stop's time is absolutely before the start, the stop
     * will be ignored essentially.
     */
    void setTriggerStopAbsolute(Clock* off_clock, Clock::Cycle off_time)
    {
        check_safe_(off_time);
        triggers_[STOP]->prepAbsolute(off_clock, off_time);
        prepped_stop_ = true;
        //We do not set the stop trigger until after the start trigger has fired, at that time we will schedule
        //a stop, so we only cached the details.
    }

    /**
     * \brief schedules a stop event relative to the CURRENT time based off the clock off_rel.
     * This is not triggering relative to another trigger event such as triggering relative to
     * the trigger's start.
     * \param off_clock the clock that is used to schedule.
     * \param off_rel the relative cycles in the future that the stop event should fire.
     */
    void setTriggerStopRelative(Clock* off_clock, Clock::Cycle off_rel)
    {
        setTriggerStopAbsolute(off_clock, off_rel + off_clock->currentCycle());
    }

    /**
     * \brief this method is going to schedule a stop event off_rel cycles
     * AFTER A START EVENT occures in the domain of off_clock.
     */
    void setTriggerStopRelativeToStart(Clock* off_clock, Clock::Cycle off_rel)
    {
        triggers_[STOP]->prepRelative(off_clock, off_rel);
        prepped_stop_ = true;
    }


    /**
     * \brief set this trigger to fire on a given period.
     * \param repeat_clk this is the clock used to schedule the repeats.
     * \param period the amount of time to span before firing the next start.
     *
     * \note see setAlignmentOptions() for more notes on how periods can affect
     * the trigger. These options should be set to determine whether or not
     * the trigger reaccures time % period = 0 or not. As well as options
     * for whether or not to fire with the same modulo prioprities around
     * the start and stop of the trigger.
     *
     * Repeats call a repeat() callback on Triggerable objects.
     * \warning A repeat may OR may NOT be called if a repeat is
     * expected to occure the same cycle as the stop event. Whether or
     * not a repeat will fire the same cycle as a stop is undefined.
     */
    void setRecurring(Clock* repeat_clk, const Clock::Cycle period)
    {
        check_safe_(period);
        triggers_[REPEAT]->prepRelative(repeat_clk, period);
        repeating_ = true;
        // Reoccuring events are scheduled by the callbacks
        // onStartTrigger_ and onRepeatTrigger_;
    }

    /**
     * \brief add another Triggerable object to be managed by
     * this Trigger. All Triggerable objects will have their
     * virtual go method called at the scheduled trigger
     * time.
     */
    void addTriggeredObject(Triggerable* triggered_item)
    {
        sparta_assert(triggered_item != nullptr);
        check_safe_(1);
        triggered_objs_.push_back(triggered_item);
    }

    /**
     * \brief print out the details about this trigger.
     */
    void print(std::ostream& o) const
    {

        o << " ** " << name_ << " ** \n" ;
        o << "Trigger's options: \n";
        o << "period_aligned = " << std::boolalpha << aligned_period_ << "\n";
        o << "Trigger details: \n";
        //print the details of which triggers are set to fire.
        if(triggers_[START]){
            o << "Start trigger using clock: " << triggers_[START]->getClock() << std::endl;
        }else{
            o << "No start trigger!" << std::endl;
        }
        if(prepped_stop_){
            o << "Stop trigger using clock : " << triggers_[STOP]->getClock() << std::endl;
        }
        if(repeating_) {
            o << "repeat trigger using clock : " << triggers_[REPEAT]->getClock() << std::endl;
        }
    }

    /**
     * \brief calling this method will make sure that the trigger
     * does not reschedule indefinetly
     */
    void forceStopRecurring(){
        triggers_[REPEAT]->deactivate();
    }
private:
    /**
     * \brief check if it is safe to modify the details of the trigger.
     * This means that the trigger has not yet begun, the first start event has not yet
     * fired.
     */
    void check_safe_(const uint64_t time)
    {
        sparta_assert(time > 0, "Trigger [ '" << name_ << "']: Cannot set start, stop, or period to a value of zero");
        sparta_assert(!started_,
                          "Trigger [ '" << name_
                          << "']: Cannot modify state after the fire event has already occured");
    }

    /**
     * \brief set an event for the stop if required.
     * this method is called by the start callback, see onStartTrigger_()
     */
    void set_stops_()
    {
        triggers_[STOP]->set();
    }
    /**
     * \brief set an event for the next repeat if required.
     * this method is called by the start callback, see onStartTrigger_()
     */
    void set_repeats_()
    {
        triggers_[REPEAT]->set();
    }


    /**
     * \brief call the go callback on all of our triggerable objects.
     *
     */
    void onStartTrigger_()
    {
        // Should not be able to get to this callback with no start trigger instantiated
        sparta_assert(triggers_[START]);
        auto scheduler = triggers_[START]->getScheduler();
        sparta_assert(scheduler);

        // print out a note about what's happening
        std::cout << SPARTA_CURRENT_COLOR_GREEN << " ->" << name_
                  << " Trigger is turning debug tools "
                  << SPARTA_CURRENT_COLOR_BOLD << "ON" << SPARTA_CURRENT_COLOR_NORMAL<< ".\n";
        std::cout << "  >using clock: " << triggers_[START]->getClock()
                  << "\n  >Current cycle: " << triggers_[START]->getClock()->currentCycle()
                  << " Tick: " << scheduler->getCurrentTick() << std::endl;
        std::cout << SPARTA_CURRENT_COLOR_NORMAL << std::endl;
        for(Triggerable* obj : triggered_objs_)
        {
            obj->go();
        }

        //schedule stop events
        if(prepped_stop_){
            set_stops_();
        }
        //schedule repeat events
        if(repeating_){
            set_repeats_();
        }
        // We can no longer modify the options/set options of the
        // trigger.  In the future we may want to allow modifying
        // trigger state from Triggerable callbacks.
        started_ = true;

    }

    /**
     * \brief a callback that calls the repeat callback on all our triggeralbe objects.
     *
     * This method is also responsible for scheduling a future repeat if necessary.
    */
    void onRepeatTrigger_()
    {
        //print out a note about what's happening
        std::cout << SPARTA_CURRENT_COLOR_GREEN << " ->" << name_ << " Trigger is firing debug tools " << SPARTA_CURRENT_COLOR_BOLD  << "REPEAT" << SPARTA_CURRENT_COLOR_NORMAL << ".\n";
        std::cout << "  >using clock: " << triggers_[REPEAT]->getClock() << "\n  >Current cycle: " << triggers_[REPEAT]->getClock()->currentCycle() << std::endl;
        std::cout << SPARTA_CURRENT_COLOR_NORMAL << std::endl;
        for(Triggerable* obj : triggered_objs_)
        {
            obj->repeat();
        }

        set_repeats_();
    }

    /**
     * \brief call the stop() callback on all of our triggerable objects.
     */
    void onStopTrigger_()
    {
        //print out a note about what's happening
        std::cout << SPARTA_CURRENT_COLOR_GREEN << " ->" << name_ << " Trigger is turning debug tools " << SPARTA_CURRENT_COLOR_BOLD  << "OFF" << SPARTA_CURRENT_COLOR_NORMAL << ".\n";
        std::cout << "  >using clock: " << triggers_[STOP]->getClock() << "\n  >Current cycle: " << triggers_[STOP]->getClock()->currentCycle() << std::endl;
        std::cout << SPARTA_CURRENT_COLOR_NORMAL << std::endl;
        for(Triggerable* obj : triggered_objs_)
        {
            obj->stop();
        }
        //We deactivate any repeat triggers from accuring after this stop.
        triggers_[REPEAT]->deactivate();
    }

    std::vector<Triggerable*> triggered_objs_;
    /*! The name of the trigger */
    std::string name_;

    //////! Various options for running self rescheduling events.
    bool aligned_period_ = true;

    /*!
     * \brief Keep an array of all the TriggerDetails for each a start, stop,
     * and repeat
     */
    std::array<std::unique_ptr<SingleTrigger>, 3> triggers_;

    /*!
     * \brief Has the start callback been fired yet
     */
    bool started_ = false;

    /*!
     * Has the user called setRecurring
     * \todo Use existance of REPEAT trigger instead
     */

    bool repeating_ = false;
    /*!
     * \brief Has the user prepped a stop event
     * \todo Use existance of STOP trigger instead
     */
    bool prepped_stop_ = false;

};


inline std::ostream& operator<<(std::ostream& os, const Trigger& trigger)
{
    trigger.print(os);
    return os;
}

    }//namespace trigger
}//namespace sparta

#endif //__SPARTA_TRIGGER_H__
