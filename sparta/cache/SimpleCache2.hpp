#ifndef _SIMPLE_CACHE2_H__
#define _SIMPLE_CACHE2_H__


#include <string>
#include <iostream>
#include <sstream>
#include "cache/Cache.hpp"
#include "cache/ReplacementIF.hpp"
#include "cache/BasicCacheSet.hpp"
#include "cache/LineData.hpp"

namespace sparta
{

    namespace cache
    {

        template< class CacheItemT >
        class CacheSetWithNT : public BasicCacheSet<CacheItemT>
        {
        public:
            CacheSetWithNT( uint32_t             set_idx,
                            uint32_t             num_ways,
                            const CacheItemT    &default_line,
                            const AddrDecoderIF *addr_decoder,
                            const ReplacementIF &rep) :
                BasicCacheSet<CacheItemT>(set_idx, num_ways, default_line, addr_decoder, rep)
            {}
            void     setPreviousNTWay(uint32_t w) { previous_nt_way_ = w;}
            uint32_t getPreviousNTWay() const { return previous_nt_way_;}
        private:
            uint32_t previous_nt_way_ = 0;
        };// CacheSetWithNT

        class LineDataWithNT : public LineData
        {
        public:
            void reset( uint64_t addr, bool nt ) {
                LineData::reset(addr);
                setNT( nt );
            }
            LineDataWithNT(uint32_t sz) : LineData(sz) {}
            void setNT( bool nt ) { is_nt_ = nt; }
            bool isNT() const { return is_nt_; }
        private:
            bool is_nt_ = false;
        };  // LineDataWithNT


        template< class CacheItemT,
                  class CacheSetT  = CacheSetWithNT<CacheItemT> >
        class SimpleCache2
        {
        public:
            typedef typename Cache<CacheItemT, CacheSetT>::iterator iterator;
            typedef typename Cache<CacheItemT, CacheSetT>::const_iterator const_iterator;

            /**
             * \brief SimpleCache2 constructor
             * \param cache_size_kb Cache size in KB
             * \param item_sz Size of cache item, in bytes
             * \param stride How far apart are the items in memory, in bytes
             * \param default_line line used to initialize the all cache items during construction
             * \param rep an instance of the replacement algorithm.  Used to intialize cache during construction.
             *
             * Example construction of a 32KB cache, with 64-byte lines, 64-byte apart:
             *    dl1_(32, 64, 64, sparta::cache::LineData(64), sparta::cache::TreePLRUReplacement(16) ),
             *    dl1_(32768, 64, 64, sparta::cache::LineData(64), sparta::cache::TreePLRUReplacement(16), false),
             */
            SimpleCache2( uint64_t cache_sz,
                          uint64_t item_sz,
                          uint64_t stride,
                          const CacheItemT  &default_line,
                          const ReplacementIF &rep,
                          bool cache_sz_unit_is_kb = true) :
                cache_(cache_sz,
                       item_sz,
                       stride,
                       default_line,
                       rep,
                       cache_sz_unit_is_kb),
                addr_decoder_(cache_.getAddrDecoder())
            {
            }

            virtual ~SimpleCache2() {};


            const AddrDecoderIF * getAddrDecoder() const
            {
                return addr_decoder_;
            }


            /**
             *\return whether an addr is in the cache
             */
            bool isHit(uint64_t addr) const
            {
                const CacheItemT *line = cache_.peekItem(addr);
                return ( line != nullptr );
            }

            CacheSetT& getCacheSet(uint64_t addr) {
                return cache_.getCacheSet(addr);
            }

            // Get a line for replacement
            // Line is not aware of the NT state
            CacheItemT &getLineForReplacement(uint64_t addr)
            {
                return cache_.getLRUItem(addr);
            }

            // Get a line for replacement
            // Line is not aware of the NT state
            CacheItemT &getLineForReplacementWithInvalidCheck(uint64_t addr)
            {
                return cache_.getCacheSet(addr).getItemForReplacementWithInvalidCheck();
            }

