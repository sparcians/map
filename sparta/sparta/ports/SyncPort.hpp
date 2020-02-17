// <SyncPort.h> -*- C++ -*-

/**
 * \file   SyncPort.hpp
 *
 * \brief  File that defines syncrhonized input/output ports.
 *
 * Explanation of ready/valid mechanism:
 *
 *   1. Receiver can drive not-ready on cycle M to indicate it cannot accept
 *      data on cycle M+1.
 *
 *   2. When receiver then drives ready on cycle N, it indicates it can
 *       accept new data on cycle N+1.
 *
 *   3. If receiver asserts not-ready on cycle M and data is sent on cycle
 *      M, then SyncPort will recirculate the data sent on cycle M,
 *      delivering it on cycle N+1.
 *
 *   4. If receiver asserts not-ready on cycle M and data is sent on cycle
 *      M' where M < M' < N+1, then SyncPort will recirculate the data sent
 *      on cycle M', delivering it on cycle N+1.  In effect, this is
 *      allowing a sender to drive valid on an arbitrary not-ready cycle,
 *      and the data is delivered when ready is finally asserted.
 *
 * Implementation of ready/valid mechanism:
 *
 *   o SyncInPort keeps track of:
 *       current value of ready
 *       previous value of ready
 *       last tick ready value changed
 *       number of sent requests not yet delivered
 *
 *   o When trying to send new data, SyncOutPort will then call
 *     SyncInPort::couldAccept_() to determine if data could be delivered
 *     on the given cycle.
 *
 *   o Data can be scheduled for sending:
 *     SyncInPort is currently ready OR
 *     SyncInPort is not ready, but became not ready this cycle OR
 *     SyncInPort is not ready, but isn't trying to deliver recirculated data

 *   o Data cannot be scheduled for sending if:
 *     (SyncInPort is trying to deliver recirculated data) AND
 *     (SyncInPort is not ready AND
 *      SyncInPort became not ready on a previous cycle) OR
 *     (SyncInPort is ready AND
 *      SyncInPort became ready on the current cycle)
 *
 *
 *  Potential race:
 *
 *      num_in_flight_ decremented first, then its value is used to allow sending vs.
 *      num_in_flight_ value used to allow sending, then value is decremented
 *
 *    This only matters if num_in_flight_ is greater than 0 AND
 *    couldAccept_() returns false.  If couldAccept_() returns false, then
 *    by definition, the receiver cannot receive data this cycle (i.e. is
 *    asserting not ready).  If couldAccept_() returns false, then
 *    getLatchedReady_() has also returned false, which means there can be
 *    no data delivered this cycle.  Therefore, I don't think this race is
 *    an issue.
 *
 * Zero cycle connections:
 *
 *    For zero-cycle connections, we don't allow one in-flight request in
 *    sync-port since we're trying to deliver the data on the same cycle it
 *    was sent.
 */

#ifndef __SPARTA_SYNC_PORT_H__
#define __SPARTA_SYNC_PORT_H__

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/DataContainer.hpp"
#include "sparta/collection/DelayedCollectable.hpp"
#include "sparta/ports/Port.hpp"
#include "sparta/events/Precedence.hpp"
#include "sparta/events/EventSet.hpp"

namespace sparta
{

    //! Forward declaration
    template<class DataT>
    class SyncInPort;

    /**
     * \class SyncOutPort
     * \brief Class that defines a synchronized DataOutPort for data writes
     *
     *  The expected sync-port use case is that the delay for sending
     *  will only be used to schedule future events, e.g. data beats on
     *  a bus.  The destination SyncInPort will handle any latch
     *  delays.  Note that since this potentially send occurs across a
     *  clock boundary, the number of cycles actually delayed won't
     *  necessarily be in the sending clock comain.
     *
     *  The rules for sending across clock boundaries are:
     *
     *      When sending from fast to slow OR between two clocks @ same frequency:
     *      1. Synchronize to posedge of slow clock
     *      2. Apply all delays in slow clock cycles
     *
     *      When sending from slow to fast
     *      1. Delay in slow clock cycles
     *      2. Synchronize to posedge of fast clock
     *
     *  The above rules are implemented by the scheduleAcrossClocks() method
     *  in Event objects.
     */
    template<class DataT>
    class SyncOutPort final : public OutPort
    {
        typedef collection::DelayedCollectable<DataT> CollectorType;

    public:
        //! A typedef for the type of data this port passes.
        typedef DataT DataType;

