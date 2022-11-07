

#pragma once

#include <cinttypes>
#include "cache/AddrDecoderIF.hpp"

namespace sparta
{

    namespace cache
    {

        // This class holds information about the cached item.  It
        // does not contain data.  User is to extend this class
        // to add data.   The cache library provides LineData.h
        // as an extension of the common use case where data is a
        // block of 2**N bytes of memory
        class BasicCacheItem
        {
        public:
            BasicCacheItem() :
                set_idx_(INVALID_VALUE_),
                way_num_(INVALID_VALUE_),
                addr_(0),
                tag_(0),
                addr_decoder_(nullptr)
            { }

            BasicCacheItem(const BasicCacheItem &) = default;

            // Required virtual constructor (rule of 3)
            virtual ~BasicCacheItem() = default;

            // The way in the cache set this item belongs
            // This method should be called only once when the
            // item is assigned to a way in the cache set.
            void setWayNum(uint32_t way_num)
            {
                assert(way_num_ == INVALID_VALUE_);
                way_num_ = way_num;
            }

            // The index of the CacheSet that contains this item
            // This method should be called only once during initialization
            void setSetIndex(uint32_t set_idx)
            {
                assert(way_num_ == INVALID_VALUE_);
                set_idx_ = set_idx;
            }

             // Set the address decoder
            void setAddrDecoder(const AddrDecoderIF *dec) { addr_decoder_ = dec; }

            // The item's address
            // This method is called when the item's address is changed
            void setAddr(uint64_t a)  {
                addr_ = addr_decoder_->calcBlockAddr(a);
                tag_  = addr_decoder_->calcTag(a);
            }

            uint64_t getAddr() const  { return addr_; }
            uint32_t getSetIndex() const  { return set_idx_; }
            uint32_t getWay()  const  { return way_num_; }
            uint64_t getTag()  const  { return tag_; }

        protected:
            uint32_t       set_idx_;
            uint32_t       way_num_;
            uint64_t       addr_;
            uint64_t       tag_;
            const AddrDecoderIF *addr_decoder_;
        private:
            static const uint32_t INVALID_VALUE_ = 0xFFFFFFFF;

        }; // class BasicCacheItem

    } // namespace cache

} // namespace sparta
