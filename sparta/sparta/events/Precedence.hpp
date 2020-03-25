// <Precedence.hpp> -*- C++ -*-


#ifndef __PRECEDENCE__H__
#define __PRECEDENCE__H__

#include <type_traits>

#include "sparta/utils/MetaStructs.hpp"
#include "sparta/events/Scheduleable.hpp"
#include "sparta/events/Event.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/events/PayloadEvent.hpp"
#include "sparta/events/SingleCycleUniqueEvent.hpp"
#include "sparta/ports/Port.hpp"
#include "sparta/ports/Bus.hpp"
#include "sparta/kernel/Vertex.hpp"

/**
 * \file   Precedence.hpp
 *
 * \brief File that defines Precedence operator>> rules between
 *        EventNode types
 *
 */
namespace sparta
{
    ////////////////////////////////////////////////////////////////////////////////
    /*! \page precedence_rules Precedence operators for EventNode/Scheduleables
     *
     * Precedence operators, since folks use unique_ptrs, pointers,
     * GOPoints, references, etc..  The compilation errors get pretty
     * nasty if a simple operator isn't found.
     *
     * Some combinations:
     * \code
     *    // Easy
     *    concrete_event >> concrete_event;
     *
     *    // Pointers
     *    event_pointer >> concrete_event;
     *    concrete_event >> event_pointer;
     *    event_pointer >> event_pointer;
     *
     *    // Unique Ptrs
     *    std::unique_ptr<eventT> >> std::unique_ptr<eventT>;
     *    std::unique_ptr<eventT> >> concrete_event;
     *    std::unique_ptr<eventT> >> event_pointer;
     *    concrete_event >> std::unique_ptr<eventT>;
     *    event_pointer >> std::unique_ptr<eventT>;
     *
     *    // GOP
     *    gop >> concrete_event;
     *    gop >> event_pointer;
     *    gop >> std::unique_ptr<eventT>;
     *    concrete_event >> gop;
     *    event_pointer >> gop;
     *    std::unique_ptr<eventT> >> gop;
     * \endcode
     */
    ////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////
    // GOP -> EventNode types
    ////////////////////////////////////////////////////////////////////////////////

    /**
     * \brief Place a precedence between a Scheduleable object and a DAG::GOPoint
     *
     * \param producer The producer (LHS) must come before consumer (RHS)
     * \param consumer The DAG to succeed the ScheduleableType
     *
     * \return The \i consumer
     *
     * \note This function returns the \i consumer (RHS) of the
     *       operation instead of the \i producer (LHS).  This is
     *       opposite of typical behavior of >> operators.
     *
     * Place a precedence on a producer to the consumer.  The consumer
     * is returned, \b not the producer.  This is to allow chaining:
     *
     * \code
     *    my_producer_event_ >> my_consumer_event_ >> my_event_following_consumption_;
     * \endcode
     *
     * This implementation of the precedence operator catches a
     * Scheduleable on the LHS preceding a DAG point (which is not a
     * Scheduleable type) and ensures the two events are within the
     * same phase.
     *
     * \code
     * (UniqueEvent, PayloadEvent, SingleCycleUniqueEvent, Event) >> GOPoint;
     * \endcode
     */
    template<class ScheduleableTypeA>
    typename std::enable_if<std::is_base_of<EventNode, ScheduleableTypeA>::value, Vertex>::
    type & operator>>(ScheduleableTypeA & producer, Vertex & consumer)
    {
        producer.getScheduleable().precedes(consumer);
        return consumer;
    }

     /**
     * \brief Place a precedence between a Scheduleable object and a DAG::GOPoint
     *
     * \param producer The DAG to succeed the ScheduleableType
     * \param consumer The producer (LHS) must come before consumer (RHS)
     *
     * \return The \i consumer
     *
     * \note This function returns the \i consumer (RHS) of the
     *       operation instead of the \i producer (LHS).  This is
     *       opposite of typical behavior of >> operators.
     *
     * Place a precedence on a producer to the consumer.  The consumer
     * is returned, \b not the producer.  This is to allow chaining:
     *
     * \code
     *    my_producer_event_ >> my_consumer_event_ >> my_event_following_consumption_;
     * \endcode
     *
     * This implementation of the precedence operator catches a
     * Scheduleable on the LHS preceding a DAG point (which is not a
     * Scheduleable type) and ensures the two events are within the
     * same phase.
     *
     * \code
     * GOPoint >> EventNode type (UniqueEvent, PayloadEvent, SingleCycleUniqueEvent, Event)
     * \endcode
     */
    template<class ScheduleableTypeA>
    typename std::enable_if<std::is_base_of<EventNode, ScheduleableTypeA>::value, ScheduleableTypeA>::
    type & operator>>(Vertex & producer, ScheduleableTypeA & consumer)
    {
        producer.precedes(consumer.getScheduleable());
        return consumer;
    }

#define __SPARTA_PHASE_ERROR_MSG                                          \
    "\nERROR: You cannot set a precedence on two Scheduleable types"    \
    " that are on different phases.  This will happen automatically by the framework."

