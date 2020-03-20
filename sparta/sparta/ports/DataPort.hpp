// <Port> -*- C++ -*-


/**
 * \file   DataPort.hpp
 *
 * \brief  File that defines Data[In,Out]Port<DataT>
 */

#ifndef __DATA_PORT_H__
#define __DATA_PORT_H__

#include <set>
#include <vector>

#include "sparta/ports/Port.hpp"
#include "sparta/utils/DataContainer.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/PayloadEvent.hpp"
#include "sparta/events/Precedence.hpp"
#include "sparta/events/Scheduleable.hpp"
#include "sparta/collection/Collectable.hpp"

namespace sparta
{

    //////////////////////////////////////////////////////////////////////
    // Data Ports
    //////////////////////////////////////////////////////////////////////

    ///! Forward declaration
    template<class DataT>
    class DataInPort;

    /**
     * \class DataOutPort
     *
     * \brief Class that defines an DataOutPort for data writes. See
     * \ref timed_primitives for more details.
     *
     * The purpose of the DataOutPort is to provide a singular point of
     * data delivery when sending data to a component.  DataOutPorts are
     * intended to bind to DataInPorts only and can bind to many DataInPorts
     * as long as the DataT is the same.  The types of data and events
     * that the ports share must be the same or the ports will not
     * bind.
     *
     * The modeler must expect the data being sent to be _copied_ into
     * the port for future (or immediate) delivery.
     *
     * <br>
     * Example:
     * \code
     *     sparta::PortSet port_set(nullptr);  // PortSet with no parent
     *     sparta::DataOutPort<uint32_t> a_delay_out(&port_set, "a_delay_out");
     *     sparta::DataInPort<uint32_t>  a_delay_in (&port_set, "a_delay_in");
     *
     *     // Bi-directional binding; no extra functionality,
     *     // just easier debug
     *     sparta::bind(a_delay_out, a_delay_in);
     *
     *     // Could do singular binding, if desired
     *     //a_delay_out.bind(a_delay_in);
     *
     *     // set up callbacks, clocks, etc...
     *     // ...
     *
     *     // send some data (1234) to be sent this cycle
     *     const sparta::Clock::Cycle when = 0;
     *     a_delay_out.send(1234, when);
     *
     * \endcode
     *
     * \todo Add support for requiring bounded ports
     * \todo Add support for automatic collection
     */
    template<class DataT>
    class DataOutPort final : public OutPort
    {
    public:
        //! A typedef for the type of data this port passes.
        typedef DataT DataType;

        /**
         * \brief Construct a DataOutPort within the given PortSet
         * \param portset Name of the sparta::PortSet this port belongs to
         * \param name Name of the port
         * \param presume_zero_delay For precedence, presume a
         *        zero-delay send() on this OutPort, i.e. a send call
         *        with zero cycle delay
         *
         */
        DataOutPort(TreeNode* portset, const std::string & name,
                    bool presume_zero_delay = true) :
            OutPort(portset, name, presume_zero_delay)
        {
            sparta_assert(name.length() != 0, "You cannot have an unnamed port.");
            sparta_assert(getClock() != nullptr, "DataOutPort '" << name << "' created without a clock");
        }

        //! No making copies!
        DataOutPort(const DataOutPort &) = delete;

        //! No making assignments!
        DataOutPort & operator=(const DataOutPort &) = delete;

        /**
         * \brief Bind to an DataInPort
         * \param in The DataInPort to bind to. The data and event types must be the
         * same
         *
         * \note Note that after this function call, the DataOutPort knows about the
         * DataInPort, but the DataInPort knows \b nothing about the DataOutPort.  This is a
         * uni-directional binding.  For a complete binding, use the sparta::bind
         * global method.
         */
        void bind(Port * in) override
        {
            DataInPort<DataT> * inp;
            if((inp = dynamic_cast<DataInPort<DataT> *>(in)) == 0) {
                throw SpartaException("ERROR: Attempt to bind DataInPort of a disparate types: '" +
                                    in->getLocation() + "' to '" + getLocation() + "'");
            }
            OutPort::bind(in);
            bound_in_ports_.push_back(inp);
        }

        //! Promote base class bind method for references
        using Port::bind;

        /**
         * \brief Send data to bound receivers
         * \param dat The data to send
         * \param rel_time The relative time for sending
         *
         * Send data to bound DataInPorts, but only after the relative time has
         * expired.  For example, to send the data 'true' to consumers 2 cycles
         * from now, call the method like so:
         *
         * \code
         * out_port->send(true, 2);
         * \endcode
         *
         * The sparta::Clock that is used is the clock that the Port
         * gather from their parent TreeNode.
         */
        void send(const DataT & dat, sparta::Clock::Cycle rel_time = 0)
        {
            sparta_assert(!bound_in_ports_.empty(),
                          "ERROR! Attempt to send data on unbound port: " << getLocation());
            for(DataInPort<DataT>* itr : bound_in_ports_) {
                itr->send_(dat, rel_time);
            }
        }