        /**
         * \brief Construct a synchronized output port
         *
         * \param portset Name of the sparta::PortSet this port belongs to
         * \param name Name of the port
         * \param clk The clock this port wil use for sending
         * \param presume_zero_delay For precedence, presume a
         *        zero-delay send() on this OutPort, i.e. a send call
         *        with zero cycle delay
         */
        SyncOutPort(TreeNode * portset, const std::string & name, const Clock * clk,
                    bool presume_zero_delay = true) :
            OutPort(portset, name, presume_zero_delay),
            clk_(clk), info_logger_(this, "pinfo", getLocation() + "_info")
        {
            sparta_assert(name.length() != 0, "You cannot have an unnamed port.");
            sparta_assert(clk_ != 0, "Clock ptr cannot be null in port: " << name);

            OutPort::sync_port_ = true; // prevent clock mismatch asserts
        }

        /**
         * \brief Bind to an SyncInPort
         * \param in The SyncInPort to bind to. The data and event types must be the
         * same
         */
        void bind(Port * in) override
        {
            SyncInPort<DataT> * inp;
            if((inp = dynamic_cast<SyncInPort<DataT> *>(in)) == 0) {
                throw SpartaException("ERROR: Attempt to bind SyncInPort of a disparate types: '" +
                                      in->getLocation() + "' to '" + getLocation() + "'");
            }

            // Sync ports only support one binding for now.
            sparta_assert(sync_in_port_ == 0, "Multiple bind attempts on port:" << getLocation());
            sync_in_port_ = inp;
            sparta_assert(sync_in_port_ != 0, "Cannot bind to null input port in:" << getLocation());

            OutPort::bind(in);

            bound_ports_.push_back(in);
        }

        //! Promote base class bind method for references
        using Port::bind;

        /**
         * Return whether the output port is ready to send data to the
         * input port.  This takes into account both the ready signal and
         * whether any data has been sent this cycle.
         *
         * \param send_delay_cycles Number of cycles the sender would
         *                          use in a send() call when sending.
         */
        bool isReady(sparta::Clock::Cycle send_delay_cycles = 0) const {
            sparta_assert(sync_in_port_ != 0, "isReady() check on unbound port:" << getLocation());
            return sync_in_port_->couldAccept_(clk_, send_delay_cycles);
        }

        /**
         * Return whether the output port is ready to send data to the
         * input port, present-state version.  This ONLY takes into account
         * the ready signal, and ignores whether data has been sent or not.
         */
        bool isReadyPS() const {
            sparta_assert(sync_in_port_ != 0, "isReadyPS() check on unbound port:" << getLocation());
            return sync_in_port_->getRawReady_();
        }

        /**
         *  \brief Send data on the output port
         *  \param dat The data to send
         *
         *  \return The delay in ticks from sending
         */
        uint64_t send(const DataT & dat)
        {
            return send(dat,0,false);
        }

        /**
         *  \brief Send data on the output port
         *  \param dat The data to send
         *  \param send_delay_cycles Cycles to delay before sending
         *
         *  \return The delay in ticks from sending
         */
        uint64_t send(const DataT & dat, sparta::Clock::Cycle send_delay_cycles)
        {
            return send(dat, send_delay_cycles, false);
        }

        /**
         *  \brief Send data on the output port and allow slide
         *  \param dat The data to send
         *
         *  \return The delay in ticks from sending
         */
        uint64_t sendAndAllowSlide(const DataT & dat)
        {
            return send(dat, 0, true);
        }

        /**
         *  \brief Send data on the output port and allow slide
         *  \param dat The data to send
         *  \param send_delay_cycles Cycles to delay before sending
         *
         *  \return The delay in ticks from sending
         */
        uint64_t sendAndAllowSlide(const DataT & dat, sparta::Clock::Cycle send_delay_cycles)
        {
            return send(dat, send_delay_cycles, true);
        }

        /**
         *  \brief Send data on the output port
         *  \param dat The data to send
         *  \param send_delay_cycles Cycles to delay before sending
         *  \param allow_slide Allows the receive to slide relative to
         *                     previous requests
         *
         *  \return The delay in ticks from sending
         */
        uint64_t send(const DataT & dat, sparta::Clock::Cycle send_delay_cycles, const bool allow_slide)
        {
            sparta_assert(sync_in_port_ != 0, "Attempting to send data on unbound port:" << getLocation());
            sparta_assert(clk_->isPosedge(), "Posedge check failed in port:" << getLocation());

            Clock::Cycle send_cycle = clk_->currentCycle() + send_delay_cycles;
            if (SPARTA_EXPECT_FALSE(info_logger_)) {
                info_logger_ << "SEND @" << send_cycle
                             << " allow_slide=" << allow_slide
                             << " # " << dat;
            }
            bool is_fwd_progress = true;
            uint64_t sched_delay_ticks = sync_in_port_->send_(dat, clk_, send_delay_cycles,
                                                              allow_slide, is_fwd_progress);

            if(SPARTA_EXPECT_FALSE(collector_ != nullptr)) {
                collector_->collectWithDuration(dat, send_delay_cycles, 1);
            }

            sparta_assert(send_cycle > prev_data_send_cycle_,
                          getLocation()
                          << ": trying to send at cycle "
                          << send_cycle
                          << ", which is not later than the previous send cycle: "
                          << prev_data_send_cycle_
                          << "; SyncOutPorts are expected to send at most once per cycle");
            prev_data_send_cycle_ = send_cycle;
            return sched_delay_ticks;
        }

