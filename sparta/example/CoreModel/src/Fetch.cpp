// <Fetch.cpp> -*- C++ -*-

//!
//! \file Fetch.cpp
//! \brief Implementation of the CoreModel Fetch unit
//!

#include <algorithm>
#include "Fetch.hpp"

#include "sparta/events/StartupEvent.hpp"

namespace core_example
{
    const char * Fetch::name = "fetch";

    // Dummy opcodes, but based on a really small piece of PowerPC...
    static std::vector<ExampleInst::StaticInfo> dummy_opcodes =
    {
        { {0x7c01f214, 0xffffffff, {}, "add.", 0 }, ExampleInst::TargetUnit::ALU0, 1, false},
        { {0x7c6f0f10, 0xffffffff, {}, "cntlzw", 0 }, ExampleInst::TargetUnit::ALU1, 1, false},
        { {0x7c800000, 0xffffffff, {}, "add", 0 }, ExampleInst::TargetUnit::ALU0, 1, false},
        { {0x7c700000, 0xffffffff, {}, "subf.", 0 }, ExampleInst::TargetUnit::ALU1, 1, false},
        { {0x7c000000, 0xffffffff, {}, "and", 0 }, ExampleInst::TargetUnit::ALU0, 1, false},
        { {0x7c000000, 0xffffffff, {}, "and", 0 }, ExampleInst::TargetUnit::ALU0, 1, false},
        { {0x7c000710, 0xffffffff, {}, "fadd.", 0 }, ExampleInst::TargetUnit::FPU, 1, false},
        { {0x7c700000, 0xffffffff, {}, "cmp", 0 }, ExampleInst::TargetUnit::ALU1, 1, false},
        { {0x7c700010, 0xffffffff, {}, "cmn", 0 }, ExampleInst::TargetUnit::ALU1, 1, false},
        { {0x7c500000, 0xffffffff, {}, "cmp", 0 }, ExampleInst::TargetUnit::ALU1, 1, false},
        { {0x7c500000, 0xffffffff, {}, "cmp", 0 }, ExampleInst::TargetUnit::ALU1, 1, false},
        { {0x7c400010, 0xffffffff, {}, "sub", 0 }, ExampleInst::TargetUnit::ALU1, 1, false},
        { {0xfc800500, 0xffffffff, {}, "fabs",0 }, ExampleInst::TargetUnit::FPU, 5, false},
        { {0xfc000700, 0xffffffff, {}, "fctid.", 0 }, ExampleInst::TargetUnit::FPU, 5, false},
        { {0xfc200d00, 0xffffffff, {}, "fadd.", 0 }, ExampleInst::TargetUnit::FPU, 5, false},
        { {0xfc800700, 0xffffffff, {}, "fadd.",0 }, ExampleInst::TargetUnit::FPU, 5, false},
        { {0xfcb10300, 0xffffffff, {}, "fadd.", 0 }, ExampleInst::TargetUnit::FPU, 5, false},
        { {0xfcb00ac0, 0xffffffff, {}, "fadd.", 0 }, ExampleInst::TargetUnit::FPU, 5, false},
        { {0xfc000800, 0xffffffff, {}, "fadd.", 0 }, ExampleInst::TargetUnit::FPU, 5, false},
        { {0xfc000d00, 0xffffffff, {}, "fadd.", 0 }, ExampleInst::TargetUnit::FPU, 5, false},
        { {0xfc300a00, 0xffffffff, {}, "fadd.", 0 }, ExampleInst::TargetUnit::FPU, 5, false},
        { {0xfc800400, 0xffffffff, {}, "fadd.", 0 }, ExampleInst::TargetUnit::FPU, 10, false},
        { {0xfc800000, 0xffffffff, {}, "fadd.",  0 }, ExampleInst::TargetUnit::FPU, 1, false},
        { {0xfc800100, 0xffffffff, {}, "fadd.",  0 }, ExampleInst::TargetUnit::FPU, 1, false},
        { {0xfc000110, 0xffffffff, {}, "fdiv",   0 }, ExampleInst::TargetUnit::FPU, 20, false},
        { {0xfc800030, 0xffffffff, {}, "fdiv.",   0 }, ExampleInst::TargetUnit::FPU, 30, false},
        { {0xfc100000, 0xffffffff, {}, "sync",  0 }, ExampleInst::TargetUnit::ROB, 1, false},
        { {0x7ea00010, 0xffffffff, {}, "lwx", 0 }, ExampleInst::TargetUnit::LSU, 10, false},
        { {0xfca00030, 0xffffffff, {}, "stw", 0 }, ExampleInst::TargetUnit::LSU, 10, true}
    };

