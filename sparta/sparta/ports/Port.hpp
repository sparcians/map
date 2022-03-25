// <Port.hpp> -*- C++ -*-


/**
 * \file   Port.hpp
 *
 * \brief  File that defines the Port base class
 */

#pragma once
#include <set>
#include <list>
#include <unordered_map>
#include <algorithm>
#include <ostream>
#include <string>
#include <vector>

#include "sparta/events/Event.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/events/Scheduleable.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/kernel/SpartaHandler.hpp"
#include "sparta/events/SchedulingPhases.hpp"

namespace sparta
{
    // Used for DAG predence rules
    class GlobalOrderingPoint;

    /**
     * \class Port
     * \brief The port interface used to bind port types together and
     *        defines a port behavior
     *
     * Ports are the glue that connect one component in simulation
     * with another.  Port do NOT exchange interfaces between
     * components, but instead allow data to be exchanged between
     * components.  Data is exchanged at the derivative level,
     * specifically with DataInPort/OutPort,
     * SignalInPort/SignalOutPort, and SyncInPort/SyncOutPort.
     *
     * The Port class is a TreeNode, meaning that it has placement
     * within a simulation Tree, specically within a sparta::PortSet.
     * Typically, this node will be called "ports" and can be found
     * via a tree walk.  For example, if a resource "blockA" has a
     * Port called "mysignal_in", it can found in the heirarchy:
     * 'top.blockA.ports.mysignal_in'.
     *
     * As for precedence, producers registered on OutPorts will always
     * precede Consumers registered on InPorts within the same
     * sparta::SchedulingPhase
     *
     */
    class Port : public TreeNode
    {
    public:
        //! A convenience typedef
        typedef std::vector<Scheduleable *> ScheduleableList;

        /**
         * \enum Direction
         * \brief The direction of this port
         */
        enum Direction {
            IN,                 /**< Port direction is in */
            OUT,                /**< Port direction is out */
            UNKNOWN,            /**< For ExportedPort types, the direction is unknown */
            N_DIRECTIONS
        };
        /**
         * \brief Construct a Port
         * \param portset The sparta::PortSet this port belongs to.
         * \param dir The direction of the port -- inny or outty
         * \param name The name of the port
         * \param phase The scheduling this Port belongs to (pertinent to InPorts)
         * \param handler_arg_count The number of required arguments
         *                          for the explicit receiving
         *                          handler.  Not pertinent for
         *                          OutPort types.
         *
         * For OutPorts, a group ID is pointless and prevents
         * optimizations in the Scheduler if nothing is ever scheduled
         * at that time.
         */
        Port(TreeNode* portset, Direction dir, const std::string & name) :
            TreeNode(nullptr, name, TreeNode::GROUP_NAME_NONE,
                     TreeNode::GROUP_IDX_NONE, "Ports"),
            dir_(dir),
            name_(name)
        {
            sparta_assert(name.length() != 0, "You cannot have an unnamed port.");
            sparta_assert(portset != nullptr,
                        "Ports must created with a PortSet: " << name);
            setExpectedParent_(portset);
            ensureParentIsPortSet_(portset);
            portset->addChild(this);
        }

        //! Destructor
        virtual ~Port() {}

        /**
         * \brief Method to bind this Port to another, pointer style
         * \param port The port to bind to
         * \note Note that the binding is uni-directional
         */
        virtual void bind(Port * port) = 0;

        /**
         * \brief Method to bind this Port to another, reference style
         * \param port The port to bind to
         * \note Note that the binding is uni-directional
         */
        void bind(Port & port) {
            bind(&port);
        }

        /**
         * \brief Is this port bound to another port?
         * \return true if bound
         */
        virtual bool isBound() const {
            return (bound_ports_.empty() == false);
        }

        /**
         * \brief The direction of the port
         * \return In or out
         */
        virtual Direction getDirection() const {
            return dir_;
        }

