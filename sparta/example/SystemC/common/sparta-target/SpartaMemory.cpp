
#include "SpartaMemory.hpp"

namespace sparta_target
{
    void SpartaMemory::receiveMemoryRequest_(const MemoryRequest & request)
    {
        uint8_t *data = request.data;
        sparta::memory::addr_t address = request.addr;
        uint32_t length = request.size;
        sparta::Clock::Cycle delay = 0;
        switch(request.cmd)
        {
            case MemoryRequest::Command::READ:
                for (unsigned int i = 0; i < length; i++)
                {
                    data[i] = m_memory[address++];         // move the data to memory
                }
                delay = read_response_delay_;
                break;
            case MemoryRequest::Command::WRITE:
                for (unsigned int i = 0; i < length; i++)
                {
                    m_memory[address++] = data[i];     // move the data to memory
                }
                delay = write_response_delay_;
                break;
            case MemoryRequest::Command::UNKNOWN:
                sparta_assert(false, "Received a bad command");
                break;
        }
        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << request << " delay: " << delay;
        }
        ev_drive_response_.preparePayload(request)->schedule(delay);
    }

    void SpartaMemory::driveMemoryResponse_(const MemoryRequest & req)
    {
        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << req << " responding";
        }
        out_memory_response_.send(req);
    }

}
