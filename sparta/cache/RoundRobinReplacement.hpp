#ifndef _ROUND_ROBIN_REPLACEMENT_H_
#define _ROUND_ROBIN_REPLACEMENT_H_

#include <list>
#include "cache/ReplacementIF.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace sparta 
{

    namespace cache
    {

        // this class implements round robin replacement 
        // algorithm using a simple round-robin counter.
        class RoundRobinReplacement : public ReplacementIF
        {
        public:

            RoundRobinReplacement(uint32_t num_ways) :
                ReplacementIF(num_ways)
            {
                RoundRobinReplacement::reset();
            }

            virtual void reset()
            {
                round_robin_ctr_ = 0;
            }
            
            // Note: the actual consumer of the round robin
            // policy must invoke this touchLRU function
            // with the right value of round-robin counter.
            virtual void touchLRU(uint32_t way)
            {
                sparta_assert(way < num_ways_);
                round_robin_ctr_ = way;
            }
    
            virtual void touchMRU(uint32_t way)
            {
                sparta_assert(way < num_ways_);
                round_robin_ctr_ = (way + 1) % num_ways_;
            }
    
            virtual void lockWay(uint32_t way)
            {
                sparta_assert(way < num_ways_);
                sparta_assert(0);
            }
    
            virtual uint32_t getLRUWay() const
            {
                return round_robin_ctr_;
            }
    
            // return the way 1 less than the Round-robin counter modulo number of ways.
            virtual uint32_t getMRUWay() const
            {
                return ( (round_robin_ctr_ + (num_ways_ - 1)) % num_ways_);
            }
    
            ReplacementIF *clone() const { return new RoundRobinReplacement(num_ways_); }

    
        private:
            uint32_t round_robin_ctr_;
    
        }; // class RoundRobinReplacement
    
    }; // namespace cache

}; // namespace sparta
#endif  // _ROUND_ROBIN_REPLACEMENT_H_
