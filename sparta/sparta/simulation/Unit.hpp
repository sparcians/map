// <Unit.hpp> -*- C++ -*-


/*!
 * \file   Unit.hpp
 *
 * \brief  File that defines the Unit class, a common grouping of sets and loggers
 */


#ifndef __UNIT__H__
#define __UNIT__H__

#include <string>

#include "sparta/simulation/Resource.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/ports/PortSet.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/log/MessageSource.hpp"
#include "sparta/log/categories/CategoryManager.hpp"

namespace sparta {

    class TreeNode;


    /*!
     * \class Unit
     * \brief The is the base class for user defined blocks in simulation.
     *
     * This class defines common set of sets (sparta::PortSet,
     * sparta::EventSet, and sparta::StatisticSet) as well as common
     * loggers (info_logger_, warn_logger_, debug_logger_).  In
     * addition, this class will establish precedences between Ports
     * and Events.  See setAutoPrecedence() for more information.
     */
    class Unit : public Resource
    {
    friend class DAG;
    public:

        static constexpr const char * INFO_LOG  = "info";
        static constexpr const char * WARN_LOG  = log::categories::WARN_STR;
        static constexpr const char * DEBUG_LOG = log::categories::DEBUG_STR;

        /*!
         * \brief Construct unit with a ResouceContainer
         * \param rc ResourceContainer that will hold this Unit until
         *           the Unit is destructed. Name and clock are
         *           extracted from this container. Must not be
         *           nullptr
         * \param name The name of this Unit
         */
        Unit(TreeNode* rc, const std::string & name) :
            Resource(rc, name),
            unit_port_set_(rc),
            unit_event_set_(rc),
            unit_stat_set_(rc),
            info_logger_ (rc, INFO_LOG,  rc->getName() + " Info Messages"),
            warn_logger_ (rc, WARN_LOG,  rc->getName() + " Warn Messages"),
            debug_logger_(rc, DEBUG_LOG, rc->getName() + " Debug Messages")
        {}

        /*!
         * \brief Construct unit with a ResouceContainer
         * \param rc ResourceContainer that will hold this Unit until
         *           the Unit is destructed. Name and clock are
         *           extracted from this container. Must not be
         *           nullptr
         */
        Unit(TreeNode* rc) :
            Unit(rc, rc->getName())
        {}

        /// Destroy!
        virtual ~Unit() {}

        /*!
         * \brief Turn off auto-precedence
         * \param auto_p True, perform auto-precedence, false, don't
         *
         * By default, the sparta::Unit will establish precedence
         * between registered events (via the unit_event_set_) and
         * registered ports (via the unit_port_set_).  Specifically,
         * the Unit will register all events in the
         * sparta::SchedulingPhase::Tick as consumers on InPorts and
         * producers on OutPorts.  If this behavior is not desired,
         * call setAutoPrecedence() with the value false.
         */
        void setAutoPrecedence(bool auto_p) {
            auto_precedence_ = auto_p;
        }

        /// \brief Return the port set
        PortSet * getPortSet() {
            return &unit_port_set_;
        }

        /// \brief Return the event set
        EventSet * getEventSet() {
            return &unit_event_set_;
        }

        /// \brief Return the stat set
        StatisticSet * getStatisticSet() {
            return &unit_stat_set_;
        }

    protected:
        //! The Unit's Ports
        sparta::PortSet      unit_port_set_;

        //! The Unit's event set
        sparta::EventSet     unit_event_set_;

        //! The Unit's statistic set
        sparta::StatisticSet unit_stat_set_;

        //! From sparta::Resource, set up precedence between ports and
        //! events registered in the sets
        virtual void onBindTreeEarly_() override {
            if(!auto_precedence_) {
                return;
            }

            for(auto & event_node : unit_event_set_.getEvents(sparta::SchedulingPhase::Tick))
            {
                // Go through all of the registered InPorts and set these
                // ports to precede any events that are on the Tick phase.
                // This is for 0-cycle precedence only.
                for(auto & pt : unit_port_set_.getPorts(Port::Direction::IN)) {
                    InPort * inp = dynamic_cast<InPort *>(pt.second);
                    sparta_assert(inp != nullptr);
                    if(inp->doesParticipateInAutoPrecedence()) {
                        inp->registerConsumerEvent(event_node->getScheduleable());
                    }
                }

                // Go through all of the registered OutPorts and set these
                // ports to succeed any events that are on the Tick phase.
                // This is for 0-cycle precedence only.
                for(auto & pt : unit_port_set_.getPorts(Port::Direction::OUT)) {
                    OutPort * outp = dynamic_cast<OutPort *>(pt.second);
                    sparta_assert(outp != nullptr);
                    if(outp->doesParticipateInAutoPrecedence()) {
                        outp->registerProducingEvent(event_node->getScheduleable());
                    }
                }
            }
        }

        //! Dump a dot
        virtual void onBindTreeLate_() override;

        //! Default info logger
        log::MessageSource info_logger_;

        //! Default warn logger
        log::MessageSource warn_logger_;

        //! Default debug logger
        log::MessageSource debug_logger_;

    private:
        //! Auto precedence boolean
        bool auto_precedence_ = true;

    };

} // namespace sparta

#define SPARTA_UNIT_BODY                                 \
    constexpr const char * sparta::Unit::INFO_LOG;       \
    constexpr const char * sparta::Unit::WARN_LOG;       \
    constexpr const char * sparta::Unit::DEBUG_LOG;

// __UNIT__H__
#endif
