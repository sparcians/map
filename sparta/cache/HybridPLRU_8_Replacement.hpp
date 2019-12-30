
#ifndef _HYBRID_PLRU_8_REPLACEMENT_H_
#define _HYBRID_PLRU_8_REPLACEMENT_H_

#include <bitset>
#include "cache/ReplacementIF.hpp"
#include "cache/TrueLRU_4_Replacement.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace sparta
{

    namespace cache
    {

        /* This class implements an 8-way PLRU algorithm using a 4-way
         * true LRU coupled with 4 more bits.
         *
         * In this implementation, we designate way0 to be left most way
         *                                                _
         *                              O                  |
         *                             / \                 |
         *                            /   \                > true LRU portion
         *                          /       \              |
         *                         0          0           _|
         *                        / \        / \
         *                       /   \      /   \
         *                      w0    w1   w2    w3  <--- top 4 ways are true LRU
         *                      b0    b1   b2    b3  <--- 4 expansion bits to expand to 8 ways
         *                     / \   / \  / \   / \       Tell which way below is LRU (1 means the one to the right)
         *                    w0 w1 w2 w3 w4 w5 w6 w7
         *
         *  To find which way is LRU
         *  1.  Ask the true LRU portion which of its way is LRU
         *  2.  lru_way = (top4_lru_way * 2) + expansion_bit[top4_lru_way]
         * */
        class HybridPLRU_8_Replacement : public ReplacementIF
        {
        public:
            HybridPLRU_8_Replacement() :
                ReplacementIF(NUM_WAYS)
            {
            	HybridPLRU_8_Replacement::reset();
            }

            void reset()
            {
                expansion_lru_bits_ = 0;
                top4_rep_.reset();
           }

            uint32_t getMRUWay() const
            {
                uint32_t top4_mru_way = top4_rep_.getMRUWay();
                sparta_assert(top4_mru_way < 4);
                // If expansion_bit is 1, way1 (of the two way set) is LRU & way0 is MRU,
                uint32_t mru_way = (top4_mru_way << 1) + (!expansion_lru_bits_[top4_mru_way]);
                return mru_way;
            }

            uint32_t getLRUWay() const
            {
                uint32_t top4_lru_way = top4_rep_.getLRUWay();
                sparta_assert(top4_lru_way < 4);
                uint32_t lru_way = (top4_lru_way << 1) + expansion_lru_bits_[top4_lru_way];
                return lru_way;
            }

            void touchMRU(uint32_t way)
            {
                sparta_assert(way < NUM_WAYS);
                uint32_t top4_mru_way         = way >> 1;
                bool     expansion_bit        = way & 0x1;
                top4_rep_.touchMRU(top4_mru_way);
                expansion_lru_bits_.set(top4_mru_way, !expansion_bit);
            }

            void touchLRU(uint32_t way)
            {
                sparta_assert(way < NUM_WAYS);
                uint32_t top4_lru_way         = way >> 1;
                bool     expansion_bit        = way & 0x1;
                top4_rep_.touchLRU(top4_lru_way);
                expansion_lru_bits_.set(top4_lru_way, expansion_bit);
            }

            void lockWay(uint32_t way)
            {
                sparta_assert(way<num_ways_);
                sparta_assert(0);
            }

            ReplacementIF *clone() const { return new HybridPLRU_8_Replacement(); }
        private:
            static const uint32_t NUM_WAYS = 8;
            TrueLRU_4_Replacement top4_rep_;
            std::bitset<32> expansion_lru_bits_;


        }; // class HybridPLRU_8_Replacment

    }; // namespace cache

}; // namespace sparta
#endif  // _HYBRID_PLRU_8_REPLACEMENT_H_
