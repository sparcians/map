#pragma once

#include <cinttypes>
#include <ostream>

#include "sparta/memory/AddressTypes.hpp"

namespace sparta_target
{
    struct MemoryRequest
    {
        enum class Command {
            READ, WRITE, UNKNOWN
        };

        Command                cmd = Command::UNKNOWN;
        sparta::memory::addr_t addr = 0;
        uint32_t               size = 0;
        uint8_t               *data = nullptr;
        void                  *meta_data = nullptr;
    };

    inline std::ostream & operator<<(std::ostream & os, const MemoryRequest &req) {
        auto flags = os.flags();
        os << (req.cmd == MemoryRequest::Command::READ ? "READ " : "WRITE ");
        os << std::hex << req.addr << " " << req.size;
        os.flags(flags);
        return os;
    }

}
