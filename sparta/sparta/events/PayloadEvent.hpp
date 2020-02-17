// <PayloadEvent.h> -*- C++ -*-


/**
 * \file   PayloadEvent.hpp
 *
 * \brief  File that defines the PayloadEvent class
 */

#ifndef __PAYLOAD_EVENT_H__
#define __PAYLOAD_EVENT_H__

#include <set>
#include <memory>
#include "sparta/events/PhasedPayloadEvent.hpp"

namespace sparta
{

    /**
     * \class PayloadEvent
     *
     * \brief Class to schedule a Scheduleable in the future with a
     *        payload, typed on both the data type and the scheduling
     *        phase.  This is the preferred class to use in
     *        simulation, but the main API is found in
     *        sparta::PhasedPayloadEvent
     *
     * \tparam DataT         The payload's data type
     * \tparam sched_phase_T The SchedulingPhase this PayloadEvent belongs to
     *
     * PayloadEvent class objects are used to schedule a payload
     * delivery to another component either in the present or the
     * future.  This is used for the fire-and-forget concept where the
     * producer sends data to consumer (for the future) and never
     * maintains that data again.  Note that the data \a must be
     * copyable or movable \b default constructable.  Typical payloads
     * are std::shared_ptr, integer, or sparta::SpartaSharedPointer
     * object types.
     *
     * Scheduling this event is different from the Scheduleable type.
     * First of all, the PayloadEvent class is not scheduleable
     * itself.  Instead, a user must \i prepare a Scheduleable with a
     * payload, then it can schedule that Scheduleable instance.
     *
     * Some quick notes: PayloadEvent class types must be created with
     * an EventSet.  This EventSet must include a clock for
     * scheduling.
     *
     * \code
     *
     *  ////////////////////////////////////////////////////////////
     *  // Event Creation
     *  //
     *  sparta::EventSet event_set(parent_tree_node_with_clock);
     *  // event_set.setClock(my_own_clock); // if different from parent node's Clock
     *
     *  sparta::PayloadEvent<uint32_t, sparta::SchedulingPhase::Tick>
     *      pevent(&event_set, "pevent", CREATE_SPARTA_HANDLER_WITH_DATA(MyClass, MyMethod, uint32_t), a_delay);
     *
     *  ////////////////////////////////////////////////////////////
     *
     *  ////////////////////////////////////////////////////////////
     *  // Event Typical Use
     *  //
     *  // Schedule an_int_value to be delivered a_delay from now.
     *  pevent.preparePayload(an_int_value)->schedule();
     *
     *  ////////////////////////////////////////////////////////////
     *
     *  ////////////////////////////////////////////////////////////
     *  // Event Alternate Uses
     *  //
     *  // Schedule an_int_value to be delivered a_different_delay
     *  // from now with a_different_clock.
     *  pevent.preparePayload(an_int_value)->schedule(&a_different_clock, a_different_delay);
     *
     *  // Create a handle to the Scheduleable for later delivery:
     *  sparta::ScheduleableHandle my_prepared_payload = pevent.preparePayload(an_int_value);
     *
     *  // ... later on
     *  my_prepared_payload.schedule();
     *
     * \endcode
     */
    template<class DataT, SchedulingPhase sched_phase_T = SchedulingPhase::Tick>
    class PayloadEvent : public PhasedPayloadEvent<DataT>
    {
    public:
        // The phase this Event was defined with
        static constexpr SchedulingPhase event_phase = sched_phase_T;

    public:

        /**
         * \brief Create an PayloadEvent base to generate PayloadEvent Scheduleables
         * \param event_set The sparta::EventSet this PayloadEvent belongs to
         * \param name      The name of this event (as it shows in the EventSet)
         * \param consumer_event_handler A SpartaHandler to the consumer's event_handler
         * \param delay The relative time (in Cycles) from "now" to schedule
         *
         * Create a PayloadEvent that can be used to schedule objects
         * of DataT now or in the future to the
         * consumer_event_handler.
         */
        PayloadEvent(TreeNode * event_set,
                     const std::string & name,
                     const SpartaHandler & consumer_event_handler,
                     Clock::Cycle delay = 0) :
            PhasedPayloadEvent<DataT>(event_set, name, sched_phase_T,
                                      consumer_event_handler, delay)
        { }

        //! Destroy!
        virtual ~PayloadEvent() = default;

        //! No assignments, no copies
        PayloadEvent<DataT, sched_phase_T> &
        operator=(const PayloadEvent<DataT, sched_phase_T> &) = delete;

        //! No moves
        PayloadEvent(PayloadEvent<DataT, sched_phase_T> &&) = delete;

        //! No assignments, no copies
        PayloadEvent(const PayloadEvent<DataT, sched_phase_T> &) = delete;
    };

}


// __PAYLOAD_EVENT_H__
#endif
