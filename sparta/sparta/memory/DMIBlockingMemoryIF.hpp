
#pragma once

#include "sparta/memory/DMI.hpp"
#include "sparta/memory/BlockingMemoryIF.hpp"

namespace sparta {
namespace memory {

/**
 * \class DMIBlockingMemoryIF
 * \brief Wrapps a DMI in a BlockingMemoryIF
 */
class DMIBlockingMemoryIF final : public BlockingMemoryIF
{
public:
    DMIBlockingMemoryIF(const DMI &dmi)
    : BlockingMemoryIF("DMI",
                       dmi.getSize(),
                       DebugMemoryIF::AccessWindow(
                           dmi.getAddr(), dmi.getAddr() + dmi.getSize()))
    , dmi_(dmi)
    {
    }

    addr_t getAddr() const
    {
        return dmi_.getAddr();
    }

    addr_t getSize() const
    {
        return dmi_.getSize();
    }

    bool inRange(const addr_t addr, const addr_t size = 0) const
    {
        return dmi_.inRange(addr, size);
    }

private:
    void *computeHostAddress_(const addr_t addr) const
    {
        return (uint8_t *)dmi_.getRawPtr() + (addr - dmi_.getAddr());
    }

    bool tryRead_(addr_t addr,
                  addr_t size,
                  uint8_t *buf,
                  const void *,
                  void *) noexcept override
    {
        memcpy(buf, computeHostAddress_(addr), size);
        return true;
    }

    bool tryWrite_(addr_t addr,
                   addr_t size,
                   const uint8_t *buf,
                   const void *,
                   void *) noexcept override
    {
        memcpy(computeHostAddress_(addr), buf, size);
        return true;
    }

    bool tryPeek_(addr_t addr, addr_t size, uint8_t *buf) const noexcept override
    {
        memcpy(buf, computeHostAddress_(addr), size);
        return true;
    }

    bool tryPoke_(addr_t addr, addr_t size, const uint8_t *buf) noexcept override
    {
        memcpy(computeHostAddress_(addr), buf, size);
        return true;
    }

    /** The wrapped DMI */
    DMI dmi_;
};

} // namespace memory
} // namespace sparta
