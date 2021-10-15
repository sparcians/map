
#pragma once

#include "inttypes.h"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/MathUtils.hpp"

namespace sparta
{

    namespace cache 
    {

        class ReplacementIF
        {
        public:
            ReplacementIF(uint32_t num_ways) : num_ways_(num_ways), way_mask_(num_ways-1)
            {
                sparta_assert( num_ways > 0 );
            }

            virtual ~ReplacementIF() {}
            
            uint32_t getNumWays() const { return num_ways_; }

            // Reset LRU state
            virtual void reset()
            {
                assert(0);
            };
            
            // Clone the current object
            virtual ReplacementIF *clone() const = 0;

            // Touch way as LRU
            virtual void           touchLRU(uint32_t way) = 0;
            virtual void           touchLRU(uint32_t way, std::vector<uint32_t> way_order) = 0; // user-defined way order
    
            // Touch way as MRU
            virtual void           touchMRU(uint32_t way) = 0;
            virtual void           touchMRU(uint32_t way, std::vector<uint32_t> way_order) = 0; // user-defined way order
    
            // Lock way
            virtual void           lockWay(uint32_t way) = 0;

            // get the LRU way
            virtual uint32_t       getLRUWay() const = 0;
            virtual uint32_t       getLRUWay(std::vector<uint32_t> way_order) = 0; // user-defined way order
    
            // get the MRU way
            virtual uint32_t       getMRUWay() const = 0;
            virtual uint32_t       getMRUWay(std::vector<uint32_t> way_order) = 0; // user-defined way order
        protected:
            uint32_t num_ways_;
            uint32_t way_mask_;
        };

    }; // namespace cache

}; // namespace sparta

