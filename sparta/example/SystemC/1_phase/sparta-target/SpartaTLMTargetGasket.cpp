
#include "SpartaTLMTargetGasket.hpp"
#include "MemoryRequest.hpp"
#include "SpartaMemory.hpp"

namespace sparta_target
{
    int SpartaTLMTargetGasket::nextID = 0;
    tlm::tlm_sync_enum SpartaTLMTargetGasket::nb_transport_fw (tlm::tlm_generic_payload &gp,
                                                               tlm::tlm_phase           &phase ,
                                                               sc_core::sc_time         &delay_time )
    {
    tlm::tlm_sync_enum return_val = tlm::TLM_COMPLETED;
        switch(phase) {
        case tlm::BEGIN_REQ: 
        {
            std::cout << "Info: Gasket: BEGIN_REQ" << std::endl;
            m_target_memory.operation(gp, delay_time); // perform memory operation now

    /*    // Convert the tlm GP to a sparta-based type.  If the modeler
        // chooses to use Sparta components to handle SysC data types,
        // the modeler could just pass the payload through as a
        // pointer on the DataOutPort.
        MemoryRequest request = {
            (gp.get_command() == tlm::TLM_READ_COMMAND ?
             MemoryRequest::Command::READ : MemoryRequest::Command::WRITE),
            gp.get_address(),
            gp.get_data_length(),

            // Always scary pointing to memory owned by someone else...
            gp.get_data_ptr(),
            (void*)&gp};

        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << " sending to memory model: " << request;
        }
  
        //
        // This is a transaction coming from SysC that is on SysC's
        // clock, not Sparta's.  Need to find the same tick cycle on
        // the Sparta clock and align the time for the transaction.
        // Keep in mind that Sparta's scheduler starts on tick 1, not
        // 0 like SysC.
        //
        // For example,
        //   - The Sparta's clock is at 7 ticks (6 from SysC POV, hence the - 1)
        //   - The SysC clock is at 10 ticks
        //   - The transaction's delay is 1 tick (to be fired at tick 11)
        //
        //   sysc_clock - sparta_clock + delay = 4 cycles on sparta clock (11)
        //
        const auto current_tick = getClock()->currentTick() - 1;
        sparta_assert(sc_core::sc_time_stamp().value() >= current_tick);
        const auto final_relative_tick =
            sc_core::sc_time_stamp().value() - current_tick + delay_time.value();

        // Send to memory with the given delay - NS -> clock cycles.
        // The Clock is on the same freq as the memory block
        out_memory_request_.send(request, getClock()->getCycle(final_relative_tick));
*/
        
        // Assume accepted.  In a real system, the gasket could keep
        // track of credits in the downstream component and the
        // initiator of the request.  In that case, the gasket would
        // either queue the requests or deny the forward
            return_val =  tlm::TLM_COMPLETED;
        break;
        }
        case tlm::END_RESP:
        std::cout << "Info: Gasket: END_RESP" << std::endl;
        return_val = tlm::TLM_COMPLETED;
        break;
        default:
        {
        return_val = tlm::TLM_ACCEPTED;
        break;
        }
        }
        return return_val;
    }

    void SpartaTLMTargetGasket::forwardMemoryResponse_(const MemoryRequest & req)
    {
        
        // non-const lvalues
        tlm::tlm_phase resp    = tlm::BEGIN_RESP;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << " sending back to transactor: " << req;
        }


        auto & gp = *((tlm::tlm_generic_payload*)req.meta_data);
        gp.set_response_status(tlm::TLM_OK_RESPONSE);

        // Send back the response to the initiator
        auto status = m_memory_socket->nb_transport_bw(*((tlm::tlm_generic_payload*)req.meta_data),
                                                       resp, delay);
        (void)status;
    }

}
