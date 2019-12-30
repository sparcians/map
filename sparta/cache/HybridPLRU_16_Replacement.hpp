
#ifndef _HYBRID_PLRU_16_REPLACEMENT_H_
#define _HYBRID_PLRU_16_REPLACEMENT_H_

#include "cache/ReplacementIF.hpp"
#include "cache/HybridPLRU_8_Replacement.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace sparta
{

    namespace cache
    {

        /* This class implements an 16-way PLRU algorithm using a two 8-way
         * hybrid PLRUs and a bit at top to choose which PLRU8
         *
         *                            top
         *                            lru
         *                            bit
         *                ___________/   \________
         *               |                        |
         *              8-way                    8-way
         *              PLRU                 PLRU
         *      +--+--+--+--+--+--+--+     +--+--+--+--+--+--+--+
         *      |  |  |  |  |  |  |  |   |  |  |  |  |  |  |  |
         *      w0 w1 w2 w3 w4 w5 w6 w7  w0 w1 w2 w3 w4 w5 w6 w7
         *
         *      w0 w1 w2 w3 w4 w5 w6 w7  w8 w9 wa wb wc wd we wf   <----16 ways
         * */
        class HybridPLRU_16_Replacement : public ReplacementIF
        {
        public:
            HybridPLRU_16_Replacement() : ReplacementIF(NUM_WAYS)
            {
                HybridPLRU_16_Replacement::reset();
            }

            void reset()
            {
                top_lru_bit_ = false;
                lru8_[0].reset();
                lru8_[1].reset();
            }

            uint32_t getMRUWay() const
            {
                if ( !top_lru_bit_ ) {
                    return lru8_[1].getMRUWay() + (NUM_WAYS/2);
                }
                else {
                    return lru8_[0].getMRUWay();
                }
            }

            uint32_t getLRUWay() const
            {
                if ( top_lru_bit_ ) {
                    return lru8_[1].getLRUWay() + (NUM_WAYS/2);
                }
                else {
                    return lru8_[0].getLRUWay();
                }
            }

            void touchMRU(uint32_t way)
            {
                sparta_assert(way < NUM_WAYS);
                bool top_mru_bit         = (way>>3) & 0x1; // top_mru_bit is bit3
                uint32_t bottom_8_way    = way & 0x7;

                top_lru_bit_             = !top_mru_bit;
                lru8_[top_mru_bit].touchMRU(bottom_8_way);
            }

            void touchLRU(uint32_t way)
            {
                sparta_assert(way < NUM_WAYS);
                uint32_t bottom_8_way = way & 0x7;

                top_lru_bit_          = (way >>3) & 0x1; // top_lru_bit is bit3
                lru8_[ top_lru_bit_ ].touchLRU(bottom_8_way);
            }

            void lockWay(uint32_t way)
            {
                sparta_assert(way<num_ways_);
                sparta_assert(0);
            }

            ReplacementIF *clone() const { return new HybridPLRU_16_Replacement(); }
        private:
            static const uint32_t NUM_WAYS = 16;
            bool                     top_lru_bit_;
            HybridPLRU_8_Replacement lru8_[2];

        }; // class HybridPLRU_16_Replacment

    }; // namespace cache

}; // namespace sparta
#endif  // _HYBRID_PLRU_16_REPLACEMENT_H_