    ////////////////////////////////////////////////////////////////////////////////
    // Start with easy, concrete, derived types
    ////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////
    // payloadevent_precedence PayloadEvent Precedence
    // Examples of PayloadEvent precedence
    //
    // PayloadEvent<T, P> >> PayloadEvent<T, P>;
    // PayloadEvent<T, P> >> UniqueEvent<P>;
    // PayloadEvent<T, P> >> SingleCycleUniqueEvent<P>;
    // PayloadEvent<T, P> >> Event<P>;
    // UniqueEvent<P>     >> PayloadEvent<T, P>;
    // SingleCycleUniqueEvent<P> >> PayloadEvent<T, P>;
    // Event<P>           >> PayloadEvent<T, P>;
    //

     /**
     * \brief Place a precedence between a PayloadEvent and another PayloadEvent
     *
     * \param producer The PayloadEvent to succeed another PayloadEvent
     * \param consumer The producer (LHS) must come before consumer (RHS)
     *
     * \return The \i consumer
     *
     * \note This function returns the \i consumer (RHS) of the
     *       operation instead of the \i producer (LHS).  This is
     *       opposite of typical behavior of >> operators.
     *
     * Place a precedence on a producer to the consumer.  The consumer
     * is returned, \b not the producer.  This is to allow chaining:
     *
     * \code
     *    my_producer_event_ >> my_consumer_event_ >> my_event_following_consumption_;
     * \endcode
     *
     * This implementation of the precedence operator catches a
     * Scheduleable/EventNode on the LHS preceding another and ensures
     * the two events are within the same phase.
     *
     * \code
     * PayloadEvent<T, P> >> PayloadEvent<T, P>;
     * \endcode
     */
    template<class DataT1, SchedulingPhase PayloadPhaseT1,
             class DataT2, SchedulingPhase PayloadPhaseT2>
    PayloadEvent<DataT2, PayloadPhaseT2> & operator>>(PayloadEvent<DataT1, PayloadPhaseT1> & producer,
                                                      PayloadEvent<DataT2, PayloadPhaseT2> & consumer)
    {
        static_assert(PayloadPhaseT1 == PayloadPhaseT2, __SPARTA_PHASE_ERROR_MSG);
        producer.getScheduleable().precedes(consumer.getScheduleable());
        return consumer;
    }

    /**
     * \brief Place a precedence between a PayloadEvent and UniqueEvent
     *
     * \param producer The PayloadEvent to succeed UniqueEvent
     * \param consumer The producer (LHS) must come before consumer (RHS)
     *
     * \return The \i consumer
     *
     * \note This function returns the \i consumer (RHS) of the
     *       operation instead of the \i producer (LHS).  This is
     *       opposite of typical behavior of >> operators.
     *
     * Place a precedence on a producer to the consumer.  The consumer
     * is returned, \b not the producer.  This is to allow chaining:
     *
     * \code
     *    my_producer_event_ >> my_consumer_event_ >> my_event_following_consumption_;
     * \endcode
     *
     * This implementation of the precedence operator catches a
     * Scheduleable/EventNode on the LHS preceding another and ensures
     * the two events are within the same phase.
     *
     * \code
     * PayloadEvent<T, P> >> UniqueEvent<P>;
     * \endcode
     */
    template<class DataT1,
             SchedulingPhase PayloadPhaseT1,
             SchedulingPhase PayloadPhaseT2>
    UniqueEvent<PayloadPhaseT2> & operator>>(PayloadEvent<DataT1, PayloadPhaseT1> & producer,
                                             UniqueEvent<PayloadPhaseT2> & consumer)
    {
        static_assert(PayloadPhaseT1 == PayloadPhaseT2, __SPARTA_PHASE_ERROR_MSG);
        producer.getScheduleable().precedes(consumer.getScheduleable());
        return consumer;
    }

    /**
     * \brief Place a precedence between a PayloadEvent and SingleCycleUniqueEvent
     *
     * \param producer The PayloadEvent to succeed SingleCycleUniqueEvent
     * \param consumer The producer (LHS) must come before consumer (RHS)
     *
     * \return The \i consumer
     *
     * \note This function returns the \i consumer (RHS) of the
     *       operation instead of the \i producer (LHS).  This is
     *       opposite of typical behavior of >> operators.
     *
     * Place a precedence on a producer to the consumer.  The consumer
     * is returned, \b not the producer.  This is to allow chaining:
     *
     * \code
     *    my_producer_event_ >> my_consumer_event_ >> my_event_following_consumption_;
     * \endcode
     *
     * This implementation of the precedence operator catches a
     * Scheduleable/EventNode on the LHS preceding another and ensures
     * the two events are within the same phase.
     *
     * \code
     * PayloadEvent<T, P> >> SingleCycleUniqueEvent<P>;
     * \endcode
     */
    template<class DataT1,
             SchedulingPhase PayloadPhaseT1,
             SchedulingPhase PayloadPhaseT2>
    SingleCycleUniqueEvent<PayloadPhaseT2> & operator>>(PayloadEvent<DataT1, PayloadPhaseT1> & producer,
                                                        SingleCycleUniqueEvent<PayloadPhaseT2> & consumer)
    {
        static_assert(PayloadPhaseT1 == PayloadPhaseT2, __SPARTA_PHASE_ERROR_MSG);
        producer.precedes(consumer.getScheduleable());
        return consumer;
    }

