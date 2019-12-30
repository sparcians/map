// <ROB.cpp> -*- C++ -*-


#include <algorithm>
#include "ROB.hpp"
#include "sparta/events/StartupEvent.hpp"

namespace core_example
{
    const char ROB::name[] = "rob";

    ROB::ROB(sparta::TreeNode * node,
             const ROBParameterSet * p) :
        sparta::Unit(node),
        stat_ipc_(&unit_stat_set_,
                  "ipc",
                  "Instructions retired per cycle",
                  &unit_stat_set_,
                  "total_number_retired/cycles"),
        num_retired_(&unit_stat_set_,
                     "total_number_retired",
                     "The total number of instructions retired by this core",
                     sparta::Counter::COUNT_NORMAL),
        num_flushes_(&unit_stat_set_,
                     "total_number_of_flushes",
                     "The total number of flushes performed by the ROB",
                     sparta::Counter::COUNT_NORMAL),
        num_to_retire_(p->num_to_retire),
        num_insts_to_retire_(p->num_insts_to_retire),
        reorder_buffer_("ReorderBuffer", p->retire_queue_depth,
                        node->getClock(), &unit_stat_set_)
    {
        // Set a cycle delay on the retire, just for kicks
        ev_retire_.setDelay(1);

        // Set up the reorder buffer to support pipeline collection.
        reorder_buffer_.enableCollection(node);

        in_reorder_buffer_write_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(ROB, robAppended_, InstGroup));

        in_reorder_flush_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(ROB, handleFlush_,
                                                                  FlushManager::FlushingCriteria));

        // This event is ALWAYS scheduled, but it should not keep
        // simulation continuing on.
        ev_ensure_forward_progress_.setContinuing(false);

        // Send initial credits to anyone that cares.  Probably Dispatch.
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(ROB, sendInitialCredits_));
    }

    /// Destroy!
    ROB::~ROB() {
        // Logging can be done from destructors in the correct simulator setup
        info_logger_ << "ROB is destructing now, but you can still see this message";
    }

    void ROB::sendInitialCredits_()
    {
        out_reorder_buffer_credits_.send(reorder_buffer_.capacity());
        ev_ensure_forward_progress_.schedule(retire_timeout_interval_);
    }

    void ROB::retireEvent_() {
        retireInstructions_();
        if (reorder_buffer_.size() > 0) {
            ev_retire_.schedule(sparta::Clock::Cycle(1));
        }

        if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            info_logger_ << "Retire event";
        }
    }

    // An illustration of the use of the callback -- instead of
    // getting a reference, you can pull the data from the port
    // directly, albeit inefficient and superfluous here...
    void ROB::robAppended_(const InstGroup &) {
        for(auto & i : in_reorder_buffer_write_.pullData()) {
            reorder_buffer_.push(i);
        }

        ev_retire_.schedule(sparta::Clock::Cycle(0));
        if(info_logger_) {
            info_logger_ << "Retire appended";
        }
    }

    void ROB::handleFlush_(const FlushManager::FlushingCriteria &)
    {
        // Clean up internals and send new credit count
        out_reorder_buffer_credits_.send(reorder_buffer_.size());
        reorder_buffer_.clear();
    }

    void ROB::retireInstructions_() {
        const uint32_t num_to_retire = std::min(reorder_buffer_.size(), num_to_retire_);

        if(SPARTA_EXPECT_FALSE(info_logger_.observed())) {
            info_logger_ << "Retire event, num to retire: " << num_to_retire;
        }

        uint32_t retired_this_cycle = 0;
        for(uint32_t i = 0; i < num_to_retire; ++i)
        {
            auto & ex_inst_ptr = reorder_buffer_.access(0);
            auto & ex_inst = *ex_inst_ptr;
            sparta_assert(ex_inst.isSpeculative() == false,
                        "Uh, oh!  A speculative instruction is being retired: " << ex_inst);
            if(ex_inst.getStatus() == ExampleInst::Status::COMPLETED)
            {
                // UPDATE:
                ex_inst.setStatus(ExampleInst::Status::RETIRED);
                if (ex_inst.isStoreInst()) {
                    out_rob_retire_ack_.send(ex_inst_ptr);
                }

                ++num_retired_;
                ++retired_this_cycle;
                reorder_buffer_.pop();

                if(SPARTA_EXPECT_FALSE(info_logger_)) {
                    info_logger_ << "Retiring " << ex_inst;
                }

                if(SPARTA_EXPECT_FALSE((num_retired_ % 1000000) == 0)) {
                    std::cout << "Retired " << num_retired_
                              << " instructions" << std::endl;
                }
                // Will be true if the user provides a -i option
                if (SPARTA_EXPECT_FALSE((num_retired_ == num_insts_to_retire_))) {
                    getScheduler()->stopRunning();
                    break;
                }

                // This is rare for the example
                if(SPARTA_EXPECT_FALSE(ex_inst.getUnit() == ExampleInst::TargetUnit::ROB))
                {
                    if(SPARTA_EXPECT_FALSE(info_logger_)) {
                        info_logger_ << "Instigating flush... " << ex_inst;
                    }
                    // Signal flush to the system
                    out_retire_flush_.send(ex_inst.getUniqueID());

                    // Redirect fetch
                    out_fetch_flush_redirect_.send(ex_inst.getVAdr() + 4);

                    ++num_flushes_;
                    break;
                }

            }
            else {
                ex_inst.setLast(true, &ev_retire_);
                break;
            }
        }
        out_reorder_buffer_credits_.send(retired_this_cycle);
        last_retirement_ = getClock()->currentCycle();
    }

    // Make sure the pipeline is making forward progress
    void ROB::checkForwardProgress_()
    {
        if(getClock()->currentCycle() - last_retirement_ >= retire_timeout_interval_)
        {
            sparta::SpartaException e;
            e << "Been a while since we've retired an instruction.  Is the pipe stalled indefinitely?";
            throw e;
        }
        ev_ensure_forward_progress_.schedule(retire_timeout_interval_);
    }

}
