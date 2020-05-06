// <Decode.h> -*- C++ -*-



#pragma once

#include <string>
#include "sparta/ports/DataPort.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "CoreTypes.hpp"
#include "FlushManager.hpp"

namespace core_example
{

    /**
     * @file   Decode.h
     * @brief Decode instructions from Fetch and send them on
     *
     * Decode unit will
     * 1. Retrieve instructions from the fetch queue (retrieved via port)
     * 2. Push the instruction down the decode pipe (internal, of parameterized length)
     */
    class Decode : public sparta::Unit
    {
    public:
        //! \brief Parameters for Decode model
        class DecodeParameterSet : public sparta::ParameterSet
        {
        public:
            DecodeParameterSet(sparta::TreeNode* n) :
                sparta::ParameterSet(n)
            { }

            PARAMETER(uint32_t, num_to_decode,     4, "Number of instructions to process")
            PARAMETER(uint32_t, fetch_queue_size, 10, "Size of the fetch queue")
        };

        /**
         * @brief Constructor for Decode
         *
         * @param node The node that represents (has a pointer to) the Decode
         * @param p The Decode's parameter set
         */
        Decode(sparta::TreeNode * node,
               const DecodeParameterSet * p);

        //! \brief Name of this resource. Required by sparta::UnitFactory
        static constexpr char name[] = "decode";

    private:

        // The internal instruction queue
        InstQueue fetch_queue_;

        // Port listening to the fetch queue appends - Note the 1 cycle delay
        sparta::DataInPort<InstGroup> fetch_queue_write_in_     {&unit_port_set_, "in_fetch_queue_write", 1};
        sparta::DataOutPort<uint32_t> fetch_queue_credits_outp_ {&unit_port_set_, "out_fetch_queue_credits"};

        // Port to the uop queue in dispatch (output and credits)
        sparta::DataOutPort<InstGroup> uop_queue_outp_      {&unit_port_set_, "out_uop_queue_write"};
        sparta::DataInPort<uint32_t>   uop_queue_credits_in_{&unit_port_set_, "in_uop_queue_credits", sparta::SchedulingPhase::Tick, 0};

        // For flush
        sparta::DataInPort<FlushManager::FlushingCriteria> in_reorder_flush_
             {&unit_port_set_, "in_reorder_flush", sparta::SchedulingPhase::Flush, 1};

        // The decode instruction event
        sparta::UniqueEvent<> ev_decode_insts_event_  {&unit_event_set_, "decode_insts_event", CREATE_SPARTA_HANDLER(Decode, decodeInsts_)};

        //////////////////////////////////////////////////////////////////////
        // Decoder callbacks
        void sendInitialCredits_();
        void fetchBufferAppended_   (const InstGroup &);
        void receiveUopQueueCredits_(const uint32_t &);
        void decodeInsts_();
        void handleFlush_(const FlushManager::FlushingCriteria & criteria);

        uint32_t uop_queue_credits_ = 0;
        const uint32_t num_to_decode_;
    };
}
