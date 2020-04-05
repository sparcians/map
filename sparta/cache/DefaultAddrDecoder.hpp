
#pragma once
#include "cassert"
#include "sparta/utils/MathUtils.hpp"
#include "cache/AddrDecoderIF.hpp"

namespace sparta
{

    namespace cache
    {
        /* DefaultAddrDecoder: decodes a 64-bit address.
         *
         * Assuming line_size==stride, the address is decoded as below
         *    +--------------------------+------+------+
         *    |tag                       |idx   |offset|
         *    *--------------------------+------+------+
         */
        class DefaultAddrDecoder: public AddrDecoderIF
        {
        public:
            DefaultAddrDecoder(uint64_t sz,                       // cache size, in KB or bytes
                               uint64_t line_sz,                  // line size, in bytes
                               uint64_t stride,                   // stride, in bytes
                               uint32_t num_ways,                 // num ways
                               bool cache_sz_unit_is_kb = true)   // choose between KB or B
            {
                assert( utils::is_power_of_2(line_sz) );
                assert( utils::is_power_of_2(stride) );

                line_size_ = line_sz;

                const uint64_t sz_bytes = (cache_sz_unit_is_kb) ? (sz*1024) : (sz);
                const uint32_t num_sets = (sz_bytes)/ (line_sz * num_ways);

                blk_offset_mask_ = line_sz - 1;
                blk_addr_mask_   = ~uint64_t(blk_offset_mask_);
                index_mask_      = num_sets - 1;
                index_shift_     = utils::floor_log2(stride);
                tag_shift_       = utils::floor_log2(num_sets * stride);

            }
            virtual uint64_t calcTag(uint64_t addr) const { return (addr >> tag_shift_); }
            virtual uint32_t calcIdx(uint64_t addr) const { return (addr >> index_shift_) & index_mask_; }
            virtual uint64_t calcBlockAddr(uint64_t addr) const { return (addr & blk_addr_mask_); }
            virtual uint64_t calcBlockOffset(uint64_t addr) const { return (addr & blk_offset_mask_); }

            uint64_t getIndexMask() { return index_mask_;}
            uint64_t getIndexShift() { return index_shift_;}
            uint64_t getBlockOffsetMask() {return blk_offset_mask_;}

        private:
            uint64_t line_size_;
            uint64_t blk_addr_mask_;    //
            uint64_t blk_offset_mask_;  //
            uint32_t index_shift_;      // Amount to shift right for index
            uint32_t index_mask_;       // Mask to apply after index shift
            uint32_t tag_shift_;        // Amount to shift right for tag
        }; // class AddrDecoderIF

    }; // namespace cache

}; // namespace sparta

