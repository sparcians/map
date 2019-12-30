
#ifndef _SPA_CACHE_RANDOM_REPLACEMENT_H_
#define _SPA_CACHE_RANDOM_REPLACEMENT_H_

#include <cstdlib>
#include "cache/ReplacementIF.hpp"

namespace sparta
{

    namespace cache
    {

        class RandomReplacement : public ReplacementIF
        {
        public:
            RandomReplacement(uint32_t num_ways) :
                ReplacementIF(num_ways)
            {
            }

            ReplacementIF *clone() const
            {
                return new RandomReplacement(num_ways_);
            }

            void reset() {}

            void touchLRU(uint32_t way) { (void) way; }
            void touchMRU(uint32_t way) { (void) way; }
            void lockWay(uint32_t way) { (void) way; }
            uint32_t getLRUWay() const
            {
                return (random()%num_ways_);
            }
            uint32_t getMRUWay() const
            {
          return (random()%num_ways_);
            }
        };

    }; // namespace cache

}; // namespace sparta
#endif //  _SPA_CACHE_RANDOM_REPLACEMENT_H_
