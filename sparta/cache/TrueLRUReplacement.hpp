
#ifndef _TRUE_LRU_REPLACEMENT_H_
#define _TRUE_LRU_REPLACEMENT_H_

#include <list>
#include "cache/ReplacementIF.hpp"
#include "sparta/utils/SpartaAssert.hpp"
namespace sparta 
{

    namespace cache
    {

        // This class implements a true LRU replacement algorithm using stl list
        class TrueLRUReplacement : public ReplacementIF
        {
        public:

            TrueLRUReplacement(uint32_t num_ways) :
                ReplacementIF(num_ways)
            {
                reset();
            }

            virtual void reset()
            {
                // Clear the contents of the ways before initializing
                ordered_ways_.clear();
                // Initialize to 0,1,..,N-1 (way0 is LRU)
                for(uint32_t i=0; i<num_ways_; ++i) {
                    ordered_ways_.push_front(i);
                }
                sparta_assert(ordered_ways_.size() == num_ways_);
            }
            
            virtual void touchLRU(uint32_t way)
            {
                // Remove the specified way and put it at end of list
                sparta_assert(way < num_ways_);
                ordered_ways_.remove(way);
                ordered_ways_.push_back(way);
                sparta_assert(ordered_ways_.size() == num_ways_);
            }
    
            virtual void touchMRU(uint32_t way)
            {
                // Remove the specified way and put it at fron of list
                sparta_assert(way < num_ways_);
                ordered_ways_.remove(way);
                ordered_ways_.push_front(way);
                sparta_assert(ordered_ways_.size() == num_ways_);
            }
    
            virtual void lockWay(uint32_t way)
            {
                sparta_assert(way < num_ways_);
                sparta_assert(0);
            }
    
            virtual uint32_t getLRUWay() const
            {
                // LRU way is at the back of list
                return ordered_ways_.back();
            }
    
            virtual uint32_t getMRUWay() const
            {
                // LRU way is at the front of list
                return ordered_ways_.front();
            }
    
            ReplacementIF *clone() const { return new TrueLRUReplacement(num_ways_); }

    
        private:
            std::list<uint32_t> ordered_ways_;  // Sorted list of ways.  MRU way is at front, LRU at back
    
        }; // class TrueLRUReplacement
    
    }; // namespace cache

}; // namespace sparta
#endif  // _TRUE_LRU_REPLACEMENT_H_
