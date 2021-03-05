
#include "systemc_example_top.hpp"

namespace systemc_example
{
    systemc_example_top::systemc_example_top(sc_core::sc_module_name name) :
        sc_module(name),
        m_bus("m_bus"),
        m_initiator_1("m_initiator_1", 101,
                      0x0000000000000100ull, 0x0000000010000100, 2),
        m_initiator_2("m_initiator_2", 102,
                      0x0000000000000200ull, 0x0000000010000200, 2)
    {
        /// bind TLM2 initiators to TLM2 target sockets on SimpleBus
        m_initiator_1.initiator_socket(m_bus.target_socket[0]);
        m_initiator_2.initiator_socket(m_bus.target_socket[1]);
   }
}
