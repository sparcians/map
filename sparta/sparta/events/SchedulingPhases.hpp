// <SchedulingPhases> -*- C++ -*-


/**
 * \file   SchedulingPhases.h
 *
 * \brief File that defines the phases used in simulation.
 */

#ifndef __SCHEDULING_PHASES_H__
#define __SCHEDULING_PHASES_H__

#include <cinttypes>
#include <iostream>

namespace sparta
{

    /*!
     * SchedulingPhase are phases within the SPARTA framework that
     * allow a user to "categorize" events, ports, collection, and
     * updatables into groups for auto-precedence establishment.
     *
     * The current organization of this framework is UPCaT
     * (Updateables, Ports, Collection, and Tick [incl PostTick]) or more specifically:
     *
     * -# Updatables are ordered first (sparta::SchedulingPhase::Update)
     * -# Ports, N-Cycle where N > 0, that transfer/append to
     *    resources (like pipes, buffers) are updated
     *    next (specifically, registered handlers are called) (sparta::SchedulingPhase::PortUpdate)
     * -# Collection for pipeline viewing (sparta::SchedulingPhase::Collection)
     * -# Tickables, or simulation components that operate on data
     *    from updatables and ports. (sparta::SchedulingPhase::Tick)
     * -# Post-tickables, or events fired after all Tick events (sparta::SchedulingPhase::PostTick)
     *
     * What happens in the framework is that all Scheduleable objects
     * that are in the SchedulingPhase::Update phase will be organized
     * on the Scheduler before SchedulingPhase::PortUpdate
     * Scheduleable events, which are before
     * SchedulingPhase::Collection Scheduleable objects, and so on.
     * Within a specific phase, a modeler can order events as well.
     * For example, two events in the SchedulingPhase::Tick can be
     * ordered using the ">>" operator.  See Precedence.h for more
     * information.
     *
     * For 0-cycle Ports, SchedulingPhase::PortUpdate events occur in
     * the SchedulingPhase::Tick scheduling phase and are ordered with
     * other events also in the SchedulingPhase::Tick scheduling
     * phase.  This only happens, however, when precedence is
     * established between the Port and that subsequent events.  This
     * precedence is established by calling
     * Port::registerConsumerEvent or Port::registerProducingEvent
     * (depending on the sparta::Port::Direction).
     */
    enum class SchedulingPhase
    {
#ifndef DO_NOT_DOCUMENT
        Trigger,
#endif
        Update,         //!< Resources are updated in this phase
        PortUpdate,     //!< N-cycle Ports are updated in this phase
        Flush,          //!< Phase where flushing of pipelines, etc can occur
        Collection,     //!< Pipeline collection occurs here
        Tick,           //!< Most operations (combinational logic) occurs in this phase
        PostTick,       //!< Operations such as post-tick pipeline collection occur here
#ifndef DO_NOT_DOCUMENT
        __last_scheduling_phase,
        Invalid = __last_scheduling_phase
#endif
    };

    //! The number of phases
    const uint32_t NUM_SCHEDULING_PHASES =
        static_cast<uint32_t>(SchedulingPhase::__last_scheduling_phase);

    /**
     * \brief Print the SchedulingPhase
     * \param os    ostream to print to
     * \param phase The SchedulingPhase to print out
     * \return The ostream
     */
    inline std::ostream & operator<<(std::ostream & os, const SchedulingPhase & phase)
    {
        switch (phase) {
        case SchedulingPhase::Trigger:
            os << "Trigger";
            break;
        case SchedulingPhase::Update:
            os << "Update";
            break;
        case SchedulingPhase::PortUpdate:
            os << "PortUpdate";
            break;
        case SchedulingPhase::Flush:
            os << "Flush";
            break;
        case SchedulingPhase::Collection:
            os << "Collection";
            break;
        case SchedulingPhase::Tick:
            os << "Tick";
            break;
        case SchedulingPhase::PostTick:
            os << "PostTick";
            break;
        case SchedulingPhase::__last_scheduling_phase:
            os << "<UNKNOWN PHASE>";
            break;
            // NO DEFAULT!  Allows for compiler errors if the enum
            // class is updated.
        }
        return os;
    }

}

#endif