        /**
         * \brief Compute the next available relative cycle for sending data, assuming all
         *        of the specified number of beats are sent starting at the
         *        current cycle plus the send_delay_cycles.
         *  \param send_delay_cycles Cycles to delay before sending
         *  \param num_beats Number of beats to be sent
         *
         *  \return The next available cycle for sending data
         */
        Clock::Cycle computeNextAvailableCycleForSend(sparta::Clock::Cycle send_delay_cycles, const uint32_t num_beats)
        {
            sparta_assert(sync_in_port_ != 0, "Attempting to send data on unbound port:" << getLocation());
            sparta_assert(clk_->isPosedge(), "Posedge check failed in port:" << getLocation());

            // Start at the current clock with the specified delay
            Clock::Cycle current_cycle = clk_->currentCycle();
            Scheduler::Tick current_tick = clk_->currentTick();

            // Send each beat of data with a slide, so know previous slide
            Scheduler::Tick prev_data_arrival_tick =
                sync_in_port_->getPrevDataArrivalTick_();

            // Send one extra beat (<=) to represent the N+1 beat to be sent
            for(uint32_t beat_count = 0; beat_count <= num_beats; ++beat_count) {
                uint64_t sched_delay_ticks =
                    sync_in_port_->computeSendToReceiveTickDelay_(clk_,
                                                                  send_delay_cycles + beat_count,
                                                                  true /*allow_slide*/,
                                                                  prev_data_arrival_tick);
                prev_data_arrival_tick = current_tick + sched_delay_ticks;
            }

            // At this point, the prev_data_arrival_tick is the absolute
            // tick at which an N+1 beat would arrive. Now, find when that
            // would have been sent to have that arrival time.
            uint64_t num_ticks_before_arrival =
                sync_in_port_->computeReverseSendToReceiveTickDelay_(clk_,
                                                                     send_delay_cycles,
                                                                     prev_data_arrival_tick);
            uint64_t send_tick = prev_data_arrival_tick - num_ticks_before_arrival;

            // Convert the absolute tick of the send event into a current
            // cycle relative cycle and return that value
            Clock::Cycle next_send_cycle = clk_->getCycle(send_tick);
            sparta_assert(next_send_cycle > current_cycle);
            Clock::Cycle delay_cycle = next_send_cycle - current_cycle;

            return delay_cycle;
        }


        //! No making copies!
        SyncOutPort(const SyncOutPort &) = delete;

        //! No making assignments!
        SyncOutPort & operator=(const SyncOutPort &) = delete;

        //! Enable pipeline collection
        void enableCollection(TreeNode* node) override {
            collector_.reset(new CollectorType(node, Port::name_, 0,
                                               "Data being sent out on this SyncOutPort"));
        }

    private:

        /// The clock we'll use when sending
        const sparta::Clock * clk_ = nullptr;

        /// The in-port all data will be sent to
        SyncInPort<DataT> * sync_in_port_ = nullptr;

        /// Pipeline collection: TODO - figure out how this works w/ sync ports
        std::unique_ptr<CollectorType> collector_;

        /// Last cycle any data was sent
        Clock::Cycle prev_data_send_cycle_ = 0;

        /// loggers
        sparta::log::MessageSource info_logger_;
    };


    /**
     * \class SyncInPort
     * \brief Class that defines an synchronized input port
     *
     */
    template<class DataT>
    class SyncInPort final : public InPort, public DataContainer<DataT>
    {
        typedef collection::Collectable<DataT> CollectorType;
    public:
        //! Expected typedef for DataT
        typedef DataT DataType;

