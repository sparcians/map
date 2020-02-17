// <StartupEvent.h> -*- C++ -*-


/**
 * \file   StartupEvent.hpp
 *
 * \brief  File that defines the StartupEvent class
 */

#ifndef __STARTUPEVENT_H__
#define __STARTUPEVENT_H__

#include <set>
#include <memory>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/events/EventNode.hpp"
#include "sparta/kernel/Scheduler.hpp"

namespace sparta
{
    namespace trigger {
        // Temporary.
        class CycleTrigger;
        class TimeTrigger;
        class CounterTrigger;
        class ManagedTrigger;
    }

    /**
     * \brief StartupEvent is a simple class for scheduling a starting event
     *        on the Scheduler.  It does not support precedence.
     *
     * The purpose of this class is for a user to create a
     * StartupEvent and place a given handler on the sparta Scheduler to
     * be called before simulation starts.  This is required since a
     * modeler cannot schedule any events during resource construction
     * -- the Scheduler's internal graphs have not been finalized.
     *
     * \note This class can only be created in pre-finalized state.
     *
     * Example usage:
     *
     * \code
     * MyResource::MyResource(TreeNode * node, MyResourceParams * params) :
     *    // ...
     * {
     *     // ... other stuff
     *
     *     // Set a startup event
     *     sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(MyResource, myStartUpFunction));
     * }
     *
     * void MyResource::myStartUpFunction()
     * {
     *     my_unique_tick_event_.schedule();
     * }
     *
     * \endcode
     *
     */
    class StartupEvent
    {
    public:

        /**
         * \brief Create and schedule a StartupEvent with the given handler
         * \param node The parent node this StartupEvent is created
         *             from.  Used to find the Scheduler.
         */
        StartupEvent(TreeNode * node,
                     const SpartaHandler & consumer_startupevent_handler)
        {
            sparta_assert(node->getPhase() < PhasedObject::TREE_FINALIZED,
                        "You cannot create a StartupEvent outside of resource construction");
            Scheduler * scheduler = node->getScheduler();
            if (!scheduler) {
                scheduler = dynamic_cast<Scheduler*>(node);
            }
            if (!scheduler) {
                std::ostringstream oss;
                oss << "Could not resolve the Scheduler from the node given to a StartupEvent at location '"
                    << node->getLocation() << "'" << std::endl;
                throw SpartaException(oss.str());
            }
            scheduler->scheduleStartupHandler_(consumer_startupevent_handler);
        }

    private:
        // Temporary, until these classes are fixed.
        friend class trigger::CycleTrigger;
        friend class trigger::TimeTrigger;
        friend class trigger::CounterTrigger;
        friend class trigger::ManagedTrigger;

        StartupEvent(Scheduler * scheduler,
                     const SpartaHandler & consumer_startupevent_handler)
        {
            sparta_assert(!scheduler->isFinalized(),
                        "You cannot create a StartupEvent outside of resource construction");
            scheduler->scheduleStartupHandler_(consumer_startupevent_handler);
        }

    };
}


// __STARTUPEVENT_H__
#endif
