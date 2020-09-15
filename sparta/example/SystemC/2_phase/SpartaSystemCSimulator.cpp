
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ResourceTreeNode.hpp"
#include "sparta/simulation/ResourceFactory.hpp"

#include "SpartaSystemCSimulator.hpp"
#include "sparta-target/SpartaMemory.hpp"
#include "sparta-target/SpartaTLMTargetGasket.hpp"

namespace sparta_sim
{
    SpartaSystemCSimulator::SpartaSystemCSimulator(sparta::Scheduler * sched) :
        sparta::app::Simulation("SpartaSystemCSimulator", sched),
        sysc_sched_runner_(sched),
        systemc_example_top_("top")
    {
        getResourceSet()->
            addResourceFactory<sparta::ResourceFactory<sparta_target::SpartaMemory,
                                                       sparta_target::SpartaMemory::SpartaMemoryParameters>>();
        getResourceSet()->
            addResourceFactory<sparta::ResourceFactory<sparta_target::SpartaTLMTargetGasket,
                                                       sparta_target::SpartaTLMTargetGasket::SpartaTLMTargetGasketParams>>();
    }

    SpartaSystemCSimulator::~SpartaSystemCSimulator() {
        getRoot()->enterTeardown();
    }

    /**
     * Build the Sparta System
     *
     * This will create nodes:
     *
     *   top.sys.memory                # The memory model
     *           memory.mem_tlm_gasket # The gasket for TLM transactions
     *
     */
    void SpartaSystemCSimulator::buildTree_()
    {
        auto rtn = getRoot(); // gets the sparta "top" node

        // Create a sys node for the heck of it.
        sparta::TreeNode * sys = nullptr;
        tns_to_delete_.emplace_back(sys = new sparta::TreeNode(rtn,
                                                               "sys",
                                                               sparta::TreeNode::GROUP_NAME_NONE,
                                                               sparta::TreeNode::GROUP_IDX_NONE,
                                                               "Dummy System"));



        const auto total_targets = 2;
        for(uint32_t i = 0; i < total_targets; ++i) {
                    // Create the Memory on the dummy system node
        sparta::TreeNode * mem = nullptr;
        tns_to_delete_.emplace_back
            (mem = new sparta::ResourceTreeNode(sys,
                                                sparta_target::SpartaMemory::name + std::to_string(i), 
                                                sparta_target::SpartaMemory::name, i,
                                                "Dummy Memory",
                                                getResourceSet()->
                                                getResourceFactory(sparta_target::SpartaMemory::name)));
            // Put the gasket on the memory tree node (can really go anywhere)
            tns_to_delete_.emplace_back(new sparta::ResourceTreeNode(mem,
                                                                     sparta_target::SpartaTLMTargetGasket::name + std::to_string(i),
                                                                     sparta_target::SpartaTLMTargetGasket::name, i,
                                                                     "TLM gasket",
                                                                     getResourceSet()->
                                                                     getResourceFactory(sparta_target::SpartaTLMTargetGasket::name)));
        }
    }

    // Nothing for now...
    void SpartaSystemCSimulator::configureTree_() {}

    /**
     * Connect the memory in/out ports to the TLM gasket.  The SystemC
     * components are NOT bound to the Sparta components yet
     */
    void SpartaSystemCSimulator::bindTree_()
    {
        auto root_node = getRoot();
        sparta::bind(root_node->getChildAs<sparta::Port>("sys.memory0.ports.in_memory_request"),
                     root_node->getChildAs<sparta::Port>("sys.memory0.mem_tlm_gasket0.ports.out_memory_request"));
        sparta::bind(root_node->getChildAs<sparta::Port>("sys.memory0.ports.out_memory_response"),
                     root_node->getChildAs<sparta::Port>("sys.memory0.mem_tlm_gasket0.ports.in_memory_response"));

        sparta::bind(root_node->getChildAs<sparta::Port>("sys.memory1.ports.in_memory_request"),
                     root_node->getChildAs<sparta::Port>("sys.memory1.mem_tlm_gasket1.ports.out_memory_request"));
        sparta::bind(root_node->getChildAs<sparta::Port>("sys.memory1.ports.out_memory_response"),
                     root_node->getChildAs<sparta::Port>("sys.memory1.mem_tlm_gasket1.ports.in_memory_response"));
*/
        auto sparta_tlm_gasket0 =
            root_node->getChild("sys.memory0.mem_tlm_gasket0")->getResourceAs<sparta_target::SpartaTLMTargetGasket>();
        auto sparta_tlm_gasket1 =
            root_node->getChild("sys.memory1.mem_tlm_gasket1")->getResourceAs<sparta_target::SpartaTLMTargetGasket>();

        // SysC binding
        systemc_example_top_.m_bus.initiator_socket[0](sparta_tlm_gasket0->m_memory_socket);
       // systemc_example_top_.m_bus.initiator_socket[1](sparta_tlm_gasket1->m_memory_socket);
    }
}
