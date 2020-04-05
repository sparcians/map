// <ROB.h> -*- C++ -*-


#pragma once
#include <string>

#include "sparta/ports/DataPort.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/log/MessageSource.hpp"

#include "CoreTypes.hpp"
#include "FlushManager.hpp"

namespace core_example
{

    /**
     * @file   ROB.h
     * @brief
     *
     * The Reorder buffer will
     * 1. retire and writeback completed instructions
     */
    class ROB : public sparta::Unit
    {
    public:
        //! \brief Parameters for ROB model
        class ROBParameterSet : public sparta::ParameterSet
        {
        public:
            ROBParameterSet(sparta::TreeNode* n) :
                sparta::ParameterSet(n)
            { }

            PARAMETER(uint32_t, num_to_retire,       4, "Number of instructions to retire")
            PARAMETER(uint32_t, retire_queue_depth, 30, "Depth of the retire queue")
            PARAMETER(uint32_t, num_insts_to_retire, 0,
                      "Number of instructions to retire after which simulation will be "
                      "terminated. 0 means simulation will run until end of testcase")

        };

        /**
         * @brief Constructor for ROB
         *
         * @param node The node that represents (has a pointer to) the ROB
         * @param p The ROB's parameter set
         *
         * In the constructor for the unit, it is expected that the user
         * register the TypedPorts that this unit will need to perform
         * work.
         */
        ROB(sparta::TreeNode * node,
            const ROBParameterSet * p);

        //! \brief Name of this resource. Required by sparta::UnitFactory
        static const char name[];

        /// Destroy!
        ~ROB();

    private:

        sparta::StatisticDef stat_ipc_;
        sparta::Counter      num_retired_;
        sparta::Counter      num_flushes_;
        sparta::Clock::Cycle last_retirement_ = 0;
        const sparta::Clock::Cycle retire_timeout_interval_ = 100000;

        const uint32_t num_to_retire_;
        const uint32_t num_insts_to_retire_; // parameter from ilimit

        InstQueue      reorder_buffer_;

        // Ports used by the ROB
        sparta::DataInPort<InstGroup> in_reorder_buffer_write_   {&unit_port_set_, "in_reorder_buffer_write", 1};
        sparta::DataOutPort<uint32_t> out_reorder_buffer_credits_{&unit_port_set_, "out_reorder_buffer_credits"};
        sparta::DataInPort<bool>      in_oldest_completed_       {&unit_port_set_, "in_reorder_oldest_completed"};
        sparta::DataOutPort<FlushManager::FlushingCriteria> out_retire_flush_ {&unit_port_set_, "out_retire_flush"};
        sparta::DataOutPort<uint64_t> out_fetch_flush_redirect_  {&unit_port_set_, "out_fetch_flush_redirect"};

        // UPDATE:
        sparta::DataOutPort<ExampleInstPtr> out_rob_retire_ack_ {&unit_port_set_, "out_rob_retire_ack"};

        // For flush
        sparta::DataInPort<FlushManager::FlushingCriteria> in_reorder_flush_
             {&unit_port_set_, "in_reorder_flush", sparta::SchedulingPhase::Flush, 1};

        // Events used by the ROB
        sparta::UniqueEvent<> ev_retire_ {&unit_event_set_, "retire_insts",
                CREATE_SPARTA_HANDLER(ROB, retireEvent_)};

        // A nice checker to make sure forward progress is being made
        // Note that in the ROB constructor, this event is set as non-continuing
        sparta::Event<> ev_ensure_forward_progress_{&unit_event_set_, "forward_progress_check",
                CREATE_SPARTA_HANDLER(ROB, checkForwardProgress_)};

        void sendInitialCredits_();
        void retireEvent_();
        void robAppended_(const InstGroup &);
        void retireInstructions_();
        void checkForwardProgress_();
        void handleFlush_(const FlushManager::FlushingCriteria & criteria);

    };
}

