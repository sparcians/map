

#include <cinttypes>
#include <memory>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/ports/PortSet.hpp"
#include "sparta/ports/DataPort.hpp"

#define ILOG(msg) \
    if(SPARTA_EXPECT_FALSE(info_logger_)) { \
        info_logger_ << msg; \
    }

//
// Basic device parameters
//
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

//
// Example of a Device in simulation
//
class MyDevice : public sparta::Unit
{
public:
    // Typical and expected constructor signature if this device is
    // build using sparta::ResourceFactory concept
    MyDevice(sparta::TreeNode * parent_node, // The TreeNode this Devive belongs to
             const MyDeviceParams * my_params);

    // Name of this resource. Required by sparta::ResourceFactory.  The
    // code will not compile without it
    static const char * name;

private:
    // A data in port that receives uint32_t
    sparta::DataInPort<uint32_t> a_delay_in_;

    // The callback to receive data from a sender
    void myDataReceiver_(const uint32_t & dat);
};


// Defined name
const char * MyDevice::name = "my_device";

////////////////////////////////////////////////////////////////////////////////
// Implementation

// Construction
MyDevice::MyDevice(sparta::TreeNode * my_node,
                   const MyDeviceParams * my_params) :
    sparta::Unit(my_node, name),
    a_delay_in_(&unit_port_set_, "a_delay_in", 1) // Receive data one cycle later
{
    // Tell SPARTA to ignore this parameter
    my_params->my_device_param.ignore();

    // Register the callback
    a_delay_in_.
        registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(MyDevice, myDataReceiver_, uint32_t));
}

// This function will be called when a sender with a DataOutPort
// sends data on its out port.  An example would look like:
//
//     a_delay_out.send(1234);
//
void MyDevice::myDataReceiver_(const uint32_t & dat)
{
    ILOG("I got data: " << dat);
}