        /*! \brief Determine if this DataOutPort has any connected
         *        DataInPort where the data is to be delivered on the
         *        given cycle.
         *
         *
         * \param rel_cycle The relative cycle (from now) the data
         *                  will be delivered
         * \return true if driven at the given cycle (data not yet delivered)
         */
        bool isDriven(Clock::Cycle rel_cycle) const override {
            for(DataInPort<DataT>* itr : bound_in_ports_) {
                if(itr->isDriven(rel_cycle)) {
                    return true;
                }
            }
            return false;
        }

        //! \brief Does this DataOutPort have _any_ DataInPort's where
        //!        the data is not yet delivered?
        //!
        //! \return true if driven at all (data not yet delivered)
         bool isDriven() const override {
            for(DataInPort<DataT>* itr : bound_in_ports_) {
                if(itr->isDriven()) {
                    return true;
                }
            }
            return false;
        }

        /**
         * \brief Cancel all outstanding port sends regardless of criteria
         * \return The number of canceled payloads (could include duplicates if multiply bound)
         *
         * This method will cancel all scheduled deliveries of
         * previously sent data on this DataOutPort.  Data already
         * delivered will not be cleared in any subsequent DataInPorts
         */
        uint32_t cancel()
        {
            uint32_t cancel_cnt = 0;
            for(DataInPort<DataT>* itr : bound_in_ports_) {
                cancel_cnt += itr->cancel();
            }
            return cancel_cnt;
        }

        /**
         * \brief Cancel any scheduled Payload that matches the given criteria
         * \param criteria The criteria to compare; must respond to operator==
         * \return The number of canceled payloads (could include duplicates if multiply bound)
         *
         * This function does a raw '==' comparison between the
         * criteria and the stashed payloads in flight.  If match, the
         * payload is squashed before the InPort receives it and the
         * event unscheduled (if scheduled).
         */
        uint32_t cancelIf(const DataT & criteria) {
            uint32_t cancel_cnt = 0;
            for(DataInPort<DataT>* itr : bound_in_ports_) {
                cancel_cnt += itr->cancelIf(criteria);
            }
            return cancel_cnt;
        }

        /**
         * \brief Cancel any scheduled Payload that matches the given function
         * \param compare The function comparitor to use
         * \return The number of canceled payloads (could include duplicates if multiply bound)
         *
         * This function allows a user to define his/her own
         * comparison operation outside of a direct operator==
         * comparison.  See sparta::PhasedPayloadEvent::cancelIf for an
         * example.
         */
        uint32_t cancelIf(std::function<bool(const DataT &)> compare) {
            uint32_t cancel_cnt = 0;
            for(DataInPort<DataT>* itr : bound_in_ports_) {
                cancel_cnt += itr->cancelIf(compare);
            }
            return cancel_cnt;
        }

    private:
        //! The bound DataIn ports
        std::vector <DataInPort<DataT>*> bound_in_ports_;
    };

    /**
     * \class DataInPort
     * \brief Class that defines an DataInPort
     *
     * The purpose of the DataInPort is to provide a singular point of
     * data reception when receiving data from a component.  DataInPorts
     * are intended to bind to DataOutPorts only and can bind to many
     * DataOutPorts.  The types of data and events that the ports share
     * must be the same or the ports will not bind.
     *
     * The main distinction between an DataOutPort and an DataInPort, is that
     * an DataInPort is the observable port, allowing a consumer to be
     * notified on incoming data via an internal instance of a
     * sparta::PayloadEvent class. An DataInPort can have multiple
     * observers.
     *
     * Another distinction between the DataInPort and the DataOutPort is how a
     * delay is represented.  For DataInPorts, this is a construction
     * parameter, meaning that if data is sent to this port, any
     * provided delay in the DataInPort will be added to the internal
     * event before scheduling.  For DataOutPorts, the delay is given at
     * the time of sending.  If both ports have a delay, they are
     * added together.
     *
     * <br>
     * Example:
     * \code
     *     // Create an DataOutPort and an DataInPort and bind them
     *     sparta::DataOutPort<bool> a_delay_out("a_delay_out");
     *     sparta::DataInPort <bool> a_delay_in ("a_delay_in");
     *     sparta::bind(a_delay_out, a_delay_in);
     *
     *     // Create a callback and attached to the DataInPort
     *     sparta::SpartaHandler cb =
     *         CREATE_SPARTA_HANDLER_WITH_DATA(MyClass, myMethod, bool);
     *     a_delay_in.registerEvent(cb);
     *
     *     // Signature of handler:
     *     //void MyClass::myMethod(const bool &data) {}
     *
     * \endcode
     *
     *
     */
    template<class DataT>
    class DataInPort final : public InPort, public DataContainer<DataT>
    {
        // Pipeline collection type
        typedef collection::Collectable<DataT> CollectorType;

