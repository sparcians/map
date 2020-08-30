
#include "SpartaMemory.hpp"

namespace sparta_target
{
    void SpartaMemory::receiveMemoryRequest_(const MemoryRequest & request)
    {
        sparta::Clock::Cycle delay = 0;
        switch(request.cmd)
        {
            case MemoryRequest::Command::READ:
                delay = read_response_delay_;
                break;
            case MemoryRequest::Command::WRITE:
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
