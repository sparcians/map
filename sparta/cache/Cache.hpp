

#pragma once

#include <vector>
#include "sparta/utils/MathUtils.hpp"
#include "cache/AddrDecoderIF.hpp"
#include "cache/DefaultAddrDecoder.hpp"
#include "cache/BasicCacheSet.hpp"
namespace sparta
{

    namespace cache
    {

        class ReplacementIF;

        template< class CacheItemT, class CacheSetT=BasicCacheSet<CacheItemT> >
        class Cache
        {
        public:
            typedef typename std::vector<CacheSetT>::iterator       iterator;
            typedef typename std::vector<CacheSetT>::const_iterator const_iterator;

            Cache( uint64_t cache_sz,
                   uint64_t item_sz,
                   uint64_t stride,
                   const CacheItemT  &default_line,
                   const ReplacementIF &rep,
                   bool cache_sz_unit_is_kb = true) :
                num_sets_(((cache_sz_unit_is_kb) ? (cache_sz*1024):(cache_sz))/(item_sz*rep.getNumWays())),
                num_ways_(rep.getNumWays())
           {
                assert( utils::is_power_of_2(item_sz) );
                assert( utils::is_power_of_2(stride) );

                default_addr_decoder_.reset(new DefaultAddrDecoder(cache_sz,
                                                                   item_sz,
                                                                   stride,
                                                                   num_ways_,
                                                                   cache_sz_unit_is_kb));
                addr_decoder_ = default_addr_decoder_.get();

                for (uint32_t i=0; i<num_sets_; ++i) {
                    sets_.push_back( CacheSetT( i,
                                                num_ways_,
                                                default_line,
                                                addr_decoder_,
                                                rep)
                                     );
                }

           }

            /*
             * \brief  Set the user provide address decoder
             */
            void setAddrDecoder(AddrDecoderIF *addr_decoder)
            {
                addr_decoder_ = addr_decoder;
                for (uint32_t i=0; i<num_sets_; ++i) {
                    sets_[i].setAddrDecoder(addr_decoder);
                }
            }

            /*
             * \brief Get the address decoder
             */
            AddrDecoderIF * getAddrDecoder() const
            {
                return addr_decoder_;
            }

            /*
             * \brief Get the cache set for given the address
             */
            CacheSetT &getCacheSet(uint64_t addr)
            {
                uint32_t set_idx = addr_decoder_->calcIdx(addr);
                assert(set_idx < num_sets_);
                return sets_[set_idx];
            }

            /*
             * \brief Get the cache set for given the address
             */
            const CacheSetT &peekCacheSet(uint64_t addr) const
            {
                uint32_t set_idx = addr_decoder_->calcIdx(addr);
                assert(set_idx < num_sets_);
                return sets_[set_idx];
            }

            /*
             * \brief Get the cache set for the given index
             */
            CacheSetT &getCacheSetAtIndex(uint set_idx)
            {
                assert(set_idx < num_sets_);
                return sets_[set_idx];
            }

            /*
             * \brief Get the cache set for the given index
             */
            CacheSetT &peekCacheSetAtIndex(uint set_idx) const
            {
                assert(set_idx < num_sets_);
                return sets_[set_idx];
            }

            /*!
             * \brief Get the cache item with the given address
             * \return Pointer to a valid line with the given address,
             *         nullptr if no matching valid item is found
             */
            CacheItemT *getItem(uint64_t addr)
            {
                uint64_t tag     = addr_decoder_->calcTag(addr);
                return getCacheSet(addr).getItem(tag);
            }

            /*!
             * \brief Get the cache item with the given address
             * \return Pointer to a valid line with the given address.
             */
            const CacheItemT *peekItem(uint64_t addr) const
            {
                uint64_t tag     = addr_decoder_->calcTag(addr);
                return peekCacheSet(addr).peekItem(tag);
            }

            CacheItemT *getItem(uint64_t addr, bool &is_cold_miss)
            {
                uint64_t tag     = addr_decoder_->calcTag(addr);
                return getCacheSet(addr).getItem(tag, is_cold_miss);
            }

            CacheItemT &getItemAtIndexWay(uint32_t set_idx, uint32_t way)
            {
                assert(set_idx < num_sets_);
                return sets_[set_idx].getItemAtWay(way);
            }

            /*!
             * \brief Get the reference to the LRU cache item given addr
             * \return Reference to cache item selected
             *         Item may or may not be valid, and may needs to be
             *         cast-out
             */
            CacheItemT &getLRUItem(uint64_t addr)
            {
                return getCacheSet(addr).getLRUItem();
            }

            const CacheItemT &peekLRUItem(uint64_t addr) const
            {
                return peekCacheSet(addr).peekLRUItem();
            }



            /*!
             * \brief Get the cache set's replacement interface.  Use
             *        this method to update the replacement policy
             */
            ReplacementIF *getReplacementIF(uint64_t addr)
            {
                return getCacheSet(addr).getReplacementIF();
            }

            uint32_t findInvalidWay(uint64_t addr) const
            {
                return peekCacheSet(addr).findInvalidWay();
            }

            uint32_t getNumWays() const {
                return num_ways_;
            }

            uint32_t getNumSets() const {
                return num_sets_;
            }

            iterator       begin() { return sets_.begin(); }
            iterator       end()   { return sets_.end(); }
            const_iterator begin() const { return sets_.begin(); }
            const_iterator end()   const { return sets_.end(); }
        private:
            Cache(const Cache &rhs);
            Cache &operator=(const Cache &rhs);

            std::unique_ptr<DefaultAddrDecoder> default_addr_decoder_;
            AddrDecoderIF   * addr_decoder_ = nullptr;

            const uint32_t                      num_sets_;
            const uint32_t                      num_ways_;
            std::vector<CacheSetT>  sets_;

        }; // class Cache

    }; // namespace cache

}; // namespace sparta