        /**
         * \brief Turn on/off auto precedence for this port
         * \param participate Set to true [default] if the Port is to
         *                    participate in auto precedence
         *                    establishment in sparta::Unit
         *
         * In sparta::Unit, registered sparta::Event types and Ports will
         * have auto precedence established between them if the user
         * of sparta::Unit allows it to do so.  However, this might not
         * be desired for some Ports that are created by the modeler
         * and internally bound before the sparta::Unit performs this
         * setup.  Calling this method with participate set to false,
         * will prevent the assertion that a Consumer/Producer Event
         * is be being registered after port binding.
         */
        virtual void participateInAutoPrecedence(bool participate) {
            participate_in_auto_precedence_ = participate;
        }

        //! \brief Does this Port participate in auto-precedence
        //!        establishment by sparta::Unit?
        //! \return true if so, false otherwise
        virtual bool doesParticipateInAutoPrecedence() const {
            return participate_in_auto_precedence_;
        }

        /**
         * \brief Stringize the Port
         * \param x unused
         * \return String representing the binding
         */
        std::string stringize(bool x) const override {
            (void)x;
            std::stringstream ss;
            ss << "[bound to] {";
            for(Port * p : bound_ports_) {
                if(p != bound_ports_[0]){
                    ss << ", ";
                }
                ss << p->getName() << " (" << p->getLocation() << ")";
            }
            ss << "}";
            return ss.str();
        }

        /**
         * \brief Set the delay for a port
         * \param delay The number of cycles to delay for this port
         *
         * This method will assert unless overridden by a child since the
         * default port doesn't have a delay.
         */
        virtual void setPortDelay(sparta::Clock::Cycle ) {
            sparta_assert ("ERROR:Parent port's don't have delays" == 0);
        }

        //! Add a double version for SyncPort.h
        virtual void setPortDelay(double ) {
            sparta_assert ("ERROR:Parent port's don't have delays" == 0);
        }

        //! Get this Port's static delay
        virtual Clock::Cycle getPortDelay() const {
            sparta_assert ("ERROR:Parent port's don't have delays" == 0);
            return 0;
        }

        //! Enable collection on the port
        virtual void enableCollection(TreeNode*) { }

        //! Do events from this port keep simulation going?
        //! \param continuing true if yes; false otherwise
        virtual void setContinuing(bool continuing) {
            continuing_ = continuing;
        };

#ifndef DO_NOT_DOCUMENT
        template<class PObjT, void * precedes_obj = nullptr>
        void precedes(const PObjT &) {
            static_assert(precedes_obj != nullptr,
                          "You cannot set a precedence on a Port anymore -- use "
                          "registerConsumerEvent and registerProducingEvent instead");
        }
#endif

        //! \brief See if the given port is already bound
        //! \param pt The port to check
        bool isAlreadyBound(const Port * pt) {
            return (std::find(bound_ports_.begin(),
                              bound_ports_.end(), pt) != bound_ports_.end());
        }

        //! \brief Determine if this Port (out or in) is driven on the
        //!        given cycle
        //! \param rel_cycle The relative cycle (from now) the data
        //!                  will be delivered
        //! \return true if driven at the given cycle (data not yet delivered)
        virtual bool isDriven(Clock::Cycle rel_cycle) const {
            (void) rel_cycle;
            bool NOT_DEFINED = true;
            sparta_assert(NOT_DEFINED == false,
                        "Function not defined for this Port: " << getName());
            return false;
        }

        //! \brief Is this Port driven at all?
        //! \return true if driven at all (data not yet delivered)
        virtual bool isDriven() const {
            bool NOT_DEFINED = true;
            sparta_assert(NOT_DEFINED == false,
                        "Function not defined for this Port: " << getName());
            return false;
        }

    private:
        //! The direction of the port
        const Direction dir_;

        //! Validate the tree after finalization
        void validateNode_() const override {
            //! \todo Check for unboundedness
        }

        //! Make sure the given TreeNode is a sparta::PortSet
        void ensureParentIsPortSet_(TreeNode * parent);

