#pragma once

#include <cinttypes>
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/events/PayloadEvent.hpp"
#include "sparta/ports/DataPort.hpp"
#include "MemoryRequest.hpp"
#include "tlm.h"
#include "tlm_utils/peq_with_get.h"                   // Payload event queue FIFO
#include "memory.h"


namespace sparta_target
{

    class SpartaTLMTargetGasket : public sparta::Unit,
                                  public sc_core::sc_module,
                                  public tlm::tlm_fw_transport_if<>
    {
    protected:
        static int nextID;

    public:
        static constexpr char name[] = "mem_tlm_gasket";
        static constexpr char scName[2][20] = {"mem_tlm_gasket0", "mem_tlm_gasket1"};

        class SpartaTLMTargetGasketParams : public sparta::ParameterSet
        {
        public:
            explicit SpartaTLMTargetGasketParams(sparta::TreeNode * n) :
                sparta::ParameterSet(n)
            {}
        };

        /**
         * \brief Construction of the Sparta TLM gasket
         *
         * \param node   The Sparta tree node
         * \param params The Gasket parameters
         * \param module_name The module name -- must be a variable that lives through construction.
         *
         * Explanation of the third parameter "module_name":
         *
         *   SystemC uses a global stack to determine the latest
         *   module (based on its name) being constructed for the
         *   tlm_fw_transport_if.  If this variable is a temporary, it
         *   will destruct BEFORE initializing the tlm_fw_transport_if
         *   and nullify the "current module being constructed."
         *
         *   You'll get this cryptic message:
         *
         *   "Error: (E122) sc_export specified outside of module:
         *    export 'tlm_base_target_socket_0' (sc_object)
         *
         *   To get around this nonsense, the module name (as a
         *   sc_core::sc_module_name) must live throughout the
         *   construction of the module.
         */
        SC_HAS_PROCESS(SpartaTLMTargetGasket);
        SpartaTLMTargetGasket(sparta::TreeNode * node,
                              const SpartaTLMTargetGasketParams * params,
                              sc_core::sc_module_name module_name = scName[nextID]) :
            Unit(node),
            sc_module(module_name),
            ID_ (nextID),
            target_memory_(
                nextID
                , sc_core::sc_time(50, sc_core::SC_NS)  // read response delay
                , sc_core::sc_time(30, sc_core::SC_NS) // write response delay)
                , 4*1024                                // memory size (bytes)
                , 4                                     // memory width (bytes)
                ),
            accept_delay_(sc_core::sc_time(0, sc_core::SC_NS))
        {
            setAutoPrecedence(false);
            // This confusing call binds this TLM socket's
            // tlm_fw_transport_if API to this class for
            // nb_transport_fw calls.  The nb_transport_bw call
            // remains unset.
            nextID++;
            memory_socket_(*this);

            // Register the callback for finished transactions coming
            // from the Sparta Memory model
            in_memory_response_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(SpartaTLMTargetGasket,
                                                 forwardMemoryResponse_, MemoryRequest));
        }

        void setTreeNode(sparta::TreeNode *treeNodePtr);

        tlm::tlm_target_socket<> & getMemorySocket() { return memory_socket_; }

    private:

        tlm::tlm_target_socket<>  memory_socket_;

        const unsigned int        ID_;                   ///< target ID
        memory target_memory_;
        sc_core::sc_time accept_delay_;
        // Nothing should call this function directly.  Should be done
        // through the tlm::tlm_fw_transport_if<> pointer
        tlm::tlm_sync_enum nb_transport_fw (tlm::tlm_generic_payload &gp,
                                            tlm::tlm_phase           &phase ,
                                            sc_core::sc_time         &delay_time ) override final;

        sparta::DataInPort<MemoryRequest>  in_memory_response_ {getPortSet(), "in_memory_response"};
        sparta::DataOutPort<MemoryRequest> out_memory_request_ {getPortSet(), "out_memory_request"};
        void send_end_request_(const MemoryRequest &);
        void forwardMemoryResponse_(const MemoryRequest &);
        unsigned long request_count_;        ///< used to calc synch transactions
        bool          nb_trans_fw_prev_warning_;
        bool          begin_resp_method_prev_warning_;
        bool          trans_dbg_prev_warning_;
        bool          get_dm_ptr_prev_warning_;

        // An event to be scheduled in the
        // sparta::SchedulingPhase::Tick phase if data is received
        sparta::PayloadEvent<MemoryRequest, sparta::SchedulingPhase::Tick> event_end_req_
            {getEventSet(), "end_req_event",
             CREATE_SPARTA_HANDLER_WITH_DATA(SpartaTLMTargetGasket, send_end_request_, MemoryRequest)};

        // Not needed methods

        /// b_transport() - Blocking Transport
        void b_transport(tlm::tlm_generic_payload &payload, sc_core::sc_time &delay_time) override { }

        /// Not implemented for this example but required by interface
        bool get_direct_mem_ptr(tlm::tlm_generic_payload &payload, tlm::tlm_dmi &dmi_data) override
        { return false; }

        /// Not implemented for this example but required by interface
        unsigned int transport_dbg(tlm::tlm_generic_payload &payload) override { return 0; }


    };
}
