
// <Scheduleable.h> -*- C++ -*-


#ifndef __SCHEDULEABLE_H__
#define __SCHEDULEABLE_H__

#include <cinttypes>

#include "sparta/simulation/Clock.hpp"
#include "sparta/kernel/SpartaHandler.hpp"

/**
 * \file   Scheduleable.hpp
 *
 * \brief  File that defines the Scheduleable class
 */

namespace sparta
{
    class ScheduleableHandle;
    class Scheduler;
    class Vertex;

    /**
     * \class Scheduleable
     *
     * \brief A class that defines the basic scheduling
     *        interface to the Scheduler.  Not intended to be used by
     *        modelers directly, but certainly can be.
     *
     * This class is used by Event, PayloadEvent, UniqueEvent, etc to
     * schedule an event on the SPARTA scheduler.  The main goal of this
     * class is to remain as light-weight as possible to allow a
     * developer to possibly copy derivatives of this class for
     * scheduling (like PayloadEvent proxy classes).
     */
    class Scheduleable
    {

    public:
        //! Typedef defining the precedence group ID
        typedef uint32_t PrecedenceGroup;

        static const PrecedenceGroup INVALID_GROUP;

        /**
         * \brief Construct a Scheduleable object
         *
         * \param consumer_event_handler The scheduled callback placed on the Scheduler
         * \param clk   Clock used if none provided
         * \param delay Any delay of scheduling
         * \param scheduler Pointer to the scheduler to used -- must NOT be nullptr
         */
        Scheduleable(const SpartaHandler & consumer_event_handler,
                     Clock::Cycle delay, SchedulingPhase sched_phase);

        /**
         * \brief Destructor
         */
        virtual ~Scheduleable() = default;

        //! Allow copies
        Scheduleable(const Scheduleable &) = default;

        //! Allow moves.  Marked as noexcept to force containers to use it
        Scheduleable(Scheduleable &&) noexcept = default;

        //! Get the scheduler this Scheduleable is assigned to
        Scheduler * getScheduler(const bool must_exist = true) {
            sparta_assert(scheduler_ || !must_exist);
            return scheduler_;
        }

        //! Get the scheduler this Scheduleable is assigned to
        const Scheduler * getScheduler(const bool must_exist = true) const {
            sparta_assert(scheduler_ || !must_exist);
            return scheduler_;
        }

        /**
         * \brief Set a fixed delay for this event
         * \param delay The clock cycle delay
         */
        void setDelay(Clock::Cycle delay) {
            delay_ = delay;
        }

        /**
         * \brief Add to the delay for this event
         * \param delay The additional clock cycle delay
         */
        void addDelay(Clock::Cycle delay) {
            delay_ += delay;
        }

        /**
         * \brief Get the delay associated with this event
         * \return The delay associated with this event
         */
        Clock::Cycle getDelay() const {
            return delay_;
        }

        /**
         * \brief This event, if continuing == true, will keep the
         * simulation running
         *
         * If this event is NOT a continuing event, just simply
         * scheduling it will NOT keep simulation running.  This is
         * useful for events like heartbeats, etc.
         */
        void setContinuing(bool continuing) {
            continuing_ = continuing;
        }

        /**
         * \brief Is this Event continuing?
         * \return true if it will keep the scheduler alive
         */
        bool isContinuing() const {
            return continuing_;
        }

        /**
         * \brief Get the consumer handler/callback associated with this event
         * \return Reference to the handler (const)
         */
        const SpartaHandler & getHandler() const {
            return consumer_event_handler_;
        }

        /**
         * \brief Get the consumer handler/callback associated with this event
         * \return Reference to the handler (non-const)
         */
        SpartaHandler & getHandler() {
            return consumer_event_handler_;
        }

        /**
         * \brief Set the consumer handler/callback associated with this event
         * \param handler Reference to the new handler
         */
        void setHandler(const SpartaHandler & handler) {
            consumer_event_handler_ = handler;
            setLabel(consumer_event_handler_.getName());
        }

        /**
         * \brief Schedule this event with its pre-set delay using the
         * pre-set Clock
         */
        void schedule()
        {
            this->schedule(delay_, local_clk_);
        }

        /**
         * \brief Schedule this event with its pre-set delay using the
         * given clock
         * \param clk Pointer to the clock to use
         */
        void schedule(const Clock * clk)
        {
            schedule(delay_, clk);
        }

