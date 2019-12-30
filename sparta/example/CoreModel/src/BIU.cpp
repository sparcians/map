// <BIU.cpp> -*- C++ -*-

#include "sparta/utils/SpartaAssert.hpp"
#include "BIU.hpp"

namespace core_example
{
    const char BIU::name[] = "biu";

    ////////////////////////////////////////////////////////////////////////////////
    // Constructor
    ////////////////////////////////////////////////////////////////////////////////

    BIU::BIU(sparta::TreeNode *node, const BIUParameterSet *p) :
        sparta::Unit(node),
        biu_req_queue_size_(p->biu_req_queue_size),
        biu_latency_(p->biu_latency)
    {
        in_biu_req_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(BIU, getReqFromLSU_, ExampleInstPtr));

        in_mss_ack_sync_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(BIU, getAckFromMSS_, bool));
        in_mss_ack_sync_.setPortDelay(static_cast<sparta::Clock::Cycle>(1));


        if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            info_logger_ << "BIU construct: #" << node->getGroupIdx();
        }
    }


    ////////////////////////////////////////////////////////////////////////////////
    // Callbacks
    ////////////////////////////////////////////////////////////////////////////////

    // Receive new BIU request from LSU
    void BIU::getReqFromLSU_(const ExampleInstPtr & inst_ptr)
    {
        appendReqQueue_(inst_ptr);

        // Schedule BIU request handling event only when:
        // (1)BIU is not busy, and (2)Request queue is not empty
        if (!biu_busy_) {
            // NOTE:
            // We could set this flag immediately here, but a better/cleaner way to do this is:
            // (1)Schedule the handling event immediately;
            // (2)Update flag in that event handler.

            ev_handle_biu_req_.schedule(sparta::Clock::Cycle(0));
            // NOTE:
            // The handling event must be scheduled immediately (0 delay). Otherwise,
            // BIU could potentially send another request to MSS before the busy flag is set
        }
        else {
            if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
                info_logger_ << "This request cannot be serviced right now, MSS is already busy!";
            }
        }
    }

    // Handle BIU request
    void BIU::handle_BIU_Req_()
    {
        biu_busy_ = true;
        out_mss_req_sync_.send(biu_req_queue_.front(), biu_latency_);

        if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            info_logger_ << "BIU request is sent to MSS!";
        }
    }

    // Handle MSS Ack
    void BIU::handle_MSS_Ack_()
    {
        out_biu_ack_.send(biu_req_queue_.front());
        biu_req_queue_.pop_front();
        biu_busy_ = false;

        // Schedule BIU request handling event only when:
        // (1)BIU is not busy, and (2)Request queue is not empty
        if (biu_req_queue_.size() > 0) {
            ev_handle_biu_req_.schedule(sparta::Clock::Cycle(0));
        }

        if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            info_logger_ << "MSS Ack is sent to LSU!";
        }
    }

    // Receive MSS access acknowledge
    void BIU::getAckFromMSS_(const bool & done)
    {
        if (done) {
            ev_handle_mss_ack_.schedule(sparta::Clock::Cycle(0));

            if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
                info_logger_ << "MSS Ack is received!";
            }

            return;
        }

        // Right now we expect MSS ack is always true
        sparta_assert(false, "MSS is NOT done!");
    }


    ////////////////////////////////////////////////////////////////////////////////
    // Regular Function/Subroutine Call
    ////////////////////////////////////////////////////////////////////////////////

    // Append BIU request queue
    void BIU::appendReqQueue_(const ExampleInstPtr& inst_ptr)
    {
        sparta_assert(biu_req_queue_.size() <= biu_req_queue_size_ ,"BIU request queue overflows!");

        // Push new requests from back
        biu_req_queue_.emplace_back(inst_ptr);

        if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            info_logger_ << "Append BIU request queue!";
        }
    }

}
