

#pragma once

#include <vector>
#include "sparta/utils/MathUtils.hpp"
#include "cache/BasicCacheItem.hpp"
#include "cache/ReplacementIF.hpp"
#include "cache/AddrDecoderIF.hpp"

namespace sparta
{

    namespace cache {

        template <class CacheItemT>
        class BasicCacheSet
        {
        public:
            typedef typename std::vector<CacheItemT>::iterator iterator;
            typedef typename std::vector<CacheItemT>::const_iterator const_iterator;

            // constructor
            BasicCacheSet( uint32_t set_idx,
                           uint32_t num_ways,
                           const CacheItemT &default_line,
                           const AddrDecoderIF *addr_decoder,
                           const ReplacementIF &rep) :
                set_idx_(set_idx),
                num_ways_( num_ways )
            {
                replacement_policy_ = rep.clone();

                ways_.resize(num_ways, default_line);

                for (uint32_t i=0; i<num_ways; ++i) {
                    ways_[i].setSetIndex(set_idx_);
                    ways_[i].setWayNum(i);
                    ways_[i].setAddrDecoder(addr_decoder);
                }
            }

            // copy constructor
            BasicCacheSet(const BasicCacheSet &rhs) :
                set_idx_(rhs.set_idx_),
                num_ways_(rhs.num_ways_),
                replacement_policy_(rhs.replacement_policy_->clone()),
                ways_(rhs.ways_)
            {
            }

            // assignment operator
            BasicCacheSet<CacheItemT> &operator=(const BasicCacheSet<CacheItemT> &rhs)
            {
                if ( this != &rhs ) {
                    num_ways_ = rhs.num_ways_;
                    replacement_policy_ = rhs.replacement_policy_->clone();
                    ways_ = rhs.ways_;
                }

                return *this;
            }

            ~BasicCacheSet()
            {
                delete replacement_policy_;
            }

            // Get the set's set-index
            uint32_t getSetIndex() const { return set_idx_; }

            // Set the address decoder
            void setAddrDecoder(const AddrDecoderIF *addr_decoder)
            {
                for (uint32_t i=0; i<num_ways_; ++i) {
                    ways_[i].setAddrDecoder(addr_decoder);

                }
            }

            // Get the replacement policy
            // Use this method when the set's replacement policy is to be updated
            ReplacementIF *getReplacementIF()
            {
                return replacement_policy_;
            }

            // Get the const pointer to the item in the cache set given
            // the tag.  If no valid item with matching tag is found
            // nullptr is returned.
            const CacheItemT *peekItem(uint64_t tag) const
            {
                const CacheItemT *line = nullptr;
                for (uint32_t i=0;i<num_ways_; ++i) {

                    if ( ways_[i].isValid() &&
                         (ways_[i].getTag() == tag) ) {
                        line = &ways_[i];
                        break;
                    }
                }
                return line;
            }

            // Get the pointer to the item in the cache set given
            // the tag.  If no valid item with matching tag is found
            // nullptr is returned.
            CacheItemT *getItem(uint64_t tag)
            {
                CacheItemT *line = nullptr;
                for (uint32_t i=0;i<num_ways_; ++i) {
                    if ( ways_[i].isValid() ) {
                        if  ( ways_[i].getTag() == tag ) {
                            line = &ways_[i];
                            break;
                        }
                    }
                }
                return line;
            }


            // Similar to previous const version of getItem, except that this flavor
            // also determines (for misses) whether it was cold i.e. cache had invalid line(s)
            CacheItemT *getItem(uint64_t tag, bool &is_cold_miss)
            {
                CacheItemT *line = nullptr;
                is_cold_miss = false;
                for (uint32_t i=0;i<num_ways_; ++i) {
                    if ( ways_[i].isValid() ) {
                        if ( ways_[i].getTag() == tag ) {
                            line = &ways_[i];
                            is_cold_miss = false;
                            break;
                        }
                    }
                    else {
                        is_cold_miss = true;
                    }
                }
                return line;
            }


            CacheItemT &getItemAtWay(uint32_t way_idx)
            {
                assert(way_idx < num_ways_);
                CacheItemT &line = ways_[way_idx];
                return line;
            }

            // Get the reference to the LRU cache item
            // Note on usage:
            //   1.  Replacement of the LRU item is to be performed in place.
            //       Users of the library is expected to
            //       get and modify that line via the reference
            //   2.  Getting and updating the LRU item with
            //       new data must be done atomically--its an error to allow
            //       cache state to be changed between the two steps
            //
            CacheItemT &getLRUItem()
            {
                uint32_t victim_way = replacement_policy_->getLRUWay();
                return ways_[victim_way];
            }

            const CacheItemT &peekLRUItem() const
            {
                uint32_t victim_way = replacement_policy_->getLRUWay();
                return ways_[victim_way];
            }

            // XXX this method is deprecated
            CacheItemT &getItemForReplacement()
            {
                return getItemForReplacementWithInvalidCheck();
            }

            CacheItemT &getItemForReplacementWithInvalidCheck()
            {
                // First Select from invalid items.  Pick the first item found
                uint32_t victim_way = findInvalidWay();

                if (victim_way >= num_ways_) {
                    victim_way = replacement_policy_->getLRUWay();
                }

                CacheItemT &victim_item = ways_[victim_way];
                return victim_item;
            }

            uint32_t findInvalidWay() const
            {
                for (uint32_t i=0;i<num_ways_; ++i) {

                    if ( ! ways_[i].isValid() ) {
                        return i;
                    }
                }
                return num_ways_;
            }

            /**
            * Search for invalid in user-defined way order.
            */
            uint32_t findInvalidWay(const std::vector<uint32_t> &way_order) const
            {
                sparta_assert(!way_order.empty(), "way_order passed is empty");

                for ( auto i:way_order ) {
                    if ( !ways_[i].isValid() ) {
                        return i;
                    }
                }
                return num_ways_;
            }

            /**
             * Determine if the cache set has any open ways.
             */
            bool hasOpenWay() const
            {
                return findInvalidWay() != num_ways_;
            }

            iterator       begin() { return ways_.begin(); }
            iterator       end()   { return ways_.end(); }
            const_iterator begin() const { return ways_.begin(); }
            const_iterator end()   const { return ways_.end(); }
        protected:
            const uint32_t          set_idx_;
            const uint32_t          num_ways_;
            ReplacementIF          *replacement_policy_;
            std::vector<CacheItemT> ways_;
        }; // class Cache

    }; // namespace cache

}; // namespace sparta

