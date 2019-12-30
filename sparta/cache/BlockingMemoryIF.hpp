#ifndef _BLOCKING_MEMORY_IF_H__
#define _BLOCKING_MEMORY_IF_H__

#include "cache/Cache.hpp"
#include "sparta/utils/ByteOrder.hpp"
namespace sparta
{

    // This class needs more work
    class BlockingMemoryIF
    {

    public:
        virtual bool read(uint64_t addr,
                          uint32_t size,
                          uint8_t *buf) = 0;
        virtual bool write(uint64_t addr,
                           uint32_t size,
                           const uint8_t *buf ) = 0;

        //! \brief protected destructor to prevent deletion via the base class
        virtual ~BlockingMemoryIF() {};

    }; // class SimpleCacheIF

}; // namespace sparta

#endif // _BLOCKING_MEMORY_IF_H__
