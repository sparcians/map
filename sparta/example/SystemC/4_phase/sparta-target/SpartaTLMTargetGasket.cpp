
#include "SpartaTLMTargetGasket.hpp"
#include "MemoryRequest.hpp"
#include "SpartaMemory.hpp"
#include "reporting.h"
//#define DIRECT_MEMORY_OPERATION 1

namespace sparta_target
{

  static const char *filename = "SpartaTLMTargetGasket.cpp"; ///< filename for reporting

  int SpartaTLMTargetGasket::nextID = 0;

  tlm::tlm_sync_enum SpartaTLMTargetGasket::nb_transport_fw(tlm::tlm_generic_payload &gp,
                                                            tlm::tlm_phase &phase,
                                                            sc_core::sc_time &delay_time)
  {
    std::ostringstream msg; // log message

    tlm::tlm_sync_enum return_val = tlm::TLM_COMPLETED;
    switch (phase)
    {
    case tlm::BEGIN_REQ:
    {
      std::cout << "Info: Gasket: BEGIN_REQ" << std::endl;
      //sc_core::sc_time delay_time = delay_time + m_accept_delay;

      MemoryRequest request = {
          (gp.get_command() == tlm::TLM_READ_COMMAND ? MemoryRequest::Command::READ : MemoryRequest::Command::WRITE),
          gp.get_address(),
          gp.get_data_length(),
          // Always scary pointing to memory owned by someone else...
          gp.get_data_ptr(),
          (void *)&gp};
      event_end_req_.preparePayload(request)->schedule(calculateSpartaOffset(m_accept_delay.value() + delay_time.value()));
      return_val = tlm::TLM_ACCEPTED;
      break;
    }
    case tlm::END_RESP:
      //  std::ostringstream        msg;                    // log message
      //m_end_resp_rcvd_event.notify (sc_core::SC_ZERO_TIME);
      std::cout << "Info: Gasket: END_RESP" << std::endl;
      return_val = tlm::TLM_COMPLETED;
      break;
      //=============================================================================
    case tlm::END_REQ:
    case tlm::BEGIN_RESP:
    {
      msg << "Target: " << m_ID
          << " Illegal phase received by target -- END_REQ or BEGIN_RESP";
      REPORT_FATAL(filename, __FUNCTION__, msg.str());
      return_val = tlm::TLM_ACCEPTED;
      break;
    }

      //=============================================================================
    default:
    {
      return_val = tlm::TLM_ACCEPTED;
      if (!m_nb_trans_fw_prev_warning)
      {
        msg << "Target: " << m_ID
            << " unknown phase " << phase << " encountered";
        REPORT_WARNING(filename, __FUNCTION__, msg.str());
        m_nb_trans_fw_prev_warning = true;
      }
      break;
    }
    }
    return return_val;
  }

  void SpartaTLMTargetGasket::send_end_request_(const MemoryRequest &req)
  {
    std::ostringstream msg; // log message
    msg.str("");
    tlm::tlm_sync_enum status = tlm::TLM_COMPLETED;

    msg.str("");
    msg << "Target: " << m_ID
        << " starting end-request method";

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    // m_target_memory.get_delay(gp, delay); // get memory operation delay

#ifdef DIRECT_MEMORY_OPERATION
    delay_time += m_accept_delay;
    m_response_PEQ.notify(gp, delay_time);
#else
    //  m_target_memory.operation(gp, delay_time); // perform memory operation now

    if (SPARTA_EXPECT_FALSE(info_logger_))
    {
      info_logger_ << " sending to memory model: " << req;
    }

    out_memory_request_.send(req, getClock()->getCycle(calculateSpartaOffset(0)));
#endif
    tlm::tlm_phase phase = tlm::END_REQ;
    delay = sc_core::SC_ZERO_TIME;

    msg << endl
        << "      "
        << "Target: " << m_ID
        << " transaction moved to send-response PEQ "
        << endl
        << "      ";
    msg << "Target: " << m_ID
        << " nb_transport_bw (GP, "
        << report::print(phase) << ", "
        << delay << ")";
    REPORT_INFO(filename, __FUNCTION__, msg.str());

    auto &gp = *((tlm::tlm_generic_payload *)req.meta_data);
    gp.set_response_status(tlm::TLM_OK_RESPONSE);
    //-----------------------------------------------------------------------------
    // Call nb_transport_bw with phase END_REQ check the returned status
    //-----------------------------------------------------------------------------
    status = m_memory_socket->nb_transport_bw(gp, phase, delay);

    msg.str("");
    msg << "Target: " << m_ID
        << " " << report::print(status) << " (GP, "
        << report::print(phase) << ", "
        << delay << ")";
    REPORT_INFO(filename, __FUNCTION__, msg.str());

    switch (status)
    {
      //=============================================================================
    case tlm::TLM_ACCEPTED:
    {
      // more phases will follow

      break;
    }

      //=============================================================================
    case tlm::TLM_COMPLETED:
    {
      msg << "Target: " << m_ID
          << " TLM_COMPLETED invalid response to END_REQ" << endl
          << "      Initiator must receive data before ending transaction";
      REPORT_FATAL(filename, __FUNCTION__, msg.str());
      break;
    }

      //=============================================================================
    case tlm::TLM_UPDATED:
    {
      msg << "Target: " << m_ID
          << " TLM_UPDATED invalid response to END_REQ" << endl
          << "      Initiator must receive data before updating transaction";
      REPORT_FATAL(filename, __FUNCTION__, msg.str());

      break;
    }

      //=============================================================================
    default:
    {
      msg << "Target: " << m_ID
          << " Illegal return status";
      REPORT_FATAL(filename, __FUNCTION__, msg.str());

      break;
    }
    } // end switch
  }

  void SpartaTLMTargetGasket::forwardMemoryResponse_(const MemoryRequest &req)
  {
    std::ostringstream msg; // log message
    msg.str("");

    // non-const lvalues
    tlm::tlm_phase resp = tlm::BEGIN_RESP;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    if (SPARTA_EXPECT_FALSE(info_logger_))
    {
      info_logger_ << " sending back to transactor: " << req;
    }

    auto &gp = *((tlm::tlm_generic_payload *)req.meta_data);
    gp.set_response_status(tlm::TLM_OK_RESPONSE);

    //m_response_PEQ.notify(gp, delay);      // put transaction in the PEQ

    // Send back the response to the initiator
    auto status = m_memory_socket->nb_transport_bw(*((tlm::tlm_generic_payload *)req.meta_data),
                                                   resp, delay);
    switch (status)
    {

      //=============================================================================
    case tlm::TLM_COMPLETED:
    {
      return;
      break;
    }

      //=============================================================================
    case tlm::TLM_ACCEPTED:
    {
      return;
      break;
    }

      //=============================================================================
    case tlm::TLM_UPDATED:
    {
      if (!m_begin_resp_method_prev_warning)
      {
        msg << "Target: " << m_ID
            << " TLM_UPDATED invalid response to BEGIN_RESP";
        REPORT_WARNING(filename, __FUNCTION__, msg.str());
      }
      else
        m_begin_resp_method_prev_warning = true;
      break;
    }

      //=============================================================================
    default:
    {
      if (!m_begin_resp_method_prev_warning)
      {
        msg << "Target: " << m_ID
            << " undefined return status ";
        REPORT_WARNING(filename, __FUNCTION__, msg.str());
      }
      else
        m_begin_resp_method_prev_warning = true;
      break;
    }
    } // end switch
  }

} // namespace sparta_target
