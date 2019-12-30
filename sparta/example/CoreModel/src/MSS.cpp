
#include "sparta/utils/SpartaAssert.hpp"
#include "MSS.hpp"

namespace core_example
{
    const char MSS::name[] = "mss";

    ////////////////////////////////////////////////////////////////////////////////
    // Constructor
    ////////////////////////////////////////////////////////////////////////////////

    MSS::MSS(sparta::TreeNode *node, const MSSParameterSet *p) :
        sparta::Unit(node),
        mss_latency_(p->mss_latency)
    {
        in_mss_req_sync_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(MSS, getReqFromBIU_, ExampleInstPtr));
        in_mss_req_sync_.setPortDelay(static_cast<sparta::Clock::Cycle>(1));


        if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            info_logger_ << "MSS construct: #" << node->getGroupIdx();
        }
    }


    ////////////////////////////////////////////////////////////////////////////////
    // Callbacks
    ////////////////////////////////////////////////////////////////////////////////

    // Receive new MSS request from BIU
    void MSS::getReqFromBIU_(const ExampleInstPtr & inst_ptr)
    {
        sparta_assert((inst_ptr != nullptr), "MSS is not handling a valid request!");

        // Handle MSS request event can only be scheduled when MMS is not busy
        if (!mss_busy_) {
            mss_busy_ = true;
            ev_handle_mss_req_.schedule(mss_latency_);
        }
        else {
            // Assumption: MSS can handle a single request each time
            sparta_assert(false, "MSS can never receive requests from BIU when it's busy!");
        }

        if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            info_logger_ << "MSS is busy servicing your request......";
        }
    }

    // Handle MSS request
    void MSS::handle_MSS_req_()
    {
        mss_busy_ = false;
        out_mss_ack_sync_.send(true);

        if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            info_logger_ << "MSS is done!";
        }
    }


    ////////////////////////////////////////////////////////////////////////////////
    // Regular Function/Subroutine Call
    ////////////////////////////////////////////////////////////////////////////////


}
