// <Decode.cpp> -*- C++ -*-


#include <algorithm>

#include "Decode.hpp"
#include "sparta/events/StartupEvent.hpp"

namespace core_example
{
    Decode::Decode(sparta::TreeNode * node,
                   const DecodeParameterSet * p) :
        sparta::Unit(node),
        fetch_queue_("FetchQueue", p->fetch_queue_size, node->getClock(), &unit_stat_set_),
        num_to_decode_(p->num_to_decode)
    {
        fetch_queue_.enableCollection(node);

        fetch_queue_write_in_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Decode, fetchBufferAppended_, InstGroup));
        uop_queue_credits_in_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Decode, receiveUopQueueCredits_, uint32_t));
        in_reorder_flush_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Decode, handleFlush_, FlushManager::FlushingCriteria));

        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(Decode, sendInitialCredits_));
    }

    // Send fetch the initial credit count
    void Decode::sendInitialCredits_()
    {
        fetch_queue_credits_outp_.send(fetch_queue_.capacity());
    }

    // Receive Uop credits from Dispatch
    void Decode::receiveUopQueueCredits_(const uint32_t & credits) {
        uop_queue_credits_ += credits;
        if (fetch_queue_.size() > 0) {
            ev_decode_insts_event_.schedule(sparta::Clock::Cycle(0));
        }

        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "Received credits: " << uop_queue_credits_in_;
        }
    }

    // Called when the fetch buffer was appended by Fetch.  If decode
    // has the credits, then schedule a decode session.  Otherwise, go
    // to sleep
    void Decode::fetchBufferAppended_(const InstGroup & insts)
    {
        // Cache the instructions in the instruction queue if we can't decode this cycle
        for(auto & i : insts)
        {
            fetch_queue_.push(i);

            if(SPARTA_EXPECT_FALSE(info_logger_)) {
                info_logger_ << "Got inst: " << i;
            }
        }
        if (uop_queue_credits_ > 0) {
            ev_decode_insts_event_.schedule(sparta::Clock::Cycle(0));
        }
    }

    // Handle incoming flush
    void Decode::handleFlush_(const FlushManager::FlushingCriteria & criteria)
    {
        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "Got a flush call for " << criteria;
        }
        fetch_queue_credits_outp_.send(fetch_queue_.size());
        fetch_queue_.clear();
    }

    // Decode instructions
    void Decode::decodeInsts_()
    {
        uint32_t num_decode = std::min(uop_queue_credits_, fetch_queue_.size());
        num_decode = std::min(num_decode, num_to_decode_);

        if(num_decode > 0)
        {
            InstGroup insts;
            // Send instructions on their way to rename
            for(uint32_t i = 0; i < num_decode; ++i) {
                insts.emplace_back(fetch_queue_.read(0));

                if(SPARTA_EXPECT_FALSE(info_logger_)) {
                    info_logger_ << "Decoded inst: " << fetch_queue_.read(0);
                }

                fetch_queue_.pop();
            }

            // Send decoded instructions to rename
            uop_queue_outp_.send(insts);

            // Decrement internal Uop Queue credits
            uop_queue_credits_ -= num_decode;

            // Send credits back to Fetch to get more instructions
            fetch_queue_credits_outp_.send(num_decode);
        }

        // If we still have credits to send instructions as well as
        // instructions in the queue, schedule another decode session
        if(uop_queue_credits_ > 0 && fetch_queue_.size() > 0) {
            ev_decode_insts_event_.schedule(1);
        }
    }
}