        /**
         * \brief Schedule an event in the future using the pre-set Clock
         * \param delay The relative time (in Clock::Cycle) to
         *              schedule from "now"
         */
        void schedule(Clock::Cycle delay)
        {
            schedule(delay, local_clk_);
        }

        /**
         * \brief Schedule this event on a relative scheduler tick
         * \param rel_tick A relative Scheduler::Tick in the future
         *
         * This method is typically \b not used in user-end models.
         * Use the schedule methods instead.
         */
        virtual void scheduleRelativeTick(const Scheduler::Tick rel_tick,
                                          Scheduler * const scheduler)
        {
            sparta_assert(scheduler != nullptr);
            scheduler->scheduleEvent(this, rel_tick, pgid_,
                                     continuing_);
        }

        /**
         * \brief Schedule an event in the future using the given Clock
         * \param delay The relative time (in Ticks) to schedule from "now"
         * \param clk   The clock this Event will use to schedule itself
         */
        void schedule(Clock::Cycle delay, const Clock *clk)
        {
            sparta_assert(clk != nullptr);
            this->scheduleRelativeTick(clk->getTick(delay),
                                       clk->getScheduler());
        }

        /*! \brief Return true if this scheduleable was scheduled at all
         * \return true if scheduled at all
         *
         * This is an expensive call as it searches all time quantums
         * for instances of this Scheduleable object.  Use with care.
         */
        bool isScheduled() const {
            return scheduler_->isScheduled(this);
        }

        /*! \brief Return true if this scheduleable is not associated
         *   with a vertex
         */
        bool isOrphan() const;

        //! \brief Return true if this Scheduleable was scheduled on the
        //!  given relative cycle
        //! \param rel_cycle The relative clock cycle to check
        bool isScheduled(Clock::Cycle rel_cycle) const {
            Scheduler::Tick rel_tick = local_clk_->getTick(rel_cycle);
            return scheduler_->isScheduled(this, rel_tick);
        }

        //! Get the internal phase number
        SchedulingPhase getSchedulingPhase() const {
            return sched_phase_;
        }

        //! Get the internal label
        const char * getLabel() const {
            return label_.c_str();
        }

        //! Set a new label for this Scheduleable -- used in debugging.
        void setLabel(const char * label);

        /*! \brief get the internal Vertex of this scheduleable
         */
        Vertex * getVertex(){
            return vertex_;
        }

        /**
         * \brief Have this Scheduleable precede another
         * \param consumer The Scheduleable to follow this Scheduleable
         * \param reason The reason for the precedence
         *
         *  \a this will preceed, or come before, the consumer
         *
         */
        void precedes(Scheduleable & consumer, const std::string & reason = "");

        /**
         * \brief Have this Scheduleable precede a Vertex
         * \param consumer The Vertex to follow this Scheduleable
         * \param reason The reason for the precedence
         *
         *  \a this will preceed, or come before, the consumer
         *
         */
        void precedes(Vertex & consumer, const std::string & reason = "") const;

        /**
         * \brief Have this Scheduleablee precede another
         * \param consumer The Scheduleable to follow this Scheduleable
         * \param reason The reason for the precedence
         *
         *  \a this will preceed, or come before, the consumer
         *
         */
        void precedes(Scheduleable * consumer, const std::string & reason = "") {
            this->precedes(*consumer, reason);
        }

        /**
         * \brief Have this Scheduleable precede a Vertex
         * \param consumer The Vertex to follow this Scheduleable
         * \param reason The reason for the precedence
         *
         *  \a this will preceed, or come before, the consumer
         *
         */
        void precedes(Vertex * consumer, const std::string & reason = "") const{
            this->precedes(*consumer, reason);
        }

        /*! \param gop Whether this Scheduleable is a GOP or not.
         * A Scheduleable is never a GOP, this is a hack to get
         * around code in Unit.cpp.
         * TODO: elminate Unit.cpp code that used this.
         */
        void setGOP(bool gop) {
            is_gop_ = gop;
        }

        /*! \param gid Internal Group ID of this Scheduleable.
         */
        void setGroupID(const PrecedenceGroup gid) {
            pgid_ = gid;
        }

        /**
         * \brief Unlink this scheduleables vertex from another
         *  scheduleables vertex.
         */
        bool unlink(Scheduleable *w);

        /**
         * \brief Get the group ID
         * \return The group ID
         */
        PrecedenceGroup getGroupID() const {
            return pgid_;
        }

