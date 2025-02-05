// <Fetch.hpp> -*- C++ -*-

//!
//! \file Fetch.hpp
//! \brief Definition of the CoreModel Fetch unit
//!


#pragma once

#include <string>
#include "sparta/ports/DataPort.hpp"
#include "sparta/events/SingleCycleUniqueEvent.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/collection/CollectableTreeNode.hpp"

#include "CoreTypes.hpp"

namespace core_example
{
    /**
     * @file   Fetch.h
     * @brief The Fetch block -- gets new instructions to send down the pipe
     *
     * This fetch unit is pretty simple and does not support
     * redirection.  But, if it did, a port between the ROB and Fetch
     * (or Branch and Fetch -- if we had a Branch unit) would be
     * required to release fetch from holding out on branch
     * resolution.
     */
    class Fetch : public sparta::Unit
    {
    public:
        //! \brief Parameters for Fetch model
        class FetchParameterSet : public sparta::ParameterSet
        {
        public:
            FetchParameterSet(sparta::TreeNode* n) :
                sparta::ParameterSet(n)
            {
                auto non_zero_validator = [](uint32_t & val, const sparta::TreeNode*)->bool {
                    if(val > 0) {
                        return true;
                    }
                    return false;
                };
                num_to_fetch.addDependentValidationCallback(non_zero_validator,
                                                            "Num to fetch must be greater than 0");
            }

            PARAMETER(uint32_t, num_to_fetch, 4, "Number of instructions to fetch")
            PARAMETER(uint32_t, inst_rand_seed, 0xdeadbeef, "Seed for random instruction fetch")
            PARAMETER(bool, fetch_max_ipc, false, "Fetch tries to maximize IPC by distributing insts")
        };

        /**
         * @brief Constructor for Fetch
         *
         * @param node The node that represents (has a pointer to) the Fetch
         * @param p The Fetch's parameter set
         */
        Fetch(sparta::TreeNode * name,
              const FetchParameterSet * p);

        ~Fetch() {
            debug_logger_ << getContainer()->getLocation()
                          << ": "
                          << example_inst_allocator.getNumAllocated()
                          << " ExampleInst objects allocated/created"
                          << std::endl;
        }

        //! \brief Name of this resource. Required by sparta::UnitFactory
        static const char * name;

    private:

        // Internal DataOutPort to the decode unit's fetch queue
        sparta::DataOutPort<InstGroup> out_fetch_queue_write_ {&unit_port_set_, "out_fetch_queue_write"};

        // Internal DataInPort from decode's fetch queue for credits
        sparta::DataInPort<uint32_t> in_fetch_queue_credits_
            {&unit_port_set_, "in_fetch_queue_credits", sparta::SchedulingPhase::Tick, 0};

        // Incoming flush from Retire w/ redirect
        sparta::DataInPort<uint64_t> in_fetch_flush_redirect_
            {&unit_port_set_, "in_fetch_flush_redirect", sparta::SchedulingPhase::Flush, 1};

        // Number of instructions to fetch
        const uint32_t num_insts_to_fetch_;

        // Number of credits from decode that fetch has
        uint32_t credits_inst_queue_ = 0;

        // Current "PC"
        uint64_t vaddr_ = 0x1000;

        // Fetch instruction event, triggered when there are credits
        // from decode.  The callback set is either to fetch random
        // instructions or a perfect IPC set
        std::unique_ptr<sparta::SingleCycleUniqueEvent<>> fetch_inst_event_;

        // A pipeline collector
        sparta::collection::AutoCollectable<uint64_t> next_pc_{
            getContainer(), "next_pc", &vaddr_};

        ////////////////////////////////////////////////////////////////////////////////
        // Callbacks

        // Receive the number of free credits from decode
        void receiveFetchQueueCredits_(const uint32_t &);

        // Read data from a trace at random or at MaxIPC and send it
        // through
        template<bool MaxIPC>
        void fetchInstruction_();

        // Receive flush from retire
        void flushFetch_(const uint64_t & new_addr);

        // A unique instruction ID
        uint64_t next_inst_id_ = 0;

        // Are we fetching a speculative path?
        bool speculative_path_ = false;
    };

}