    /**
     * \brief Place a precedence between a PayloadEvent and an Event
     *
     * \param producer The PayloadEvent to succeed an Event
     * \param consumer The producer (LHS) must come before consumer (RHS)
     *
     * \return The \i consumer
     *
     * \note This function returns the \i consumer (RHS) of the
     *       operation instead of the \i producer (LHS).  This is
     *       opposite of typical behavior of >> operators.
     *
     * Place a precedence on a producer to the consumer.  The consumer
     * is returned, \b not the producer.  This is to allow chaining:
     *
     * \code
     *    my_producer_event_ >> my_consumer_event_ >> my_event_following_consumption_;
     * \endcode
     *
     * This implementation of the precedence operator catches a
     * Scheduleable/EventNode on the LHS preceding another and ensures
     * the two events are within the same phase.
     *
     * \code
     * PayloadEvent<T, P> >> Event<P>;
     * \endcode
     */
    template<class DataT1,
             SchedulingPhase PayloadPhaseT1,
             SchedulingPhase PayloadPhaseT2>
    Event<PayloadPhaseT2> & operator>>(PayloadEvent<DataT1, PayloadPhaseT1> & producer,
                                       Event<PayloadPhaseT2> & consumer)
    {
        static_assert(PayloadPhaseT1 == PayloadPhaseT2, __SPARTA_PHASE_ERROR_MSG);
        producer.getScheduleable().precedes(consumer.getScheduleable());
        return consumer;
    }

    /**
     * \brief Place a precedence between a PayloadEvent and an UniqueEvent
     *
     * \param producer The UniqueEvent to succeed a PayloadEvent
     * \param consumer The producer (LHS) must come before consumer (RHS)
     *
     * \return The \i consumer
     *
     * \note This function returns the \i consumer (RHS) of the
     *       operation instead of the \i producer (LHS).  This is
     *       opposite of typical behavior of >> operators.
     *
     * Place a precedence on a producer to the consumer.  The consumer
     * is returned, \b not the producer.  This is to allow chaining:
     *
     * \code
     *    my_producer_event_ >> my_consumer_event_ >> my_event_following_consumption_;
     * \endcode
     *
     * This implementation of the precedence operator catches a
     * Scheduleable/EventNode on the LHS preceding another and ensures
     * the two events are within the same phase.
     *
     * \code
     * UniqueEvent<P> >> PayloadEvent<T, P>;
     * \endcode
     */
    template<SchedulingPhase PayloadPhaseT1,
             class DataT2,
             SchedulingPhase PayloadPhaseT2>
    PayloadEvent<DataT2, PayloadPhaseT2> & operator>>(UniqueEvent<PayloadPhaseT1> & producer,
                                                      PayloadEvent<DataT2, PayloadPhaseT2> & consumer)
    {
        static_assert(PayloadPhaseT1 == PayloadPhaseT2, __SPARTA_PHASE_ERROR_MSG);
        producer.precedes(consumer.getScheduleable());
        return consumer;
    }

    /**
     * \brief Place a precedence between a PayloadEvent and an SingleCycleUniqueEvent
     *
     * \param producer The SingleCycleUniqueEvent to succeed a PayloadEvent
     * \param consumer The producer (LHS) must come before consumer (RHS)
     *
     * \return The \i consumer
     *
     * \note This function returns the \i consumer (RHS) of the
     *       operation instead of the \i producer (LHS).  This is
     *       opposite of typical behavior of >> operators.
     *
     * Place a precedence on a producer to the consumer.  The consumer
     * is returned, \b not the producer.  This is to allow chaining:
     *
     * \code
     *    my_producer_event_ >> my_consumer_event_ >> my_event_following_consumption_;
     * \endcode
     *
     * This implementation of the precedence operator catches a
     * Scheduleable/EventNode on the LHS preceding another and ensures
     * the two events are within the same phase.
     *
     * \code
     * SingleCycleUniqueEvent<P> >> PayloadEvent<T, P>;
     * \endcode
     */
    template<SchedulingPhase PayloadPhaseT1,
             class DataT2,
             SchedulingPhase PayloadPhaseT2>
    PayloadEvent<DataT2, PayloadPhaseT2> & operator>>(SingleCycleUniqueEvent<PayloadPhaseT1> & producer,
                                                      PayloadEvent<DataT2, PayloadPhaseT2> & consumer)
    {
        static_assert(PayloadPhaseT1 == PayloadPhaseT2, __SPARTA_PHASE_ERROR_MSG);
        producer.precedes(consumer.getScheduleable());
        return consumer;
    }

