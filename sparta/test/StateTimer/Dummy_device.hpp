// <Fetch.h> -*- C++ -*-


#include <cinttypes>
#include <memory>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/Resource.hpp"
#include "sparta/ports/PortSet.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/simulation/StateTimerUnit.hpp"

// Define enum class for state set. The state set should be as follow:
// 1. Start with "NONE = 0," and has "__FIRST = NONE,"
// 2. End with "__LAST"
enum class DummyState1{
    NONE = 0,
    __FIRST = NONE,
    DS1_1,
    DS1_2,
    DS1_3,
    __LAST
};

enum class DummyState2{
    NONE = 0,
    __FIRST = NONE,
    DS2_1,
    DS2_2,
    DS2_3,
    __LAST
};

enum class DummyState3{
    NONE = 0,
    __FIRST = NONE,
    DS3_1,
    DS3_2,
    DS3_3,
    __LAST
};

class DummyOp
{
public:
    std::shared_ptr<sparta::StateTimerUnit::StateTimer> getTimer(){ return timer_; }
    void setTimer(std::shared_ptr<sparta::StateTimerUnit::StateTimer> timer) { timer_ = timer; }
    uint64_t getOpId() { return dummy_op_id_; }
    DummyOp(uint64_t op_id):
        dummy_op_id_(op_id)
    {}
private:
    std::shared_ptr<sparta::StateTimerUnit::StateTimer> timer_;
    uint64_t dummy_op_id_;
};
typedef std::shared_ptr<DummyOp> DummyOpPtr;

class DummyDeviceParams : public sparta::ParameterSet
{
public:
    DummyDeviceParams(sparta::TreeNode* n) :
        sparta::ParameterSet(n)
    {
        auto dummy_validator = [] (bool & val, const sparta::TreeNode*)->bool {
            if (val == true)
            {
                return true;
            }
            return false;
        };
        dummy_device_param.addDependentValidationCallback(dummy_validator, "validator needs to be true");
    }
    PARAMETER(bool, dummy_device_param, true, "An example device parameter")

};

class DummyDevice : public sparta::Resource
{
public:

    DummyDevice(sparta::TreeNode * parent_node,
                const DummyDeviceParams * params, uint32_t device_id, sparta::Clock * clk);

    static const char * name;

private:
    // port set
    sparta::PortSet   dummy_ports_;
    // DateInPort
    sparta::DataInPort<DummyOpPtr> in_port_ ;
    // DataOutPort
    sparta::DataOutPort<DummyOpPtr> out_port_ ;
    // callback to receive data
    void dummyDataReceiver_(const DummyOpPtr & dat);
    uint32_t device_id_;
    sparta::Clock * clk_;
};

const char * DummyDevice::name = "dummy_device";
DummyDevice::DummyDevice(sparta::TreeNode * parent_node,
                        const DummyDeviceParams * params, uint32_t device_id,
                        sparta::Clock * clk):
    sparta::Resource(parent_node, name),
    dummy_ports_(parent_node, "DummyDevice Ports"),
    in_port_(&dummy_ports_, "in_port", 2),
    out_port_(&dummy_ports_, "out_port"),
    device_id_(device_id),
    clk_(clk)
{
    params->dummy_device_param.ignore();
    in_port_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(DummyDevice, dummyDataReceiver_, DummyOpPtr));
}

void DummyDevice::dummyDataReceiver_(const DummyOpPtr & dat)
{
//    std::cout << "Device: "<<device_id_<<" got timer data: " << temp << std::endl;

    if(device_id_ == 1)
    {
        dat->getTimer()->startState(DummyState2::DS2_1);
        std::cout << "dummy_device1 got dummy_op_:" << dat->getOpId() << "at Cycle: "<< clk_->currentCycle() << std::endl;
        std::cout << "state_timer in dummy_op_:" << dat->getOpId() << " State: DummyState2::DS2_1 ("<< static_cast<uint32_t>(DummyState2::DS2_1) <<"), start"<< " at Cycle: "<< clk_->currentCycle() << std::endl;
        out_port_.send(dat);
    }
    else // device_id ==2
    {
        dat->getTimer()->startState(DummyState2::DS2_2);
        std::cout << "dummy_device2 got dummy_op_:" << dat->getOpId() << "at Cycle: "<< clk_->currentCycle() << std::endl;
        std::cout << "state_timer in dummy_op_:" << dat->getOpId() << " State: DummyState2::DS2_2 ("<< static_cast<uint32_t>(DummyState2::DS2_2) <<"), start"<< " at Cycle: "<< clk_->currentCycle() << std::endl;
    }
}