        //! Does this port participate in auto precedence?
        bool participate_in_auto_precedence_ = true;

    protected:

        //! Are any InPorts connected to this OutPort to schedule
        //! stuff continuing?
        bool continuing_ = true;

        //! The name of this port
        const std::string name_;

        //! List of bound ports
        std::vector<Port *> bound_ports_;

        //! Explicit consumer handler registered via registerConsumerHandler
        SpartaHandler explicit_consumer_handler_{"base_port_null_consumer_handler"};
    };

    // Forward declaration
    class OutPort;

    //! \class InPort
    //! \brief Base class for all InPort types
    class InPort : public Port
    {
    public:
        /**
         * \brief Base class for all InPort types (sparta::DataInPort,
         *        sparta::SignalInPort, sparta::SyncInPort)
         * \param portset The PortSet this InPort belongs to
         * \param name    The name of this InPort
         * \param delivery_phase The SchedulingPhase this port delivers its data
         */
        InPort(TreeNode* portset, const std::string & name,
               sparta::SchedulingPhase delivery_phase) :
            Port(portset, Port::Direction::IN, name),
            delivery_phase_(delivery_phase)
        { }

        /**
         * \brief Register a handler to this Port (must be
         *        Direction::IN) to handle data arrival
         * \param handler The handler to be invoked when data is sent
         *                to this port.  There can be only one.
         * \param phase The phase the handler should be called in.
         *              Default is PortUpdate
         *
         * When data arrives on this InPort, the given handler will be
         * called with the data that arrived.  The data will still be
         * available on the Port and retrivable via pullData and
         * peekData.
         *
         * The time (phase) in which the handler is called is
         * dependent on the delay given on the in port + the send
         * delay.  If the total delay is zero, then the handler will
         * be invoked in the SchedulingPhase::Tick phase, otherwise it
         * will be invoked on the SchedulingPhase::PortUpdate phase.
         * The biggest difference is when pipline collection will
         * occur.  If the the handler is invoked on the
         * SchedulingPhase::Tick phase and the handler updates a
         * collected resource, it will *not* be collected as expected
         * and will have to be manually collected.
         *
         */
        void registerConsumerHandler(const SpartaHandler & handler)
        {
            sparta_assert(!explicit_consumer_handler_,
                        "Only one handler/callback is supported on this port: " << getName() <<
                        " \n\tCurrent registered handler: " << explicit_consumer_handler_.getName() <<
                        " \n\tTrying to register: " << handler.getName());

            explicit_consumer_handler_ = handler;

            // Let subclasses check out the handler...
            registerConsumerHandler_(handler);
        }

        /**
         * \brief Add an event "listener" to this port
         * \param consumer A UniqueEvent or Event that will be
         *                 scheduled when data arrives on this port.
         *                 PayloadEvent types are not supported.
         * \throws sparta::SpartaException if the direction != Port::Direction::IN
         *
         * The given listener will be prioritized after a payload
         * delivery to the port, so the user does not have to be
         * concerned on ordering.
         *
         * This method can \b only be called before the TreeNodes are
         * finalized to ensure proper DAG ordering.  The best practice
         * here is to register the listener at sparta::Resource
         * construction time.
         */
        void registerConsumerEvent(Scheduleable & consumer)
        {
            sparta_assert(getDirection() == Direction::IN,
                        "You cannot register a consumer on an OUT port -- that doesn't make sense: "
                        << getName() << " consumer being registered: " << consumer.Scheduleable::getLabel());
            // sparta_assert(getPhase() < TREE_FINALIZED);
            sparta_assert(isBound() == false,
                        "You cannot register a consuming event after the port is bound.  \n\tPort: '"
                        << getName() << "' Event: '" << consumer.Scheduleable::getLabel() << "'"
                        << "\n\tIf this is happening from sparta::Unit auto-precedence, set this Port's "
                        << "\n\tauto-precedence rule to false by calling the Port's method participateInAutoPrecedence(false)");

            port_consumers_.push_back(&consumer);
        }