        /*!
         * \brief Cancel all the times that this Scheduleable was placed
         *        on the Scheduler
         */
        void cancel() {
            scheduler_->cancelEvent(this);
            eventCancelled_();
        }

        /*!
         * \brief Cancel this Scheduleable at the given time, if
         *        placed on the Scheduler
         * \param rel_cycle The relative time to look for the event
         *
         * This will cancel all instances of this event at the given time.
         */
        void cancel(Clock::Cycle rel_cycle) {
            Scheduler::Tick rel_tick = local_clk_->getTick(rel_cycle);
            scheduler_->cancelEvent(this, rel_tick);

            // If the event was scheduled at the given time, it will
            // be cancelled by the Scheduler.  We don't explicitly
            // cancel this here.
        }


        /*!
         * \brief Set the clock and scheduler of this Scheduleable
         * \param clck The clock to be associated with this Scheduleable
         *
         */
        void setScheduleableClock(const Clock * clk) {
            sparta_assert(clk != nullptr);
            local_clk_ = clk;
            scheduler_ = clk->getScheduler();
        }

        /*!
         * \brief Set the Scheduler of this Scheduleable, and set the local
         * vertex_ to a new vertex from the Vertex Factory
         * \param sched The Scheduler  to be associated with this Scheduleable
         *
         */
        void setScheduler(Scheduler * sched){
            scheduler_ = sched;
        }

        /*!
         * \brief Set the local vertex_ to a new vertex from the Vertex Factory.
         * This needs to happen before the Scheduleable goes through the DAGs
         * linking process in onSchedulerAssignment
         * \param sched The Scheduler  to be associated with this Scheduleable
         *
         */
        void setVertex();

#ifndef DO_NOT_DOCUMENT

        virtual void onSchedulerAssignment_() {
            setupDummyPrecedence_ThisMethodToGoAwayOnceDaveAddsPhaseSupportToDAG();
        }
        void setupDummyPrecedence_ThisMethodToGoAwayOnceDaveAddsPhaseSupportToDAG();

#endif

    protected:
        //! The Consumer callback registered with the Event
        SpartaHandler         consumer_event_handler_;

        //! A local clock for speed
        const Clock * local_clk_ = nullptr;

        //! Return the number of outstanding handles pointing to this
        //! Scheduleable
        uint32_t getScheduleableHandleCount_() const {
            return scheduleable_handle_count_;
        }

        /**
         * \brief Set the group ID of this Scheduleable's derivatives
         * \param gid The group ID within the DAG this event should
         *            use
         *
         * Called by the DAG when setting up precedence.
         */
        virtual void setGroupID_(const PrecedenceGroup gid) {
            pgid_ = gid;
        }

        // Allow the Scheduler to cancel the event
        friend class Scheduler;

        //! Called by the Scheduler to let this Scheduleable know it
        //! was canceled.
        virtual void eventCancelled_() {}

        /**
         * \class PrecedenceSetup
         *
         * \brief An internal class used in Scheduleble to cache a pointer to
         *        the scheduler
         *
         * This class is used by Scheduleable objects
         */
        class PrecedenceSetup {
        public:
            explicit PrecedenceSetup(Scheduleable * scheduleable) :
                scheduleable_(scheduleable)
            {}

            PrecedenceSetup & operator=(Scheduler * scheduler);

            Scheduler * operator->() {
                return scheduler_;
            }

            const Scheduler * operator->() const {
                return scheduler_;
            }

            operator Scheduler*() {
                return scheduler_;
            }

            operator const Scheduler*() const {
                return scheduler_;
            }

            operator bool() const {
                return scheduler_ != nullptr;
            }

            bool equals(const PrecedenceSetup & other) const {
                return scheduler_ == other.scheduler_;
            }

        private:
            Scheduleable * scheduleable_ = nullptr;
            Scheduler    * scheduler_ = nullptr;
        };//End class PrecedenceSetup

        //! Cache a pointer to the scheduler used
        PrecedenceSetup scheduler_{this};

    private:
        friend class DAG;

        friend class Unit; // temporary

        friend class ScheduleableHandle;

        //! Internal Vertex this Scheduleable is associated with
        //! Vertex used to connect to DAG elements (GOPs aka Vertices,
        //! or other Scheduleables)
        Vertex * vertex_ = nullptr;

        //! Internal label for this Scheduleable
        std::string label_;

        //! The group ID assigned to this Scheduleable -- used by the scheduler
        PrecedenceGroup pgid_ = 0;

