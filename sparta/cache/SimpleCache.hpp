#pragma once


#include <string>
#include <iostream>
#include <sstream>
#include "cache/BlockingMemoryIF.hpp"
#include "cache/Cache.hpp"
#include "cache/ReplacementIF.hpp"

namespace sparta
{

    namespace cache
    {

        template< class CacheItemT >
        class SimpleCache  : public sparta::BlockingMemoryIF
        {
        public:




            /**
             * \brief SimpleCache constructor
             * \param cache_size_kb Cache size in KB
             * \param item_sz Size of cache item, in bytes
             * \param stride How far apart are the items in memory, in bytes
             * \param default_line line used to initialize the all cache items during construction
             * \param rep an instance of the replacement algorithm.  Used to intialize cache during construction.
             *
             * Example construction of a 32KB cache, with 64-byte lines, 64-byte apart:
             *    dl1_(32, 64, 64, sparta::cache::LineData(64), sparta::cache::TreePLRUReplacement(16) ),
             * By default, cache in in write-back, and no-write-allocate mode.  For write-through,
             * call setWriteThroughMode(true).  For write-allocate, call setWriteAllocateMode(true)
             */
            SimpleCache( uint64_t cache_sz_kb,
                         uint64_t item_sz,
                         uint64_t stride,
                         const CacheItemT  &default_line,
                         const ReplacementIF &rep) :
                cache_(cache_sz_kb,
                       item_sz,
                       stride,
                       default_line,
                       rep),
                addr_decoder_(cache_.getAddrDecoder()),
                is_write_through_(false),
                is_write_allocate_(false),
                stat_num_castouts_(0),
                stat_num_reloads_(0),
                stat_num_reads_(0),
                stat_num_writes_(0),
                stat_num_read_misses_(0),
                stat_num_write_misses_(0),
                stat_num_write_next_level_(0),
                stat_num_getline_misses_(0)
            {
            }

            virtual ~SimpleCache() {};


            const AddrDecoderIF * getAddrDecoder() const
            {
                return addr_decoder_;
            }

            /**
             *\brief Set the cache's write through mode
             */
            void setWriteThroughMode(bool wt)  { is_write_through_ = wt; }

            /**
             *\brief Set the cache's write allocate mode
             * Write-allocate means a line is allocated on a write miss.
             */
            void setWriteAllocateMode(bool wa) { is_write_allocate_ = wa; }

            /**
             *\return whether an addr is in the cache
             */
            bool isHit(uint64_t addr) const
            {
                const CacheItemT *line = cache_.peekItem(addr);
                return ( line != nullptr );
            }

            bool isHitWithCastout(uint64_t addr, bool &need_castout, uint64_t &castout_addr, bool &is_dirty) const
            {
                need_castout = false;
                const CacheItemT *line = cache_.peekItem(addr);
                if ( line == nullptr )
                {
                    const CacheItemT &victim_line = cache_.peekLRUItem(addr);
                    if ( victim_line.isValid()  )
                    {
                        need_castout = true;
                        is_dirty = victim_line.isModified();
                        castout_addr = victim_line.getAddr();
                    }
                }

                return ( line != nullptr );
            }

            /**
             *\return whether the read succeeded
             */
            bool read(uint64_t addr,
                      uint32_t size,
                      uint8_t  *buf)
            {
                ++ stat_num_reads_;
                CacheItemT *line = cache_.getItem(addr);
                if ( line == nullptr )
                {
                    ++ stat_num_read_misses_;
                    CacheItemT &victim_line = replaceLine_(addr);
                    line = &victim_line;
                }

                // update replacement
                uint32_t line_way = line->getWay();
                ReplacementIF *rep = cache_.getReplacementIF(addr);
                rep->touchMRU(line_way);

                uint64_t offset = addr_decoder_->calcBlockOffset(addr);
                bool rc = line->read(offset, size, buf);
                return rc;
            }

            /**
             *\return whether the write succeeded
             */
            bool write(uint64_t addr,
                       uint32_t size,
                       const uint8_t  *buf)
            {
                ++ stat_num_writes_;
                bool need_write_to_next_level = is_write_through_;
                CacheItemT *line = cache_.getItem(addr);
                if ( line == nullptr )
                {
                    ++ stat_num_write_misses_;
                    need_write_to_next_level |= !is_write_allocate_;

                    if ( is_write_allocate_ )
                    {
                        CacheItemT &victim_line = replaceLine_(addr);
                        line = &victim_line;
                    }
                    else {
                        need_write_to_next_level = true;
                    }

                }

                bool rc = true;
                if ( line != nullptr ) {
                    // update replacement
                    uint32_t line_way = line->getWay();
                    ReplacementIF *rep = cache_.getReplacementIF(addr);
                    rep->touchMRU(line_way);

                    uint64_t offset = addr_decoder_->calcBlockOffset(addr);
                    rc &= line->write(offset, size, buf);
                    line->setModified(true);
                }

                if ( need_write_to_next_level ) {
                    writeNextLevel_(addr, size, buf);
                }
                return rc;
            }

