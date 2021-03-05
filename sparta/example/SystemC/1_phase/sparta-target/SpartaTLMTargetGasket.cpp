
#include "SpartaTLMTargetGasket.hpp"
#include "MemoryRequest.hpp"
#include "SpartaMemory.hpp"
#include "reporting.h"
#include "sparta/utils/SysCSpartaSchedulerAdapter.hpp"

//#define DIRECT_MEMORY_OPERATION 1

namespace sparta_target
{

    static const char *filename = "SpartaTLMTargetGasket.cpp"; ///< filename for reporting

    int SpartaTLMTargetGasket::nextID = 0;
    tlm::tlm_sync_enum SpartaTLMTargetGasket::nb_transport_fw(tlm::tlm_generic_payload &gp,
                                                              tlm::tlm_phase &phase,
                                                              sc_core::sc_time &delay_time)
    {
        tlm::tlm_sync_enum return_val = tlm::TLM_COMPLETED;
        switch (phase)
        {
            case tlm::BEGIN_REQ:
            {
                std::cout << "Info: Gasket: BEGIN_REQ" << std::endl;
                //-----------------------------------------------------------------------------
                // Force synchronization multiple timing points by returning TLM_ACCEPTED
                // use a payload event queue to schedule BEGIN_RESP timing point
                //-----------------------------------------------------------------------------
                target_memory_.get_delay(gp, delay_time); // get memory operation delay

#ifdef DIRECT_MEMORY_OPERATION
                delay_time += accept_delay_;
                response_PEQ_.notify(gp, delay_time);
#else
                //  target_memory_.operation(gp, delay_time); // perform memory operation now

                // Convert the tlm GP to a sparta-based type.  If the modeler
                // chooses to use Sparta components to handle SysC data types,
                // the modeler could just pass the payload through as a
                // pointer on the DataOutPort.
                MemoryRequest request = {
                    (gp.get_command() == tlm::TLM_READ_COMMAND ? MemoryRequest::Command::READ : MemoryRequest::Command::WRITE),
                    gp.get_address(),
                    gp.get_data_length(),

                    // Always scary pointing to memory owned by someone else...
                    gp.get_data_ptr(),
                    (void *)&gp};

                if (SPARTA_EXPECT_FALSE(info_logger_))
                {
                    info_logger_ << " sending to memory model: " << request;
                }

                // Send to memory with the given delay - NS -> clock cycles.
                // The Clock is on the same freq as the memory block
                out_memory_request_.send(request, getClock()->getCycle
                                         (sparta::sparta_sysc_utils::calculateSpartaOffset(getClock(),delay_time.value())));
#endif
                delay_time = accept_delay_;
                // In a real system, the gasket could keep
                // track of credits in the downstream component and the
                // initiator of the request.  In that case, the gasket would
                // either queue the requests or deny the forward
                return_val = tlm::TLM_ACCEPTED;
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

    void SpartaTLMTargetGasket::send_end_request_(const MemoryRequest &req)
    {
        //Not implemented for 1 phase target
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
        auto status = memory_socket_->nb_transport_bw(*((tlm::tlm_generic_payload *)req.meta_data),
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
                if (!begin_resp_method_prev_warning_)
                {
                    msg << "Target: " << ID_
                        << " TLM_UPDATED invalid response to BEGIN_RESP";
                    REPORT_WARNING(filename, __FUNCTION__, msg.str());
                }
                else
                    begin_resp_method_prev_warning_ = true;
                break;
            }

            //=============================================================================
            default:
            {
                if (!begin_resp_method_prev_warning_)
                {
                    msg << "Target: " << ID_
                        << " undefined return status ";
                    REPORT_WARNING(filename, __FUNCTION__, msg.str());
                }
                else
                    begin_resp_method_prev_warning_ = true;
                break;
            }
        } // end switch
    }
} // namespace sparta_target
