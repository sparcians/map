

#pragma once

/*!
 * \file AddrDecoder.h
 * \brief Interface to the address decoder
 */

#include <cinttypes>

namespace sparta
{

namespace cache
{

class AddrDecoderIF
{
public:
    virtual ~AddrDecoderIF() {}

    /**
     * \brief Calculate the cache item's tag given the address
     * \param addr The address
     */
    virtual uint64_t calcTag(uint64_t addr) const = 0;

    /**
     * \brief Calculate the set index of the cache item given its address
     * \param addr The address
     */
    virtual uint32_t calcIdx(uint64_t addr) const = 0;

    /**
     * \brief Calculate the block address given the full address
     * \param addr The address
     */
    virtual uint64_t calcBlockAddr(uint64_t addr) const = 0;

    /**
     * \brief Calculate the line offset from the given address
     * \param addr The address
     */
    virtual uint64_t calcBlockOffset(uint64_t addr) const = 0;
}; // class AddrDecoderIF

} // namespace cache

} // namespace sparta