    // Fetch a random instruction or MaxIPC
    template<bool MaxIPC>
    void Fetch::fetchInstruction_()
    {
        const uint32_t upper = std::min(credits_inst_queue_, num_insts_to_fetch_);

        // Nothing to send.  Don't need to schedule this again.
        if(upper == 0) { return; }

        InstGroup insts_to_send;
        for(uint32_t i = 0; i < upper; ++i) {
            ExampleInstPtr ex_inst;
            if(MaxIPC) {
                ex_inst =
                    sparta::allocate_sparta_shared_pointer<ExampleInst>(example_inst_allocator,
                                                                        dummy_opcodes[i], getClock());
                // This can be done instead, but you will lose about
                // ~20% performance in an experiment running 5M
                // instructions
                //ex_inst.reset(new ExampleInst(dummy_opcodes[i], getClock()));
            }
            else {
                ex_inst =
                    sparta::allocate_sparta_shared_pointer<ExampleInst>(example_inst_allocator,
                                                                        dummy_opcodes[rand() % dummy_opcodes.size()], getClock());
            }
            ex_inst->setUniqueID(++next_inst_id_);
            ex_inst->setVAdr(vaddr_);
            ex_inst->setSpeculative(speculative_path_);
            insts_to_send.emplace_back(ex_inst);

            if(SPARTA_EXPECT_FALSE(info_logger_)) {
                info_logger_ << "RANDOM: Sending: " << ex_inst << " down the pipe";
            }
            speculative_path_ = (ex_inst->getUnit() == ExampleInst::TargetUnit::ROB);

            vaddr_ += 4;
        }


        out_fetch_queue_write_.send(insts_to_send);

        credits_inst_queue_ -= upper;
        if(credits_inst_queue_ > 0) {
            fetch_inst_event_->schedule(1);
        }

        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "Fetch: send num_inst=" << insts_to_send.size()
                         << " instructions, remaining credit=" << credits_inst_queue_;
        }
    }

    Fetch::Fetch(sparta::TreeNode * node,
                 const FetchParameterSet * p) :
        sparta::Unit(node),
        num_insts_to_fetch_(p->num_to_fetch)
    {
        in_fetch_queue_credits_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Fetch, receiveFetchQueueCredits_, uint32_t));

        if (p->fetch_max_ipc == true) {
            fetch_inst_event_.reset(new sparta::SingleCycleUniqueEvent<>(&unit_event_set_, "fetch_max_ipc",
                                                                         CREATE_SPARTA_HANDLER(Fetch, fetchInstruction_<true>)));
            // Schedule a single event to start reading -- not needed since receiveFetchQueueCredits launches fetch_instruction_event.
            //sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(Fetch, fetchInstruction_<true>));
        }
        else {
            fetch_inst_event_.reset(new sparta::SingleCycleUniqueEvent<>(&unit_event_set_, "fetch_random",
                                                                         CREATE_SPARTA_HANDLER(Fetch, fetchInstruction_<false>)));
            // Schedule a single event to start reading from a trace file -- not needed since receiveFetchQueueCredits launches fetch_instruction_event.
            //sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(Fetch, fetchInstruction_<false>));
        }

        in_fetch_flush_redirect_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Fetch, flushFetch_, uint64_t));

        srand(p->inst_rand_seed);
    }

    // Called when decode has room
    void Fetch::receiveFetchQueueCredits_(const uint32_t & dat) {
        credits_inst_queue_ += dat;

        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "Fetch: receive num_decode_credits=" << dat
                         << ", total decode_credits=" << credits_inst_queue_;
        }

        // Schedule a fetch event this cycle
        fetch_inst_event_->schedule(sparta::Clock::Cycle(0));
    }

    // Called from Retire via in_fetch_flush_redirect_ port
    void Fetch::flushFetch_(const uint64_t & new_addr) {
        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << "Fetch: receive flush on new_addr=0x"
                         << std::hex << new_addr << std::dec;
        }

        // New address to fetch from
        vaddr_ = new_addr;

        // Cancel all previously sent instructions on the outport
        out_fetch_queue_write_.cancel();

        // No longer speculative
        speculative_path_ = false;
    }

}
