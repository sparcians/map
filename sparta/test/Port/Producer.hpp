
#pragma once


/* Controls the rate at which Jobs arrive at the Machine */
#include <inttypes.h>
#include <string>

#include "sparta/ports/DataPort.hpp"
#include "sparta/ports/SignalPort.hpp"
#include "sparta/events/EventSet.hpp"

namespace sparta {
    class Clock;
    class Resource;
}

class Producer
{
public:
    Producer (sparta::TreeNode * rtn,
              sparta::DataOutPort<double> * delay0,
              sparta::DataOutPort<double> * delay1,
              sparta::DataOutPort<double> * delay10,
              sparta::DataOutPort<double> * delay1_non_continuing,
              sparta::SignalOutPort * signalout,
              sparta::Clock * clk);
    virtual ~Producer ();

    void writeDelays ();

    std::string getName() const {
        return "Producer";
    }
    void scheduleTests();

private:

    void driveNonContinuingPort();
    sparta::TreeNode * root_ = nullptr;


    sparta::DataOutPort<double> * delay0_ = nullptr;
    sparta::DataOutPort<double> * delay1_ = nullptr;
    sparta::DataOutPort<double> * delay10_ = nullptr;
    sparta::DataOutPort<double> * delay1_non_continuing_ = nullptr;
    sparta::SignalOutPort * signal_out_ = nullptr;

    sparta::Clock * clk_ = nullptr;

    sparta::EventSet event_set_{root_};
    sparta::Event<> delay_write_ev_{&event_set_, "delay_write_ev",
            CREATE_SPARTA_HANDLER(Producer, writeDelays)};
    sparta::Event<> non_continuing_port_driver_{&event_set_, "non_continuing_port_driver_event",
            CREATE_SPARTA_HANDLER(Producer, driveNonContinuingPort)};

    uint32_t non_cont_count_ = 0;

};