        /**
         * \brief Bind to an OutPort
         * \param out The OutPort to bind to. The data and event types must be the
         * same
         */
        void bind(Port * out) override
        {
            if(out->getDirection() != Direction::OUT) {
                throw SpartaException("ERROR: Attempt to bind an inny: '" + out->getName() +
                                    "' to an inny: '" + getName() + "'");
            }

            // Make the OutPort do all the work including calling
            // bind_() further down...
            out->bind(this);
        }

        /**
         * \brief Get the list of port tick consumers
         * \return The list of consumer of this Port
         *
         * This is a list of consumers on this port, if it were a
         * zero-cycle port.
         */
        const ScheduleableList & getPortTickConsumers() const {
            return port_consumers_;
        }

        //! Get the Scheduling phase this port was directed to deliver
        //! its data on.
        sparta::SchedulingPhase getDeliverySchedulingPhase() const {
            return delivery_phase_;
        }

        /**
         * \brief Ensure data entering this Port is handled before
         *        data on another.
         *
         * \param consumer The port to follow this port
         */
        void precedes(InPort & consumer)
        {
            sparta_assert(getScheduleable_().getSchedulingPhase() == consumer.getScheduleable_().getSchedulingPhase(),
                          "ERROR: You cannot set precedence between two Ports on different phases: "
                          "producer: " << getLocation() << " consumer: " << consumer.getLocation());
            getScheduleable_().precedes(consumer.getScheduleable_());
        }

    protected:

        //! Return the internally used Scheduleable for precedence
        virtual Scheduleable & getScheduleable_() = 0;

        //! The OutPort will call bind_ and setProducerPrecedence_
        friend OutPort;

        //! Methods used for precedence have access to the internal scheduleable
        friend InPort& operator>>(const GlobalOrderingPoint&, InPort&);
        friend const GlobalOrderingPoint& operator>>(InPort&, const GlobalOrderingPoint&);

        //! Let derived classes look over the registered consumer handler
        virtual void registerConsumerHandler_(const sparta::SpartaHandler &) {}

        //! Allow derived InPort types to set up precedence between a
        //! producer on an OutPort and the InPort's internal events
        virtual void setProducerPrecedence_(Scheduleable * producer) {
            (void) producer;
        };

        //! Called by the OutPort, remember the binding
        virtual void bind_(Port * outp) {
            bound_ports_.push_back(outp);
        }

        //! Common method for checking phasing.
        void checkSchedulerPhaseForZeroCycleDelivery_(const sparta::SchedulingPhase & user_callback_phase)
        {
            (void) user_callback_phase;
            sparta_assert(user_callback_phase >= scheduler_->getCurrentSchedulingPhase(),
                        "\n\n\tThe currently firing event: '" <<
                        scheduler_->getCurrentFiringEvent()->getLabel() <<
                        "' is in SchedulingPhase::" << scheduler_->getCurrentSchedulingPhase() <<
                        "\n\tand is driving an OutPort that's connected to a zero-cycle Inport: " << getLocation() <<
                        "\n\tUnfortunately, this InPort's registered handler '" <<
                        explicit_consumer_handler_.getName() << "' is in phase SchedulingPhase::" << user_callback_phase <<
                        ".\n\n\tThis won't work for a for a zero-cycle out_port->in_port send (where send delay == 0) "
                        "since an event on a higher phase cannot schedule an event on a lower phase within the same cycle."
                        "\n\n\tTo fix this, in the constructor of InPort '" << getName() <<
                        "' move the registered handler to at least 'sparta::SchedulingPhase::" <<
                        scheduler_->getCurrentSchedulingPhase() << "' or a later phase.\n\t\tExample: " <<
                        getName() << "(..., sparta::SchedulingPhase::" <<
                        scheduler_->getCurrentSchedulingPhase() << ");\n\n"
                        "\tOR you add a cycle delay to the InPort '" << getName() << "' via its last construction argument.\n\n");
        }

