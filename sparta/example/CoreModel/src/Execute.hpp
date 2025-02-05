// <Execute.h> -*- C++ -*-


/**
 * @file   Execute.h
 * @brief
 *
 *
 */

#pragma once

#include "sparta/ports/PortSet.hpp"
#include "sparta/ports/SignalPort.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/ports/Port.hpp"
#include "sparta/events/StartupEvent.hpp"
#include "sparta/collection/CollectableTreeNode.hpp"

#include "CoreTypes.hpp"
#include "FlushManager.hpp"

namespace core_example
{
    /**
     * @class Execute
     * @brief
     */
    class Execute : public sparta::Unit
    {

    public:
        //! \brief Parameters for Execute model
        class ExecuteParameterSet : public sparta::ParameterSet
        {
        public:
            ExecuteParameterSet(sparta::TreeNode* n) :
                sparta::ParameterSet(n)
            { }
            PARAMETER(bool, ignore_inst_execute_time, false,
                      "Ignore the instruction's execute time, "
                      "use execute_time param instead")
            PARAMETER(uint32_t, execute_time, 1, "Time for execution")
            PARAMETER(uint32_t, scheduler_size, 8, "Scheduler queue size")
            PARAMETER(bool, in_order_issue, true, "Force in order issue")
        };

        /**
         * @brief Constructor for Execute
         *
         * @param node The node that represents (has a pointer to) the Execute
         * @param p The Execute's parameter set
         */
        Execute(sparta::TreeNode * node,
            const ExecuteParameterSet * p);

        //! \brief Name of this resource. Required by sparta::UnitFactory
        static const char name[];

    private:
        // Ports and the set -- remove the ", 1" to experience a DAG issue!
        sparta::DataInPort<InstQueue::value_type> in_execute_inst_ {
            &unit_port_set_, "in_execute_write", 1};
        sparta::DataOutPort<uint32_t> out_scheduler_credits_{&unit_port_set_, "out_scheduler_credits"};
        sparta::DataInPort<FlushManager::FlushingCriteria> in_reorder_flush_
            {&unit_port_set_, "in_reorder_flush", sparta::SchedulingPhase::Flush, 1};

        // Ready queue
        typedef std::list<ExampleInstPtr> ReadyQueue;
        ReadyQueue  ready_queue_;

        // busy signal for the attached alu
        bool unit_busy_ = false;
        // Execution unit's execution time
        const bool     ignore_inst_execute_time_ = false;
        const uint32_t execute_time_;
        const uint32_t scheduler_size_;
        const bool in_order_issue_;

        using IterableCollectorType = sparta::collection::IterableCollector<std::list<ExampleInstPtr>>;

        // Collection
        IterableCollectorType ready_queue_collector_{
            getContainer(), "scheduler_queue", &ready_queue_, scheduler_size_};

        sparta::collection::ManualCollectable<ExampleInst> collected_inst_{
            getContainer(), getContainer()->getName()};

        // Events used to issue and complete the instruction
        sparta::UniqueEvent<> issue_inst_{&unit_event_set_, getName() + "_issue_inst",
                CREATE_SPARTA_HANDLER(Execute, issueInst_)};
        sparta::PayloadEvent<ExampleInstPtr> complete_inst_{
            &unit_event_set_, getName() + "_complete_inst",
                CREATE_SPARTA_HANDLER_WITH_DATA(Execute, completeInst_, ExampleInstPtr)};

        // Counter
        sparta::Counter total_insts_issued_{
            getStatisticSet(), "total_insts_issued",
            "Total instructions issued", sparta::Counter::COUNT_NORMAL
        };
        sparta::Counter total_insts_executed_{
            getStatisticSet(), "total_insts_executed",
            "Total instructions executed", sparta::Counter::COUNT_NORMAL
        };

        void sendInitialCredits_();
        ////////////////////////////////////////////////////////////////////////////////
        // Callbacks
        void issueInst_();
        void getInstsFromDispatch_(const ExampleInstPtr&);

        // Used to complete the inst in the FPU
        void completeInst_(const ExampleInstPtr&);

        // Used to flush the ALU
        void flushInst_(const FlushManager::FlushingCriteria & criteria);
    };
} // namespace core_example