    public:

        //! Expected typedef for DataT
        typedef DataT DataType;

        /**
         * \brief Construct a DataInPort with a specific delivery phase
         * \param portset Name of the sparta::PortSet this port belongs to
         * \param name The name of the DataInPort (optional)
         * \param delivery_phase The phase where the Port is updated
         *                       with new data AND any registered
         *                       callback is called
         * \param delay Delay added to the sender
         */
        DataInPort(TreeNode* portset, const std::string & name,
                   sparta::SchedulingPhase delivery_phase, sparta::Clock::Cycle delay) :
            InPort(portset, name, delivery_phase),
            DataContainer<DataT>(getClock()),
            data_in_port_events_(this),
            port_delay_(delay)
        {
            receiver_clock_ = getClock();
            scheduler_ = receiver_clock_->getScheduler();

            sparta_assert(receiver_clock_ != nullptr,
                          "DataInPort " << name << " does not have a clock");
            sparta_assert(name.length() != 0, "You cannot have an unnamed port.");

            user_payload_delivery_.reset(new PhasedPayloadEvent<DataT>(&data_in_port_events_, name + "_forward_event",
                                                                       delivery_phase,
                                                                       CREATE_SPARTA_HANDLER_WITH_DATA(DataInPort<DataT>, receivePortData_, DataT)));
        }

        /**
         * \brief Construct a DataInPort with a default delivery phase based on the delay
         * \param portset Pointer to the portset to drop the port into
         * \param name    The name of this port
         * \param delay   Delay added to the sender
         *
         * If the delay == 0, this DataInPort will deliver its data on
         * the sparta::SchedulingPhase::Tick.  If the delay > 0, then
         * the data will be delivered on the
         * sparta::SchedulingPhase::PortUpdate phase.  This can be
         * overridden using the other DataInPort constructor.
         *
         * The reason for moving the InPort to the Tick phase for 0 cycle is that the typical use
         * case is an event in another unit is driving the OutPort in the Tick phase.  If the InPort was
         * was on the Update phase, you will get a runtime error.
         */
        DataInPort(TreeNode* portset, const std::string & name, sparta::Clock::Cycle delay = 0) :
            DataInPort(portset, name, (delay == 0 ? sparta::SchedulingPhase::Tick : sparta::SchedulingPhase::PortUpdate), delay)
        {}

        //! No making copies
        DataInPort(const DataInPort &) = delete;

        //! No assignments
        DataInPort & operator=(const DataInPort &) = delete;

        //! Check the Data types
        void bind(Port * out) override
        {
            DataOutPort<DataT> * outp;
            if((outp = dynamic_cast<DataOutPort<DataT> *>(out)) == 0) {
                throw SpartaException("ERROR: Attempt to bind DataOutPort of a disparate types: '" +
                                      out->getLocation() + "' to '" + getLocation() + "'");
            }
            InPort::bind(out);
        }

        //! Promote base class bind method for references
        using Port::bind;

        /**
         * \brief Get the port delay associated with this port
         * \return The port delay
         */
        Clock::Cycle getPortDelay() const override final {
            return port_delay_;
        }

        //! \brief Do events from this port keep simulation going?
        //! \param continuing true [default] if yes, it does; false otherwise
        void setContinuing(bool continuing) override final {
            Port::setContinuing(continuing);
            user_payload_delivery_->getScheduleable().setContinuing(continuing);
        }

        /*!
         * \brief Determine if this DataInPort is driven on the
         *        given cycle
         * \param rel_cycle The relative cycle (from now) the data
         *                  will be delivered
         * \return true if driven at the given cycle (data not yet delivered)
         * \note If the DataInPort was driven with a zero-cycle delay,
         *       this function will always return false.
         */
        bool isDriven(Clock::Cycle rel_cycle) const override {
            return user_payload_delivery_->isScheduled(rel_cycle);
        }

        /*! \brief Is this Port driven at all?
         *  \return true if driven at all (data not yet delivered)
         *  \note If the DataInPort was driven with a zero-cycle delay,
         *       this function will always return false.
         */
        bool isDriven() const override {
            return user_payload_delivery_->isScheduled();
        }

        /**
         * \brief Cancel all outstanding incoming data *not delivered*
         * \return The number of canceled payloads
         *
         * This method will cancel all scheduled deliveries of
         * previously sent data on the bound DataOutPorts.  Data
         * already delivered on this DataInPort will not be cleared.
         */
        uint32_t cancel() {
           return user_payload_delivery_->cancel();
        }