        //! List of consumer events to be notified when data is
        //! received by this port. Only valid on Direction::In ports.
        ScheduleableList port_consumers_;

        //! The scheduler used
        Scheduler * scheduler_ = nullptr;

        //! The receiving clock
        const Clock * receiver_clock_ = nullptr;

        //! The delivery phase of this InPort
        sparta::SchedulingPhase delivery_phase_;
    };

    //! \class OutPort
    //! \brief Base class for all OutPort types
    class OutPort : public Port
    {
    public:
        /**
         * \brief Base class for all OutPort types (sparta::DataOutPort,
         *        sparta::SignalOutPort, sparta::SyncOutPort)
         * \param portset The PortSet this InPort belongs to
         * \param name    The name of this InPort
         *
         * \param presume_zero_delay Used in automatic binding (via
         *        sparta::Unit).  If set to true, and any bound InPort
         *        has a delay of 0, the framework will automatically
         *        set a precedence between any registered producers on
         *        this port and the consumer of the bound in port.
         */
        OutPort(TreeNode* portset, const std::string & name,
                bool presume_zero_delay) :
            Port(portset, Port::Direction::OUT, name),
            presume_zero_delay_(presume_zero_delay)
        { }

        /**
         * \brief Add an event "producer" to this port
         * \param producer A Scheduleable type that might be
         *                 scheduled before data is driven on this port.
         *
         * When data is sent on this OutPort in zero-cycles, it is
         * guaranteed that any and all consumers on the paired InPorts
         * will be scheduled \b after the registered producing event
         * within the same cycle.
         *
         * This method can \b only be called before the TreeNodes are
         * finalized to ensure proper DAG ordering.  The best practice
         * here is to register the listener at sparta::Resource
         * construction time.
         */
        void registerProducingEvent(Scheduleable & producer)
        {
            sparta_assert(isBound() == false,
                        "You cannot register a producing event after the port is bound.  \n\tPort: '"
                        << getName() << "' Event: '" << producer.Scheduleable::getLabel() << "'"
                        << "\n\tIf this is happening from sparta::Unit auto-precedence, set this Port's "
                        << "\n\tauto-precedence rule to false by calling the Port's method participateInAutoPrecedence(false)");

            port_producers_.push_back(&producer);

            // Let derived classes know about it for precedence
            this->registerProducingEvent_(producer);
        }

        /**
         * \brief Add an InPort "producer" to this OutPort
         * \param producer An InPort whose handler will most likely drive this OutPort
         *
         * When data is sent on this OutPort in zero-cycles, it is
         * guaranteed that any and all consumers on the paired InPorts
         * will be scheduled \b after the producing InPort within the
         * same cycle.
         *
         * This method can \b only be called before the TreeNodes are
         * finalized to ensure proper DAG ordering.  The best practice
         * here is to register the listener at sparta::Resource
         * construction time.
         */
        void registerProducingPort(InPort & producer)
        {
            sparta_assert(isBound() == false,
                          "You cannot register a producing port after the port is bound.  \n\tOutPort: '"
                          << getName() << "' InPort: '" << producer.getLocation() << "'"
                          << "\n\tIf this is happening from sparta::Unit auto-precedence, set this Port's "
                          << "\n\tauto-precedence rule to false by calling the Port's method participateInAutoPrecedence(false)");

            port_producers_.push_back(&producer.getScheduleable_());

            // Let derived classes know about it for precedence
            this->registerProducingEvent_(producer.getScheduleable_());
        }

