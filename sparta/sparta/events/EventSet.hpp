// <StatisticSet> -*- C++ -*-


/**
 * \file   EventSet.hpp
 *
 * \brief  File that defines the EventSet class
 */

#ifndef __EVENT_SET_H__
#define __EVENT_SET_H__

#include <iostream>
#include <array>

#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/events/EventNode.hpp"

namespace sparta
{
    /*!
     * \brief Set of Events that a unit (or sparta::TreeNode, sparta::Resource) contains
     *        and are visible through a sparta Tree
     */
    class EventSet : public TreeNode
    {
    public:

        //! \brief Name of all EventSet nodes
        static constexpr char NODE_NAME[] = "events";

        /*!
         * \brief Constructor
         * \param parent parent node
         * \note The constructed EventSet will be named
         *       EventSet::NODE_NAME. Therefore, only one EventSet may
         *       exist as a child of any given node
         */
        EventSet(TreeNode* parent) :
            TreeNode(NODE_NAME,
                     TreeNode::GROUP_NAME_BUILTIN,
                     TreeNode::GROUP_IDX_NONE,
                     "Event Set")
        {
            if(parent){
                setExpectedParent_(parent);
                parent->addChild(this);
            }
        }

        /*!
         * \brief Destructor
         */
        ~EventSet() {}

        // Overload of TreeNode::stringize
        virtual std::string stringize(bool pretty=false) const override {
            (void) pretty;
            std::stringstream ss;
            std::array<EventsVector, NUM_SCHEDULING_PHASES>::size_type event_cnt = 0;
            for(const auto & ea : events_) {
                event_cnt += ea.size();
            }
            ss << '<' << getLocation() << ' ' << event_cnt << " events>";
            return ss.str();
        }

    public:
        //! Type for holding outside events
        typedef std::vector<EventNode *> EventsVector;

        //! Get the registered events for the given phase
        //! \param phase The phase to get
        EventsVector & getEvents(sparta::SchedulingPhase phase) {
            return events_[static_cast<uint32_t>(phase)];
        }

    private:

        /*!
         * \brief React to a child registration
         * \param child TreeNode child that must be downcastable to a
         *              sparta::StatisticDef or a
         *              sparta::CounterBase. This is a borrowed
         *              reference - child is *not* copied. Child
         *              lifetime must exceed that of this EventSet
         *              instance.
         * \pre Must not be finalized
         *
         * Overrides TreeNode::onAddingChild_
         */
        virtual void onAddingChild_(TreeNode* child) override {
            if(isFinalized()){
                throw SpartaException("Cannot add a child event once a EventSet is finalized. "
                                    "Error with: ")
                    << getLocation();
            }

            EventNode* event_node = dynamic_cast<EventNode*>(child);
            if(nullptr != event_node){
                // Add event to events_ list for tracking.
                events_[static_cast<uint32_t>(event_node->getSchedulingPhase())].push_back(event_node);
                return;
            }

            throw SpartaException("Cannot add TreeNode child ")
                << child->getName() << " to EventSet " << getLocation()
                << " because the child is not a CounterBase or Event";
        }

        /*!
         * \brief All events contained by this set whether allocated by this
         * set or not (superset of owned_events_)
         */
        std::array<EventsVector, NUM_SCHEDULING_PHASES> events_;
    };
}

// __EVENT_SET_H__
#endif
