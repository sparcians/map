
#ifndef _HASH_ADDR_DECODER_IF_H__
#define _HASH_ADDR_DECODER_IF_H__
#include "cassert"
#include "sparta/utils/MathUtils.hpp"
#include "cache/DefaultAddrDecoder.hpp"

namespace sparta
{

    namespace cache
    {
        /* L3hashAddrDecoder: decodes a 64-bit address.
         *
         * Assuming line_size==stride, the address is decoded as below
         *    +--------------------------+------+------+
         *    |tag                       |idx   |offset|
         *    *--------------------------+------+------+
         */
        class HashAddrDecoder: public AddrDecoderIF
        {
        public:
            HashAddrDecoder(uint64_t sz_kb,     // cache size, in KB
                               uint64_t line_sz,   // line size, in bytes
                               uint64_t stride,    // stride, in bytes
                              uint32_t num_ways,
                              std::vector<std::vector<uint32_t>> hash={{}})  // num ways
            {
                assert( utils::is_power_of_2(line_sz) );
                assert( utils::is_power_of_2(stride) );

                line_size_ = line_sz;

                uint32_t num_sets = (sz_kb * 1024)/ (line_sz * num_ways);
                assert( utils::is_power_of_2(num_sets) );

                blk_offset_mask_ = line_sz - 1;
                blk_addr_mask_   = ~uint64_t(blk_offset_mask_);
                index_mask_      = num_sets - 1;
                index_shift_     = utils::floor_log2(stride);
                tag_shift_       = utils::floor_log2(num_sets * stride);
                index_hash_ = hash;

            }
            virtual uint64_t calcTag(uint64_t addr) const { return (addr >> tag_shift_); }
#if 0
            virtual uint32_t calcIdx(uint64_t addr) const {
                uint32_t index = (addr >> index_shift_) & index_mask_;
                uint32_t hash = (addr & (0x1 << 7)) >> 7;
                hash ^= (addr & (0x1 << 12)) >> 12;
                hash ^= (addr & (0x1 << 16)) >> 16;
                hash ^= (addr & (0x1 << 18)) >> 18;
                hash ^= (addr & (0x1 << 21)) >> 21;
                index &= ~0x3;
                index |= hash;

                hash = (addr & (0x1 << 6)) >> 6;
                //                hash ^= (addr & (0x1 << 7)) >> 7;
                hash ^= (addr & (0x1 << 8)) >> 8;
                hash ^= (addr & (0x1 << 10)) >> 10;
                hash ^= (addr & (0x1 << 13)) >> 13;
                hash ^= (addr & (0x1 << 14)) >> 14;
                hash ^= (addr & (0x1 << 17)) >> 17;
                hash ^= (addr & (0x1 << 18)) >> 18;
                hash ^= (addr & (0x1 << 22)) >> 22;
                hash ^= (addr & (0x1 << 23)) >> 23;
                hash <<= 1;
                index |= hash;
                return index;
            }
#else
            virtual uint32_t calcIdx(uint64_t addr) const {
                uint32_t hash_index = 0;
                uint32_t index = (addr >> index_shift_) & index_mask_;
                uint32_t pass=0;
                for(auto &slice : index_hash_) {
                    uint32_t hash_bit;
                    uint32_t hash = 0;
                    for(uint32_t iter=0; iter<slice.size();iter++) {
                        hash_bit = (addr  >> slice[iter]) & 0x1;
                        hash ^= hash_bit;
                    }
                    hash_index |= (hash << pass);
                    pass++;
                }
                index &= ~((0x1 << pass) -1);
                index |= hash_index;
                //                std::cout << "\n Addr " << sparta::utils::uint64_to_hexstr(addr) << " hash " << hash_index << " index " << sparta::utils::uint64_to_hexstr(index);
                return index;
            }
#endif
            virtual uint64_t calcBlockAddr(uint64_t addr) const { return (addr & blk_addr_mask_); }
            virtual uint64_t calcBlockOffset(uint64_t addr) const { return (addr & blk_offset_mask_); }
        private:
            uint64_t line_size_;
            uint64_t blk_addr_mask_;    //
            uint64_t blk_offset_mask_;  //
            uint32_t index_shift_;      // Amount to shift right for index
            uint32_t index_mask_;       // Mask to apply after index shift
            uint32_t tag_shift_;        // Amount to shift right for tag
            std::vector<std::vector<uint32_t>> index_hash_;
        }; // class AddrDecoderIF

    }; // namespace cache

}; // namespace sparta

#endif //  _SPA_CACHE_DEFAULT_ADDR_DECODER_IF_H__
