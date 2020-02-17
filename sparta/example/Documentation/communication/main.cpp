// <main.cpp> -*- C++ -*-

#include <memory>

#ifdef PORTS_EXAMPLE
#include "Ports_example.hpp"
#endif

#ifdef EVENTS_EXAMPLE
#include "Events_example.hpp"
#endif

#ifdef EVENTS_DUAL_EXAMPLE
#include "Events_dual_example.hpp"
#endif

#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/ports/PortSet.hpp"
#include "sparta/log/Tap.hpp"

int main()
{
    sparta::RootTreeNode rtn;
    sparta::TreeNode     device_tn(&rtn, "my_device", "My Device TreeNode");
    sparta::Scheduler    scheduler;
    sparta::Clock        clk("clock", &scheduler);
    rtn.setClock(&clk);

    sparta::PortSet ps(&rtn, "out_ports");

    std::unique_ptr<MyDeviceParams> my_dev_params_(new MyDeviceParams(&device_tn));
    std::unique_ptr<MyDevice>       my_device     (new MyDevice(&device_tn, my_dev_params_.get()));

    sparta::DataOutPort<uint32_t> a_delay_out (&ps, "a_delay_out");
    sparta::DataOutPort<uint32_t> a_delay_out2(&ps, "a_delay_out2");

#ifdef EVENTS_DUAL_EXAMPLE
    sparta::bind(rtn.getChildAs<sparta::Port>("my_device.ports.a_delay_in_source1"), a_delay_out);
    sparta::bind(rtn.getChildAs<sparta::Port>("my_device.ports.a_delay_in_source2"), a_delay_out2);
#else
    sparta::bind(rtn.getChildAs<sparta::Port>("my_device.ports.a_delay_in"), a_delay_out);
#endif

    // Place an info Tap on the tree to get logger output
    sparta::log::Tap info_log(&rtn, "info", std::cout);

    rtn.enterConfiguring();
    rtn.enterFinalized();
    clk.getScheduler()->finalize();
    clk.getScheduler()->run(1); // perform initializations

    a_delay_out.send(1234);

    // It is an error to drive a port that is not bound
    if(a_delay_out2.isBound()) {
        a_delay_out2.send(4321);
    }

    clk.getScheduler()->run();

    rtn.enterTeardown();

    return 0;
}
