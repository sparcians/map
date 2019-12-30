/*
 */

#ifndef _BUBBLE_UP_REPLACEMENT_H_
#define _BUBBLE_UP_REPLACEMENT_H_

#include <vector>
#include "cache/ReplacementIF.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace sparta {
namespace cache {

/* This class implements a bubble-up replacement policy
   1. Insertion:  insert at LRU position, and then immediately bubble up.
                  (this is the same as inserting at next to LRU)
   2. Access:  bubble up.  If the accessed way has a rank of R, then
               accessed way's rank becomes R-1; the way with rank of R-1
               now has a rank of R.
*/
class BubbleUpReplacement : public ReplacementIF
{
public:

    BubbleUpReplacement (uint32_t num_ways) :
        ReplacementIF (num_ways)
    {
        reset();
    }

    virtual void reset()
    {
        // Initialize to 0,1,..,N-1 (way0 is top)
        ordered_ways_.resize (num_ways_, 0);
        for (uint32_t i = 0; i < num_ways_; ++i) {
            ordered_ways_[i] = i;
        }
    }

    virtual void touchLRU(uint32_t way)
    {
        // Bubble the way back one position
        sparta_assert(way < num_ways_);
        for (uint32_t i = 0; i < num_ways_; ++i) {
            if (ordered_ways_[i] == way) {
                if (i != (num_ways_-1)) {
                    ordered_ways_[i] = ordered_ways_[i+1];
                    ordered_ways_[i+1] = way;
                }
                break;
            }
        }
    }

    virtual void touchMRU(uint32_t way)
    {
        // Bubble the way forward one position
        sparta_assert(way < num_ways_);
        for (uint32_t i = 0; i < num_ways_; ++i) {
            if (ordered_ways_[i] == way) {
                if (i != 0) {
                    ordered_ways_[i] = ordered_ways_[i-1];
                    ordered_ways_[i-1] = way;
                }
                break;
            }
        }
    }

    virtual void lockWay(uint32_t way)
    {
        sparta_assert (way < num_ways_);
        sparta_assert (false);
    }

    virtual uint32_t getLRUWay() const
    {
        // Bottom way is at the back of list
        return ordered_ways_.back();
    }

    virtual uint32_t getMRUWay() const
    {
        // Top way is at the front of list
        return ordered_ways_.front();
    }

    ReplacementIF *clone() const
    {
        return new BubbleUpReplacement(num_ways_);
    }

private:

    // Sorted list of ways.  Top way at front and bottom at back
    std::vector<uint32_t> ordered_ways_;

}; // class BubbleUpReplacement

}; // namespace cache
}; // namespace sparta

#endif  // _BUBBLE_UP_REPLACEMENT_H_
