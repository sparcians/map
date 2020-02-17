// <EventNode.h> -*- C++ -*-


/**
 * \file   EventNode.hpp
 *
 * \brief  File that defines the EventNode class
 */

#ifndef __EVENT_NODE_H__
#define __EVENT_NODE_H__

#include <set>
#include <memory>
#include <string>

#include "sparta/simulation/Clock.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/events/SchedulingPhases.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace sparta
{

    // Forward declare the Scheduler -- not used by EventNode
    class Scheduler;
    class Scheduleable;
    class StartupEvent;

    /**
     * \class EventNode
     * \brief EventNode is the base class for all event types in SPARTA.
     *        Not to be used by the modeler.  Its main purpose is to
     *        look for a clock and scheduler for the event
     */
    class EventNode : public TreeNode
    {
    protected:

        /**
         * \brief Create an Event Node
         * \param event_set The sparta::EventSet this node belongs to
         * \param name The name of this node
         * \param sched_phase The phase of this event
         */
        EventNode(TreeNode * event_set,
                  const std::string & name,
                  sparta::SchedulingPhase sched_phase) :
            TreeNode(nullptr, name, name + " EventNode"),
            sched_phase_(sched_phase)
        {
            sparta_assert(event_set != nullptr,
                        "Events must created with an EventSet: " << name);
            setExpectedParent_(event_set);
            ensureParentIsEventSet_(event_set);
            event_set->addChild(this);
        }

        //! Do not allow copies
        EventNode(const EventNode&) = delete;

        //! Do not allow moves
        EventNode(EventNode&&) = delete;

        //! Do not allow assignments
        EventNode& operator=(const EventNode &) = delete;

        //! Destroy!  Does nothing
        virtual ~EventNode() = default;

        //! Center point of Scheduler location
        static Scheduler * determineScheduler(const Clock * clk) {
            if (!clk) {
                return nullptr;
            }
            auto scheduler = clk->getScheduler();
            sparta_assert(scheduler, "Clock with no scheduler passed to EventNode::determineScheduler()");
            return scheduler;
        }

        friend StartupEvent;

    public:

        //! \brief Get the name of this EventNode as registered in the
        //! consumer handler for the TopoSortable
        //! \return The name of this EventNode
        const char * getLabel() const {
            return getName().c_str();
        }

        //! \brief Get the scheduling phase of this event node
        //! \return The scheduling phase
        sparta::SchedulingPhase getSchedulingPhase() const {
            return sched_phase_;
        }

        //! Get the scheduleable associated with this event node
        virtual Scheduleable & getScheduleable() = 0;

    private:

        //! Make sure the parent is an EventNodeSet
        void ensureParentIsEventSet_(sparta::TreeNode* parent);

        //! Scheduling phase of this node
        const sparta::SchedulingPhase sched_phase_;
    };
}


// __EVENT_NODE_H__
#endif