    /**
     * \brief Place a precedence between a Event and an PayloadEvent
     *
     * \param producer The Event to succeed a PayloadEvent
     * \param consumer The producer (LHS) must come before consumer (RHS)
     *
     * \return The \i consumer
     *
     * \note This function returns the \i consumer (RHS) of the
     *       operation instead of the \i producer (LHS).  This is
     *       opposite of typical behavior of >> operators.
     *
     * Place a precedence on a producer to the consumer.  The consumer
     * is returned, \b not the producer.  This is to allow chaining:
     *
     * \code
     *    my_producer_event_ >> my_consumer_event_ >> my_event_following_consumption_;
     * \endcode
     *
     * This implementation of the precedence operator catches a
     * Scheduleable/EventNode on the LHS preceding another and ensures
     * the two events are within the same phase.
     *
     * \code
     * Event<P> >> PayloadEvent<T, P>;
     * \endcode
     */
    template<SchedulingPhase PayloadPhaseT1,
             class DataT2,
             SchedulingPhase PayloadPhaseT2>
    PayloadEvent<DataT2, PayloadPhaseT2> & operator>>(Event<PayloadPhaseT1> & producer,
                                                      PayloadEvent<DataT2, PayloadPhaseT2> & consumer)
    {
        static_assert(PayloadPhaseT1 == PayloadPhaseT2, __SPARTA_PHASE_ERROR_MSG);
        producer.precedes(consumer.getScheduleable());
        return consumer;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // UniqueEvent Precedence
    //
    // UniqueEvent<P> >> UniqueEvent<P>;
    // UniqueEvent<P> >> SingleCycleUniqueEvent<P>;
    // SingleCycleUniqueEvent<P> >> UniqueEvent<P>;
    //

    /**
     * \brief Place a precedence between a UniqueEvent and an UniqueEvent
     *
     * \param producer The UniqueEvent to succeed a UniqueEvent
     * \param consumer The producer (LHS) must come before consumer (RHS)
     *
     * \return The \i consumer
     *
     * \note This function returns the \i consumer (RHS) of the
     *       operation instead of the \i producer (LHS).  This is
     *       opposite of typical behavior of >> operators.
     *
     * Place a precedence on a producer to the consumer.  The consumer
     * is returned, \b not the producer.  This is to allow chaining:
     *
     * \code
     *    my_producer_event_ >> my_consumer_event_ >> my_event_following_consumption_;
     * \endcode
     *
     * This implementation of the precedence operator catches a
     * Scheduleable/EventNode on the LHS preceding another and ensures
     * the two events are within the same phase.
     *
     * \code
     * UniqueEvent<P> >> UniqueEvent<P>;
     * \endcode
     */
    template<SchedulingPhase PayloadPhaseT1,
             SchedulingPhase PayloadPhaseT2>
    UniqueEvent<PayloadPhaseT2> & operator>>(UniqueEvent<PayloadPhaseT1> & producer,
                                             UniqueEvent<PayloadPhaseT2> & consumer)
    {
        static_assert(PayloadPhaseT1 == PayloadPhaseT2, __SPARTA_PHASE_ERROR_MSG);
        producer.precedes(consumer.getScheduleable());
        return consumer;
    }

    /**
     * \brief Place a precedence between a UniqueEvent and an SingleCycleUniqueEvent
     *
     * \param producer The UniqueEvent to succeed a SingleCycleUniqueEvent
     * \param consumer The producer (LHS) must come before consumer (RHS)
     *
     * \return The \i consumer
     *
     * \note This function returns the \i consumer (RHS) of the
     *       operation instead of the \i producer (LHS).  This is
     *       opposite of typical behavior of >> operators.
     *
     * Place a precedence on a producer to the consumer.  The consumer
     * is returned, \b not the producer.  This is to allow chaining:
     *
     * \code
     *    my_producer_event_ >> my_consumer_event_ >> my_event_following_consumption_;
     * \endcode
     *
     * This implementation of the precedence operator catches a
     * Scheduleable/EventNode on the LHS preceding another and ensures
     * the two events are within the same phase.
     *
     * \code
     * UniqueEvent<P> >> SingleCycleUniqueEvent<P>;
     * \endcode
     */
    template<SchedulingPhase PayloadPhaseT1,
             SchedulingPhase PayloadPhaseT2>
    SingleCycleUniqueEvent<PayloadPhaseT2> & operator>>(UniqueEvent<PayloadPhaseT1> & producer,
                                                        SingleCycleUniqueEvent<PayloadPhaseT2> & consumer)
    {
        static_assert(PayloadPhaseT1 == PayloadPhaseT2, __SPARTA_PHASE_ERROR_MSG);
        producer.precedes(consumer.getScheduleable());
        return consumer;
    }

    /**
     * \brief Place a precedence between a SingleCycleUniqueEvent and an UniqueEvent
     *
     * \param producer The SingleCycleUniqueEvent to succeed an UniqueEvent
     * \param consumer The producer (LHS) must come before consumer (RHS)
     *
     * \return The \i consumer
     *
     * \note This function returns the \i consumer (RHS) of the
     *       operation instead of the \i producer (LHS).  This is
     *       opposite of typical behavior of >> operators.
     *
     * Place a precedence on a producer to the consumer.  The consumer
     * is returned, \b not the producer.  This is to allow chaining:
     *
     * \code
     *    my_producer_event_ >> my_consumer_event_ >> my_event_following_consumption_;
     * \endcode
     *
     * This implementation of the precedence operator catches a
     * Scheduleable/EventNode on the LHS preceding another and ensures
     * the two events are within the same phase.
     *
     * \code
     * SingleCycleUniqueEvent<P> >> UniqueEvent<P>;
     * \endcode
     */
    template<SchedulingPhase PayloadPhaseT1,
             SchedulingPhase PayloadPhaseT2>
    UniqueEvent<PayloadPhaseT2> & operator>>(SingleCycleUniqueEvent<PayloadPhaseT1> & producer,
                                             UniqueEvent<PayloadPhaseT2> & consumer)
    {
        static_assert(PayloadPhaseT1 == PayloadPhaseT2, __SPARTA_PHASE_ERROR_MSG);
        producer.precedes(consumer.getScheduleable());
        return consumer;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // SingleCycleUniqueEvent Precedence
    //
    /**
     * \brief Place a precedence between a SingleCycleUniqueEvent and a SingleCycleUniqueEvent
     *
     * \param producer The SingleCycleUniqueEvent to succeed a SingleCycleUniqueEvent
     * \param consumer The producer (LHS) must come before consumer (RHS)
     *
     * \return The \i consumer
     *
     * \note This function returns the \i consumer (RHS) of the
     *       operation instead of the \i producer (LHS).  This is
     *       opposite of typical behavior of >> operators.
     *
     * Place a precedence on a producer to the consumer.  The consumer
     * is returned, \b not the producer.  This is to allow chaining:
     *
     * \code
     *    my_producer_event_ >> my_consumer_event_ >> my_event_following_consumption_;
     * \endcode
     *
     * This implementation of the precedence operator catches a
     * Scheduleable/EventNode on the LHS preceding another and ensures
     * the two events are within the same phase.
     *
     * \code
     * SingleCycleUniqueEvent<P> >> SingleCycleUniqueEvent<P>;
     * \endcode
     */
    template<SchedulingPhase PhaseT1, SchedulingPhase PhaseT2>
    SingleCycleUniqueEvent<PhaseT2> & operator>>(SingleCycleUniqueEvent<PhaseT1> & producer,
                                                 SingleCycleUniqueEvent<PhaseT2> & consumer)
    {
        static_assert(PhaseT1 == PhaseT2, __SPARTA_PHASE_ERROR_MSG);
        producer.getScheduleable().precedes(consumer.getScheduleable());
        return consumer;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Event Precedence
    //
    /**
     * \brief Place a precedence between a Event and another Event
     *
     * \param producer The Event to succeed a Event
     * \param consumer The producer (LHS) must come before consumer (RHS)
     *
     * \return The \i consumer
     *
     * \note This function returns the \i consumer (RHS) of the
     *       operation instead of the \i producer (LHS).  This is
     *       opposite of typical behavior of >> operators.
     *
     * Place a precedence on a producer to the consumer.  The consumer
     * is returned, \b not the producer.  This is to allow chaining:
     *
     * \code
     *    my_producer_event_ >> my_consumer_event_ >> my_event_following_consumption_;
     * \endcode
     *
     * This implementation of the precedence operator catches a
     * Scheduleable/EventNode on the LHS preceding another and ensures
     * the two events are within the same phase.
     *
     * \code
     * Event<P> >> Event<P>;
     * \endcode
     */
    template<SchedulingPhase PhaseT1, SchedulingPhase PhaseT2>
    Event<PhaseT2> & operator>>(Event<PhaseT1> & producer, Event<PhaseT2> & consumer)
    {
        static_assert(PhaseT1 == PhaseT2, __SPARTA_PHASE_ERROR_MSG);
        producer.precedes(consumer.getScheduleable());
        return consumer;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // // Unique Ptrs
    // std::unique_ptr<eventT> >> std::unique_ptr<eventT>;
    // std::unique_ptr<eventT> >> concrete_event;
    // std::unique_ptr<eventT> >> event_pointer;
    // concrete_event >> std::unique_ptr<eventT>;
    // event_pointer >> std::unique_ptr<eventT>;

    /**
     * \brief Place a precedence between a std::unique_ptr<EventT1> and another std::unique_ptr<EventT2>
     *
     * \param producer The EventT1 to succeed a EventT2
     * \param consumer The producer (LHS) must come before consumer (RHS)
     *
     * \return The \i consumer
     *
     * \note This function returns the \i consumer (RHS) of the
     *       operation instead of the \i producer (LHS).  This is
     *       opposite of typical behavior of >> operators.
     *
     * Place a precedence on a producer to the consumer.  The consumer
     * is returned, \b not the producer.  This is to allow chaining:
     *
     * \code
     *    my_producer_event_ >> my_consumer_event_ >> my_event_following_consumption_;
     * \endcode
     *
     * This implementation of the precedence operator catches a
     * Scheduleable/EventNode on the LHS preceding another and ensures
     * the two events are within the same phase.
     *
     * \code
     * std::unique_ptr<EventT1> >> std::unique_ptr<EventT2>;
     * \endcode
     */
    template<class EventT1, class EventT2>
    std::unique_ptr<EventT2> & operator>>(std::unique_ptr<EventT1> & producer,
                                          std::unique_ptr<EventT2> & consumer)
    {
        (*producer) >> (*consumer);
        return consumer;
    }

    /**
     * \brief Place a precedence between an EventT1 and std::unique_ptr<EventT2>
     *
     * \param producer The EventT1 to succeed a EventT2
     * \param consumer The producer (LHS) must come before consumer (RHS)
     *
     * \return The \i consumer
     *
     * \note This function returns the \i consumer (RHS) of the
     *       operation instead of the \i producer (LHS).  This is
     *       opposite of typical behavior of >> operators.
     *
     * Place a precedence on a producer to the consumer.  The consumer
     * is returned, \b not the producer.  This is to allow chaining:
     *
     * \code
     *    my_producer_event_ >> my_consumer_event_ >> my_event_following_consumption_;
     * \endcode
     *
     * This implementation of the precedence operator catches a
     * Scheduleable/EventNode on the LHS preceding another and ensures
     * the two events are within the same phase.
     *
     * \code
     * EventT1 >> std::unique_ptr<EventT2>;
     * \endcode
     */
    template<class EventT1, class EventT2>
    std::unique_ptr<EventT2> & operator>>(EventT1 & producer, std::unique_ptr<EventT2> & consumer)
    {
        producer >> (*consumer);
        return consumer;
    }

    /**
     * \brief Place a precedence between a std::unique_ptr<EventT1> and EventT2
     *
     * \param producer The EventT1 to succeed a EventT2
     * \param consumer The producer (LHS) must come before consumer (RHS)
     *
     * \return The \i consumer
     *
     * \note This function returns the \i consumer (RHS) of the
     *       operation instead of the \i producer (LHS).  This is
     *       opposite of typical behavior of >> operators.
     *
     * Place a precedence on a producer to the consumer.  The consumer
     * is returned, \b not the producer.  This is to allow chaining:
     *
     * \code
     *    my_producer_event_ >> my_consumer_event_ >> my_event_following_consumption_;
     * \endcode
     *
     * This implementation of the precedence operator catches a
     * Scheduleable/EventNode on the LHS preceding another and ensures
     * the two events are within the same phase.
     *
     * \code
     * std::unique_ptr<EventT1> >> EventT2;
     * \endcode
     */
    template<class EventT1, class EventT2>
    EventT2 & operator>>(std::unique_ptr<EventT1> & producer, EventT2 & consumer)
    {
        (*producer) >> consumer;
        return consumer;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Scheduleable <-> PayloadEvent
    ////////////////////////////////////////////////////////////////////////////////

    /**
     * \brief Place a precedence between a Scheduleable and a PayloadEvent
     *
     * \param producer The Scheduleable to succeed the PayloadEvent
     * \param consumer The producer (LHS) must come before consumer (RHS)
     *
     * \return The \i consumer
     *
     * \note This function returns the \i consumer (RHS) of the
     *       operation instead of the \i producer (LHS).  This is
     *       opposite of typical behavior of >> operators.
     *
     * Place a precedence on a producer to the consumer.  The consumer
     * is returned, \b not the producer.  This is to allow chaining:
     *
     * \code
     *    my_producer_event_ >> my_consumer_event_ >> my_event_following_consumption_;
     * \endcode
     *
     * This implementation of the precedence operator catches a
     * Scheduleable/EventNode on the LHS preceding another and ensures
     * the two events are within the same phase.
     *
     * \code
     * Scheduleable >> PayloadEvent<DataT, PhaseT>;
     * \endcode
     */
    template<class DataT2, SchedulingPhase phase>
    PayloadEvent<DataT2, phase> & operator>>(Scheduleable & producer,
                                             PayloadEvent<DataT2, phase> & consumer)
    {
        sparta_assert(producer.getSchedulingPhase() == consumer.getSchedulingPhase(),
                    __SPARTA_PHASE_ERROR_MSG);
        producer.precedes(consumer.getScheduleable());
        return consumer;
    }

    /**
     * \brief Place a precedence between a PayloadEvent and a Scheduleable
     *
     * \param producer The PayloadEvent to succeed a Scheduleable
     * \param consumer The producer (LHS) must come before consumer (RHS)
     *
     * \return The \i consumer
     *
     * \note This function returns the \i consumer (RHS) of the
     *       operation instead of the \i producer (LHS).  This is
     *       opposite of typical behavior of >> operators.
     *
     * Place a precedence on a producer to the consumer.  The consumer
     * is returned, \b not the producer.  This is to allow chaining:
     *
     * \code
     *    my_producer_event_ >> my_consumer_event_ >> my_event_following_consumption_;
     * \endcode
     *
     * This implementation of the precedence operator catches a
     * Scheduleable/EventNode on the LHS preceding another and ensures
     * the two events are within the same phase.
     *
     * \code
     * PayloadEvent<DataT, PhaseT> >> Scheduleable;
     * \endcode
     */
    template<class DataT1, SchedulingPhase phase>
    Scheduleable & operator>>(PayloadEvent<DataT1, phase> & producer,
                              Scheduleable & consumer)
    {
        sparta_assert(consumer.getSchedulingPhase() == producer.getSchedulingPhase(),
                    __SPARTA_PHASE_ERROR_MSG);
        producer.getScheduleable().precedes(consumer);
        return consumer;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Bus Support
    ////////////////////////////////////////////////////////////////////////////////

    // This is where an event must be scheduled before any events
    // scheduled as a results of OutPorts on the Bus.

    /**
     * \brief Place a precedence between a Scheduleable and a Bus
     *
     * \param producer The Scheduleable to succeed a Bus
     * \param consumer The producer (LHS) must come before consumer (RHS)
     *
     * \return The \i consumer
     *
     * \note This function returns the \i consumer (RHS) of the
     *       operation instead of the \i producer (LHS).  This is
     *       opposite of typical behavior of >> operators.
     *
     * Place a precedence on a producer to the consumer.  The consumer
     * is returned, \b not the producer.  This is to allow chaining:
     *
     * \code
     *    my_producer_event_ >> my_consumer_event_ >> my_event_following_consumption_;
     * \endcode
     *
     * This implementation of the precedence operator catches a
     * Scheduleable/EventNode on the LHS preceding another and ensures
     * the two events are within the same phase.
     *
     * \code
     * ev_tick >> my_outgoing_bus;
     * \endcode
     */
    template<class ScheduleableType>
    typename
    std::enable_if<std::is_base_of<Scheduleable, ScheduleableType>::value, Bus>::
    type & operator>>(ScheduleableType & producer, Bus & consumer)
    {
        consumer.outportsSucceed(producer);
        return consumer;
    }

    // This is where an event must be scheduled after data arrival on
    // InPorts within the Bus:
    //
    //  my_incoming_bus >> ev_tick;
    //

    /**
     * \brief Place a precedence between a Bus and a Scheduleable
     *
     * \param producer The Bus to succeed a Scheduleable
     * \param consumer The producer (LHS) must come before consumer (RHS)
     *
     * \return The \i consumer
     *
     * \note This function returns the \i consumer (RHS) of the
     *       operation instead of the \i producer (LHS).  This is
     *       opposite of typical behavior of >> operators.
     *
     * Place a precedence on a producer to the consumer.  The consumer
     * is returned, \b not the producer.  This is to allow chaining:
     *
     * \code
     *    my_producer_event_ >> my_consumer_event_ >> my_event_following_consumption_;
     * \endcode
     *
     * This implementation of the precedence operator catches a
     * Scheduleable/EventNode on the LHS preceding another and ensures
     * the two events are within the same phase.
     *
     * \code
     * my_outgoing_bus >> ev_tick;
     * \endcode
     */
    template<class ScheduleableType>
    typename
    std::enable_if<std::is_base_of<Scheduleable, ScheduleableType>::value, ScheduleableType>::
    type & operator>>(Bus & producer, ScheduleableType & consumer)
    {
        producer.inportsPrecede(consumer);
        return consumer;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Add support for a group of event types
    ////////////////////////////////////////////////////////////////////////////////

    /**
     * \class EventGroup
     * \brief Group a series of events together for precedence establishment
     *
     * This class is used to group Scheduleable types and
     * PayloadEvents and establish a precedence between each event in
     * the group with either another group or a singular Scheduleable type.
     * Example:
     *
     * \code
     *  sparta::Event<sparta::SchedulingPhase::Update> prod0(...);
     *  sparta::Event<sparta::SchedulingPhase::Update> prod1(...);
     *  sparta::Event<sparta::SchedulingPhase::Update> prod2(...);
     *
     *  sparta::Event<sparta::SchedulingPhase::Update> cons0(...);
     *  sparta::Event<sparta::SchedulingPhase::Update> cons1(...);
     *  sparta::Event<sparta::SchedulingPhase::Update> cons2(...);
     *
     *  // Make each producer come before each consumer
     *  sparta::EventGroup(prod0, prod1, prod2) >> sparta::EventGroup(cons0, cons1, cons2);
     *
     *  // This is the same as doing this:
     *  prod0 >> cons0;
     *  prod0 >> cons1;
     *  prod0 >> cons2;
     *
     *  prod1 >> cons0;
     *  prod1 >> cons1;
     *  prod1 >> cons2;
     *
     *  prod2 >> cons0;
     *  prod2 >> cons1;
     *  prod2 >> cons2;
     *
     * \endcode
     */
    class EventGroup : public std::vector<Scheduleable *>
    {
        // Make sure no one is adding a Port in the mix (this was the
        // old framework)
        template<SchedulingPhase phase, class ScheduleableT, class ...ArgsT>
        typename std::enable_if<std::is_base_of<Port, ScheduleableT>::value, void>::
        type addScheduable_(ScheduleableT & /*port*/, ArgsT&... /*scheds*/) {
            static_assert(std::is_base_of<Port, ScheduleableT>::value,
                          "\nERROR: You cannot set up a precedence between an Scheduleable type and a Port. "
                          "\n\tIf you want to do this, register the Scheduleable with the Port using the "
                          "registerConsumerEvent/registerProducingEvent method on the Port."
                          "\n\tSee the SPARTA documentation for more information."
                          "\n\tBTW, in this particular error, there is a Port type in the EventGroup that's bad");
        }

        // Add a Scheduable type, not a PayloadEventType
        template<SchedulingPhase phase, class ScheduleableT, class ...ArgsT>
        typename std::enable_if<std::is_base_of<sparta::Scheduleable, ScheduleableT>::value, void>::
        type addScheduable_(ScheduleableT & sched, ArgsT&... scheds) {
            static_assert(phase == ScheduleableT::event_phase,
                          "\nERROR: You cannot set a precedence on two Scheduleable types"
                          " that are on different phases.  This will happen automatically by the framework."
                          "\n\tIn this particular error, there is a Scheduleable type in the EventGroup that's bad");
            emplace_back(&sched);
            addScheduable_<ScheduleableT::event_phase>(scheds...);
        }

        // Need a specialization for PayloadEvents since they are not direct Scheduleables
        template<SchedulingPhase phase, class DataT, SchedulingPhase PLEPhase, class ...ArgsT>
        void addScheduable_(PayloadEvent<DataT, PLEPhase> & sched, ArgsT&... scheds) {
            static_assert(phase == PLEPhase,
                          "\nERROR: You cannot set a precedence on two Scheduleable types"
                          " that are on different phases.  This will happen automatically by the framework."
                          "\n\tIn this particular error, there is a PayloadEvent type in the EventGroup that's bad");
            emplace_back(&sched.getScheduleable());
            addScheduable_<PLEPhase>(scheds...);
        }

        // end of list
        template<SchedulingPhase phase>
        void addScheduable_() {}

    public:

        /**
         * \brief Construct the EventGroup
         * \param sched  The first scheduleable
         * \param scheds The remaining scheduleables
         *
         * This is a variatic template constructor.  You can crate a
         * group class with as many Event types as possible.  Examples:
         *
         * \code
         * sparta::EventGroup(event1);
         * sparta::EventGroup(event2, event3, event4);
         * \endcode
         */
        template<class ScheduleableT, class ...ArgsT>
        explicit EventGroup(ScheduleableT & sched, ArgsT&...scheds) {
            static_assert(std::is_same<Port, ScheduleableT>::value == false,
                          "\nERROR: You cannot add Ports to a precedence group as Ports do not support precedence. "
                          "If you have a zero-cycle Port and a receiving event, register that event with the Port instead."
                          "\n\tIn this particular error, there is a Port type in the EventGroup that's bad");
            static_assert(std::is_same<Bus, ScheduleableT>::value == false,
                          "\nERROR: You cannot add a Bus to a precedence group as Buses do not support precedence directly. "
                          "\n\tInstead, use Bus' inportsPrecede(event) or outportsSucceed(event) methods OR the >> operator.");
            addScheduable_<ScheduleableT::event_phase>(sched, scheds...);
        }

        /**
         * \brief Construct the EventGroup
         * \param sched  The first scheduleable (if a PayloadEvent)
         * \param scheds The remaining scheduleables
         *
         * This is a variatic template constructor.  You can crate a
         * group class with as many Event types as possible.  Examples:
         *
         * \code
         * sparta::EventGroup(event1);
         * sparta::EventGroup(event2, event3, event4);
         * \endcode
         */
        template<class DataT, SchedulingPhase PhaseT, class ...ArgsT>
        explicit EventGroup(PayloadEvent<DataT, PhaseT> & sched, ArgsT&...scheds) {
            addScheduable_<PhaseT>(sched, scheds...);
        }

    };

    /*!
     * This template supports the following:
     * \param producers Group of Scheduleables that must come before the given consumer
     * \param consumer  The Scheduleable that succeed each of the Scheduleables in the EventGroup
     *
     * \code
     * sparta::EventGroup(ProEvent1, ProEvent2, ProEvent3, ...) >> ConsEvent;
     * \endcode
     *
     *  Where the consuming event is preceded by each producing event.
     *  The producing events are still independent.
     *
     */
    template<class ScheduleableTypeB>
    ScheduleableTypeB & operator>>(const EventGroup & producers,
                                   ScheduleableTypeB & consumer)
    {
        for(auto & prod : producers) {
            prod->precedes(consumer);
        }
        return consumer;
    }

    /*!
     * This template supports the following:
     * \param producer The Scheduleable that precedes each of the Scheduleables in the EventGroup
     * \param consumer Group of Scheduleables that must come after the given producer
     * \code
     * ProEvent1 >> sparta::EventGroup(ConsEvent1, ConsEvent2, ConsEvent3, ... );
     * \endcode
     *
     *  Where each consuming event is preceded by the single producing
     *  event.  The consuming events are still independent.
     */
    template<class ScheduleableTypeA>
    const EventGroup & operator>>(ScheduleableTypeA & producer,
                                  const EventGroup & consumers)
    {
        for(auto & cons : consumers) {
            producer.precedes(*cons);
        }
        return consumers;
    }

    /*!
     *
     * This template supports the following:
     * \param producers Group of Scheduleables that must come before each of the consumers in the EventGroup
     * \param consumers Group of Scheduleables that must come after each of the producers in the EventGroup
     * \code
     * sparta::EventGroup( ProdEvent1, ProdEvent2, ProdEvent3, ... )
     *     >> sparta::EventGroup( ConsEvent1, ConsEvent2, ConsEvent3, ... );
     * \endcode
     *
     * Where each consuming event is preceded by each producing event.
     * The consuming and producing events are still independent of
     * each other.
     */
    inline const EventGroup & operator>>(const EventGroup & producers,
                                         const EventGroup & consumers)
    {
        for(auto & prod : producers) {
            for(auto & cons : consumers) {
                sparta_assert(prod->getSchedulingPhase() == cons->getSchedulingPhase(),
                                  "The scheduling phase of '"
                                  << prod->getLabel() << "' phase '" << prod->getSchedulingPhase()
                                  << "' does not equal the scheduling phase of '"
                                  << cons->getLabel() << "' phase '" << cons->getSchedulingPhase()
                                  <<"'");

                prod->precedes(cons);
            }
        }
        return consumers;
    }

}

// __PRECEDENCE__H__
#endif