            // Get a line for replacement
            // Line & the cache are aware of the NT state
            CacheItemT &getLineForReplacement(uint64_t addr, bool nt)
            {
                if ( nt ) {
                    auto &cache_set = cache_.getCacheSet(addr);
                    CacheItemT &way0_line = cache_set.getItemAtWay(0);
                    CacheItemT &way1_line = cache_set.getItemAtWay(1);
                    // a)  Fill into way 0,
                    //     1) if both way0 and way1 is non-NT
                    //     2) if both way0 and way1 is NT, way0 is older than way1
                    // b)  Fill into way 1,
                    //     1) if way0 is NT and way1 is non-NT
                    //     2) if both way0 and way1 is NT, way1 is older than way0
                    if ( !way0_line.isNT() ) {
                        return way0_line;
                    }
                    else if ( !way1_line.isNT() ) {
                        return way1_line;
                    }
                    else {
                        if ( cache_set.getPreviousNTWay() == 0 ) {
                            return way0_line;
                        }
                        else {
                            return way1_line;
                        }
                    }
                }
                else {
                    return cache_.getLRUItem(addr);
                }

            }

            /**
             *\return Pointer to line with addr.  nullptr is returned if not fould
             */
            CacheItemT * getLine(uint64_t addr)
            {
                return cache_.getItem(addr);
            }

            /**
             *\return Pointer to line with addr.  nullptr is returned if not fould
             */
             const CacheItemT * peekLine(uint64_t addr) const
            {
                return cache_.peekItem(addr);
            }

            void touchLRU(const CacheItemT &line)
            {
                ReplacementIF *rep = cache_.getCacheSetAtIndex( line.getSetIndex() ).getReplacementIF();
                rep->touchLRU( line.getWay() );
            }

            void touchMRU(const CacheItemT &line)
            {
                ReplacementIF *rep = cache_.getCacheSetAtIndex( line.getSetIndex() ).getReplacementIF();
                rep->touchMRU( line.getWay() );
            }

            void readWithMRUUpdate(const CacheItemT &line,
                                   uint64_t  addr,
                                   uint32_t  size,
                                   uint8_t   *buf)
            {
                uint64_t offset = addr_decoder_->calcBlockOffset(addr);
                bool rc         = line.read(offset, size, buf);
                sparta_assert(rc, "CacheLib:  error while reading line at addr=0x" << std::hex << addr);

                touchMRU(line);
            }

            void writeWithMRUUpdate(CacheItemT &line,
                                    uint64_t   addr,
                                    uint32_t   size,
                                    const uint8_t  *buf)
            {
                uint64_t offset = addr_decoder_->calcBlockOffset(addr);
                bool rc = line.write(offset, size, buf);
                sparta_assert(rc, "CacheLib:  error while writing to line at addr=0x" << std::hex << addr);
                line.setModified(true);

                touchMRU(line);
            }

            // Allocate 'line' as having the new 'addr'
            // 'line' doesn't know anything about NT
            void allocateWithMRUUpdate(CacheItemT &line,
                                       uint64_t   addr)
            {
                line.reset( addr );
                touchMRU( line );
            }

            // Allocate 'line' as having the new 'addr'
            // 'line' carries the NT state
            void allocateWithMRUUpdate(CacheItemT &line,
                                       uint64_t   addr,
                                       bool       nt)
            {
                line.reset( addr, nt );
                if (nt) {
                    auto &cache_set = cache_.getCacheSet(addr);
                    cache_set.setPreviousNTWay( line.getWay() );
                }
                touchMRU( line );
            }

            void invalidateLineWithLRUUpdate(CacheItemT &line)
            {
                static const uint64_t addr=0;
                static const bool     nt=false;
                line.reset( addr, nt );
                line.setValid( false );
                touchLRU( line );
            }

            void invalidateAll()
            {
                auto set_it = cache_.begin();
                for (; set_it != cache_.end(); ++set_it) {
                    auto line_it = set_it->begin();
                    for (; line_it != set_it->end(); ++line_it) {
                        line_it->setValid(false);
                    }
                    set_it->getReplacementIF()->reset();

                }
            }

            /**
             * determine if there are any open ways in the set.
             */
            bool hasOpenWay(const uint64_t addr)
            {
                return cache_.getCacheSet(addr).hasOpenWay();
            }

            uint32_t getNumWays() const {
                return cache_.getNumWays();
            }

            uint32_t getNumSets() const {
                return cache_.getNumSets();
            }


            /**
             * Provide iterators to the cache sets. From the sets you
             * can then iterators the lines similar to the way this is done
             * in the invalidateAll method.
             */
            iterator begin() { return cache_.begin(); }
            iterator end() { return cache_.end(); }
            const_iterator begin() const { return cache_.begin(); }
            const_iterator end() const { return cache_.end(); }

        protected:

            Cache<CacheItemT, CacheSetT> cache_;
            const AddrDecoderIF * const addr_decoder_;

         }; // class SimpleCache2



    }; // namespace cache

}; // namespace sparta

#endif // _SIMPLE_CACHE2_H__
