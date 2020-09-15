
#include "SpartaTLMTargetGasket.hpp"
#include "MemoryRequest.hpp"
#include "SpartaMemory.hpp"
#include "reporting.h"
//#define DIRECT_MEMORY_OPERATION 1

namespace sparta_target
{

    static const char *filename = "SpartaTLMTargetGasket.cpp";	///< filename for reporting

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
    //-----------------------------------------------------------------------------
// Force synchronization multiple timing points by returning TLM_ACCEPTED 
// use a payload event queue to schedule BEGIN_RESP timing point  
//-----------------------------------------------------------------------------
        m_target_memory.get_delay(gp, delay_time);  // get memory operation delay
        
#ifdef DIRECT_MEMORY_OPERATION    
        delay_time += m_accept_delay;
        m_response_PEQ.notify(gp, delay_time);  
#else
     //  m_target_memory.operation(gp, delay_time); // perform memory operation now

        // Convert the tlm GP to a sparta-based type.  If the modeler
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
        auto current_sc_time = sc_core::sc_time_stamp().value();
        const auto current_tick = getClock()->currentTick() - 1;
        sparta_assert(sc_core::sc_time_stamp().value() >= current_tick);
        const auto final_relative_tick =
            current_sc_time - current_tick + delay_time.value() + m_accept_delay.value();

        // Send to memory with the given delay - NS -> clock cycles.
        // The Clock is on the same freq as the memory block
        out_memory_request_.send(request, getClock()->getCycle(final_relative_tick));
#endif
        delay_time = m_accept_delay;
        // In a real system, the gasket could keep
        // track of credits in the downstream component and the
        // initiator of the request.  In that case, the gasket would
        // either queue the requests or deny the forward
            return_val =  tlm::TLM_ACCEPTED;
        break;
        }
        case tlm::END_RESP:
        m_end_resp_rcvd_event.notify (sc_core::SC_ZERO_TIME);
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
       // tlm::tlm_phase resp    = tlm::BEGIN_RESP;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << " sending back to transactor: " << req;
        }


        auto & gp = *((tlm::tlm_generic_payload*)req.meta_data);
        gp.set_response_status(tlm::TLM_OK_RESPONSE);
        
        m_response_PEQ.notify(gp, delay);      // put transaction in the PEQ


        /* Send back the response to the initiator
       // auto status = m_memory_socket->nb_transport_bw(*((tlm::tlm_generic_payload*)req.meta_data),
                                                       resp, delay);
        (void)status;*/
    }

    
//=============================================================================
/// begin_response method function implementation
//
// This method is statically sensitive to m_response_PEQ.get_event 
//
//=============================================================================
void SpartaTLMTargetGasket::begin_response_method (void)
{
  std::ostringstream        msg;                    // log message
  tlm::tlm_generic_payload  *transaction_ptr;       // generic payload pointer
  msg.str("");
  tlm::tlm_sync_enum        status = tlm::TLM_COMPLETED;

//-----------------------------------------------------------------------------  
//  Process all transactions scheduled for current time a return value of NULL 
//  indicates that the PEQ is empty at this time
//----------------------------------------------------------------------------- 

  while ((transaction_ptr = m_response_PEQ.get_next_transaction()) != NULL)
  {
    msg.str("");
    msg << "Target: " << m_ID 
        << " starting response method";
    REPORT_INFO(filename,  __FUNCTION__, msg.str());    
      
    sc_core::sc_time delay  = sc_core::SC_ZERO_TIME;
   #ifdef DIRECT_MEMORY_OPERATION
    m_target_memory.operation(*transaction_ptr, delay); /// perform memory operation
   #endif
    tlm::tlm_phase  phase = tlm::BEGIN_RESP; 
                    delay = sc_core::SC_ZERO_TIME;
                    
    msg.str("");
    msg << "Target: " << m_ID 
        << " nb_transport_bw (GP, BEGIN_RESP, SC_ZERO_TIME)";
    REPORT_INFO(filename,  __FUNCTION__, msg.str());

//-----------------------------------------------------------------------------
// Call nb_transport_bw with phase BEGIN_RESP check the returned status 
//-----------------------------------------------------------------------------
    status = m_memory_socket->nb_transport_bw(*transaction_ptr, phase, delay);
    
    msg.str("");
    msg << "Target: " << m_ID
        << " " << report::print(status) << " (GP, "
        << report::print(phase) << ", "
        << delay << ")"; 
    REPORT_INFO(filename,  __FUNCTION__, msg.str());

    switch (status)
    { 
    
//=============================================================================
    case tlm::TLM_COMPLETED:    
      {          
        next_trigger (delay);               // honor the annotated delay 
        return; 
        break;
      }
      
//=============================================================================
    case tlm::TLM_ACCEPTED:
      {     
        next_trigger (m_end_resp_rcvd_event); // honor end-response rule  
        return; 
        break;
      }

//=============================================================================
    case tlm::TLM_UPDATED:   
      {
      if(!m_begin_resp_method_prev_warning)
        {
          msg << "Target: " << m_ID 
              << " TLM_UPDATED invalid response to BEGIN_RESP";
          REPORT_WARNING(filename, __FUNCTION__, msg.str()); 
        }
      else m_begin_resp_method_prev_warning = true;
      break;
      }
 
//=============================================================================
    default:                  
      {
        if(!m_begin_resp_method_prev_warning)
          {
            msg << "Target: " << m_ID 
                << " undefined return status ";
           REPORT_WARNING(filename, __FUNCTION__, msg.str()); 
          }
        else m_begin_resp_method_prev_warning = true;
        break;
      }
    }// end switch
      
  } // end while
  
  next_trigger (m_response_PEQ.get_event()); 

} //end begin_response_queue_active

void SpartaTLMTargetGasket::end_request_method(void) {
	//Not implemented for 1 phase
}
}
