
#pragma once

#include <cinttypes>

#include "sparta/simulation/Resource.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/events/PayloadEvent.hpp"
#include "sparta/ports/DataPort.hpp"

#include "MemoryRequest.hpp"

namespace sparta_target
{
    class SpartaMemory : public sparta::Unit
    {
    public:
        static constexpr char name[] = "memory";

        class SpartaMemoryParameters : public sparta::ParameterSet
        {
        public:
            explicit SpartaMemoryParameters(sparta::TreeNode* n):
                sparta::ParameterSet(n)
            { }
            PARAMETER(uint32_t, memory_size         , 4, "Memory Size")
            PARAMETER(uint32_t, memory_width        , 4, "Memory width")
            PARAMETER(uint32_t, accept_delay        , 4, "Acceptance delay for new transactions")
            PARAMETER(uint32_t, read_response_delay , 4, "Read response delay")
            PARAMETER(uint32_t, write_response_delay, 4, "Write response delay")
        };

        SpartaMemory(sparta::TreeNode * container_node,
                     const SpartaMemoryParameters * params) :
            sparta::Unit(container_node),
            memory_size_         (params->memory_size         ),
            memory_width_        (params->memory_width        ),
            accept_delay_        (params->accept_delay        ),
            read_response_delay_ (params->read_response_delay ),
            write_response_delay_(params->write_response_delay)
        {
            /// Allocate and initalize an array for the target's memory
                m_memory = new uint8_t[size_t(memory_size_)]; 
            /// Clear memory
                memset(m_memory, 0,  size_t(memory_size_));
                sparta_assert(memory_width_ > 0);
                sparta_assert(memory_size_ % memory_width_ == 0);
                in_memory_request_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(SpartaMemory,
                                                                        receiveMemoryRequest_,
                                                                        MemoryRequest));
        }

        ~SpartaMemory() {
            delete m_memory;
        }
        void memoryOperation(MemoryRequest &);
    private:
        sparta::DataInPort<MemoryRequest>  in_memory_request_  {getPortSet(), "in_memory_request"};
        sparta::DataOutPort<MemoryRequest> out_memory_response_{getPortSet(), "out_memory_response"};

        void receiveMemoryRequest_(const MemoryRequest &);
        void driveMemoryResponse_(const MemoryRequest &);

        sparta::PayloadEvent<MemoryRequest> ev_drive_response_{getEventSet(), "ev_drive_response_",
                                                               CREATE_SPARTA_HANDLER_WITH_DATA(SpartaMemory,
                                                                                               driveMemoryResponse_,
                                                                                               MemoryRequest)};
        const uint32_t memory_size_;
        const uint32_t memory_width_;
        const uint32_t accept_delay_;
        const uint32_t read_response_delay_;
        const uint32_t write_response_delay_;
        uint8_t *m_memory;    ///< memory

    };
}
