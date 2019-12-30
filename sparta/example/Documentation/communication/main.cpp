// <Ports.cpp> -*- C++ -*-


#ifdef PORTS_EXAMPLE
#include "Ports_example.h"
#endif

#ifdef EVENTS_EXAMPLE
#include "Events_example.h"
#endif

#ifdef EVENTS_DUAL_EXAMPLE
#include "Events_dual_example.h"
#endif

int main()
{
    sparta::RootTreeNode rtn;
    sparta::TreeNode     device_tn(&rtn, "my_device", "My Device TreeNode");
    sparta::Clock        clk("clock");
    rtn.setClock(&clk);

    sparta::PortSet ps(&rtn, "out_ports");

    std::unique_ptr<MyDeviceParams> my_dev_params_(new MyDeviceParams(&device_tn));
    std::unique_ptr<MyDevice>       my_device(new MyDevice(&device_tn, my_dev_params_.get()));

    sparta::DataOutPort<uint32_t> a_delay_out (&ps, "a_delay_out");
    sparta::DataOutPort<uint32_t> a_delay_out2(&ps, "a_delay_out2");

#ifdef EVENTS_DUAL_EXAMPLE
    sparta::bind(rtn.getChildAs<sparta::Port>("my_device.ports.a_delay_in_source1"), a_delay_out);
    sparta::bind(rtn.getChildAs<sparta::Port>("my_device.ports.a_delay_in_source2"), a_delay_out2);
#else
    sparta::bind(rtn.getChildAs<sparta::Port>("my_device.ports.a_delay_in"), a_delay_out);
#endif

    rtn.enterConfiguring();
    rtn.enterFinalized();
    clk.getScheduler()->finalize();
    clk.getScheduler()->run(1); // perform initializations

    a_delay_out.send(1234);

    // It is an error to drive a port that is not bound
    if(a_delay_out2.isBound()) {
        a_delay_out2.send(4321);
    }

    clk.getScheduler()->run(1);

    rtn.enterTeardown();

    return 0;
}