        /**
         * \brief Cancel any scheduled Payload that matches the given criteria
         * \param criteria The criteria to compare; must respond to operator==
         * \return The number of canceled payloads
         *
         * This function does a raw '==' comparison between the
         * criteria and the stashed payloads in flight.  If match, the
         * payload is squashed before the InPort receives it and the
         * event unscheduled (if scheduled).
         */
        uint32_t cancelIf(const DataT & criteria) {
            return user_payload_delivery_->cancelIf(criteria);
        }

        /**
         * \brief Cancel any scheduled Payload that matches the given function
         * \param compare The function comparitor to use
         * \return The number of canceled payloads
         *
         * This function allows a user to define his/her own
         * comparison operation outside of a direct operator==
         * comparison.  See sparta::PhasedPayloadEvent::cancelIf for an
         * example.
         */
        uint32_t cancelIf(std::function<bool(const DataT &)> compare) {
            return user_payload_delivery_->cancelIf(compare);
        }

        /*!
         * \brief Enable pipeline collection
         * \param node The TreeNode to add the collector
         */
        void enableCollection(TreeNode* node) override {
            collector_.reset(new CollectorType(node, Port::name_, 0,
                                               "Data being received on this DataInPort"));
        }

    private:

        Scheduleable & getScheduleable_() override final {
            return user_payload_delivery_->getScheduleable();
        }

        void setProducerPrecedence_(Scheduleable * pd) override final {
            if(pd->getSchedulingPhase() == user_payload_delivery_->getSchedulingPhase()) {
                pd->precedes(user_payload_delivery_->getScheduleable(), "Port::bind of OutPort to " + getName() + ": '" +
                             pd->getLabel() + "' is a registered driver");
            }
       }

        void registerConsumerHandler_(const SpartaHandler & handler) override final
        {
            sparta_assert(handler.argCount() == 1,
                        "DataInPort: " << getName()
                        << ": The handler associated with the DataInPort must take at least one argument: "
                        << handler.getName());
            handler_name_ = getName() + "<DataInPort>[" + handler.getName() + "]";
            user_payload_delivery_->getScheduleable().setLabel(handler_name_.c_str());
        }

        void bind_(Port * outp) override final
        {
            InPort::bind_(outp);
            for(auto & consumer : port_consumers_)
            {
                if(consumer->getSchedulingPhase() == user_payload_delivery_->getSchedulingPhase()) {
                    user_payload_delivery_->getScheduleable().precedes(consumer, "Port::bind(" + getName() + "->" + outp->getName() + "),'"
                                                                       + consumer->getLabel() + "' is registered consumer");
                }
            }
        }

        //! The DataOutPort will send the transaction over as well as bind
        friend class DataOutPort<DataT>;

        /*!
         * \brief Called by DataOutPort, send the data across (schedule event)
         * \param dat The Data to send over
         * \param rel_time The relative time to schedule into the future
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
        void send_(const DataT & dat, sparta::Clock::Cycle rel_time)
        {
            const uint32_t total_delay = rel_time + port_delay_;

            // Most of the time there is a delay.
            if(SPARTA_EXPECT_FALSE(total_delay == 0))
            {
                checkSchedulerPhaseForZeroCycleDelivery_(user_payload_delivery_->getSchedulingPhase());
                if(user_payload_delivery_->getSchedulingPhase() == scheduler_->getCurrentSchedulingPhase()) {
                    // Receive the port data now
                    receivePortData_(dat);
                    return;
                }
            }
            user_payload_delivery_->preparePayload(dat)->schedule(total_delay, receiver_clock_);
        }

        //! Event Set for this port
        sparta::EventSet data_in_port_events_;

        //! The User-specified delivery notification
        std::unique_ptr<PhasedPayloadEvent<DataT>> user_payload_delivery_;

        //! The handler name for scheduler debug
        std::string handler_name_;

        //! The receiving clock
        const Clock * receiver_clock_ = nullptr;

        /// Pipeline collection
        std::unique_ptr<CollectorType> collector_;

        //! Data receiving point
        void receivePortData_(const DataT & dat)
        {
            DataContainer<DataT>::setData_(dat);
            if(SPARTA_EXPECT_TRUE(explicit_consumer_handler_)) {
                explicit_consumer_handler_((const void*)&dat);
            }
            if(SPARTA_EXPECT_FALSE(collector_ != nullptr)) {
                if(SPARTA_EXPECT_FALSE(collector_->isCollected())) {
                    collector_->collect(dat);
                }
            }
        }

        //! This port's additional delay for receiving the data
        const Clock::Cycle port_delay_;
    };
}



// __DATA_PORT_H__
#endif
