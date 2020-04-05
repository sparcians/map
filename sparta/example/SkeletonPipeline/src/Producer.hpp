// <Producer.hpp> -*- C++ -*-

#pragma once

#include <cinttypes>
#include <string>

#include "sparta/simulation/Unit.hpp"
#include "sparta/ports/PortSet.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/ports/SignalPort.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/simulation/ParameterSet.hpp"

// Possible to create this class outside of the Producer class, but
// simply not that clean.  It's better to put it in the Producer class
// (for namespacing), but definitely not required.
class ProducerParameterSet : public sparta::ParameterSet
{
public:
    ProducerParameterSet(sparta::TreeNode* n) :
        sparta::ParameterSet(n)
    {
        // See test_arch_with_override.sh for explanation about this
        // parameter. It is being used for a test as part of make regress.
        arch_override_test_param = "reset_in_constructor";
        auto non_zero_validator = [](uint32_t & val, const sparta::TreeNode*)->bool {
            if(val > 0) {
                return true;
            }
            return false;
        };
        max_ints_to_send.addDependentValidationCallback(non_zero_validator,
                                                        "Num to send must be greater than 0");
    }

    PARAMETER(uint32_t, max_ints_to_send, 100, "Send a bunch of ints")
    VOLATILE_PARAMETER(uint32_t, test_param, 0, "A dummy parameter")
    PARAMETER(std::string, arch_override_test_param, "arch_override_default_value", "Set this to true in ParameterSet construction")
};

//
// The Producer class
//
class Producer : public sparta::Unit
{
public:

    Producer (sparta::TreeNode * name,
              const ProducerParameterSet * p);

    // Name of this resource. Required by sparta::ResourceFactory
    static const char * name;

private:

    // Producer's ports
    sparta::DataOutPort<uint32_t> producer_out_port_{&unit_port_set_, "producer_out_port"};
    sparta::SignalInPort          producer_go_port_ {&unit_port_set_, "producer_go_port"};

    // Producer's producer handler
    void produceData_();

    // Event to drive data, phase Tick, 1 cycle delay
    sparta::UniqueEvent<> ev_producing_event_{&unit_event_set_, "ev_producing_event",
            CREATE_SPARTA_HANDLER(Producer, produceData_), 1 /* delay */};

    // Internal count
    const uint32_t max_ints_to_send_;
    uint32_t current_ints_count_ = 0;

    // Stats
    sparta::Counter num_produced_{&unit_stat_set_, "num_produced",
                                  "Number of items produced", sparta::Counter::COUNT_NORMAL};

    // Loggers
    sparta::log::MessageSource producer_info_;
};

