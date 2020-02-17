// <SignalPort> -*- C++ -*-

/**
 * \file   SignalPort.hpp
 *
 * \brief  File that defines the SignalInPort
 */

#ifndef __SIGNAL_PORT_H__
#define __SIGNAL_PORT_H__
#include <set>
#include <list>
#include <unordered_map>

#include "sparta/ports/Port.hpp"
#include "sparta/utils/DataContainer.hpp"
#include "sparta/events/Precedence.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/UniqueEvent.hpp"

namespace sparta
{

    //////////////////////////////////////////////////////////////////////
    // Signal Ports
    //////////////////////////////////////////////////////////////////////

    ///! Forward declaration
    class SignalInPort;

    /**
     * \class SignalOutPort
     * \brief Class that defines an SignalOutPort
     *
     * The purpose of the SignalOutPort is to provide a singular point
     * of signal delivery when sending a signal to a component.
     * SignalOutPorts are intended to bind to SignalInPorts only and
     * bind to many SignalInPorts.  Signal In/Out Ports are less
     * expensive than data ports as there is no data to cache.
     *
     * <br>
     * Example:
     * \code
     *     sparta::SignalOutPort a_delay_out("a_delay_out");
     *     sparta::SignalInPort  a_delay_in ("a_delay_in");
     *     sparta::bind(a_delay_out, a_delay_in);
     *
     *     // set up code for clocks, callbacks, etc
     *     // ...
     *
     *     // Send a signal
     *     const sparta::Clock::Cycle when = 0;
     *     a_delay_out.send(when);
     * \endcode
     */
    class SignalOutPort final : public OutPort
    {
    public:

        /**
         * \brief Construct an param
         *
         * \param portset Name of the sparta::PortSet this port belongs to
         * \param name Name of the port
         * \param presume_zero_delay For precedence, presume a
         *        zero-delay send() on this OutPort, i.e. a send call
         *        with zero cycle delay
         */
        SignalOutPort(TreeNode * portset, const std::string & name,
                      bool presume_zero_delay = true) :
            OutPort(portset, name, presume_zero_delay)
        {
            sparta_assert(name.length() != 0, "You cannot have an unnamed port.");
        }

        /**
         * \brief Bind to an SignalInPort
         * \param in The SignalInPort to bind to. The data and event types must be the
         * same
         *
         * \note Note that after this function call, the SignalOutPort knows about the
         * SignalInPort, but the SignalInPort knows \b nothing about the SignalOutPort.  This is a
         * uni-directional binding.  For a complete binding, use the sparta::bind
         * global method.
         */
        void bind(Port * in) override;

        //! Promote base class bind method for references
        using Port::bind;

        /**
         * \brief Send data to bound receivers
         * \param rel_time The relative time for sending
         *
         * Send data to bound SignalInPorts, but only after the relative time has
         * expired.  For example, to send the data true to consumers 2 cycles
         * from now, call the method like so:
         * \code
         * signal_out_port->send(&clk, 2);
         * \endcode
         */
        void send(sparta::Clock::Cycle rel_time = 0);

        //! No making copies!
        SignalOutPort(const SignalOutPort &) = delete;

        //! No assignments
        SignalOutPort & operator=(const SignalOutPort &) = delete;

    private:

        //! The bound in ports for this out port
        std::vector <SignalInPort*> bound_in_ports_;
    };

    /**
     * \class SignalInPort
     * \brief Class that defines an SignalInPort
     *
     * The purpose of the SignalInPort is to provide a singular point
     * of signal reception when receiving a signal from a component.
     * SignalInPorts are intended to bind to SignalOutPorts only and
     * can bind to many SignalOutPorts.
     *
     * The main distinction between an SignalOutPort and an
     * SignalInPort, is that an SignalInPort is the observable port,
     * allowing a consumer to be notified via an Event on an incoming
     * signal. An SignalInPort can have multiple observers.
     *
     * <br>
     * Example:
     * \code
     *     // Create an SignalOutPort and an SignalInPort and bind them
     *     sparta::SignalOutPort a_delay_out("a_delay_out");
     *     sparta::SignalInPort  a_delay_in ("a_delay_in");
     *     sparta::bind(a_delay_out, a_delay_in);
     *
     *     // Create a callback and attached to the SignalInPort
     *     sparta::SpartaHandler cb = CREATE_SPARTA_HANDLER(MyClass, myMethod);
     *     a_delay_in.registerEvent(cb);
     *
     *     // Signature of handler:
     *     //void MyClass::myMethod() {}
     *
     * \endcode
     *
     *
     */
    class SignalInPort final : public InPort, public DataContainer<bool>
    {
    public:

        /**
         * \brief Create an SignalInPort with the given name
         * \param portset Name of the sparta::PortSet this port belongs to
         * \param name The name of the SignalInPort (optional)
         * \param delay Delay added to the sender
         */
        SignalInPort(TreeNode * portset, const std::string & name,
                     sparta::SchedulingPhase phase, sparta::Clock::Cycle delay) :
            InPort(portset, name, phase),
            DataContainer<bool>(getClock()),
            port_delay_(delay),
            signal_events_(this)
        {
            receiver_clock_ = getClock();
            scheduler_ = receiver_clock_->getScheduler();

            sparta_assert(receiver_clock_ != nullptr,
                        "SignalInPort " << name << " does not have a clock");
            sparta_assert(name.length() != 0, "You cannot have an unnamed port.");
            user_signal_delivery_.
                reset(new PhasedUniqueEvent(&signal_events_, name + "_forward_event",
                                            phase, CREATE_SPARTA_HANDLER(SignalInPort, receiveSignal_)));
        }

        /**
         * \brief Construct a SignalInPort with a default delivery phase based on the delay
         * \param portset Pointer to the portset to drop the port into
         * \param name    The name of this port
         * \param delay   Delay added to the sender
         *
         * If the delay == 0, this SignalInPort will deliver its data
         * on the sparta::SchedulingPhase::Tick.  If the delay > 0, then
         * the data will be delivered on the
         * sparta::SchedulingPhase::PortUpdate phase.  This can be
         * overridden using the other SignalInPort constructor.
         */
        SignalInPort(TreeNode* portset, const std::string & name, sparta::Clock::Cycle delay = 0) :
            SignalInPort(portset, name, (delay == 0 ?
                                         sparta::SchedulingPhase::Tick :
                                         sparta::SchedulingPhase::PortUpdate), delay)
        {}

        //! No making copies
        SignalInPort(const SignalInPort &) = delete;

        //! No assignments
        SignalInPort & operator=(const SignalInPort &) = delete;

        /**
         * \brief Bind to an SignalOutPort
         * \param out The SignalOutPort to bind to. The data and event types must be the
         * same
         *
         * \note Note that after this function call, the SignalInPort knows about the
         * SignalOutPort, but the SignalOutPort knows \b nothing about the SignalInPort.  This is a
         * uni-directional binding.  For a complete binding, use the sparta::bind
         * global method.
         */
        void bind(Port * out) override;

        //! Promote base class bind method for references
        using Port::bind;

        /**
         * \brief Get the port delay associated with this port
         * \return The port delay
         */
        Clock::Cycle getPortDelay() const override final {
            return port_delay_;
        }

        //! Do events from this port keep simulation going?
        //! \param continuing true [default] if yes, it does; false otherwise
        void setContinuing(bool continuing) override final {
            // signal_arrive_event_.setContinuing(continuing);
            user_signal_delivery_->setContinuing(continuing);
        }

    private:

        Scheduleable & getScheduleable_() override final {
            return user_signal_delivery_->getScheduleable();
        }

        void setProducerPrecedence_(Scheduleable * pd) override final {
            if(pd->getSchedulingPhase() == user_signal_delivery_->getSchedulingPhase()) {
                pd->precedes(*user_signal_delivery_, "Port::bind of OutPort to " + getName() + ": '" +
                             pd->getLabel() + "' is a registered driver");
            }
        }

        void registerConsumerHandler_(const SpartaHandler & handler) override final {
            sparta_assert(handler.argCount() == 0,
                        "SignalInPort: " << getName()
                        << ": The handler associated with the SignalInPort must not expect an argument: "
                        << handler.getName());
            // Help the user identify their events/callbacks from the Scheduler debug
            handler_name_ = getName() + "<SignalInPort>[" + explicit_consumer_handler_.getName() + "]";
            user_signal_delivery_->setLabel(handler_name_.c_str());
        }