        //! Is this a GOP? -if yes then it transferGID().
        // TODO: elminate Unit.cpp code that used this.
        bool is_gop_ = false;

        //! Counter for the number of Handles
        mutable uint32_t scheduleable_handle_count_ = 0;

        //! Method called by the ScheduleableHandle to reclaim this
        //! Scheduleable.  Used by PayloadEvent's internal proxy
        //! mostly...
        virtual void reclaim_() {}

        //! A pre-set delay for this event
        Clock::Cycle        delay_;

        //! Internal representation of the phase number this
        //! Scheduleable belong to
        const SchedulingPhase sched_phase_ = SchedulingPhase::Invalid;

        //! Is this event continuing? ... meaning it should keep the
        //! Scheduler alive even if the Scheduler has no other events
        //! to do.  Default is true, meaning it should keep simulation
        //! going.
        bool continuing_ = true;

    };//End class Scheduleable


    /*!
     * \class ScheduleableHandle
     * \brief A light-weight reference counting handle for Scheduleables -- DOES NOT delete
     *
     * Mostly used by PayloadEvent and Audience, this
     * class allows a user to pass a Scheduleable pointer to another
     * object without that object being concerned about ownership.
     *
     * \note This class DOES NOT handle memory management, i.e. it
     *       will \b not delete any Scheduleable type that it points
     *       to -- it will simply tell the object to reclaim itself.
     */
    class ScheduleableHandle
    {
    private:
        //! Disconnect
        void disconnect_() {
            if(scheduleable_ && --scheduleable_->scheduleable_handle_count_ == 0) {
                scheduleable_->reclaim_();
            }
        }

        //! Connect
        void connect_() {
            if(scheduleable_) {
                ++scheduleable_->scheduleable_handle_count_;
            }
        }

    public:
        //! Create an empty Handle
        ScheduleableHandle() = default;

        //! Copy a Handle, increment the count
        ScheduleableHandle(const ScheduleableHandle & orig)
        {
            disconnect_();
            scheduleable_ = orig.scheduleable_;
            connect_();
        }

        /*!
         * \brief Create a ScheduleableHandle with a given
         *        Scheduleable pointer. NO reference counting is
         *        performed with this constructor.
         * \param scheduleable Pointer to the scheduleable to handle
         */
        ScheduleableHandle(Scheduleable * scheduleable) :
            scheduleable_(scheduleable)
        {
            connect_();
        }

        /*!
         * \brief Create a ScheduleableHandle with a given
         *        Scheduleable reference. NO reference counting is
         *        performed with this constructor.
         * \param scheduleable Pointer to the scheduleable to handle
         */
        ScheduleableHandle(Scheduleable & scheduleable) :
            scheduleable_(&scheduleable)
        {
            connect_();
        }

        //! Reclaim the Scheduleable if nothing is pointing to it
        //! anymore.  If the Scheduleable was created an alloc'ed
        //! Scheduleable, it will be deleted; otherwise, it will be
        //! reclaimed (as in PayloadEvent)
        ~ScheduleableHandle()
        {
            disconnect_();
        }

        //! Get at the underlying Scheduleable
        Scheduleable * operator->() const {
            return scheduleable_;
        }

        //! Is this ScheduleableHandle equivalent to another (used by
        //! Audience)
        bool operator==(const ScheduleableHandle & rhs) const {
            return (scheduleable_ == rhs.scheduleable_);
        }

        //! Is this ScheduleableHandle not equivalent to another (used
        //! by Audience)
        bool operator!=(const ScheduleableHandle & rhs) const {
            return !(scheduleable_ == rhs.scheduleable_);
        }

        //! Assignment
        ScheduleableHandle & operator=(const ScheduleableHandle & rhs) {
            disconnect_();
            scheduleable_ = rhs.scheduleable_;
            connect_();
            return *this;
        }

    private:
        Scheduleable *scheduleable_ = nullptr;
    };//End class ScheduleableHandle


    // Scheduleable >> Scheduleable
    inline Scheduleable & operator>>(Scheduleable & producer, Scheduleable & consumer)
    {
        sparta_assert(producer.getSchedulingPhase() == consumer.getSchedulingPhase(),
                    "The Producer: " << producer.getLabel() << " scheduling phase is not equal to"
                    " the consumer: " << consumer.getLabel());
        producer.precedes(consumer);
        return consumer;
    }

}

#endif
