// <Dispatch.cpp> -*- C++ -*-



#include <algorithm>
#include "Dispatch.hpp"
#include "sparta/events/StartupEvent.hpp"

namespace core_example
{
    const char Dispatch::name[] = "dispatch";

    // Constructor
    Dispatch::Dispatch(sparta::TreeNode * node,
                       const DispatchParameterSet * p) :
        sparta::Unit(node),
        dispatch_queue_("dispatch_queue", p->dispatch_queue_depth,
                        node->getClock(), getStatisticSet()),
        num_to_dispatch_(p->num_to_dispatch)
    {
        weighted_unit_distribution_context_.assignContextWeights(p->context_weights);
        dispatch_queue_.enableCollection(node);

        // Start the no instructions counter
        stall_counters_[current_stall_].startCounting();

        // Register consuming events with the InPorts.
        in_dispatch_queue_write_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Dispatch, dispatchQueueAppended_, InstGroup));

        in_fpu_credits_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Dispatch, fpuCredits_, uint32_t));
        in_fpu_credits_.enableCollection(node);

        in_alu0_credits_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Dispatch, alu0Credits_, uint32_t));
        in_alu0_credits_.enableCollection(node);

        in_alu1_credits_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Dispatch, alu1Credits_, uint32_t));
        in_alu1_credits_.enableCollection(node);

        in_br_credits_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Dispatch, brCredits_, uint32_t));
        in_br_credits_.enableCollection(node);

        in_lsu_credits_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Dispatch, lsuCredits_, uint32_t));
        in_lsu_credits_.enableCollection(node);

        in_reorder_credits_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Dispatch, robCredits_, uint32_t));
        in_reorder_credits_.enableCollection(node);

        in_reorder_flush_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Dispatch, handleFlush_, FlushManager::FlushingCriteria));
        in_reorder_flush_.enableCollection(node);

        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(Dispatch, sendInitialCredits_));
    }

    void Dispatch::sendInitialCredits_()
    {
        out_dispatch_queue_credits_.send(dispatch_queue_.capacity());
    }

    void Dispatch::fpuCredits_ (const uint32_t& credits) {
        credits_fpu_ += credits;
        if (credits_rob_ >0 && dispatch_queue_.size() > 0) {
            ev_dispatch_insts_.schedule(sparta::Clock::Cycle(0));
        }
        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "FPU got " << credits << " credits, total: " << credits_fpu_;
        }
    }

    void Dispatch::alu0Credits_ (const uint32_t& credits) {
        credits_alu0_ += credits;
        if (credits_rob_ >0 && dispatch_queue_.size() > 0) {
            ev_dispatch_insts_.schedule(sparta::Clock::Cycle(0));
        }
        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "ALU0 got " << credits << " credits, total: " << credits_alu0_;
        }
    }

    void Dispatch::alu1Credits_ (const uint32_t& credits) {
        credits_alu1_ += credits;
        if (credits_rob_ >0 && dispatch_queue_.size() > 0) {
            ev_dispatch_insts_.schedule(sparta::Clock::Cycle(0));
        }
        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "ALU1 got " << credits << " credits, total: " << credits_alu1_;
        }
    }

    void Dispatch::brCredits_ (const uint32_t& credits) {
        credits_br_ += credits;
        if (credits_rob_ >0 && dispatch_queue_.size() > 0) {
            ev_dispatch_insts_.schedule(sparta::Clock::Cycle(0));
        }
        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "BR got " << credits << " credits, total: " << credits_br_;
        }
    }

    void Dispatch::lsuCredits_(const uint32_t& credits) {
        credits_lsu_ += credits;
        if (credits_rob_ >0 && dispatch_queue_.size() > 0) {
            ev_dispatch_insts_.schedule(sparta::Clock::Cycle(0));
        }
        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "LSU got " << credits << " credits, total: " << credits_lsu_;
        }
    }

    void Dispatch::robCredits_(const uint32_t&) {
        uint32_t nc = in_reorder_credits_.pullData();
        credits_rob_ += nc;
        if (((credits_fpu_ > 0)|| (credits_alu0_ > 0) || (credits_alu1_ > 0) || (credits_br_ > 0))
            && dispatch_queue_.size() > 0) {
            ev_dispatch_insts_.schedule(sparta::Clock::Cycle(0));
        }
        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "ROB got " << nc << " credits, total: " << credits_rob_;
        }
    }

    void Dispatch::dispatchQueueAppended_(const InstGroup &) {
        for(auto & i : in_dispatch_queue_write_.pullData()) {
            dispatch_queue_.push(i);
        }

        if (((credits_fpu_ > 0)|| (credits_alu0_ > 0) || (credits_alu1_ > 0) || (credits_br_ > 0) || credits_lsu_ > 0)
            && credits_rob_ >0) {
            ev_dispatch_insts_.schedule(sparta::Clock::Cycle(0));
        }
    }

    void Dispatch::handleFlush_(const FlushManager::FlushingCriteria & criteria)
    {
        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "Got a flush call for " << criteria;
        }
        out_dispatch_queue_credits_.send(dispatch_queue_.size());
        dispatch_queue_.clear();
        credits_fpu_  += out_fpu_write_.cancel();
        credits_alu0_ += out_alu0_write_.cancel();
        credits_alu1_ += out_alu1_write_.cancel();
        credits_br_   += out_br_write_.cancel();
        credits_lsu_  += out_lsu_write_.cancel();
        out_reorder_write_.cancel();
    }

    void Dispatch::dispatchInstructions_()
    {
        uint32_t num_dispatch = std::min(dispatch_queue_.size(), num_to_dispatch_);
        num_dispatch = std::min(credits_rob_, num_dispatch);

        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "Num to dispatch: " << num_dispatch;
        }

        // Stop the current counter
        stall_counters_[current_stall_].stopCounting();

        if(num_dispatch == 0) {
            stall_counters_[current_stall_].startCounting();
            return;
        }

        current_stall_ = NOT_STALLED;

        InstGroup insts_dispatched;
        bool keep_dispatching = true;
        for(uint32_t i = 0; (i < num_dispatch) && keep_dispatching; ++i)
        {
            bool dispatched = false;
            ExampleInstPtr & ex_inst_ptr = dispatch_queue_.access(0);
            ExampleInst & ex_inst = *ex_inst_ptr;

            switch(ex_inst.getUnit())
            {
            case ExampleInst::TargetUnit::FPU:
                {
                    if(credits_fpu_ > 0) {
                        --credits_fpu_;
                        dispatched = true;
                        out_fpu_write_.send(ex_inst_ptr);
                        ++unit_distribution_[static_cast<uint32_t>(ExampleInst::TargetUnit::FPU)];
                        ++(unit_distribution_context_.context(static_cast<uint32_t>(ExampleInst::TargetUnit::FPU)));
                        ++(weighted_unit_distribution_context_.context(static_cast<uint32_t>(ExampleInst::TargetUnit::FPU)));

                        if(SPARTA_EXPECT_FALSE(info_logger_)) {
                            info_logger_ << "Sending instruction: "
                                         << ex_inst_ptr << " to FPU ";
                        }
                    }
                    else {
                        current_stall_ = FPU_BUSY;
                        keep_dispatching = false;
                    }
                }
                break;
            case ExampleInst::TargetUnit::ALU0:
                {
                    if(credits_alu0_ > 0) {
                        --credits_alu0_;
                        dispatched = true;
                        //out_alu0_write_.send(ex_inst_ptr);  // <- This will cause an assert in the Port!
                        out_alu0_write_.send(ex_inst_ptr, 1);
                        ++unit_distribution_[static_cast<uint32_t>(ExampleInst::TargetUnit::ALU0)];
                        ++(unit_distribution_context_.context(static_cast<uint32_t>(ExampleInst::TargetUnit::ALU0)));
                        ++(weighted_unit_distribution_context_.context(static_cast<uint32_t>(ExampleInst::TargetUnit::ALU0)));
                        ++(alu0_context_.context(0));

                        if(SPARTA_EXPECT_FALSE(info_logger_)) {
                            info_logger_ << "Sending instruction: "
                                         << ex_inst_ptr << " to ALU0 ";
                        }
                    }
                    else {
                        current_stall_ = ALU0_BUSY;
                        keep_dispatching = false;
                    }
                }
                break;
            case ExampleInst::TargetUnit::ALU1:
                {
                    if(credits_alu1_ > 0)
                    {
                        --credits_alu1_;
                        dispatched = true;
                        out_alu1_write_.send(ex_inst_ptr, 1);
                        ++unit_distribution_[static_cast<uint32_t>(ExampleInst::TargetUnit::ALU1)];
                        ++(unit_distribution_context_.context(static_cast<uint32_t>(ExampleInst::TargetUnit::ALU1)));
                        ++(weighted_unit_distribution_context_.context(static_cast<uint32_t>(ExampleInst::TargetUnit::ALU1)));

                        if(SPARTA_EXPECT_FALSE(info_logger_)) {
                            info_logger_ << "Sending instruction: "
                                         << ex_inst_ptr << " to ALU1 ";
                        }
                    }
                    else {
                        current_stall_ = ALU0_BUSY;
                        keep_dispatching = false;
                    }
                }
                break;
             case ExampleInst::TargetUnit::BR:
                {
                    if(credits_br_ > 0)
                    {
                        --credits_br_;
                        dispatched = true;
                        out_br_write_.send(ex_inst_ptr, 1);
                        ++unit_distribution_[static_cast<uint32_t>(ExampleInst::TargetUnit::BR)];
                        ++(unit_distribution_context_.context(static_cast<uint32_t>(ExampleInst::TargetUnit::BR)));
                        ++(weighted_unit_distribution_context_.context(static_cast<uint32_t>(ExampleInst::TargetUnit::BR)));

                        if(SPARTA_EXPECT_FALSE(info_logger_)) {
                            info_logger_ << "Sending instruction: "
                                         << ex_inst_ptr << " to BR ";
                        }
                    }
                    else {
                        current_stall_ = BR_BUSY;
                        keep_dispatching = false;
                    }
                }
                break;
             case ExampleInst::TargetUnit::LSU:
                {
                    if(credits_lsu_ > 0)
                    {
                        --credits_lsu_;
                        dispatched = true;
                        out_lsu_write_.send(ex_inst_ptr, 1);
                        ++unit_distribution_[static_cast<uint32_t>(ExampleInst::TargetUnit::LSU)];
                        ++(unit_distribution_context_.context(static_cast<uint32_t>(ExampleInst::TargetUnit::LSU)));
                        ++(weighted_unit_distribution_context_.context(static_cast<uint32_t>(ExampleInst::TargetUnit::LSU)));

                        if(SPARTA_EXPECT_FALSE(info_logger_)) {
                            info_logger_ << "sending instruction: "
                                         << ex_inst_ptr << " to LSU ";
                        }
                    }
                    else {
                        current_stall_ = LSU_BUSY;
                        keep_dispatching = false;
                    }
                }
                break;
             case ExampleInst::TargetUnit::ROB:
                {
                    ex_inst.setStatus(ExampleInst::Status::COMPLETED);
                    // Indicate that this instruction was dispatched
                    // -- it goes right to the ROB
                    dispatched = true;
                }
                break;
            default:
                sparta_assert(!"Should not have gotten here");
            }

            if(dispatched) {
                insts_dispatched.emplace_back(ex_inst_ptr);
                dispatch_queue_.pop();
                --credits_rob_;
            } else {
                if(SPARTA_EXPECT_FALSE(info_logger_)) {
                    info_logger_ << "Could not dispatch: "
                                   << ex_inst_ptr
                                 << " ALU0_B(" << std::boolalpha << (credits_alu0_ == 0)
                                 << ") ALU1_B(" << (credits_alu1_ == 0)
                                 << ") FPU_B(" <<  (credits_fpu_ == 0)
                                 << ") BR_B(" <<  (credits_br_ == 0) << ")";
                }
                break;
            }
        }

        if(!insts_dispatched.empty()) {
            out_dispatch_queue_credits_.send(insts_dispatched.size());
            out_reorder_write_.send(insts_dispatched);
        }

        if ((credits_rob_ > 0) && (dispatch_queue_.size() > 0) && (current_stall_ == NOT_STALLED)) {
            ev_dispatch_insts_.schedule(1);
        }

        stall_counters_[current_stall_].startCounting();
    }
}