        /**
         * \brief Bind to an InPort
         * \param in The DataInPort to bind to. The data and event types must be the
         *           same
         */
        void bind(Port * in) override
        {
            if(in->getDirection() != Port::Direction::IN) {
                throw SpartaException("ERROR: Attempt to bind an outty: '" + in->getName() +
                                    "' to an outty: '" + getName() + "'");
            }

            if(!sync_port_) {
                sparta_assert(in->getClock()->getFrequencyMhz() == getClock()->getFrequencyMhz(),
                            "Trying to bind two ports that are on different clocks with different freq. "
                            "Recommend using SyncPorts");
            }

            sparta_assert(!isAlreadyBound(in),
                        "Port: '" << getLocation()
                        << "' is already bound to '" << in->getLocation() << "' ");

            InPort * inp = nullptr;
            if((inp = dynamic_cast<InPort *>(in)) == 0) {
                throw SpartaException("ERROR: Could not cast '" +
                                    in->getName() + "' to an InPort for some reason...");
            }

            // This outport precedes the newly binded in port if the
            // delay of the inport is zero and the modeler intends to
            // use the OutPort as zero delay.
            //
            // The precedence that's established is the following, but
            // only if the producer, the inport's internal events, and
            // the consumer event are all in the same phase.
            //
            //          ,-----------------------------------------------.
            //          |                                               V
            // producer -> [      inport internal delivery      ]* -> consumer
            //             [ inport handler delivery (optional) ]
            //
            //
            // * The inport -> consumer precedence is established
            //   during the consumer registration from a call to
            //   InPort::registerConsumerEvent()
            //
            if((inp->getPortDelay() == 0) && presume_zero_delay_) {
                for(auto & pd : port_producers_)
                {
                    for(auto & cons : inp->port_consumers_) {
                        sparta_assert(pd != cons,
                                    "Somehow, someway, '" << pd->getLabel() << "' is registered "
                                    << "as a producer of Port: '" << getLocation()
                                    << "' and, at the same time, a consumer of Port: '"
                                    << inp->getLocation() << "'");
                        if(pd->getSchedulingPhase() == cons->getSchedulingPhase()) {
                            std::string reason = "Port::bind(" + getLocation() + "->" + inp->getLocation() + ")";
                            pd->precedes(cons, reason);
                        }
                    }
                    inp->setProducerPrecedence_(pd);
                }
            }
            bound_ports_.push_back(inp);
            inp->setContinuing(continuing_);
            inp->bind_(this);
        }

    protected:

        //! Let derived classes know about a registered producing
        //! event.  Most likely ignored by OutPorts until binding.
        virtual void registerProducingEvent_(Scheduleable &) {}

        //! List of producers driving this port.  This list is
        //! populated by a modeler register producer events.  Only
        //! valid on Direction::Out ports.
        ScheduleableList port_producers_;

        //! Presume that data sent on this OutPort to be zero-delay
        bool presume_zero_delay_ = true;

        //! Is this port a syncport?
        bool sync_port_ = false;
    };

    //////////////////////////////////////////////////////////////////////
    // Binding methods
    //////////////////////////////////////////////////////////////////////

    /**
     * \brief Bind two ports together
     * \param p1 First port to bind to
     * \param p2 Second port to bind to
     *
     * This method will bi-directionally bind two ports together
     */
    inline void bind(Port * p1, Port * p2) {
        sparta_assert(p1 != nullptr);
        sparta_assert(p2 != nullptr);
        p1->bind(p2);
    }

    /**
     * \brief Bind two ports together
     * \param p1 First port to bind to
     * \param p2 Second port to bind to
     *
     * This method will bi-directionally bind two ports together
     */
    inline void bind(Port & p1, Port & p2) {
        bind(&p1, &p2);
    }

    /**
     * \brief Bind two ports together
     * \param p1 First port to bind to
     * \param p2 Second port to bind to
     *
     * This method will bi-directionally bind two ports together
     */
    inline void bind(Port * p1, Port & p2) {
        sparta_assert(p1 != nullptr);
        bind(p1, &p2);
    }

    /**
     * \brief Bind two ports together
     * \param p1 First port to bind to
     * \param p2 Second port to bind to
     *
     * This method will bi-directionally bind two ports together
     */
    inline void bind(Port & p1, Port * p2) {
        sparta_assert(p2 != nullptr);
        bind(&p1, p2);
    }
}