        // The SignalOutPort will send the transaction over as well as bind
        friend class SignalOutPort;

        //! Called by the DataOutPort, remember the binding
        void bind_(Port * out) override final
        {
            InPort::bind_(out);
            for(auto & consumer : port_consumers_)
            {
                if(consumer->getSchedulingPhase() == user_signal_delivery_->getSchedulingPhase()) {
                    user_signal_delivery_->precedes(consumer, "Port::bind(" + getName() + "->" + out->getName() + "),'"
                                                    + consumer->getLabel() + "' is registered driver");
                }
            }
        }

        /**
         * \brief Called bu the SignalOutPort
         * \param rel_time Relative offset for scheduling
         * \param outport_presumes_zero_delay true if the outport sending data assumes a zero delay
         *
         * Data is being sent from an event driving an OutPort.  If
         * the total delay between the sending the receiving is zero,
         * then the data is immediately dropped on the InPort and the
         * user's registered handler is scheduled to accept the data
         * within the same cycle.  The user's handler's
         * SchedulingPhase MUST be either equal to or greater than the
         * phase of the sender.  Otherwise, the user will get a
         * sparta::Scheduler precedence issue.
         */
        void send_(sparta::Clock::Cycle rel_time)
        {
            const uint32_t total_delay = rel_time + port_delay_;
            // Most of the time there is a delay.
            if(SPARTA_EXPECT_FALSE(total_delay == 0))
            {
                checkSchedulerPhaseForZeroCycleDelivery_(user_signal_delivery_->getSchedulingPhase());
                if(user_signal_delivery_->getSchedulingPhase() == scheduler_->getCurrentSchedulingPhase()) {
                    // Receive the port data now
                    receiveSignal_();
                    return;
                }
            }
            user_signal_delivery_->schedule(total_delay, receiver_clock_);
       }

        //! The additional port delay for scheduling
        const Clock::Cycle port_delay_;

        //! Internal event
        void receiveSignal_() {
            // Set's the timestamp
            DataContainer<bool>::setData_(true);
            if(SPARTA_EXPECT_TRUE(explicit_consumer_handler_)) {
                explicit_consumer_handler_();
            }
        }

        //! The handler name for scheduler debug
        std::string handler_name_;

        //! Event Set for this port
        sparta::EventSet signal_events_;

        //! Unique event for scheduling the signal arrival
        // sparta::UniqueEvent<SchedulingPhase::PortUpdate> signal_arrive_event_ {
        //     &signal_events_, getName() + "_port_delivery_event",
        //         CREATE_SPARTA_HANDLER(SignalInPort, receiveSignal_)};
        std::unique_ptr<PhasedUniqueEvent> user_signal_delivery_;
    };


    ////////////////////////////////////////////////////////////////////////////////
    // External definitions

    inline void SignalInPort::bind(Port * out)
    {
        SignalOutPort * outp;
        if((outp = dynamic_cast<SignalOutPort*>(out)) == 0) {
            throw SpartaException("ERROR: Attempt to bind to something that isn't a SignalOutPort: '" +
                                out->getName() + "' to '" + getName() + "'");
        }
        InPort::bind(outp);
    }

    inline void SignalOutPort::bind(Port * in)
    {
        SignalInPort * inp;
        if((inp = dynamic_cast<SignalInPort *>(in)) == 0) {
            throw SpartaException("ERROR: Attempt to bind to something that isn't a SignalInPort: '" +
                                in->getName() + "' to '" + getName() + "'");
        }
        OutPort::bind(in);
        bound_in_ports_.push_back(inp);
    }

    inline void SignalOutPort::send(sparta::Clock::Cycle rel_time)
    {
        sparta_assert(!bound_in_ports_.empty(),
                    "ERROR! Attempt to send data on unbound port: " << getLocation());
        for(SignalInPort* itr : bound_in_ports_) {
            itr->send_(rel_time);
        }
    }
}



// __SIGNAL_PORT_H__
#endif
