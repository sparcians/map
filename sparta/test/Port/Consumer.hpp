
#pragma once

#include "sparta/simulation/Resource.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/ports/SignalPort.hpp"
#include "sparta/events/EventSet.hpp"

/* Controls the rate at which Jobs arrive at the Machine */

class Consumer : public sparta::Resource
{
public:
    Consumer (sparta::TreeNode * rtn,
              sparta::DataInPort<double> * delay0,
              sparta::DataInPort<double> * delay1,
              sparta::DataInPort<double> * delay10,
              sparta::DataInPort<double> * delay1_non_continuing,
              sparta::SignalInPort * signal_port,
              sparta::Clock *clk);
    virtual ~Consumer () {}

    void myDelay0EventCallback(const double &);
    void myDelay0EventCallbackPortUpdate();

    void myDelay1EventCallback();
    void myDelay1EventCallbackPortUpdate();

    // The handler SHOULD be called before the callback
    void myDelay10EventHandler(const double & dat);
    void myDelay10EventCallback();

    void mySignalEventCallback();
    void mySignalEventHandler() { } // does nothing -- just tests the registration

    void myNonContinuingEventCallback(const double & );

    uint32_t getNumTimes() const {
        return num_times_;
    }

    std::string getName() const {
        return "Consumer";
    }
private:
    sparta::TreeNode * root_ = nullptr;
    sparta::DataInPort<double> * delay0_ = nullptr;
    sparta::DataInPort<double> * delay1_ = nullptr;
    sparta::DataInPort<double> * delay10_ = nullptr;
    sparta::DataInPort<double> * delay1_non_continuing_ = nullptr;
    sparta::SignalInPort * signal_port_ = nullptr;

    uint32_t delay1_size_  = 0;
    uint32_t delay10_size_ = 0;
    double delay1_time_    = 0;
    double delay10_time_   = 0;

    sparta::utils::ValidValue<double> delay10_dat_;

    uint32_t num_times_ = 0;
    uint32_t num_times_got_non_continuing_data_ = 0;

    sparta::Clock * clk_ = nullptr;
    sparta::EventSet event_set_{root_};

    sparta::PayloadEvent<double> delay0_receive_event_{
        &event_set_, "delay0_receive_event", CREATE_SPARTA_HANDLER_WITH_DATA(Consumer, myDelay0EventCallback, double)};
    sparta::Event<sparta::SchedulingPhase::PortUpdate> delay0_receive_event_port_update_{
        &event_set_, "delay0_receive_event_pu", CREATE_SPARTA_HANDLER(Consumer, myDelay0EventCallbackPortUpdate)};

    sparta::Event<> delay1_receive_event_     {&event_set_, "delay1_receive_event", CREATE_SPARTA_HANDLER(Consumer, myDelay1EventCallback)};
    sparta::Event<> delay10_receive_event_    {&event_set_, "delay10_receive_event", CREATE_SPARTA_HANDLER(Consumer, myDelay10EventCallback)};

    sparta::Event<sparta::SchedulingPhase::PortUpdate> delay1_receive_event_update_phase_{
        &event_set_, "delay1_receive_event_update_phase", CREATE_SPARTA_HANDLER(Consumer, myDelay1EventCallbackPortUpdate)};
    bool port_update_call_made_ = false;

    // For testing in compilation static_assert
    sparta::Event<sparta::SchedulingPhase::PortUpdate> bad_event_{&event_set_, "bad_event", CREATE_SPARTA_HANDLER(Consumer, mySignalEventHandler)};

};