        /**
         * \brief Create an SyncInPort with the given name
         * \param portset Name of the sparta::PortSet this port belongs to
         * \param name The name of the SyncInPort
         * \param clk The Clock this port should use
         * \param delivery_phase When the data should be delivered to
         *                       the consumer (and the Port updated)
         */
        SyncInPort(TreeNode* portset, const std::string & name, const Clock * clk,
                   sparta::SchedulingPhase delivery_phase = sparta::SchedulingPhase::PortUpdate) :
            InPort(portset, name, delivery_phase),
            DataContainer<DataT>(clk),
            sync_port_events_(this),
            info_logger_(this, "pinfo", getLocation() + "_info")
        {
            receiver_clock_ = getClock();
            scheduler_ = receiver_clock_->getScheduler();

            sparta_assert(name.length() != 0, "You cannot have an unnamed port.");
            sparta_assert(receiver_clock_ != 0, "Clock ptr cannot be null in port: " << name);

            //sync_port_events_.setClock(clk);
            forward_event_.reset(new PhasedPayloadEvent<DataT>
                                 (&sync_port_events_, name + "_forward_event",
                                  delivery_phase,
                                  CREATE_SPARTA_HANDLER_WITH_DATA(SyncInPort<DataT>, forwardData_, DataT)));
        }

        //! No making copies
        SyncInPort(const SyncInPort &) = delete;

        //! No assignments
        SyncInPort & operator=(const SyncInPort &) = delete;

        /**
         */
        ~SyncInPort()
        { }

