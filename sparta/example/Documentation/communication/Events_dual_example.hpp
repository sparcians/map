
#include <cinttypes>
#include <memory>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/ports/PortSet.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/utils/SpartaAssert.hpp"

// Include Event.h
#include "sparta/events/Event.hpp"

#define ILOG(msg) \
    if(SPARTA_EXPECT_FALSE(info_logger_)) { \
        info_logger_ << msg; \
    }

class MyDeviceParams : public sparta::ParameterSet
{
public:
    MyDeviceParams(sparta::TreeNode* n) :
        sparta::ParameterSet(n)
    {

        auto a_dumb_true_validator = [](bool & val, const sparta::TreeNode*)->bool {
            // Really dumb validator
            if(val == true) {
                return true;
            }
            return false;
        };
        my_device_param.addDependentValidationCallback(a_dumb_true_validator,
                                                       "My device parameter must be true");
    }
    PARAMETER(bool, my_device_param, true, "An example device parameter")
};

class MyDevice : public sparta::Unit
{
public:
    MyDevice(sparta::TreeNode * parent_node,
             const MyDeviceParams * my_params);

    //! \brief Name of this resource. Required by sparta::ResourceFactory
    static const char * name;

private:
    // A data in port that receives uint32_t from source 1
    sparta::DataInPort<uint32_t> a_delay_in_source1_;

    // The callback to receive data from the first sender
    void myDataReceiverFromSource1_(const uint32_t & dat1);

    // A data in port that receives uint32_t from a second source
    sparta::DataInPort<uint32_t> a_delay_in_source2_;

    // The callback to receive data from a second sender
    void myDataReceiverFromSource2_(const uint32_t & dat2);

    // An event to be scheduled if data is received, but it's unique!
    // This means it can scheduled many times for a given cycle, but
    // it's only called once.  Also, this event in the
    // SchedulingPhase::Tick phase with a delay of 0.
    sparta::UniqueEvent<> event_do_some_work_{&unit_event_set_, "do_some_work_event",
            CREATE_SPARTA_HANDLER(MyDevice, doSomeWork_)};

    // Method called by the event 'event_do_some_work_'
    void doSomeWork_();

    // The data in question. DO NOT initialize
    sparta::utils::ValidValue<uint32_t> data1_;
    sparta::utils::ValidValue<uint32_t> data2_;

    // The result after getting both data
    uint32_t total_data_ = 0;
};

// Source
const char * MyDevice::name = "my_device";

MyDevice::MyDevice(sparta::TreeNode * my_node,
                   const MyDeviceParams * my_params) :
    sparta::Unit(my_node, name),
    a_delay_in_source1_(&unit_port_set_, "a_delay_in_source1", 1), // Receive data one cycle later
    a_delay_in_source2_(&unit_port_set_, "a_delay_in_source2", 1)  // Receive data one cycle later
{
    // Tell SPARTA to ignore this parameter
    my_params->my_device_param.ignore();

    // Register the callbacks.  These callbacks are called in the
    // Port's SchedulingPhase::PortUpdate phase (which is before
    // SchedulingPhase::Tick)
    a_delay_in_source1_.registerConsumerHandler(
       CREATE_SPARTA_HANDLER_WITH_DATA(MyDevice, myDataReceiverFromSource1_, uint32_t));
    a_delay_in_source2_.registerConsumerHandler(
       CREATE_SPARTA_HANDLER_WITH_DATA(MyDevice, myDataReceiverFromSource2_, uint32_t));

}

// This function will be called when a sender with a DataOutPort
// sends data on its out port.  An example would look like:
//
//     a_delay_out_source1_.send(1234);
//
void MyDevice::myDataReceiverFromSource1_(const uint32_t & dat)
{
    ILOG("I got data from Source1: " << dat);
    ILOG("Time to do some work this cycle: " << getClock()->currentCycle());

    // Schedule doSomeWork_() for THIS cycle.  Doesn't matter if data
    // from Source2 is here yet.  Since the event_do_some_work_ is in
    // the SchedulingPhase::Tick phase, it will be scheduled for later
    // in this cycle. No argument to schedule == 0 cycle delay.
    event_do_some_work_.schedule();
    sparta_assert(!data1_.isValid());

    // Save the data
    data1_ = dat;
}

// This function will be called when a sender with a DataOutPort
// sends data on its out port.  An example would look like:
//
//     a_delay_out_source2_.send(4321);
//
void MyDevice::myDataReceiverFromSource2_(const uint32_t & dat)
{
    ILOG("I got data from Source2: " << dat);
    ILOG("Time to do some work this cycle: " << getClock()->currentCycle());

    // Schedule doSomeWork_() for THIS cycle.  Since the
    // event_do_some_work_ is in the SchedulingPhase::Tick phase, it
    // will be scheduled for later in this cycle.
    event_do_some_work_.schedule();
    sparta_assert(!data2_.isValid());

    // Save the data
    data2_ = dat;
}

// Called from the scheduler; scheduled by the event_do_some_work_
// event.
void MyDevice::doSomeWork_() {
    ILOG("Well, it's time to do some work. Cycle: " << getClock()->currentCycle());

    sparta_assert(data1_.isValid() && data2_.isValid(),
                  "Hey, we didn't get data1 and data2 before"" this function was called!");

    ILOG("Got these values: "
         << data1_.getValue() << " and "
         << data2_.getValue());

    total_data_ = data1_.getValue() + data2_.getValue();
    data1_.clearValid();
    data2_.clearValid();
}
