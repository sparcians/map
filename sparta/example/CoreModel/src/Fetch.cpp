// <Fetch.cpp> -*- C++ -*-


#include <algorithm>
#include "Fetch.hpp"

#include "sparta/events/StartupEvent.hpp"

namespace core_example
{
    const char * Fetch::name = "fetch";

    // Dummy opcodes, but based on a really small piece of ARM...
    static std::vector<ExampleInst::StaticInfo> dummy_opcodes =
    {
        { {0x02a00000, 0x0fe00000, {}, "adc", 0 }, ExampleInst::TargetUnit::ALU0, 1, false},
        { {0x016f0f10, 0x0fff0ff0, {}, "clz", 0 }, ExampleInst::TargetUnit::ALU1, 1, false},
        { {0x02800000, 0x0fe00000, {}, "add", 0 }, ExampleInst::TargetUnit::ALU0, 1, false},
        { {0x03700000, 0x0ff0f000, {}, "cmn", 0 }, ExampleInst::TargetUnit::ALU1, 1, false},
        { {0x02000000, 0x0fe00000, {}, "and", 0 }, ExampleInst::TargetUnit::ALU0, 1, false},
        { {0x00000000, 0x0fe00010, {}, "and", 0 }, ExampleInst::TargetUnit::ALU0, 1, false},
        { {0xf2000710, 0xfe800f10, {}, "vaba", 0 }, ExampleInst::TargetUnit::FPU, 1, false},
        { {0x01700000, 0x0ff0f010, {}, "cmn", 0 }, ExampleInst::TargetUnit::ALU1, 1, false},
        { {0x01700010, 0x0ff0f090, {}, "cmn", 0 }, ExampleInst::TargetUnit::ALU1, 1, false},
        { {0x03500000, 0x0ff0f000, {}, "cmp", 0 }, ExampleInst::TargetUnit::ALU1, 1, false},
        { {0x01500000, 0x0ff0f010, {}, "cmp", 0 }, ExampleInst::TargetUnit::ALU1, 1, false},
        { {0x00400010, 0x0fe00090, {}, "sub", 0 }, ExampleInst::TargetUnit::ALU1, 1, false},
        { {0xf2800500, 0xfe800f50, {}, "vabal",0 }, ExampleInst::TargetUnit::FPU, 5, false},
        { {0xf2000700, 0xfe800f10, {}, "vabd", 0 }, ExampleInst::TargetUnit::FPU, 5, false},
        { {0xf3200d00, 0xffa00f10, {}, "vabd", 0 }, ExampleInst::TargetUnit::FPU, 5, false},
        { {0xf2800700, 0xfe800f50, {}, "vabdl",0 }, ExampleInst::TargetUnit::FPU, 5, false},
        { {0xf3b10300, 0xffb30b90, {}, "vabs", 0 }, ExampleInst::TargetUnit::FPU, 5, false},
        { {0x0eb00ac0, 0x0fbf0ed0, {}, "vabs", 0 }, ExampleInst::TargetUnit::FPU, 5, false},
        { {0xf2000800, 0xff800f10, {}, "vadd", 0 }, ExampleInst::TargetUnit::FPU, 5, false},
        { {0xf2000d00, 0xffa00f10, {}, "vadd", 0 }, ExampleInst::TargetUnit::FPU, 5, false},
        { {0x0e300a00, 0x0fb00e50, {}, "vadd", 0 }, ExampleInst::TargetUnit::FPU, 5, false},
        { {0xf2800400, 0xff800f50, {}, "vaddhn", 0 }, ExampleInst::TargetUnit::FPU, 10, false},
        { {0xf2800000, 0xfe800f50, {}, "vaddl",  0 }, ExampleInst::TargetUnit::FPU, 1, false},
        { {0xf2800100, 0xfe800f50, {}, "vaddw",  0 }, ExampleInst::TargetUnit::FPU, 1, false},
        { {0xf2000110, 0xffb00f10, {}, "vand",   0 }, ExampleInst::TargetUnit::FPU, 20, false},
        { {0xf2800030, 0xfeb800b0, {}, "vbic",   0 }, ExampleInst::TargetUnit::FPU, 30, false},
        { {0xf8100000, 0xfe500000, {}, "flush",  0 }, ExampleInst::TargetUnit::ROB, 1, false},
        { {0x01a00010, 0x0fe000f0, {}, "ldr", 0 }, ExampleInst::TargetUnit::LSU, 10, false},
        { {0x01a00030, 0x0fe000f0, {}, "str", 0 }, ExampleInst::TargetUnit::LSU, 10, true}
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
        num_insts_to_fetch_(p->num_to_fetch),
        next_pc_(node, "next_pc", &vaddr_)
    {
        in_fetch_queue_credits_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Fetch, receiveFetchQueueCredits_, uint32_t));

        if (p->fetch_max_ipc == true) {
            fetch_inst_event_.reset(new sparta::SingleCycleUniqueEvent<>(&unit_event_set_, "fetch_max_ipc",
                                                                         CREATE_SPARTA_HANDLER(Fetch, fetchInstruction_<true>)));
            // Schedule a single event to start reading
            sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(Fetch, fetchInstruction_<true>));
        }
        else {
            fetch_inst_event_.reset(new sparta::SingleCycleUniqueEvent<>(&unit_event_set_, "fetch_random",
                                                                         CREATE_SPARTA_HANDLER(Fetch, fetchInstruction_<false>)));
            // Schedule a single event to start reading from a trace file
            sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(Fetch, fetchInstruction_<false>));
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