        /**
         * \brief Bind to an SyncOutPort
         * \param out The SyncOutPort to bind to. The data and event types must be the
         * same
         */
        void bind(Port * out) override
        {
            SyncOutPort<DataT> * outp;
            if((outp = dynamic_cast<SyncOutPort<DataT> *>(out)) == 0) {
                throw SpartaException("ERROR: Attempt to bind SyncOutPort of a disparate types: '" +
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
        Clock::Cycle getPortDelay() const final {
            return receive_delay_cycles_;
        }

        /**
         * \brief Set the port delay associated with this port
         * \param delay_cycles The port delay in cycles
         */
        void setPortDelay(sparta::Clock::Cycle delay_cycles) override {
            sparta_assert(isBound() == false, "Attempt to set a port delay after binding. \n"
                          "This can adversely affect precedence rules.  If possible call setPortDelay BEFORE\n"
                          "binding the port");
            sparta_assert(delay_was_set_ == false,
                          "Attempt to set port delay twice (that's not expected) for: " << getLocation());
            receive_delay_cycles_ = delay_cycles;
            receive_delay_ticks_  = receiver_clock_->getTick(delay_cycles);
            delay_was_set_ = true;
            if (SPARTA_EXPECT_FALSE(info_logger_)) {
                info_logger_ << "setPortDelay [cycles]: "
                             << " delay_cycles=" << delay_cycles
                             << " => receive_delay_ticks_=" << receive_delay_ticks_
                             << " receive_delay_cycles_=" << receive_delay_cycles_
                             << std::endl;
            }
        }

        /**
         * \brief Set the port delay associated with this port
         * \param delay_cycles The port delay in fractional cycles
         */
        void setPortDelay(double delay_cycles) override {
            sparta_assert(isBound() == false, "Attempt to set a port delay after binding. \n"
                          "This can adversely affect precedence rules.  If possible call setPortDelay BEFORE\n"
                          "binding the port");
            sparta_assert(delay_was_set_ == false,
                          "Attempt to set port delay twice (that's not expected) for: " << getLocation());
            receive_delay_ticks_ = receiver_clock_->getTick(delay_cycles);
            receive_delay_cycles_ = (receive_delay_ticks_ + (receiver_clock_->getPeriod() - 1)) / receiver_clock_->getPeriod();
            delay_was_set_ = true;
            if (SPARTA_EXPECT_FALSE(info_logger_)) {
                info_logger_ << "setPortDelay [double]: "
                             << " delay_cycles=" << delay_cycles
                             << " => receive_delay_ticks_=" << receive_delay_ticks_
                             << " receive_delay_cycles_=" << receive_delay_cycles_
                             << std::endl;
            }
        }

        //! Enable pipeline collection
        void enableCollection(TreeNode* node) override
        {
            collector_.reset(new CollectorType(node, Port::name_, 0,
                                               "Data being recirculated on this SyncInPort"));
        }

        //! Set the ready state for the port before simulation begins
        void setInitialReadyState(bool is_ready) {
            sparta_assert(scheduler_->isRunning() == false);
            sparta_assert(scheduler_->getCurrentTick() == 0);
            cur_is_ready_ = is_ready;
            prev_is_ready_ = is_ready;
        }

        /*!
         *  Put backpressure on the connection to indicate that the input
         *  port isn't ready for anymore requests.
         */
        void setReady(bool is_ready) {
            if (SPARTA_EXPECT_FALSE(info_logger_)) {
                info_logger_ << "setting ready to: " << is_ready << "; num_in_flight = " << num_in_flight_ << "\n";
            }

            Scheduler::Tick cur_tick = scheduler_->getCurrentTick();
            if (cur_tick > set_ready_tick_) {
                set_ready_tick_ = cur_tick;
                prev_is_ready_ = cur_is_ready_;
                cur_is_ready_ = is_ready;
            } else {
                sparta_assert(cur_tick == set_ready_tick_,
                              "Unexpected set-ready in the past for: " << getLocation());
                sparta_assert(is_ready == cur_is_ready_,
                              "Double-ready setting must be of the same value for: " << getLocation());
            }


        }

        /*!
         *  Return whether this input port is ready or not
         */
        bool getReady() const {
            return (cur_is_ready_);
        }

        //! Do events from this port keep simulation going?
        //! \param continuing true [default] if yes, it does; false otherwise
        void setContinuing(bool continuing) final {
            Port::setContinuing(continuing);
            forward_event_->setContinuing(continuing);
        }

        /**
         * \brief Ensure data entering this Port is handled before
         *        a payload is delivered.
         *
         * \tparam ConsDataT The Payload's Data Type
         * \tparam PhaseT The phase of the payload (must be equal or greater)
         * \param consumer The payload to follow this port
         *
         * If the Phase of the PayloadEvent is not equal to this
         * Port's phase, this precedence has no effect.
         */
        template<class ConsDataT, sparta::SchedulingPhase PhaseT>
        void precedes(PayloadEvent<ConsDataT, PhaseT> & consumer)
        {
            sparta_assert(PhaseT >= forward_event_->getSchedulingPhase(),
                          "The phase of the PayloadEvent is less than this Port's -- you cannot "
                          "force the Port to come before the PayloadEvent due to this constraint");
            if(PhaseT == forward_event_->getSchedulingPhase()) {
                forward_event_->getScheduleable().
                    precedes(consumer.getScheduleable());
            }
        }

        /**
         * \brief Ensure data entering this Port is handled before
         *        the given UniqueEvent.
         *
         * \tparam PhaseT The phase of the UniqueEvent (must be equal or greater)
         * \param consumer The consuming UniqueEvent
         */
        template<sparta::SchedulingPhase PhaseT>
        void precedes(UniqueEvent<PhaseT> & consumer)
        {
            sparta_assert(PhaseT >= forward_event_->getSchedulingPhase(),
                          "The phase of the PayloadEvent is less than this Port's -- you cannot "
                          "force the Port to come before the PayloadEvent due to this constraint");
            if(PhaseT == forward_event_->getSchedulingPhase()) {
                forward_event_->getScheduleable().precedes(consumer);
            }
        }

        //! Promote base class' precedes
        using InPort::precedes;

        /**
         * \brief Return the internal forwarding event
         * \return Reference to the internal forwarding event.
         */
        std::unique_ptr<PhasedPayloadEvent<DataT>> & getForwardingEvent() {
            return forward_event_;
        }

    private:

        Scheduleable & getScheduleable_() final {
            return forward_event_->getScheduleable();
        }

        void setProducerPrecedence_(Scheduleable * pd) final {
            if(pd->getSchedulingPhase() == forward_event_->getSchedulingPhase()) {
                pd->precedes(forward_event_->getScheduleable(), "Port::bind of OutPort to " + getName() + ": '" +
                             pd->getLabel() + "' is a registered driver");
            }
        }

        void registerConsumerHandler_(const SpartaHandler & handler) final
        {
            sparta_assert(handler.argCount() == 1,
                          "The handler associated with the SyncInPort must take at least one argument");

            // Help the user identify their events/callbacks from the Scheduler debug
            handler_name_ = getName() + "<SyncInPort>[" + explicit_consumer_handler_.getName() + "]";
            forward_event_->getScheduleable().setLabel(handler_name_.c_str());
        }

        void bind_(Port * outp) override final
        {
            InPort::bind_(outp);
            for(auto & consumer : port_consumers_)
            {
                if(consumer->getSchedulingPhase() == forward_event_->getSchedulingPhase()) {
                    forward_event_->getScheduleable().precedes(consumer, "Port::bind(" + getName() + "->" + outp->getName() + "),'"
                                                               + consumer->getLabel() + "' is registered driver");
                }
            }
        }

        /*!
         * Get the latched (internal) value of ready for the current tick.
         * The receiver can drive ready on tick 'n', but it shouldn't be seen
         * until tick 'n+1'.
         *
         * Instead of a true present-state / next-state value for ready, we
         * just record the most recent version of ready (prev_is_ready_),
         * and disallow multiple ready updates on the same tick.  This  is
         * enforced in setReady().
         */
        bool getLatchedReady_(sparta::Scheduler::Tick cur_tick) const {
            bool internal_ready = true;
            if ((set_ready_tick_ == cur_tick && !prev_is_ready_) || // 'ready' was set this cycle and the previous value of 'ready' was false OR
                (set_ready_tick_ < cur_tick && !cur_is_ready_))     // 'ready' was set in a previous cycle and the current value of 'ready' is false
            {
                internal_ready = false;
            }
            return internal_ready;
        }

        /*
         * Return the actual, non-latched ready value.
         */
        bool getRawReady_() const {
            sparta_assert(getPortDelay() == 0,
                          "Only expected raw-ready to be returned for 0-cycle connections");
            return cur_is_ready_;
        }

        /*!
         * This method is the handler for all incoming events so that the
         * SyncPort can hold data when it's not ready to receive it.  In
         * the normal case, the data is fowrarded to the original handler
         * the user passed in.  When not-ready, we self-schedule for 1
         * cycle later
         */
        void forwardData_(const DataT & dat)
        {
            sparta::Scheduler::Tick cur_tick =
                scheduler_->getCurrentTick();

            sparta_assert(set_ready_tick_ <= cur_tick, "Assert in port: " << getLocation());

            // If the owner of the Inport isn't ready, then re-send the
            // data to ourself until the owner is ready to accept.
            sparta_assert(num_in_flight_ > 0);
            num_in_flight_--;
            if (getLatchedReady_(cur_tick) == false) {
                if (SPARTA_EXPECT_FALSE(info_logger_)) {
                    info_logger_ << "RESENDING @" << receiver_clock_->currentCycle()
                                 << "(" << num_in_flight_ << ") "
                                 << " # " << dat;
                }
                bool allow_slide = false;
                bool is_fwd_progress = false;

                send_(dat, receiver_clock_, sparta::Clock::Cycle(0), allow_slide, is_fwd_progress);
                sparta_assert(num_in_flight_ > 0);
            }

            // Else, forward the data to the original handler
            else {
                DataContainer<DataT>::setData_(dat);

                // Always call the consumer_handler BEFORE scheduling
                // listeners.
                if(SPARTA_EXPECT_TRUE(explicit_consumer_handler_)) {
                    explicit_consumer_handler_((const void*)&dat);
                }

                // Show the data that has arrived on this OutPort that
                // the receiver now sees
                if(SPARTA_EXPECT_FALSE(collector_ != nullptr)) {
                    collector_->collectWithDuration(dat, 1);
                }

                if (SPARTA_EXPECT_FALSE(info_logger_)) {
                    info_logger_ << "DELIVERING @" << receiver_clock_->currentCycle()
                                 << "(" << num_in_flight_ << ") "
                                 << " # " << dat;
                }
            }
        }

        //! Event Set for this port
        sparta::EventSet sync_port_events_;

        //! Typedef for vector of callbacks
        std::unique_ptr<PhasedPayloadEvent<DataT>> forward_event_;

        //! Allow SyncOutPort::bind to set precedence on the internal forwarding event
        friend void SyncOutPort<DataT>::bind(Port * in);

        //! Allow SyncOutPort::send() to call SyncInPort::send_()
        friend uint64_t SyncOutPort<DataT>::send(const DataT &, uint64_t, const bool);

        //! Allow DataOutPort::isReady() to call DataInPort::couldAccept_()
        friend bool SyncOutPort<DataT>::isReady(sparta::Clock::Cycle send_delay_cycles) const;

        //! Allow DataOutPort::isReadyPS() to call DataInPort::getLatchedReady_()
        friend bool SyncOutPort<DataT>::isReadyPS() const;

        //! Allow DataOutPort::computeNextAvailableCycleForSend() to call
        //!  DataInPort::computeSendToReceiveTickDelay_() and
        //!  DataInPort::getPrevDataArrivalTick_()
        friend uint64_t SyncOutPort<DataT>::computeNextAvailableCycleForSend(sparta::Clock::Cycle, const uint32_t);

        /*!
         *  Return whether the input port could receive data if it were
         *  sent from a different sender's clock domain, given the number
         *  of delay cycles.
         *
         * \param send_clk The sender's clock
         * \param send_delay_cycles The number of sender-introduced delay
         * cycles
         */
        bool couldAccept_(const Clock *send_clk, Clock::Cycle send_delay_cycles) const {
            return couldAccept_(send_clk, static_cast<double>(send_delay_cycles));
        }

        /*!
         *  Return whether the input port could receive data if it were
         *  sent from a different sender's clock domain, given the number
         *  of delay cycles.
         *
         * \param send_clk The sender's clock
         * \param send_delay_cycles The number of sender-introduced delay
         * cycles
         */
        bool couldAccept_(const Clock *send_clk, double send_delay_cycles) const {

            Scheduler::Tick num_delay_ticks =
                calculateClockCrossingDelay(send_clk->getTick(send_delay_cycles), send_clk, receive_delay_ticks_, receiver_clock_);

            sparta::Scheduler::Tick cur_tick =
                scheduler_->getCurrentTick();

            sparta::Scheduler::Tick abs_scheduled_tick =
                num_delay_ticks + cur_tick;

            bool retval = (abs_scheduled_tick > prev_data_arrival_tick_ || prev_data_arrival_tick_ == PREV_DATA_ARRIVAL_TICK_INIT);

            sparta_assert(cur_tick >= set_ready_tick_, "Someone drove setReady() in the future in" << getLocation());

            // Check for sync-port ready/valid backpressure and override
            // 'ready' if downstream indicated it couldn't take data
            if (getLatchedReady_(cur_tick) == false) {
                sparta_assert(send_clk->getFrequencyMhz() == receiver_clock_->getFrequencyMhz(), "Error in port:" << getLocation());
                sparta_assert(send_delay_cycles == 0, "Error in port:" << getLocation());
                sparta_assert(getPortDelay() == 1 || getPortDelay() == 0,
                              "Ready/Valid only tested for zero and one cycle delays (not "<< getPortDelay() <<"); relax this assert once more testing is done; location=" << getLocation());

                // Can't accept anything if there's already one request
                // waiting to be delivered OR if this is a 0-cycle delay
                // (since we need to deliver on the same cycle its sent)
                if (num_in_flight_ > 0 || getPortDelay() == 0) {
                    retval = false;
                }
            }

            return retval;
        }

        /*!
         *  \brief Returns the previous data arrival tick
         *
         *  \return Returns the previous data arrival tick
         */
        Scheduler::Tick getPrevDataArrivalTick_() const
        {
            return prev_data_arrival_tick_;
        }

        /*!
         *  \brief Used to compute the number of ticks from send to receive
         *  \param clk The clock used to schedule an event
         *  \param send_delay_cycles The relative time to schedule into the future
         *  \param allow_slide The destination cycle may be slid to the next cycle on collision
         *  \param prev_data_arrival_tick The tick on which the previous data arrived
         *
         *  \return Returns the number of ticks from send to receive
         */
        uint64_t computeSendToReceiveTickDelay_(const Clock *send_clk,
                                                const double send_delay_cycles,
                                                const bool allow_slide,
                                                const Scheduler::Tick prev_data_arrival_tick) const
        {
            Scheduler::Tick num_delay_ticks =
                calculateClockCrossingDelay(send_clk->getTick(send_delay_cycles), send_clk, receive_delay_ticks_, receiver_clock_);

            sparta::Scheduler::Tick current_tick       = scheduler_->getCurrentTick();
            sparta::Scheduler::Tick abs_scheduled_tick = num_delay_ticks + current_tick;

            // Slide pushes this send out past the previous arrival,
            // rather than faulting as user error on sending too early
            if (allow_slide &&
                ( abs_scheduled_tick <= prev_data_arrival_tick &&
                  prev_data_arrival_tick != PREV_DATA_ARRIVAL_TICK_INIT)) {
                abs_scheduled_tick = prev_data_arrival_tick + receiver_clock_->getPeriod();
            }

            num_delay_ticks = abs_scheduled_tick - current_tick;

            // Underlying assumption is that all destinations get their
            // event at the same time.
            sparta_assert((abs_scheduled_tick % receiver_clock_->getPeriod()) == 0, "Failed posedge check in:" << getLocation()); // posedge check

            return num_delay_ticks;
        }


        /*!
         *  \brief Used to compute the number of ticks from receive back to send
         *  \param clk The clock used to schedule an event
         *  \param send_delay_cycles The relative time to schedule into the future
         *  \param data_arrival_tick The tick on which the data arrives
         *
         *  \return Returns the number of ticks from receive back to send
         */
        uint64_t computeReverseSendToReceiveTickDelay_(const Clock *send_clk,
                                                       const double send_delay_cycles,
                                                       const Scheduler::Tick data_arrival_tick) const
        {
            sparta::Scheduler::Tick raw_dst_clk_posedge_tick =
                data_arrival_tick / receiver_clock_->getPeriod() * receiver_clock_->getPeriod();
            sparta_assert(data_arrival_tick == raw_dst_clk_posedge_tick);

            Scheduler::Tick num_delay_ticks =
                calculateReverseClockCrossingDelay(data_arrival_tick,
                                                   send_clk->getTick(send_delay_cycles),
                                                   send_clk, receive_delay_ticks_, receiver_clock_);

            sparta_assert( data_arrival_tick > num_delay_ticks );
            return num_delay_ticks;
        }

        //! Called by the DataOutPort, remember the binding
        void bind_(SyncOutPort<DataT> * inp) {
            bound_ports_.push_back(inp);
        }

        /*!
         * \brief Called by SyncOutPort, send the data across (schedule event)
         * \param dat The Data to send over
         * \param clk The clock used to schedule an event
         * \param send_delay_cycles The relative time to schedule into the future
         * \param allow_slide The destination cycle may be slid to the next cycle on collision
         * \param is_fwd_progress Whether this call should be considered forward progress
         * \return The delay in ticks from sending
         */
        uint64_t send_(const DataT & dat, const Clock *send_clk, Clock::Cycle send_delay_cycles,
                       const bool allow_slide, bool is_fwd_progress)
        {
            return send_(dat, send_clk, static_cast<double>(send_delay_cycles), allow_slide, is_fwd_progress);
        }

        /*!
         * \brief Called by SyncOutPort, send the data across (schedule event)
         * \param dat The Data to send over
         * \param clk The clock used to schedule an event
         * \param send_delay_cycles The relative time to schedule into the future
         * \param allow_slide The destination cycle may be slid to the next cycle on collision
         * \param is_fwd_progress Whether this call should be considered forward progress
         * \return The delay in ticks from sending
         */
        uint64_t send_(const DataT & dat, const Clock *send_clk, double send_delay_cycles,
                       const bool allow_slide, bool is_fwd_progress)
        {
            Scheduler::Tick num_delay_ticks =
                computeSendToReceiveTickDelay_(send_clk, send_delay_cycles, allow_slide, prev_data_arrival_tick_);

            sparta::Scheduler::Tick current_tick = scheduler_->getCurrentTick();
            sparta::Scheduler::Tick abs_scheduled_tick = num_delay_ticks + current_tick;

            // Underlying assumption is that all destinations get their
            // event at the same time.
            sparta_assert((abs_scheduled_tick % receiver_clock_->getPeriod()) == 0, "Failed posedge check in:" << getLocation()); // posedge check

            if (SPARTA_EXPECT_FALSE(info_logger_)) {
                info_logger_ << "RECEIVE SCHEDULED @" << receiver_clock_->getCycle(abs_scheduled_tick)
                             << "(" << num_in_flight_ << ") "
                             << " # " << dat;
            }

            // Only one item can be received per cycle
            sparta_assert(prev_data_arrival_tick_ < abs_scheduled_tick || prev_data_arrival_tick_ == PREV_DATA_ARRIVAL_TICK_INIT,
                          getLocation() << ": attempt to schedule send for tick "
                          << abs_scheduled_tick
                          << ", which is not later than the previous data at tick "
                          << prev_data_arrival_tick_
                          << "; SyncInPorts should only get data once per cycle; data was: " << dat);

            prev_data_arrival_tick_ = abs_scheduled_tick;

            if(num_delay_ticks == 0) {
                checkSchedulerPhaseForZeroCycleDelivery_(forward_event_->getSchedulingPhase());
            }
            forward_event_->preparePayload(dat)->scheduleRelativeTick(num_delay_ticks, scheduler_);
            num_in_flight_++;

            if (SPARTA_EXPECT_TRUE(is_fwd_progress && is_continuing_)) {
                scheduler_->kickTheDog();
            }

            return num_delay_ticks;
        }

    private:

        //! The handler name for scheduler debug
        std::string handler_name_;

        //! This port's additional delay (and a flag indicating whether it
        //! has ever been set to a non-default value)
        bool delay_was_set_                = false;
        Clock::Cycle receive_delay_cycles_ = 0;
        sparta::Scheduler::Tick receive_delay_ticks_ = 0;

        //! The last time data arrived on this port
        const Scheduler::Tick PREV_DATA_ARRIVAL_TICK_INIT = 0xffffffffffffffff;
        Scheduler::Tick prev_data_arrival_tick_ = PREV_DATA_ARRIVAL_TICK_INIT;

        //! The current and previous state of the port w.r.t receiving input
        bool cur_is_ready_ = true;
        bool prev_is_ready_ = true;

        //! Does the current handler expect simulation to continue if data arrives on this port?
        bool is_continuing_ = true;

        //! Number of in flight packets through this port
        uint32_t num_in_flight_ = 0;

        //! Last cycle that someone called setReady.  Class ensures that we
        //! only call setReady() once per cycle.
        Scheduler::Tick set_ready_tick_ = 0;

        //! Pipeline collection.  TODO: See if this works for syncports
        std::unique_ptr<CollectorType> collector_;

        /// loggers
        sparta::log::MessageSource info_logger_;
    };
}

#endif
