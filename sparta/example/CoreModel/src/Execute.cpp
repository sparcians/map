// <Execute.cpp> -*- C++ -*-

#include "sparta/utils/SpartaAssert.hpp"

#include "Execute.hpp"

namespace core_example
{
    const char Execute::name[] = "execute";
    Execute::Execute(sparta::TreeNode * node,
                     const ExecuteParameterSet * p) :
        sparta::Unit(node),
        ignore_inst_execute_time_(p->ignore_inst_execute_time),
        execute_time_(p->execute_time),
        scheduler_size_(p->scheduler_size),
        in_order_issue_(p->in_order_issue),
        collected_inst_(node, node->getName())
    {
        in_execute_inst_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Execute, getInstsFromDispatch_,
                                                                  ExampleInstPtr));

        in_reorder_flush_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Execute, flushInst_,
                                                                  FlushManager::FlushingCriteria));
        // Startup handler for sending initiatl credits
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(Execute, sendInitialCredits_));
        // Set up the precedence between issue and complete
        // Complete should come before issue because it schedules issue with a 0 cycle delay
        // issue should always schedule complete with a non-zero delay (which corresponds to the
        // insturction latency)
        complete_inst_ >> issue_inst_;

        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "Execute construct: #" << node->getGroupIdx();
        }

    }

    void Execute::sendInitialCredits_()
    {
        out_scheduler_credits_.send(scheduler_size_);
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Callbacks
    void Execute::getInstsFromDispatch_(const ExampleInstPtr & ex_inst)
    {
        // Insert at the end if we are doing in order issue or if the scheduler is empty
        if (in_order_issue_ == true || ready_queue_.size() == 0) {
            ready_queue_.emplace_back(ex_inst);
        }
        else {
            // Stick the instructions in a random position in the ready queue
            uint64_t issue_pos = std::rand() % ready_queue_.size();
             if (issue_pos == ready_queue_.size()-1) {
                 ready_queue_.emplace_back(ex_inst);
             }
             else {
                 uint64_t pos = 0;
                 auto iter = ready_queue_.begin();
                 while (iter != ready_queue_.end()) {
                     if (pos == issue_pos) {
                         ready_queue_.insert(iter, ex_inst);
                         break;
                     }
                     ++iter;
                     ++pos;
                 }
             }
        }
        // Schedule issue if the alu is not busy
        if (unit_busy_ == false) {
            issue_inst_.schedule(sparta::Clock::Cycle(0));
        }
    }

    void Execute::issueInst_() {
        // Issue a random instruction from the ready queue
        sparta_assert_context(unit_busy_ == false && ready_queue_.size() > 0,
                            "Somehow we're issuing on a busy unit or empty ready_queue");
        // Issue the first instruction
        ExampleInstPtr & ex_inst_ptr = ready_queue_.front();
        auto & ex_inst = *ex_inst_ptr;
        ex_inst.setStatus(ExampleInst::Status::SCHEDULED);
        const uint32_t exe_time =
            ignore_inst_execute_time_ ? execute_time_ : ex_inst.getExecuteTime();
        collected_inst_.collectWithDuration(ex_inst, exe_time);
        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "Executing: " << ex_inst << " for "
                         << exe_time + getClock()->currentCycle();
        }
        sparta_assert(exe_time != 0);

        ++total_insts_issued_;
        // Mark the instruction complete later...
        complete_inst_.preparePayload(ex_inst_ptr)->schedule(exe_time);
        // Mark the alu as busy
        unit_busy_ = true;
        // Pop the insturction from the scheduler and send a credit back to dispatch
        ready_queue_.pop_front();
        out_scheduler_credits_.send(1, 0);
    }

    // Called by the scheduler, scheduled by complete_inst_.
    void Execute::completeInst_(const ExampleInstPtr & ex_inst) {
        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "Completing inst: " << ex_inst;
        }

        ++total_insts_executed_;
        ex_inst->setStatus(ExampleInst::Status::COMPLETED);
        // We're not busy anymore
        unit_busy_ = false;
        // Schedule issue if we have instructions to issue
        if (ready_queue_.size() > 0) {
            issue_inst_.schedule(sparta::Clock::Cycle(0));
        }
    }

    void Execute::flushInst_(const FlushManager::FlushingCriteria & criteria)
    {
        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "Got flush for criteria: " << criteria;
        }

        // Flush instructions in the ready queue
        ReadyQueue::iterator it = ready_queue_.begin();
        uint32_t credits_to_send = 0;
        while(it != ready_queue_.end()) {
            if((*it)->getUniqueID() >= uint64_t(criteria)) {
                ready_queue_.erase(it++);
                ++credits_to_send;
            }
            else {
                ++it;
            }
        }
        if(credits_to_send) {
            out_scheduler_credits_.send(credits_to_send, 0);
        }

        // Cancel outstanding instructions awaiting completion and
        // instructions on their way to issue
        auto cancel_critera = [criteria](const ExampleInstPtr & inst) -> bool {
            if(inst->getUniqueID() >= uint64_t(criteria)) {
                return true;
            }
            return false;
        };
        complete_inst_.cancelIf(cancel_critera);
        issue_inst_.cancel();

        if(complete_inst_.getNumOutstandingEvents() == 0) {
            unit_busy_ = false;
            collected_inst_.closeRecord();
        }
    }

}
