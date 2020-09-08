
#include "sysc/kernel/sc_module.h"

// Found in the SystemC examples directory
#include "initiator_top.h"
#include "SimpleBusAT.h"

namespace systemc_example
{
    class systemc_example_top : public sc_core::sc_module
    {
    public:
        systemc_example_top(sc_core::sc_module_name name);
        SimpleBusAT<2, 1>       m_bus;
        initiator_top           m_initiator_1;
        initiator_top           m_initiator_2;
    };

}