            /**
             *\return Pointer to line with addr.  If line is not already
             *        in cache, a line is allocated for that address.
             *        MRU is updated.
             */
            CacheItemT * getLine(uint64_t addr)
            {
                CacheItemT *line = cache_.getItem(addr);
                if ( line == nullptr )
                {
                    ++ stat_num_getline_misses_;
                    CacheItemT &victim_line = replaceLine_(addr);
                    line = &victim_line;
                }

                sparta_assert( line != nullptr, "SimpleCache: got nullptr");

                // update replacement
                uint32_t line_way = line->getWay();
                ReplacementIF *rep = cache_.getReplacementIF(addr);
                rep->touchMRU(line_way);

                return line;
            }

            const CacheItemT * peekLine(uint64_t addr) const
            {
                return cache_.peekItem(addr);
            }

            void invalidateLine(uint64_t addr)
            {
                CacheItemT *line = cache_.getItem(addr);
                if ( line == nullptr )
                {
                    sparta_assert( line != nullptr, "SimpleCache: got nullptr");
                }

                line->setValid(false);
                cache_.getReplacementIF(line->getAddr())->touchLRU(line->getWay());

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



            void resetStats()
            {
                stat_num_castouts_ = 0;
                stat_num_reloads_ = 0;
                stat_num_reads_ = 0;
                stat_num_writes_ = 0;
                stat_num_read_misses_ = 0;
                stat_num_write_misses_ = 0;
                stat_num_write_next_level_ = 0;
                stat_num_getline_misses_ = 0;
            }

            // Notes on SimpleCache Stats:  they are an approximation only.  Use at your own risk.
            const uint64_t& getNumGetLineMisses() const { return stat_num_getline_misses_; };
            const uint64_t& getNumCastouts() const { return stat_num_castouts_; };
            const uint64_t& getNumReloads() const { return stat_num_reloads_; }
            const uint64_t& getNumReads() const { return stat_num_reads_; }
            const uint64_t& getNumWrites() const { return stat_num_writes_; }
            const uint64_t& getNumReadMisses() const { return stat_num_read_misses_; }
            const uint64_t& getNumWriteMisses() const { return stat_num_write_misses_; }
            const uint64_t& getNumWriteNextLevel() const { return stat_num_write_next_level_; }
            std::string getStatDisplayString() const {
                std::stringstream strstream;
                strstream << "  num_reads:            " << stat_num_reads_ << std::endl;
                strstream << "  num_writes:           " << stat_num_writes_ << std::endl;
                strstream << "  num_read_misses:      " << stat_num_read_misses_ << std::endl;
                strstream << "  num_write_misses:     " << stat_num_write_misses_ << std::endl;
                strstream << "  num_castouts:         " << stat_num_castouts_ << std::endl;
                strstream << "  num_reloads:          " << stat_num_reloads_ << std::endl;
                strstream << "  num_write_next_level: " << stat_num_write_next_level_ << std::endl;
                strstream << "  num_getline_misses:   " << stat_num_getline_misses_ ;
                return strstream.str();
            }

        protected:

            virtual void castout_(const CacheItemT &line)
            {
                (void) line;
                ++ stat_num_castouts_;
            }

            virtual void reload_(uint64_t blk_addr, CacheItemT &line)
            {
                ++ stat_num_reloads_;

                // Make sure item is valid and updated with new address
                line.setValid(true);
                line.setAddr(blk_addr);
                line.setModified(false);

            }

            virtual void writeNextLevel_(uint64_t addr,
                                         uint32_t size,
                                         const uint8_t *buf)
            {
                (void) addr;
                (void) size;
                (void) buf;
                ++ stat_num_write_next_level_;
            }


            Cache<CacheItemT> cache_;
            const AddrDecoderIF * const addr_decoder_;
            bool  is_write_through_;
            bool  is_write_allocate_;


            // Note:  SimpleCache cannot keep accurate statistics because
            // it does not have all the contextual information of an access.
            // These stats are an approximation only.  User of SimpleCache
            // should keep their own statistics.
            mutable uint64_t stat_num_castouts_;
            mutable uint64_t stat_num_reloads_;
            mutable uint64_t stat_num_reads_;
            mutable uint64_t stat_num_writes_;
            mutable uint64_t stat_num_read_misses_;
            mutable uint64_t stat_num_write_misses_;
            mutable uint64_t stat_num_write_next_level_;
            mutable uint64_t stat_num_getline_misses_;

        private:



            CacheItemT &replaceLine_(uint64_t addr)
            {
                CacheItemT &victim_line = cache_.getLRUItem(addr);
                if (victim_line.isValid() &&
                    victim_line.isModified() )
                {
                    castout_(victim_line);
                }

                uint64_t blk_addr = addr_decoder_->calcBlockAddr(addr);
                reload_(blk_addr, victim_line);

               return victim_line;
            }

        }; // class SimpleCache



    }; // namespace cache

}; // namespace sparta

